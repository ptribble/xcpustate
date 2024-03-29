/* System dependent file for Silicon Graphics Iris4d systems */
/* Mark Moraes, ANT, University of Toronto <moraes@cs.toronto.edu> */
/* LINTLIBRARY */
#include <stdio.h>
#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>
#include <sys/param.h>

#define NSTATES 6

extern char *xmalloc(/* int nbytes */);
extern void shorten(/* char *hname */);

extern sep_wait;

static int *errs;
static struct sysinfo **si;
static struct sysinfo **last_si;
static struct sysinfo **tmp_si;

/* Called by -version */
void
version()
{
	printf("IRIX: maxdisk=0\n");
}

/* Called at the beginning to inquire how many bars are needed. */
int
num_bars()
{
    return sysmp(MP_NPROCS);
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
        if(sep_wait) 
		items[i] = NSTATES; 
	else 
		items[i] = NSTATES-1; 
}

/* Called after num_bars to ask for the bar names */
/* ARGSUSED */
char **
label_bars(nbars)
{
    char **names;
    int i;
    extern char *strcpy(/* char *, const char * */);
    static char hname[MAXHOSTNAMELEN + 1];

    hname[MAXHOSTNAMELEN] = '\0';
    if (gethostname(hname, MAXHOSTNAMELEN) < 0) {
	perror("gethostname");
	*hname = '\0';
    }
    shorten(hname);
    names = (char **) xmalloc(nbars * sizeof(char *));
    for(i = 0; i < nbars; i++) {
#define CPUNAME	"%s %d"
	char buf[MAXHOSTNAMELEN + 1 + 32];

	(void) sprintf(buf, CPUNAME, hname, i);
	names[i] = strcpy(xmalloc(strlen(buf) + 1), buf);
    }
    return names;
}

/* 
 *  Called after the bars are created to perform any machine dependent
 *  initializations.
 */
void
init_bars(nbars)
int nbars;
{
    int i;
    int ret;
    
    /* Initialize sysinfo by taking first reading */

    last_si = (struct sysinfo **) xmalloc(nbars * sizeof(struct sysinfo *));
    si = (struct sysinfo **) xmalloc(nbars * sizeof(struct sysinfo *));
    errs = (int *) xmalloc(nbars * sizeof(int));

    for(i = 0; i < nbars; i++) {
	last_si[i] = (struct sysinfo *) xmalloc(sizeof(struct sysinfo));
	si[i] = (struct sysinfo *) xmalloc(sizeof(struct sysinfo));
	errs[i] = 0;
	ret = sysmp(MP_SAGET1, MPSA_SINFO, (int) last_si[i],
		    sizeof(struct sysinfo), i);
	if (ret != 0) {
	    (void) fprintf(stderr,
	     "sysmp(MP_SAGET1, MPSA_SINFO) returned %d on processor %d\n",
	     ret, i);
	    (void) fflush(stderr);
	    errs[i]++;
	    continue;
	}
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
    struct sysinfo dummy;
    int states[sizeof(dummy.cpu)/sizeof(time_t)];
    int nstates;
    int ret;
    int i;
    extern void draw_bar(/*int bar_num, int *states, int num_states*/);

    for(i = 0; i < nbars; i++) {
	if (errs[i] != 0)
	    continue;
	ret = sysmp(MP_SAGET1, MPSA_SINFO, (int) si[i],
		    sizeof(struct sysinfo), i);
	if (ret != 0) {
	    (void) fprintf(stderr,
	     "sysmp(MP_SAGET1, MPSA_SINFO) returned %d on processor %d\n",
	      ret, i);
	    (void) fflush(stderr);
	    errs[i]++;
	    continue;
	}
	    
#define delta(cpustate) \
((int) (si[i]->cpu[(cpustate)] - last_si[i]->cpu[(cpustate)]))

	nstates = 0;
	if(sep_wait) {
	    states[nstates++] = delta(CPU_IDLE);
	    states[nstates++] = delta(CPU_WAIT);
	} else {
	    states[nstates++] = delta(CPU_IDLE) + delta(CPU_WAIT);
	}
	states[nstates++] = delta(CPU_USER);
	states[nstates++] = delta(CPU_KERNEL);
	states[nstates++] = delta(CPU_SXBRK);
	states[nstates++] = delta(CPU_INTR);
	draw_bar(i, states, nstates);
    }
    tmp_si = last_si;
    last_si = si;
    si = tmp_si;
}
