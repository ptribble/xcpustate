/*
 * System dependent file for Cygwin.  Adapted from s-linux.c originally
 * contributed by  Kumsup Lee <klee@ima.umn.edu>
 * 
 * John DiMarco <jdd@cs.toronto.edu>,  University of Toronto
 */

/* LINTLIBRARY */
#include <sys/param.h>
#include <sys/types.h>

#include <stdio.h>

#ifndef STATFILE
#define STATFILE "/proc/stat"
#endif /* STATFILE */

#define MAXHOSTNAMELEN 64

#define CPUSTATES 4 	/* idle, user, nice, system */
#ifndef NR_CPUS
#define NR_CPUS 1
#endif /* NR_CPUS */

#define WHITESPACE "	 "
#define CPU "cpu"

extern char *xmalloc(/* int nbytes */);

extern int open(), read();
extern long lseek();

static long	cp_time[NR_CPUS+1][CPUSTATES];
static long	cp_old[NR_CPUS+1][CPUSTATES];

static char line[1024];

/* Called by -version */
void
version()
{
	printf("Cygwin: maxcpu=%d, maxdisk=0\n", NR_CPUS);
}

int numcpus=0;

/* Called at the beginning to inquire how many bars are needed. */
int
num_bars()
{
	/* 
	 * Assume any line in /proc/stat beginning with cpuN, where N is a
	 * number, corresponds to a specific CPU.  If there are no such lines,
	 * this must be a uniprocessor -- look for the line beginning with 
	 * "cpu ".  If there isn't one of those, there's no recognizable 
	 * evidence of any cpu in /proc/stat, which means something is wrong.
	 */

	FILE *proc_stat;
	long junk;
	int foundcpu=0;  /* we found a line beginning with "cpu " */


	/* count the number of CPUs mentioned in the stat file */
	if(0>(proc_stat=fopen(STATFILE, "r"))) {
		perror(STATFILE);
		return 0;
	}
	while(NULL!=fgets(line, sizeof(line)-1, proc_stat)){
		if(0==strncmp(line, CPU, strlen(CPU))){
			if(' '==line[strlen(CPU)]){
				/* line looks like "cpu ..." */
				foundcpu=1;
			} else if(isdigit(line[strlen(CPU)])){
				/* line looks like "cpuN ..." */
				numcpus++;
			}
		}
	}
	(void)fclose(proc_stat);

	/* 
	 * If there is no line beginning with "cpuN" (eg. cpu0), then 
	 * this is a uniprocessor system.
	 */
	if(0==numcpus) {
		numcpus=foundcpu; 
	} 
#ifdef SHOWAVG
	  else {
		numcpus++;
	}
#endif /* SHOWAVG */
	if(numcpus>(NR_CPUS+1)) numcpus=NR_CPUS+1;
	return(numcpus);
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
    int i;
    extern char *strcpy(/* char *, const char * */);
    static char hname[MAXHOSTNAMELEN+1];

    hname[MAXHOSTNAMELEN] = '\0';
    if (gethostname(hname, MAXHOSTNAMELEN) < 0) {
	perror("gethostname");
	*hname = '\0';
    }
    shorten(hname);
    names=(char **) xmalloc(nbars * sizeof(char *));
    for(i=0; i < nbars; i++) {
#define CPUNAME "%s %d"
	char buf[MAXHOSTNAMELEN + 1 + 32];

#ifdef SHOWAVG
	if ( i == 0 )
		(void) sprintf(buf, "CPU");
	else
		(void) sprintf(buf, CPUNAME, hname, i-1);
#else  /* !SHOWAVG */
	(void) sprintf(buf, CPUNAME, hname, i);
#endif /* !SHOWAVG */
    	names[i] = strcpy(xmalloc(strlen(buf) + 1), buf);
    }
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
	FILE *proc_stat;
	char dummy[256];
	int i, j;

	if((proc_stat=fopen(STATFILE,"r")) == NULL ) {
		perror(STATFILE);
  		for( i=0; i < nbars; i++)
		    for(j=0;j<CPUSTATES;j++) cp_old[i][j] = 0;
	} else {
#ifndef SHOWAVG
	    if(nbars>1){
		/* skip the first ("cpu") line */
		   fscanf(proc_stat, "%s %ld %ld %ld %ld",
                       dummy, &cp_time[0][0], &cp_time[0][1], &cp_time[0][2],
                       &cp_time[0][3]);
	    }
#endif /* SHOWAVG */
            for (i=0; i < nbars; i++)
                fscanf(proc_stat, "%s %ld %ld %ld %ld \n",
		       dummy, &cp_old[i][0], &cp_old[i][1], &cp_old[i][2],
		       &cp_old[i][3]);
	}
	(void)fclose(proc_stat);
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
	int	i, j;
	FILE *proc_stat;
	char dummy[256];
	extern void draw_bar(/*int bar_num, int *states, int num_states*/);

        if((proc_stat=fopen(STATFILE,"r")) == NULL ) {
                perror(STATFILE);
        } else {
#ifndef SHOWAVG
	    if(nbars>1){
		/* skip the first ("cpu") line */
		   fscanf(proc_stat, "%s %ld %ld %ld %ld",
                       dummy, &cp_time[0][0], &cp_time[0][1], &cp_time[0][2],
                       &cp_time[0][3]);
		/* skip chars until newline */
		while ((getc(proc_stat)) != '\n')
		  ; /*empty loop body*/
	    }
#endif /* SHOWAVG */
            for (i=0; i < nbars; i++)
	    {
                fscanf(proc_stat, "%s %ld %ld %ld %ld",
                       dummy, &cp_time[i][0], &cp_time[i][1], &cp_time[i][2],
                       &cp_time[i][3]);
               /* skip chars until newline */
                while ((getc(proc_stat)) != '\n')
                  ; /*empty loop body*/
	    }
        }
	(void)fclose(proc_stat);

	for (i=0; i < nbars; i++) {
#define delta(cpustate) ((int) (cp_time[i][(cpustate)] - cp_old[i][(cpustate)]))

		nstates = 0;
		states[nstates++] = delta(3); /* IDLE */
		states[nstates++] = delta(0); /* USER */
		states[nstates++] = delta(1); /* NICE */
		states[nstates++] = delta(2); /* SYS  */
		draw_bar(i, states, nstates);
			cp_old[i][0] = cp_time[i][0];
			cp_old[i][1] = cp_time[i][1];
			cp_old[i][2] = cp_time[i][2];
			cp_old[i][3] = cp_time[i][3];
	}
}

