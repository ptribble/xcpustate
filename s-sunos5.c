/* 
 * System dependent file for SunOS5.x machines, using the kstat library.
 * 
 * John DiMarco, CSLab, University of Toronto <jdd@cs.toronto.edu> 
 */

/* LINTLIBRARY */

#include <stdio.h>
#include <sys/types.h>
#include <kstat.h>
#include <fcntl.h>
#include <sys/var.h>
#include <sys/sysinfo.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#ifndef MAXHOSTNAMELEN
  /* Some U*x deviants managed to not have this in sys/param.h */
# define MAXHOSTNAMELEN 64
#endif

static kstat_ctl_t *kctl;

static struct kstat_list {
	kstat_t *k;
	struct kstat_list *next;
} *disk=NULL, *cpu=NULL;

#ifndef kstime_t
#define kstime_t hrtime_t
#endif 

static ulong *cpu_old[CPU_STATES];
static kstime_t *old_update, *old_rtime, *old_wtime;

#define DISKSTATES 3 /* idle, wait, run */
#define NSTATES 4 /* idle, wait, user, kernel */

extern char *xmalloc(/* int nbytes */);
extern void shorten(/* char *hname */);

extern int read();
extern long lseek();

extern int diskflag;
extern int cpuflag;
extern int sep_wait;

int ncpu;       /* number of CPUs */
int ndisk;      /* number of disks */

int maxcpunum=0;	/* highest-numbered CPU */

int *cpupos;

#define BUFFSIZE 64

extern void draw_bar(/*int bar_num, int *states, int num_states*/);

/* Called by -version */
void
version()
{
	printf("SunOS 5.x\n");
}

/* Called at the beginning to inquire how many bars are needed. */
int
num_bars()
{
	kstat_t *k;
	struct kstat_list *diskcur,*cpucur;

	/* Open the kstat interface */
	if(NULL==(kctl=kstat_open())){
		perror("kstat");
		exit(1);
	}

	/* 
	 * Follow the kstat chain, making note of each useful module we
	 * come across. Usefulness = disk if diskflag, cpu if cpuflag.
         */
	for(k=kctl->kc_chain;k;k=k->ks_next){
		if(cpuflag){
			if(0==(strncmp(k->ks_name, "cpu_stat", 8))){
				struct kstat_list *new;
				int cpunum;

				(void)sscanf(k->ks_name, "cpu_stat%d", &cpunum);
				if(cpunum>maxcpunum) maxcpunum = cpunum;
				new = (struct kstat_list *)xmalloc(
					sizeof(struct kstat_list));
				new->k=k;
				new->next=NULL;
				if(NULL==cpu){
					cpu=new;
					cpucur=new;
				} else {
					cpucur->next=new;
					cpucur=new;
				}
				ncpu++;
			}
		}
		if(diskflag){
			if(0==(strcmp(k->ks_class, "disk"))
			   && KSTAT_TYPE_IO==k->ks_type){
				struct kstat_list *new;

				new = (struct kstat_list *)xmalloc(
						sizeof(struct kstat_list));
				new->k=k;
				new->next=NULL;
				if(NULL==disk){
					disk=new;
					diskcur=new;
				} else {
					diskcur->next=new;
					diskcur=new;
				}

				ndisk++;
			}
		}
	}
	if(maxcpunum<ncpu) maxcpunum=ncpu;
	return(((cpuflag*ncpu)+(diskflag*ndisk)));
}

/* Called after num_bars to ask for the bar names */
char **
label_bars(nbars)
{
        char **names;
        int i, base, c;
	struct kstat_list *k;
        static char hname[MAXHOSTNAMELEN + 1];
	char buf[MAXHOSTNAMELEN + 1 + BUFFSIZE];
	char *cpname="";

        hname[MAXHOSTNAMELEN] = '\0';
        if (0 >gethostname(hname, MAXHOSTNAMELEN)) {
                perror("gethostname");
                *hname = '\0';
        }
        shorten(hname);
        names = (char **) xmalloc(nbars * sizeof(char *));

	base=0; 
	if(cpuflag && cpu) {
		/* do cpu names */

		/* set up cpupos to map cpu numbers to cpu index */
		cpupos=(int *)xmalloc((maxcpunum+1) * sizeof(int));
		for(i=0;i<=maxcpunum;i++) cpupos[i]=-1;
		for(k=cpu;k;k=k->next){
			(void)sscanf(k->k->ks_name, "cpu_stat%d", &i);
			cpupos[i]=1;
		}
		c=0;
		for(i=0;i<=maxcpunum;i++) {
			if(0<cpupos[i]) {
				cpupos[i]=c;
				(void) sprintf(buf, "%s%s%s%d", hname, 
					hname[0]?" ":"", cpname, i);
				names[c] = strcpy(xmalloc(strlen(buf)+1), buf);
				c++;
			}
		}

		base+=ncpu;
	}
	if(diskflag && disk) {
		/* do disk names */
		for(i=0,k=disk;k;i++,k=k->next){
			(void) sprintf(buf, "%s%s%s", hname, hname[0]?" ":"", 
					k->k->ks_name);
			names[base+i] = strcpy(xmalloc(strlen(buf) + 1), buf);
		}
		base+=ndisk;
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
	if(cpuflag) {
		int i;
		for(i=0;i<CPU_STATES;i++){
			cpu_old[i] = (ulong *)xmalloc(ncpu*sizeof(ulong));
		}
	}
	if(diskflag){
		old_update = (kstime_t *)xmalloc(ndisk*sizeof(kstime_t));
		old_rtime = (kstime_t *)xmalloc(ndisk*sizeof(kstime_t));
		old_wtime = (kstime_t *)xmalloc(ndisk*sizeof(kstime_t));
	}
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
    int i, n=0;

    if(cpuflag){
        for(i=0; i<ncpu; i++,n++){
		if(sep_wait) {
			items[n] = NSTATES;
		} else {
			items[n] = NSTATES-1;
		}
        }
    }
    if(diskflag){
        for(i=0; i<ndisk; i++,n++){
                items[n] = DISKSTATES;
        }
    }
}

static void
display_cpu(base)
int base;
{
	int i,j,nstates;
	int states[CPU_STATES];
	cpu_stat_t cpustat;
	struct kstat_list *k;
#define delta(cpustate) ((int)(cpustat.cpu_sysinfo.cpu[(cpustate)] - cpu_old[(cpustate)][i]))


	for(k=cpu;k;k=k->next){	/* for each cpu... */
		int cpunum;

		(void)sscanf(k->k->ks_name, "cpu_stat%d", &cpunum);
		i=cpupos[cpunum];

		/* grab info */
		if(-1==kstat_read(kctl, k->k, (void *)&cpustat)){
			perror("kstat: cpustat");
			exit(1);
		}
		
		nstates=0;
		if(sep_wait) {
			states[nstates++] = delta(CPU_IDLE);
			states[nstates++] = delta(CPU_WAIT);
		} else {
			states[nstates++] = delta(CPU_IDLE) + delta(CPU_WAIT);
		}
		states[nstates++] = delta(CPU_USER);
		states[nstates++] = delta(CPU_KERNEL);

		draw_bar(i+base, states, nstates);

		/* save old values */
		for(j=0;j<CPU_STATES;j++){
			cpu_old[j][i]=cpustat.cpu_sysinfo.cpu[j];
		}
	}
}

static void
display_disk(base)
int base;
{
	int i;
	kstat_io_t dstat;
	struct kstat_list *k;
	int states[DISKSTATES], nstates;
	
	for(i=0,k=disk;k;k=k->next,i++){ /* for each disk... */
		double elapsed, run, wait, idle;
		/* grab info */
		if(-1==kstat_read(kctl, k->k, (void *)&dstat)){
			perror(	"kstat: diskstat");
			exit(1);
		}

		/* 
		 * The goofy *(double *)& stuff is to support both compilers
		 * that know about long long and those that don't. 
		 * <sys/types.h> defines long long as a union of long[2]
		 * and double if it thinks the compiler doesn't know 
		 * about long long. 
		 */
		elapsed = *(double *)&dstat.wlastupdate - 
			  *(double *)&old_update[i];
		if(elapsed <= 0.0) elapsed = (double) NANOSEC;

		run = 100 * (*(double *)&dstat.rtime - 
				*(double *)&old_rtime[i]) /elapsed; 
		wait = 100 * (*(double *)&dstat.wtime - 
				*(double *)&old_wtime[i]) /elapsed;
		idle = 100 - (run + wait);
		if(idle<0.0) idle=0.0;

		nstates=0;
		states[nstates++] = (int)idle;
		states[nstates++] = (int)wait;
		states[nstates++] = (int)run;

		draw_bar(i+base, states, 3);

		/* save old values */

		*(double *)&old_update[i] = *(double *)&dstat.wlastupdate;
		*(double *)&old_rtime[i] = *(double *)&dstat.rtime;
		*(double *)&old_wtime[i] = *(double *)&dstat.wtime;

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
int nbars;
{
	int n=0;
	
	if(cpuflag) {
		display_cpu(n);
		n+=ncpu;
	}
	if(diskflag) {
		display_disk(n);
		n+=ndisk;
	}
}
