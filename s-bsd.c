/*
 * System dependent file for BSD derivatives that have _cp_time in their
 * kernels to hold the CPU states. Seen to work on SunOS and Ultrix.
 */
/* Chris Siebenmann <cks@sys.toronto.edu>, University of Toronto */
/* LINTLIBRARY */
#include <sys/param.h>
#include <sys/dk.h>
#include <nlist.h>

#ifndef MAXHOSTNAMELEN
  /* Some U*x deviants managed to not have this in sys/param.h */
# define MAXHOSTNAMELEN 64
#endif

extern char *xmalloc(/* int nbytes */);
extern void shorten(/* char *hname */);

extern int open(), read();
extern long lseek();

extern char *kernelSymbols;

long	cp_time[CPUSTATES];
long	cp_old[CPUSTATES];
int	kmem;			/* file descriptor of /dev/kmem. */
struct nlist	nl[] = {
#define	X_CP_TIME	0
	{ "_cp_time" },
	{ "" },
};

#define NPROCS 1

/* Called by -version */
void
version()
{
	printf("Generic BSD: maxcpu=1, maxdisk=0\n");
}

/* Called at the beginning to inquire how many bars are needed. */
int
num_bars()
{
	return NPROCS;
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

/* Called after num_bars to ask for the bar names */
/* ARGSUSED */
char **
label_bars(nbars)
{
	static char hname[MAXHOSTNAMELEN + 1];
	static char *names[NPROCS];

	names[0] = hname;
	hname[MAXHOSTNAMELEN] = '\0';
	if (gethostname(hname, MAXHOSTNAMELEN) < 0) {
		perror("gethostname");
		*hname = '\0';
	}
	shorten(hname);
	return names;
}

/* 
 *  Called after the bars are created to perform any machine dependent
 *  initializations.
 */
/* ARGSUSED */
void
init_bars(nbars)
int nbars;
{
	if ((kmem = open("/dev/kmem", 0)) < 0) {
		perror("/dev/kmem");
		exit(1);
	}
	(void)nlist(kernelSymbols?kernelSymbols:"/vmunix", nl);
	if (lseek(kmem, (long) nl[X_CP_TIME].n_value, 0) !=
	    (long) nl[X_CP_TIME].n_value)
		perror("lseek");
	if (read(kmem, (char *) cp_old, sizeof(cp_old)) != sizeof(cp_old))
		perror("read");
}

/* 
 *  This procedure gets called every interval to compute and display the
 *  bars. It should call draw_bar() with the bar number, the array of
 *  integer values to display in the bar, and the number of values in
 *  the array.
 */
/* ARGSUSED */
void
display_bars(nbars)
{
	int	states[CPUSTATES];
	int	nstates;
	int	i;
	extern void draw_bar(/*int bar_num, int *states, int num_states*/);
	
	if (lseek(kmem, (long) nl[X_CP_TIME].n_value, 0) !=
	    (long) nl[X_CP_TIME].n_value)
		perror("lseek");
	if (read(kmem, (char *) cp_time, sizeof(cp_time)) !=
	    sizeof(cp_time))
		perror("read");
	
	
#define delta(cpustate) ((int) (cp_time[(cpustate)] - cp_old[(cpustate)]))

	nstates = 0;
	states[nstates++] = delta(CP_IDLE);
	states[nstates++] = delta(CP_USER);
	states[nstates++] = delta(CP_NICE);
	states[nstates++] = delta(CP_SYS);
	draw_bar(0, states, nstates);
	for (i = 0; i < CPUSTATES; i ++)
		cp_old[i] = cp_time[i];
}
