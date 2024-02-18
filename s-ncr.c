/* System dependent file for NCR System V Release 4 MP/RAS     */

/* $Id: s-ncr.c,v 1.3 2004/05/29 18:21:29 jdd Exp $ */

#include "s.h"
#include <stdio.h>
#include <nlist.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/unistd.h>
#include <sys/mman.h>

#include <sys/utsname.h>
#include <sys/pic.h>
#include <sys/sysinfo.h>
#include <sys/mp/percpu.h>
#include <sys/immu.h>

#include <sys/scsi/cs.h>

#include <errno.h>

extern int   diskflag, cpuflag;
extern char *kernelSymbols;

extern void draw_bar(int bar_num, int *states, int num_states);
extern void display_bars(int);

struct cpu_names
{
  int   cpu_type;
  char  cpu_type_name[8];
};

const struct cpu_names  cpu_model_names[] =
{
  { CPU_386,     "i386" },
  { CPU_486,     "i486" },
#ifdef CPU_586
  { CPU_586,     "Pentium" },
#elif defined CPU_Pentium
  { CPU_Pentium, "Pentium" },
#endif
};


/*  The following defines are used to index the variable kernel_nl[] */
#define KERNEL_UTSNAME   0
#define KERNEL_PERCPUP   1
#define KERNEL_CPU_INFOS 2
#define KERNEL_NCPU      3
#define KERNEL_SCSI_STAT 4
#define KERNEL_LBOLT     5

/* kernel_nl[]  --  list of symbol addresses needed.
*/
static struct nlist kernel_nl[] =
{
  { "utsname",       0 },
  { "percpup",       0 },
  { "cpu_infos",     0 },
  { "ncpu",          0 },
  { "SCSI_Stat_Ptr", 0 },
  { "lbolt",         0 },

  { 0,               0 }
};



/* kernel_symbols[] -- list of places to look for the running kernel's symbols
*/
char *kernel_symbols[] = {
  0,                   /* used by -kernel option  */
  "/stand/unix.diag",
  "/stand/unix",
  "/stand/unix.old",

  0
};
  


enum stype
{
  CPU_STATE,
  DISK_STATE,
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
percpu_t      **kpercpup;          /* array of percpu_t pointers         */
percpu_t      **xpercpup;          /* percpu_t pointers when mmapped     */
percpu_t      **mypercpup    = 0;  /* Pointers to each cpu's percpu area */
time_t         *lastcpu;           /* last snapshot of each cpu's times  */
cpu_info_t     *kcpu_infos;        /* State of each cpu                  */


/* SCSI Disk Information
*/
int             ndisks;
long            kscsi_stat_ptr_addr;
long            kaddr;

typedef struct diskinfo
{
  dev_t            di_dev;
  SCSI_Device_t   *di_kscsi;
  StatStructure_t  di_stats;
} diskinfo_t;

long           *lboltp;
long            last_lbolt, new_lbolt;
diskinfo_t     *kscsi_stat;
SCSI_Device_t  *lscsi_stat;
SCSI_Device_t   tscsi_stat;



/* The size of the array which holds the cpu
   execution information.
*/
#if defined( CPU_Pentium )
#define  MPSYSINFO     c_sysinfo
#else
#define  MPSYSINFO     c_mpinfo
#endif

#define CPU_STATE_SZ   sizeof(mypercpup[0]->MPSYSINFO.cpu)


/* Computes the time spent in the specified state.
*/

#define delta(state) \
   ((int) mypercpup[cpu]->MPSYSINFO.cpu[state] - cputime[state])


/* Save the state times for a specific cpu
*/
#define savecpuinfo(cpu) \
   memcpy(cputime, mypercpup[cpu]->MPSYSINFO.cpu, CPU_STATE_SZ);


/* The size of the local state time array
*/
#define NSTATES   CPU_STATE_SZ
#define CPUSTATES (CPU_STATE_SZ / sizeof(mypercpup[0]->MPSYSINFO.cpu[0]))-1

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
  if (-1 == lseek(desc, seekpos, SEEK_SET))  \
    ERR_EXIT("lseeking " msg);               \
 \
  if ((sz) != read(desc, buf, (sz)))         \
    ERR_EXIT("reading " msg);

#define seekreaderr(desc, seekpos, buf, sz, msg) \
   ( -1  == lseek(desc, seekpos, SEEK_SET) ||  \
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

  if (!*knamelist) knamelist++;  /* skip over initial NULL pointer */

  for (cpp = knamelist; *cpp; cpp++)
    {
      for (nf = 0; kernel_nl[nf].n_name; nf++)
	kernel_nl[nf].n_value = 0;

      nf = nlist(*cpp, kernel_nl);
      if (-1 == nf || 0 == kernel_nl[KERNEL_UTSNAME].n_value)
	continue;

      if (!seekreaderr(kmemfd, kernel_nl[KERNEL_UTSNAME].n_value,
		     &nname.nodename, nsz, "utsname") &&
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

  seekreadok(kmem,
	     kernel_nl[KERNEL_NCPU].n_value,
	     &kncpu,
	     sizeof(kncpu),
	     "/dev/kmem [ncpu]");

  if (kncpu > 64 || kncpu < 0)
    {
      fprintf(stderr, "Unlikely value for ncpu found.\n");
      return 0;
    }

  kcpu_infos = mapthis(kmem,
		       (void *)kernel_nl[KERNEL_CPU_INFOS].n_value,
		       kncpu*sizeof(cpu_info_t));
  if (-1 == (int)kcpu_infos)
    ERR_EXIT("mapping cpu_infos[]");

  kpercpup   = mapthis(kmem,
		       (void *)kernel_nl[KERNEL_PERCPUP].n_value,
		       kncpu * sizeof(percpu_t *));
  if (-1 == (int)kpercpup)
    ERR_EXIT("mapping percpup[]");

  mypercpup = (percpu_t **) malloc( sizeof(percpu_t *) * kncpu );
  if (!mypercpup)
    ERR_EXIT("allocating memory");

  xpercpup   = (percpu_t  **) malloc( kncpu * sizeof(percpu_t *));
  if (!kpercpup)
    ERR_EXIT("allocating memory");

  memset(xpercpup, 0, kncpu * sizeof(percpu_t *));

  for (ncpus = nf = 0;  nf < kncpu; nf++)
    if (kcpu_infos[nf].cpu_flag & CPU_EXISTS)
      {
	if (kpercpup[nf])
	  {
	    xpercpup[ncpus] = kpercpup[nf];
	    mypercpup[ncpus] = mapthis(kmem,
				       xpercpup[ncpus],
				       sizeof(*(kpercpup[nf])));
	    if (-1 == mypercpup[ncpus])
	      ERR_EXIT("mapping percpu area");
	  }
	ncpus++;
      }

  nf = ncpus * CPU_STATE_SZ;
  lastcpu = (time_t *) malloc(nf);
  memset(lastcpu, 0, nf);

  return ncpus;
}


/* scsitype()  --  return string naming scsi device type.
*/
char *
scsitype(int t)
{
  switch (t)
    {
    case SCSI_DAD_TYPE:     return "disk";
    case SCSI_SAD_TYPE:     return "tape";
    case SCSI_PRT_TYPE:     return "printer";
    case SCSI_PROC_TYPE:    return "proc";
    case SCSI_WORM_TYPE:    return "worm";
    case SCSI_CD_ROM_TYPE:  return "cdrom";
    case SCSI_SCAN_TYPE:    return "scanner";
    case SCSI_OPTICAL_TYPE: return "optical";
    case SCSI_CHANGER_TYPE: return "changer";
    case SCSI_COMM_TYPE:    return "comm";
    }

  return "<unknown>";
}


/* diskname()   --  Return string naming disk device.
*/
char *
diskname(Partition_Identifier_t *s)
{
  static char name[52];

  if (0 == s->IO_Bus)
    {
      if (0 == s->SCSI_Controller)
	sprintf(name, "c%xt%xd%xs%x",
		s->SCSI_Bus,
		s->PUN, s->LUN, s->Partition);
      else
	sprintf(name, "c%x%xt%xd%xs%x",
		s->SCSI_Controller, s->SCSI_Bus,
		s->PUN, s->LUN, s->Partition);
    }
  else
    sprintf(name, "c%x%x%xt%xd%xs%x",
	    s->IO_Bus, s->SCSI_Controller, s->SCSI_Bus,
	    s->PUN,    s->LUN,             s->Partition);

  return name;
}

int
init_disk_info(void)
{
  long kaddr;
  int  cdisk;
  int head = 0, nprocs = 0, ncdroms = 0, ntapes = 0;
  
  seekreadok(kmem,
	     kernel_nl[KERNEL_SCSI_STAT].n_value,
	     &kscsi_stat_ptr_addr,
	     sizeof(kscsi_stat_ptr_addr),
	     "/dev/kmem [SCSI_Stat_Ptr]");

  kaddr  = kscsi_stat_ptr_addr;
  ndisks = 0;

  do
    {
      seekreadok(kmem,kaddr,&tscsi_stat,sizeof(tscsi_stat),"/dev/kmem [SCSI]");
      switch (tscsi_stat.inquiry_data.Periph_Device_Type)
	{
	case SCSI_DAD_TYPE:     ndisks++;   break;
	case SCSI_SAD_TYPE:     ntapes++;   break;
	case SCSI_CD_ROM_TYPE:  ncdroms++;  break;
	case SCSI_PROC_TYPE:    nprocs++;   break;
	}
      kaddr = (long) tscsi_stat.Statistics.ForwardPointer;
    }
  while (kscsi_stat_ptr_addr != kaddr);

  fprintf(stderr, "SCSI: %d disks, %d tapes, %d cdroms, %d controllers\n",
	  ndisks, ntapes, ncdroms, nprocs);

  kscsi_stat = (diskinfo_t *) malloc((1+ndisks) * sizeof(diskinfo_t));
  if (!kscsi_stat)
    ERR_EXIT("Allocate SCSI Disk memory");
#if 0
  lscsi_stat = (SCSI_Device_t *) malloc((1+ndisks) * sizeof(SCSI_Device_t));
  if (!lscsi_stat)
    ERR_EXIT("Allocate SCSI Statistics array");
  memset(lscsi_stat, 0, ndisks * sizeof(SCSI_Device_t));
#endif

  kaddr  = kscsi_stat_ptr_addr;
  cdisk = 0;

  lboltp = (long *) mapthis(kmem, (void *)kernel_nl[KERNEL_LBOLT].n_value,
			    sizeof(long));
  if (-1 == (long) lboltp)
    {
      perror("Can't map lbolt");
      return 0;
    }
  last_lbolt = *lboltp;

  do
    {
      lscsi_stat = (SCSI_Device_t *) mapthis(kmem, (void *)kaddr,
					     sizeof(tscsi_stat));
      if (-1 == (long) lscsi_stat)
	{
	  perror("Can't map SCSI information");
	  return 0;
	}

      kscsi_stat[cdisk].di_kscsi = (SCSI_Device_t *) lscsi_stat;
      kscsi_stat[cdisk].di_dev   = lscsi_stat->dev;
      memcpy(&kscsi_stat[cdisk].di_stats, &lscsi_stat->Statistics,
	     sizeof(lscsi_stat->Statistics));

      if (!head++)
       fprintf(stderr,"  #  Bus  Ctrlr  SBus  PUN  LUN  Part  dev_num Name\n");

      fprintf(stderr, "%3d  %3d  %5d  %4d  %3d  %3d  %4d %08x %s (%s)\n",
	      cdisk,
	      lscsi_stat->SCSI_device.IO_Bus,
	      lscsi_stat->SCSI_device.SCSI_Controller,
	      lscsi_stat->SCSI_device.SCSI_Bus,
	      lscsi_stat->SCSI_device.PUN,
	      lscsi_stat->SCSI_device.LUN,
	      lscsi_stat->SCSI_device.Partition,
	      lscsi_stat->dev,
	      diskname(&lscsi_stat->SCSI_device),
	      scsitype(lscsi_stat->inquiry_data.Periph_Device_Type));
	      
      kaddr = (long) lscsi_stat->Statistics.ForwardPointer;

      if (SCSI_DAD_TYPE == lscsi_stat->inquiry_data.Periph_Device_Type)
	cdisk++;
    }
  while (kscsi_stat_ptr_addr != kaddr);

  if (cdisk != ndisks)
    ERR_EXIT("Error counting disks");

  return ndisks;
}


/* Called by -version */
void
version()
{
	rintf("NCR SVR4 MP/RAS\n");
}

/* Called at the beginning to inquire how many bars are needed
*/
int
num_bars(void)
{
  int         nf;
  int         errs;
  int         sz;
  char       *pgs    = 0;

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
      if (cpuflag)
	fprintf(stderr, "Only disk or cpu state can be monitored\n");
      state  = DISK_STATE;
      return init_disk_info();
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
  int i;
  int nlevels = (DISK_STATE == state) ? 2 : CPUSTATES;

  for (i = 0; i < nbars; )
    items[i++] = nlevels;
}


/* Called after num_bars to ask for bar names */
char **
label_cpu_bars(nbars)
{
  char **names;
  int    hsz   = strlen(nodename.nodename);
/*****                  ' '   '#'   %d    ' '   'i486'   ' '  *****/
  int    rowsz = hsz +   1 +   1 +   3 +   1 +      7 +   2;
  int    sz    = nbars * rowsz;
  int    cpu, i;
  char  *cp;

  shorten(nodename.nodename);

  names  = (char **)malloc(sz + sizeof(char *) * nbars);

  cp     = (char *)names + sizeof(char *) * nbars;

  for (cpu = i = 0; i < kncpu; i++)
    if (kcpu_infos[i].cpu_flag & CPU_EXISTS)
      {
	names[cpu++] = cp;

	if (1 == ncpus)
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
label_disk_bars(nbars)
{
  char **names;
  char  *cp;
  int    i;
  int    hsz    = sizeof("cxxxtxdxsx");

  names = (char **) malloc( hsz * nbars + nbars * sizeof(char *) );
  cp    = (char *) names + sizeof(char *) * nbars;

  for (i = 0; i < nbars; i++)
    {
      names[i]   = cp;
      lscsi_stat = kscsi_stat[i].di_kscsi;
      strcpy(cp, diskname(&lscsi_stat->SCSI_device));
      cp += hsz;
    }

  return names;
}


char **
label_bars(nbars)
{
  switch (state)
    {
    case CPU_STATE:  return label_cpu_bars(nbars);
    case DISK_STATE: return label_disk_bars(nbars);
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
{
  int states[NSTATES];
  int nstates;
  int cpu, i;
  time_t *cputime = lastcpu;

  for (i = cpu = 0; i < kncpu && cpu < nbars; i++)
    {
      if (0 == (kcpu_infos[i].cpu_flag & CPU_EXISTS))
	continue;

      nstates = 0;

      if (kcpu_infos[i].cpu_flag & CPU_RUNNING && kpercpup[i])
	{
	  if (kpercpup[cpu] != xpercpup[i])
	    { /* mapping changed or became available */
	      xpercpup[cpu] = kpercpup[i];
	      mypercpup[ i] = mapthis(kmem, xpercpup[cpu],
				      sizeof(*(xpercpup[cpu])));
	    }
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
display_disk_bars(nbars)
{
  int    states[2];
  int    i;
  time_t t;
  static time_t maxt = 0;
  static int    npr  = 0;
  time_t dlt;
  struct iotime *new, *old;

  new_lbolt  = *lboltp;
  dlt        = new_lbolt - last_lbolt;
  last_lbolt = new_lbolt;
  
  for (i = 0; i < ndisks; i++)
    {
      lscsi_stat = kscsi_stat[i].di_kscsi;

      if (kscsi_stat[i].di_dev != lscsi_stat->dev)
	{ /* device changed */
	  states[0] = 100;  /* idle */
	  states[1] = 0;    /* busy */
	}
      else
	{
	  new       = &lscsi_stat->Statistics.SCSI_Stats;
	  old       = &kscsi_stat[i].di_stats.SCSI_Stats;
	  t         = new->io_act - old->io_act;

	  states[1] = t;
	  if (states[1] > dlt)
	    states[0] = 0;
	  else
	    states[0] = dlt - states[1];

	  old->io_act  = new->io_act;
	  old->io_resp = new->io_resp;
	}
      draw_bar(i, states, 2);
    }
}

void
display_bars(nbars)
{
  if (kmem_list)
    refreshfakes();

  switch (state)
    {
    case CPU_STATE:  display_cpu_bars(nbars);
    case DISK_STATE: display_disk_bars(nbars);
    }

  return;
}
