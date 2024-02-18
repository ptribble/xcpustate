/* s-gould-np1.c : has _Cptime instead of _cp_time */

/*
 * Hugues Leroy, INRIA, RENNES France <Hugues.Leroy@irisa.fr>
 * added support for the Gould NP1
 * based on code by
 * Chris Siebenmann, University of Toronto <cks@sys.toronto.edu>
 * and
 * Salvador P. Abreu, U.Nova de Lisboa, PORTUGAL <spa@fctunl.rccn.pt>
*/
        
/* LINTLIBRARY */
#include <sys/param.h>
#include <sys/dk.h>
#include <nlist.h>

extern int open(), read();
extern long lseek();

extern char *kernelSymbols;

/* NPROCS is now always 2 */

#define NPROCS 2

long	cp_time[NPROCS][CPUSTATES];
long	cp_old [NPROCS][CPUSTATES];
int	kmem;			/* file descriptor of /dev/kmem. */
struct nlist	nl[] = {
#define	X_CP_TIME	0
	{ "_Cptime" },
	{ "" },
};

/* Called by -version */
void
version()
{
	printf("Gould NP1: maxcpu=%d, maxdisk=0\n", NPROCS);
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
    int i;

    for(i = 0; i < nbars; i++)
        items[i] = CPUSTATES;
}

/* Called after num_bars to ask for the bar names */
/* ARGSUSED */
char **
label_bars(nbars)
{
    static char *name[NPROCS];

    name[0] = "CPU 1";
    name[1] = "CPU 2";
    return name;
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
    if (read(kmem, (char *) cp_old, sizeof(cp_old)) !=
	sizeof(cp_old))
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
	int	i,j;

	
	if (lseek(kmem, (long) nl[X_CP_TIME].n_value, 0) !=
	    (long) nl[X_CP_TIME].n_value)
		perror("lseek");
	if (read(kmem, (char *) cp_time, sizeof(cp_time)) !=
	    sizeof(cp_time))
		perror("read");


#define delta(cpustate) ((int) (cp_time[i][(cpustate)] -
cp_old[i][(cpustate)]))

	for (i=0; i<NPROCS; i++) {
	nstates = 0;
	states[nstates++] = delta(CP_IDLE);
	states[nstates++] = delta(CP_USER);
	states[nstates++] = delta(CP_NICE);
	states[nstates++] = delta(CP_SYS);
	draw_bar(i, states, nstates);
	}
	for (i = 0; i < CPUSTATES; i ++)
	for (j=0; j< NPROCS; j++)
		cp_old[j][i] = cp_time[j][i];


}
