/* 
 * rstat RPC code
 */
/* John DiMarco, CSLab, University of Toronto <jdd@cs.toronto.edu> */

#ifdef RSTAT

/* LINTLIBRARY */
#define MULTIPROCESSOR

#include <sys/types.h>
#include <sys/file.h>
#include <stdio.h>
#include <rpc/rpc.h>
#include "rstat.h"

/* most typical cpu states; for BSD systems */
#define CP_USER         0
#define CP_NICE         1
#define CP_SYS          2
#define CP_IDLE         3

#ifndef MAXDISKXFER
#define MAXDISKXFER 100
#endif /* MAXDISKXFER */

#define DISKSTATES 2 /* idle, transfer */

#define MAXCPUS 1 /* rstat only supports uniprocessors */
int ncpu;       /* number of CPUs */
int ndisk;      /* number of disks */

#define BUFFSIZE 64

#ifndef MAXHOSTNAMELEN
  /* Some U*x deviants managed to not have this in sys/param.h */
# define MAXHOSTNAMELEN 64
#endif

static statstime *sts;

static CLIENT *cl;

extern void draw_bar(/*int bar_num, int *states, int num_states*/);

extern char *rstathost;
extern int cpuflag, diskflag;

extern char *xmalloc(/* int nbytes */);
extern void shorten(/* char *hname */);

/* Called by -version */
void
rstat_version()
{
        printf("RSTAT: maxcpu=%d, maxdisk=%d\n", MAXCPUS,  RSTAT_DK_NDRIVE);
}

/* Called at the beginning to inquire how many bars are needed. */
int
rstat_num_bars()
{
	int nbars=0;

	if(NULL==(cl=clnt_create(rstathost, RSTATPROG, RSTATVERS_TIME, "udp"))){
		clnt_pcreateerror(rstathost);
		exit(1);
	}

	if(cpuflag){
		ncpu=MAXCPUS; 
		nbars += ncpu;
	}
	if(diskflag){
		ndisk=RSTAT_DK_NDRIVE;
		nbars += ndisk;
	}
	return(nbars);
}

/*
 * Indicates how many levels each bar has.  For most machines, each bar will
 * have the same stuff.  But one can, for instance, display memory use on one
 * bar, processor levels on others, etc.
 */
void
rstat_bar_items(nbars, items)
int nbars;
int items[];    /* nbars items in this */
{
    int i, n=0;

    if(cpuflag){
        for(i=0; i<ncpu; i++,n++){
                items[n] = RSTAT_CPUSTATES;
        }
    }
    if(diskflag){
	for(i=0; i<ndisk; i++,n++){
		items[n] = DISKSTATES;
	}
    }
}

/* Called after num_bars to ask for the bar names */
char **
rstat_label_bars(nbars)
{
        char **names;
        int i, base;
        static char hname[MAXHOSTNAMELEN + 1];
	char buf[MAXHOSTNAMELEN + 1 + BUFFSIZE];
	char *cpname="";

	(void)strcpy(hname, rstathost);
        shorten(hname);
        names = (char **) xmalloc(nbars * sizeof(char *));

	base=0; 

	if(cpuflag) {
		/* do cpu names */
		for(i=0; i<ncpu; i++) {
			(void) sprintf(buf, "%s%s%s%d", hname, 
					hname[0]?" ":"", cpname, i);
			names[base+i] = xmalloc(strlen(buf) + 1);
			strcpy(names[base+i], buf);
		}
		base+=ncpu;
	}
	if(diskflag) {
		/* do disk names */
		for(i=0; i <ndisk; i++) {
			(void) sprintf(buf, "%s%sd%d", hname, hname[0]?" ":"", 
					i);
			names[base+i] = xmalloc(strlen(buf) + 1);
			strcpy(names[base+i], buf);
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
rstat_init_bars(nbars)
int nbars;
{
}

static void
display_cpu(base)
int base;
{
	int states[RSTAT_CPUSTATES], i;
	static long old[RSTAT_CPUSTATES];

	states[0] = (long)(sts->cp_time[CP_IDLE] - old[CP_IDLE]);
	states[1] = (long)(sts->cp_time[CP_NICE] - old[CP_NICE]);
	states[2] = (long)(sts->cp_time[CP_USER] - old[CP_USER]);
	states[3] = (long)(sts->cp_time[CP_SYS] - old[CP_SYS]);
	for(i=0;i<RSTAT_CPUSTATES;i++) old[i]=sts->cp_time[i];
	draw_bar(base, states, RSTAT_CPUSTATES);
}

static void
display_disk(base)
int base;
{
	int states[2], i;
	static long old[RSTAT_DK_NDRIVE];

	for(i=0;i<RSTAT_DK_NDRIVE;i++){
		long xfer;

		xfer = sts->dk_xfer[i]-old[i];
		if(xfer>MAXDISKXFER) xfer=MAXDISKXFER;
		states[0] = MAXDISKXFER - xfer;
		states[1] = xfer;
		draw_bar(base+i, states, 2);
		old[i] = sts->dk_xfer[i];
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
rstat_display_bars(nbars)
int nbars;
{
	int n=0;
	if(NULL==(sts=rstatproc_stats_3((void *)NULL, cl))){
		clnt_perror(cl, rstathost);
		exit(1);
	}
	if(cpuflag) {
		display_cpu(n);
		n+=ncpu;
	}
	if(diskflag) {
		display_disk(n);
		n+=ndisk;
	}
}

#endif /* RSTAT */
