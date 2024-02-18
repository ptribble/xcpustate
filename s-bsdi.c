/*
 * System dependent file for BSDI and possibly BSD4.4
 */
/* Andy Beals, Telebit <asb@telebit.com> */
/* LINTLIBRARY */
#include <sys/param.h>
#include <sys/cpustats.h>
#include <sys/types.h>
#include <sys/kinfo.h>
#include <sys/kinfo_proc.h>
#include <sys/sysinfo.h>
#include <nlist.h>

#ifndef MAXHOSTNAMELEN
  /* Some U*x deviants managed to not have this in sys/param.h */
# define MAXHOSTNAMELEN 64
#endif

extern char *xmalloc(/* int nbytes */);

extern int open(), read();
extern long lseek();

long	cp_time[CPUSTATES];
long	cp_old[CPUSTATES];

#define NPROCS 1

/* Called by -version */
void
version()
{
	printf("BSDI: maxcpu=%d, maxdisk=0\n", NPROCS);
}

/* Called at the beginning to inquire how many bars are needed. */
int
num_bars()
{
    return NPROCS;
}

/* Called after num_bars to ask for the bar names */
/* ARGSUSED */
char **
label_bars(nbars)
{
    static char *name[NPROCS];
    static char hname[MAXHOSTNAMELEN];

    name[0] = hname;
    if (gethostname(name[0], MAXHOSTNAMELEN) < 0) {
	perror("gethostname");
	*name[0] = '\0';
    }
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
	struct cpustats	s;
	int t = sizeof s, j;

	if(getkerninfo(KINFO_CPU, &s, &t, 0) == -1) {
		perror("getkerninfo");
		for(j=0;j<CPUSTATES;j++) cp_old[j] = 0;
	} else {
	    for(j=0;j<CPUSTATES;j++) cp_old[j] = s.cp_time[j];
	}
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
	struct cpustats	s;
	
	i=sizeof s;
	if(getkerninfo(KINFO_CPU, &s, &i, 0) == -1) {
		perror("getkerninfo");
	} else {
	    for(i=0;i<CPUSTATES;i++) cp_time[i] = s.cp_time[i];
	}
	
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
