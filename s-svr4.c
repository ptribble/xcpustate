/* System dependent file for System V Release 4.0      */
/* Tested on Unixware 1.1 (SVR4.2)                     */

/* $Id: s-svr4.c,v 1.4 2004/05/29 18:21:29 jdd Exp $ */

#include "s.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <nlist.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/immu.h>

#include <sys/utsname.h>
#include <sys/sysinfo.h>

#include <errno.h>

extern int   diskflag, cpuflag;
extern char *kernelSymbols;

extern void draw_bar(int bar_num, int *states, int num_states);
extern void display_bars(int);



/*  The following defines are used to index the variable kernel_nl[] */
#define KERNEL_UTSNAME   0
#define KERNEL_SYSINFO   1
#define KERNEL_LBOLT     2

/* kernel_nl[]  --  list of symbol addresses needed.
*/
static struct nlist kernel_nl[] =
{
  { "utsname",       0 },
  { "sysinfo",       0 },
  { "lbolt",         0 },

  { 0,               0 }
};



/* kernel_symbols[] -- list of places to look for the running kernel's symbols
*/
char *kernel_symbols[] = {
  "/stand/unix.diag",
  "/stand/unix",
  "/stand/unix.old",
  0,                   /* used by -kernel option  */

  0
};



enum stype
{
  CPU_STATE,
};


enum stype      state = CPU_STATE;
int             kmem;              /* file descriptor for /dev/kmem      */
char           *kernel_name;       /* name of kernel (/stand/unix)       */
struct utsname  nodename;          /* nodename.nodename                  */
int             kmem_is_mapped;
int             kmem_is_reading;
int             kmem_can_map    = 1;


/*  Fake kernel memory mapping information
*/
typedef struct kmem_map
{
  struct kmem_map *km_next;
  void            *km_addr;
  size_t           km_size;
  char             km_buf[4];
} kmem_map_t;

kmem_map_t *kmem_list;             /* linked list of fake memory mappings */

/*  CPU Information
*/
int             kncpu;             /* maximum number of cpu's on system  */
int             ncpus;             /* number of cpu's which exist        */
time_t         *lastcpu;           /* last snapshot of each cpu's times  */
struct sysinfo *cpuinfo;



#define CPU_STATE_SZ   sizeof(cpuinfo[0].cpu)

/* Computes the time spent in the specified state.
*/

#define delta(state) \
   ((int) cpuinfo[cpu].cpu[state] - cputime[state])


/* Save the state times for a specific cpu
*/
#define savecpuinfo(cpu) \
   memcpy(cputime, cpuinfo[cpu].cpu, CPU_STATE_SZ);


/* The size of the local state time array
*/
#define NSTATES   CPU_STATE_SZ
#define CPUSTATES (CPU_STATE_SZ / sizeof(cpuinfo[0].cpu[0]))

/* Display an error message based upon errno and
   return 0 to caller
*/
#define ERR_EXIT(msg) \
{               \
   perror(msg); \
   return 0;    \
}


/* Read a specified number of bytes of kernel memory
   and report any error conditions which might arise.
*/
#define seekreadok(desc, seekpos, buf, sz, msg) \
  if (-1 == lseek(desc, (off_t)seekpos, SEEK_SET))  \
    ERR_EXIT("lseeking " msg);               \
 \
  if ((sz) != read(desc, buf, (sz)))         \
    ERR_EXIT("reading " msg);

#define seekreaderr(desc, seekpos, buf, sz, msg) \
   ( -1  == lseek(desc, (off_t)seekpos, SEEK_SET) ||  \
    (sz) != read(desc, buf, (sz)))


/* fakethis()  -- provide faked out memory mappings if unable to mmap kernel.
*/
void *
fakethis(void *loc, size_t sz)
{
  kmem_map_t *mapping = (kmem_map_t *)malloc( sizeof(kmem_map_t) + sz );

  if (!mapping)
    return 0;

  mapping->km_addr = loc;
  mapping->km_size = sz;
  mapping->km_next = kmem_list;

  seekreadok(kmem, mapping->km_addr, mapping->km_buf, mapping->km_size,
	     "faking mmap");

  if (!kmem_list && kmem_can_map)
    fprintf(stderr, "Unable to mmap kernel addresses, using lseek/read\n");

  kmem_list = mapping;
  kmem_is_reading++;

  return (void *) mapping->km_buf;
}

/* refreshfakes()  --  Refresh fake mmap snapshots.
*/
int
refreshfakes(void)
{
  kmem_map_t *mapping;
  int         errs    = 0;

  for (mapping = kmem_list; mapping; mapping = mapping->km_next)
    if (seekreaderr(kmem, mapping->km_addr, mapping->km_buf, mapping->km_size,
		    "refreshing fake mmap's"))
      errs++;

  return errs;
}

/*  Given an address (and size in bytes) in kernel virtual memory,
    this function uses the mmap function to provide a mapping for
    those addresses in the current process.  The result is either
    a pointer to the data in this processes virtual space or
    (void *)-1 if unable to provide the mapping.
*/
void *
mapthis(int desc, void *locp, unsigned long sz)
{
  unsigned long pgoffs = (unsigned long) locp & POFFMASK;
  unsigned long pgstrt = (unsigned long) locp & PG_ADDR;
  unsigned long pgend  = ((unsigned long) locp + sz) & PG_ADDR;
  unsigned long mapsz  = pgend - pgstrt + NBPP;
  char         *map    = (char *)-1;

  if (kmem_can_map)
    map = mmap(0, mapsz, PROT_READ, MAP_SHARED, desc, pgstrt);

  if ((char *)-1 == map)
    {
      if (kmem_is_mapped)
	fprintf(stderr, "Mixing mapping and lseek/read kernel memory?\n");

      map = fakethis(locp, sz);
      return map ? map : (void *)-1;
    }

  kmem_is_mapped++;
  return (void *) (map + pgoffs);
}



/* get_kernel_symbols  --  Find the file containing the symbol definitions
                           for the currently running kernel.

   The kernel must be running out of /stand (on an svr4 system) and the
   symbol value for utsname must match the name returned by the uname()
   system call.

   This function is called given a list of possible files from /stand which
   may be running and returns the name of the first file which appears
   to be correct.
*/

char *
get_kernel_symbols(int kmemfd, char **knamelist)
{
  char **cpp;
  int    nf, errs;
  int    nsz = strlen(nodename.nodename) + 1;
  struct utsname nname;

  if (1 >= nsz)
    fprintf(stderr, "xcpustate: Empty node name\n");

  if (kernelSymbols)
    *knamelist = kernelSymbols;

  if (0 == *knamelist) knamelist++;  /* skip over initial NULL pointer */

  for (cpp = knamelist; *cpp; cpp++)
    {
      for (nf = 0; kernel_nl[nf].n_name; nf++)
	kernel_nl[nf].n_value = 0;

      nf = nlist(*cpp, kernel_nl);
      if (-1 == nf || 0 == kernel_nl[KERNEL_UTSNAME].n_value)
	continue;

      if (!seekreaderr(kmemfd, kernel_nl[KERNEL_UTSNAME].n_value,
		     &nname, sizeof(struct utsname), "utsname") &&
	  0 == strncmp(nodename.nodename, nname.nodename, nsz))
	{
	  for (errs = nf = 0; kernel_nl[nf].n_name; nf ++)
	    if (0 == kernel_nl[nf].n_value)
	      {
		fprintf(stderr, "kernel symbol `%s' not found\n",
			kernel_nl[nf].n_name);
		errs++;
	      }

	  if (errs)
	    return 0;

	  return *cpp;
	}
    }

  fprintf(stderr, "xcpustate: No kernel symbol file matches running kernel\n");
  return 0;
}



/* init_cpu_init()  --  Initialize for CPU display
*/
int
init_cpu_info(void)
{
  int nf;

  cpuinfo    = mapthis(kmem, (void *)kernel_nl[KERNEL_SYSINFO].n_value,
		       sizeof(struct sysinfo));
  if (-1 == (int)cpuinfo)
    ERR_EXIT("mapping sysinfo");

  ncpus = kncpu = 1;

  nf = kncpu * CPU_STATE_SZ;
  lastcpu = (time_t *) malloc(nf);
  memset(lastcpu, 0, nf);

  return ncpus;
}


/* Called by -version */
void
version()
{
	printf("SVR4: maxdisk=0\n");
}


/* Called at the beginning to inquire how many bars are needed
*/
int
num_bars(void)
{
  kmem   = open("/dev/kmem", O_RDONLY);
  if (-1 == kmem)
    ERR_EXIT("opening /dev/kmem");

  if (-1 == uname(&nodename))
    ERR_EXIT("uname");

  kernel_name = get_kernel_symbols(kmem, kernel_symbols);
  if (!kernel_name)
    return 0;

  if (diskflag)
    {
      fprintf(stderr, "Only cpu state information is supported\n");
      return 0;
    }

  return init_cpu_info();
}


/*
 * Indicates how many levels each bar has.  For most machines, each bar will
 * have the same stuff.  But one can, for instance, display memory use on one
 * bar, processor levels on others, etc.
 */
void
bar_items(nbars, items)
int nbars;
int items[];    /* nbars items in this */
{
    items[0] = CPUSTATES;
}


/* Called after num_bars to ask for bar names */
char **
label_cpu_bars(nbars)
  int    nbars;
{
  char **names;
  int    hsz   = strlen(nodename.nodename);
  int    rowsz = (hsz +   1 +   2 + 4) * kncpu;
  int    sz    = nbars * rowsz;
  int    cpu, i;
  char  *cp;

  shorten(nodename.nodename);

  names  = (char **)malloc(sz + sizeof(char *) * nbars);

  cp     = (char *)names + sizeof(char *) * nbars;

  for (cpu = i = 0; i < kncpu; i++)
    {
      names[cpu++] = cp;

      if (1 == kncpu)
	sprintf(cp, "%s", nodename.nodename);
      else if (0 == i)
	sprintf(cp, "%s #%d", nodename.nodename, i);
      else
	sprintf(cp, "%*s #%d", hsz, "", i);

      cp += rowsz;
    }

  return names;
}



char **
label_bars(nbars)
  int    nbars;
{
  switch (state)
    {
    case CPU_STATE:  return label_cpu_bars(nbars);
    }

  return NULL;
}




/*  Called after the bars are created to do machine dependent inits */
void
init_bars(nbars)
  int nbars;
{
  (void) display_bars(nbars);
}




/*
   This function is called every interval to compute and display
   the bars.  It should call draw_bar() with the bar number, the array of
   integer values to display in the bar, and the number of values in the
   array.
*/
void
display_cpu_bars(nbars)
  int nbars;
{
  int states[NSTATES];
  int nstates;
  int cpu, i;
  time_t *cputime = lastcpu;

  for (i = cpu = 0; i < kncpu && cpu < nbars; i++)
    {
      nstates = 0;

      if (kncpu)
	{
	  states[nstates++] = delta(CPU_IDLE);
	  states[nstates++] = delta(CPU_USER);
	  states[nstates++] = delta(CPU_KERNEL);
	  states[nstates++] = delta(CPU_WAIT) + delta(CPU_SXBRK);
#if 0
	  states[nstates++] = delta(CPU_SXBRK);
#endif
	}
      else
	{
	  states[nstates++] = 100;  /* CPU_IDLE   */
	  states[nstates++] =   0;  /* CPU_USER   */
	  states[nstates++] =   0;  /* CPU_KERNEL */
	  states[nstates++] =   0;  /* CPU_WAIT   */
#if 0
	  states[nstates++] =   0;  /* CPU_SXBRK  */
#endif
	}
      savecpuinfo(cpu);
      draw_bar(cpu, states, nstates);

      cpu++;
      cputime += CPU_STATE_SZ / sizeof(time_t);
    }
}



void
display_bars(nbars)
  int nbars;
{
  if (kmem_list)
    refreshfakes();

  switch (state)
    {
    case CPU_STATE:  display_cpu_bars(nbars);
    }

  return;
}
