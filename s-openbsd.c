/*
 * System dependent file for OpenBSD 3.0+ -- doesn't require setgid kmem
 * John DiMarco <jdd@cs.toronto.edu>
 */
/* LINTLIBRARY */
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/dkstat.h>

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif /* MAXHOSTNAMELEN */

extern void shorten(/* char *hname */);
extern int cpuflag;

static long cp_time[CPUSTATES];
static long cp_old[CPUSTATES];

static int ncpus=0;

/* Called by -version */
void version()
{
	printf("OpenBSD 3.0+: maxcpu=1, maxdisk=0\n");
}

/* Called at the beginning to inquire how many bars are needed. */
int num_bars()
{
	int x=0;
	if(cpuflag) {
		int mib[2], size;
		mib[0]=CTL_HW;
		mib[1]=HW_NCPU;
		size=sizeof(ncpus);
		if(0>sysctl(mib, sizeof(mib)/sizeof(mib[0]), &ncpus, 
				&size, NULL, 0)){
			perror("num_bars sysctl HW_DISKCOUNT");
		}
		if(ncpus>1) ncpus=1; /* only support uniprocessor now */
		
		x+=ncpus;
	}
	return(x);
}

/*
 * Indicates how many levels each bar has.  
 */
void bar_items(nbars, items)
int nbars;
int items[];
{
	int x=0;
	if(cpuflag){
		while(x<ncpus && x<nbars) items[x++] = CPUSTATES;
	}
}

/* Called after num_bars to ask for the bar names */
char **
label_bars(nbars)
{
	static char hname[MAXHOSTNAMELEN], **names, tmp[10]; 
	int i, x=0;

	names=(char **)xmalloc(nbars*sizeof(char *));

	hname[MAXHOSTNAMELEN] = '\0';
	if (gethostname(hname, MAXHOSTNAMELEN) < 0) {
		perror("label_bars gethostname");
		*hname = '\0';
	}
	shorten(hname);
	if(cpuflag) {
		for(i=0;i<ncpus;i++){
			snprintf(tmp, sizeof(tmp), "%s%d", 
				(int)hname[0]?" ":"", i);
			names[x] = (char *)xmalloc(strlen(hname)+strlen(tmp)+1);
			(void)strcpy(names[x], hname);
			(void)strcat(names[x], tmp);
			x++;
		}
	}
	return names;
}

/* 
 *  Called after the bars are created to perform any machine dependent
 *  initializations.
 */
void init_bars(nbars)
int nbars;
{
	if(cpuflag){
		int mib[2], size;
		mib[0]=CTL_KERN;
		mib[1]=KERN_CPTIME;
		size=sizeof(cp_old);
		if(0>sysctl(mib, sizeof(mib)/sizeof(mib[0]), cp_old, 
				&size, NULL, 0)){
			perror("init_bars sysctl KERN_CPTIME");
		}
	}
}

/* 
 *  This procedure gets called every interval to compute and display the
 *  bars. It should call draw_bar() with the bar number, the array of
 *  integer values to display in the bar, and the number of values in
 *  the array.
 */
void display_bars(nbars)
{
	extern void draw_bar(/*int bar_num, int *states, int num_states*/);
	int x=0;

	if(cpuflag){
		int states[CPUSTATES], nstates, i;
		int mib[2], size;
		mib[0]=CTL_KERN;
		mib[1]=KERN_CPTIME;
		size=sizeof(cp_time);
		if(0>sysctl(mib, sizeof(mib)/sizeof(mib[0]), cp_time, 
				&size, NULL, 0)){
			perror("display_bars sysctl KERN_CPTIME");
		}
		
#define delta(cpustate) ((int) (cp_time[(cpustate)] - cp_old[(cpustate)]))

		nstates = 0;
		states[nstates++] = delta(CP_IDLE);
		states[nstates++] = delta(CP_USER);
		states[nstates++] = delta(CP_NICE);
		states[nstates++] = delta(CP_SYS);
		states[nstates++] = delta(CP_INTR);
		draw_bar(x, states, nstates);
		for (i=0; i<CPUSTATES; i++) cp_old[i] = cp_time[i];
		x++;
	}
}
