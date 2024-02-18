/*
 * System dependent file for Ultrix 4.2A
 */

/* 
 * Ricardo E Gonzalez, Stanford University <recardog@chroma.stanford.edu>
 *      based on code by
 * Salvador P. Abreu, U.Nova de Lisboa, PORTUGAL <spa@fctunl.rccn.pt>
 *      based on code by
 * Chris Siebenmann, University of Toronto <cks@sys.toronto.edu>
 */

/* LINTLIBRARY */
#include <sys/param.h>
#include <sys/dk.h>
#include <machine/cpu.h>
#include <sys/cpudata.h>
#include <nlist.h>
#include <stdio.h>
#include <errno.h>

#define kmseek(N) if (lseek(kmem, (long) (N), 0) != (long) (N)) perror ("lseek kmem");
#define KMEM 	"/dev/kmem"

extern char *xmalloc(/* int nbytes */);

extern int open(), read();
extern long lseek();

extern char *kernelSymbols;

struct cpudata *cpu_data[MAXCPU];
long **cptime_old = NULL;

int	kmem;			/* file descriptor of /dev/kmem. */
struct nlist	nl[] = {
#define X_ACTIVECPU 0
	{ "_cpu_avail" },	/* number of active CPUs */
#define X_CPUDATA 1
	{ "_cpudata" },		/* cpudata[] array */
#define X_LOWCPU 2
	{ "_lowcpu" },		/* lowest cpu number */
#define X_HIGHCPU 3
	{ "_highcpu" },		/* highest cpu number */
	{ 0 },
};

int activecpu;
int lowcpu = 0;
int highcpu = 0;
static unsigned long cpudata_offset = 0;

/* Called by -version */
void
version()
{
	printf("Ultrix: maxcpu=%d, maxdisk=0\n", MAXCPU);
}

/* Called at the beginning to inquire how many bars are needed. */
int
num_bars()
{
    if ((kmem = open("/dev/kmem", 0)) < 0) {
	perror("/dev/kmem");
	exit(1);
    }
    (void) nlist(kernelSymbols?kernelSymbols:"/vmunix", nl);

/*
    printf("ACTIVE : %x\n", nl[X_ACTIVECPU].n_value);
    printf("CPUDATA: %x\n", nl[X_CPUDATA].n_value);
    fflush(stdout);
*/

    (void) getkval(nl[X_LOWCPU].n_value, (int *)(&lowcpu), sizeof(lowcpu),
	nl[X_LOWCPU].n_name);
    (void) getkval(nl[X_HIGHCPU].n_value, (int *)(&highcpu), sizeof(highcpu),
	nl[X_HIGHCPU].n_name);
    activecpu = highcpu - lowcpu + 1;
    cpudata_offset = nl[X_CPUDATA].n_value;

/*
    printf("CPUS   : %d\n", activecpu);
    fflush(stdout);
    return (activecpu);
*/

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

    for(i = 0; i < nbars; i++)
        items[i] = CPUSTATES;
}


/* Called after num_bars to ask for the bar names */
/* ARGSUSED */
char **
label_bars(nbars)
{
    static char **names;
    static char hname[MAXHOSTNAMELEN];
    int i;

    names = (char **) malloc (nbars * sizeof (char *));
    for (i=0; i<nbars; i++) {
	names[i] = (char *) malloc (strlen("cpuxxx")+1);
	(void) sprintf (names[i], "cpu%d", i);
    }
    return names;
}

/* 
 *  Called after the bars are created to perform any machine dependent
 *  initializations.
 */
/* ARGSUSED */
void init_bars(nbars)
    int nbars;
{
    int ncpu, i;
    register struct cpudata cpu;

    (void) getkval(cpudata_offset, cpu_data, sizeof(cpu_data), "_cpudata");
	
    cptime_old = (long **) malloc (nbars * sizeof (long *));
    for (ncpu=lowcpu; ncpu<=highcpu; ++ncpu) {
	if (cpu_data[ncpu] != NULL) {
	    (void) getkval(cpu_data[ncpu], &cpu, sizeof(cpu), "???");
	    cptime_old[ncpu] = (long *) malloc (CPUSTATES * sizeof (long));
	    if (cpu.cpu_state & CPU_RUN) {
		for (i = 0; i < CPUSTATES; i ++) {
		    cptime_old[ncpu][i] = cpu.cpu_cptime[i];
		}
	    }
	}
    }
}

/* 
 *  This procedure gets called every interval to compute and display the
 *  bars. It should call draw_bar() with the bar number, the array of
 *  integer values to display in the bar, and the number of values in
 *  the array.
 */
#define delta(ncpu, cpustate) \
    ((int) (cpu.cpu_cptime[(cpustate)] - cptime_old[ncpu][(cpustate)]))

/* ARGSUSED */
void
display_bars(nbars)
{
    int	states[CPUSTATES];
    int	nstates;
    int	i, ncpu;
    int status;
    register struct cpudata cpu;
    extern void draw_bar(	/*int bar_num, int *states, int num_states*/);
	

    (void) getkval(cpudata_offset, cpu_data, sizeof(cpu_data), "_cpudata");
    for (ncpu = lowcpu; ncpu <= highcpu; ++ncpu) {
	if (cpu_data[ncpu] != NULL) {
	    (void) getkval(cpu_data[ncpu], &cpu, sizeof(cpu), "???");
	    if (cpu.cpu_state & CPU_RUN) {
		nstates = 0;
		states[nstates++] = delta(ncpu, CP_IDLE);
		states[nstates++] = delta(ncpu, CP_USER);
		states[nstates++] = delta(ncpu, CP_NICE);
		states[nstates++] = delta(ncpu, CP_SYS);

		draw_bar(ncpu, states, nstates);

		for (i = 0; i < CPUSTATES; i ++) {
		    cptime_old[ncpu][i] = cpu.cpu_cptime[i];
/*		    printf ("%d\t", cptime_old[ncpu][i]); */
		}
/*		printf ("\n"); */
	    }
	}
    }
}


/*
 *  getkval(offset, ptr, size, refstr) - get a value out of the kernel.
 *      "offset" is the byte offset into the kernel for the desired value,
 *      "ptr" points to a buffer into which the value is retrieved,
 *      "size" is the size of the buffer (and the object to retrieve),
 *      "refstr" is a reference string used when printing error meessages,
 *          if "refstr" starts with a '!', then a failure on read will not
 *          be fatal (this may seem like a silly way to do things, but I
 *          really didn't want the overhead of another argument).
 *  
 */

getkval(offset, ptr, size, refstr)

unsigned long offset;
int *ptr;
int size;
char *refstr;

{
    if (lseek(kmem, (long)offset, SEEK_SET) == -1) {
        if (*refstr == '!')
            refstr++;
        (void) fprintf(stderr, "%s: lseek to %s: %s\n", KMEM, 
                       refstr, strerror(errno));
        exit(23);
    }
    if (read(kmem, (char *) ptr, size) == -1) {
        if (*refstr == '!') 
            return(0);
        else {
            (void) fprintf(stderr, "%s: reading %s: %s\n", KMEM, 
                           refstr, strerror(errno));
            exit(23);
        }
    }
    return(1);
}
