/* 
 * System dependent file for SunOS4.x machines, using the kvm library.
/* John DiMarco, CSLab, University of Toronto <jdd@cs.toronto.edu> */

/* LINTLIBRARY */
#define MULTIPROCESSOR

#include <sys/types.h>
#include <sys/file.h>
#include <sys/dk.h>
#include <nlist.h>
#include <kvm.h>
#include <sundev/mbvar.h>

#ifdef OMNI
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/buf.h>
#include <sys/filio.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <sundev/mbvar.h>
#include "/usr/sys/sundev/nereg.h"

#define OMNICPU ns_st.nst_cpu_cur
#define OMNISTATES 2 /* system, idle */

static int nomni = 0;
#define NEMAX 16
#define NEBASE "/dev/ne"
static int omfd[NEMAX];
static int omnum[NEMAX];
extern int omniflag;
extern int open();

#endif

#ifndef MAXHOSTNAMELEN
  /* Some U*x deviants managed to not have this in sys/param.h */
# define MAXHOSTNAMELEN 64
#endif

/* define some extended processor states here, in case they're not defined
   in sys/dk.h (which is the case for 4.1.1 and below). */
#ifndef XPSTATES
#define XPSTATES 8
#define XP_SPIN 0
#define XP_DISK 1
#define XP_SERV 2
#endif /* XPSTATES */

static int x_cp_time;

static char *dk_name[DK_NDRIVE];
static int hz;
static long dk_bps[DK_NDRIVE];

extern char *xmalloc(/* int nbytes */);
extern void shorten(/* char *hname */);

extern int diskflag;
extern int cpuflag;
extern char *kernelSymbols;

#define NCPU 4  /* maximum possible number of CPUs. This isn't likely to   */
                /* change under SunOS4.x. SunOS5.x will require a new port */
                /* of xcpustate anyways, so no need to think ahead here.   */

#define DISKSTATES 3 /* idle, seek, transfer */
#define MPSTATES 6 /* idle, user, nice, system, spin, serv */

#define NULL (char *)0

static kvm_t *kd;

int ncpu;       /* number of CPUs */
int ndisk;      /* number of disks */
int procset;    /* flag of CPUs */

struct nlist	nl[] = {
#define N_NCPU          0
        { "_ncpu" },
#define N_DK_BPS        1
        { "_dk_bps" },
#define N_CP_TIME       2
        { "_cp_time" },
#define N_DK_TIME       3
        { "_dk_time" },
#define N_DK_WDS        4
        { "_dk_wds" },
#define N_MP_TIME       5
        { "_mp_time" },
#define N_XP_TIME       6
        { "_xp_time" },
#define N_MBDINIT       7
        { "_mbdinit" },
#define N_DK_IVEC       8
        { "_dk_ivec" },
#define N_HZ            9
        { "_hz" },
#define N_PROCSET      10
        { "_procset" },
	{ "" },
};

#define DK_WDS_SIZE 64.0
#define BUFFSIZE 64

extern void draw_bar(/*int bar_num, int *states, int num_states*/);

static char *kernel_getstring(address, info)
char *address;
char *info;
{
        static char buff[BUFFSIZE];

        if(kvm_read(kd, (u_long)address, (char *)buff, 
		sizeof(buff)) != sizeof(buff)){
                perror(info);
        }
        buff[BUFFSIZE-1]='\0'; /* just in case it's not null-terminated */
        return(buff);
}

static void kernel_get(addr, dest, size, info)
u_long addr;
char *dest;
u_int size;
char *info;
{
	/* get something from the kernel */
	if(kvm_read(kd, addr, dest, size)!=size){
		perror(info);
		exit(1);
	}
}

static void kernel_init(name)
char *name;
{
	/* initialize kernel */
	if((kvm_t *)NULL==(kd=kvm_open(kernelSymbols, NULL, NULL, O_RDONLY, 
			name))){
		perror("kvm_open");
		exit(1);
	}

	/* read namelist */
	if(-1==kvm_nlist(kd, nl)){
		perror("kvm_nlist");
		exit(1);
	}
}

#ifdef OMNI
static int omni_init()
{
	/* count OMNI network coprocessors, initialize omfd/num tables */

	char buff[BUFFSIZE];
	int fd, there=0, i, n=0;

	/* probe /dev/ne0../dev/neNEMAX-1, check if present */
	for(i=0;i<NEMAX;i++){
		(void) sprintf(buff, "%s%d", NEBASE, i);
		if(-1!=(fd=open(buff, O_RDONLY))){
			if(-1!=ioctl(fd, NIOISTHERE, &there)) {
				if(there) {
					omfd[n]=fd;
					omnum[n]=i;
					n++;
				}
			}
		}
	}
	return(n);
}
#endif /* OMNI */

static int cpu_init()
{
	/* count cpus */

	int n=1; /* assume uniprocessor by default */

	if(N_UNDF!=nl[N_NCPU].n_type) {
		kernel_get(nl[N_NCPU].n_value, (char *)&n, sizeof(n), "ncpu");
		if(NCPU<n || n<1){
			/* if n is unreasonable, assume uniprocessor */
			n=1;
		}
		procset = 1;
		kernel_get(nl[N_PROCSET].n_value, (char *)&procset, sizeof(procset), "procset");
	} 
	return(n);
}

static int disk_init()
{
	/* count disks, initialize dk_name table */

	int n=0;
	if(N_UNDF!=nl[N_DK_IVEC].n_type){
		/* devinfo machine (openboot prom) */
		struct dk_ivec div, *div_p;

		for(div_p=(struct dk_ivec *)nl[N_DK_IVEC].n_value;
			n<DK_NDRIVE;div_p++){
			char *name;

			kernel_get((u_long)div_p, (char *)&div, sizeof(div),
					"div");
			if(NULL==div.dk_name) break;
			name=kernel_getstring(div.dk_name, "div.dk_name");
			if('\0'==*name) continue; 

			dk_name[n]=xmalloc(BUFFSIZE);
			(void) sprintf(dk_name[n], "%s%d", name, div.dk_unit);
			n++;
		}
	} else {
		/* mainbus machine */
		struct mb_device mbd, *mbd_p;
		struct mb_driver mdr;

		for(mbd_p=(struct mb_device *)nl[N_MBDINIT].n_value;
			n<DK_NDRIVE;mbd_p++){
			kernel_get((u_long)mbd_p, (char *)&mbd, sizeof(mbd),
					 "mbd");

			if(NULL==(char *)mbd.md_driver) break; 

			if(mbd.md_dk<0 || 0==mbd.md_alive) continue; 

			kernel_get((u_long)mbd.md_driver, (char *)&mdr, 
					sizeof(mdr), "mdr");
			dk_name[mbd.md_dk]=xmalloc(BUFFSIZE);
			(void) sprintf(dk_name[mbd.md_dk],
				"%s%d", kernel_getstring(mdr.mdr_dname, 
							 "mdr_dname"),
				mbd.md_unit);
			n++;
		}
	}

#if 0
	/* pad names so that they're the same length */
	for(i=0;i<n;i++){
		tmp=strlen(dk_name[i]);	
		if(tmp>maxlen) maxlen=tmp;
	}
	for(i=0;i<n;i++){
		tmp=strlen(dk_name[i]);
		for(j=tmp;j<maxlen;j++){
			*(dk_name[i]+j)=' ';
		}
		*(dk_name[i]+maxlen)='\0';
	}
#endif /* 0 */

	return(n);
}

/* Called by -version */
void
version()
{
	printf("SunOS 4.x: maxcpu=%d\n", NCPU);
#ifdef OMNI
	printf("Omni Network Coprocessors: maxcpu=%d\n", NEMAX);
#endif /* OMNI */
}

/* Called at the beginning to inquire how many bars are needed. */
int
num_bars()
{

	int nbars=0; 

	/* Open the kernel interface */
	kernel_init("xcpustate");

	if(cpuflag){
		ncpu=cpu_init();
		if(ncpu>0) {
			nbars += ncpu;
		} else {
			cpuflag=0;
		}
	}
#ifdef OMNI
	if(omniflag){
		nomni=omni_init();
		if(nomni>0) {
			nbars += nomni;
		} else {
			omniflag=0;
		}
	}
#endif /* OMNI */
	if(diskflag){
		ndisk=disk_init();
		if(ndisk>0) {
			nbars += ndisk;
		} else {
			diskflag=0;
		}
	}
	return(nbars);
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
		items[n] = (ncpu>1)?MPSTATES:CPUSTATES;
	}
    }
#ifdef OMNI
    if(omniflag){
	for(i=0; i<nomni; i++,n++){
		items[n] = OMNISTATES;
	}
    }
#endif /* OMNI */
    if(diskflag){
	for(i=0; i<ndisk; i++,n++){
		items[n] = DISKSTATES;
	}
    }
}

/* Called after num_bars to ask for the bar names */
char **
label_bars(nbars)
{
        char **names;
        int i, j, base;
        extern char *strcpy();
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

#ifdef OMNI
	if(omniflag) cpname="CP"; /* to distinguish main CPU(s) from nc400s */
#endif /* OMNI */

	if(cpuflag) {
		/* do cpu names */
		for(i=0, j=0; j<NCPU; j++) {
		    if ((procset >> j) & 0x01) {
			(void) sprintf(buf, "%s%s%s%d", hname, 
					hname[0]?" ":"", cpname, j);
			names[base+i] = strcpy(xmalloc(strlen(buf) + 1), buf);
			i++;
		    }
		}
		base+=ncpu;
	}
#ifdef OMNI
	if(omniflag) {
		/* do NC400 names */
		for(i=0; i<nomni; i++) {
			(void) sprintf(buf, "%s%s%s%d", hname, 
					hname[0]?" ":"", "NE", omnum[i]);
			names[base+i] = strcpy(xmalloc(strlen(buf) + 1), buf);
		}
		base+=nomni;
	}
#endif /* OMNI */
	if(diskflag) {
		/* do disk names */
		for(i=0; i <ndisk; i++) {
			(void) sprintf(buf, "%s%s%s", hname, hname[0]?" ":"", 
					dk_name[i]);
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
	if(diskflag){
		kernel_get(nl[N_HZ].n_value, (char *)&hz, sizeof(hz), "hz");
		kernel_get(nl[N_DK_BPS].n_value, (char *)dk_bps, 
			sizeof(dk_bps), "dk_bps");
	}
	if(cpuflag){
		if(ncpu>1){
			x_cp_time=N_MP_TIME;
		} else {
			x_cp_time=N_CP_TIME;
		}
	}
}

#ifdef OMNI
static void
display_omni(base)
int base;
{
	/* display OMNI bars */
	int states[OMNISTATES], nstates, i;
	struct ne_usrstat u;
	struct iovec iov;

	iov.iov_base=(char *)&u;
	iov.iov_len=sizeof(u);

	for(i=0;i<nomni;i++){
		nstates=0;

		u.OMNICPU = 0; /* in case the ioctl fails */
		(void)ioctl(omfd[i], NIOGETSTAT, &iov);

		states[nstates++] = 100-u.OMNICPU;
		states[nstates++] = u.OMNICPU;
		draw_bar(i+base, states, nstates);
	}
}
#endif /* OMNI */

static void
display_cpu(base)
int base;
{
	int states[CPUSTATES+XPSTATES], nstates, i, j, p;
	long cp_time[NCPU][CPUSTATES], xp_time[NCPU][XPSTATES];
	static long cp_old[NCPU][CPUSTATES], xp_old[NCPU][XPSTATES];

	kernel_get(nl[x_cp_time].n_value, (char *)cp_time, sizeof(cp_time), 
			"cp_time");
	if(ncpu>1){
		kernel_get(nl[N_XP_TIME].n_value, (char *)xp_time, 
				sizeof(xp_time), "xp_time");
	}
#define xdelta(cpu, cpustate) ((int) (xp_time[cpu][(cpustate)] - \
        xp_old[cpu][(cpustate)]))

#define delta(cpu, cpustate) ((int) (cp_time[cpu][(cpustate)] - \
        cp_old[cpu][(cpustate)]))


	for(p=0,i=0; i<NCPU; i++){
	    if ((procset >> i) & 0x01) {
		nstates=0;
		states[nstates++] = delta(i, CP_IDLE);
		states[nstates++] = delta(i, CP_USER);
		states[nstates++] = delta(i, CP_NICE);
		if(ncpu>1){
			long sys, spin, serv;
			
			sys = delta(i, CP_SYS);
			spin = xdelta(i, XP_SPIN);
			serv = xdelta(i, XP_SERV);
			
			states[nstates++] = sys - (spin + serv);
			states[nstates++] = spin;
			states[nstates++] = serv;
		} else {
			states[nstates++] = delta(i, CP_SYS);
		}
		draw_bar(p+base, states, nstates);

		/* save old values */
		for (j = 0; j < XPSTATES; j++){
			xp_old[i][j] = xp_time[i][j];
		}
		for (j = 0; j < CPUSTATES; j ++){
			cp_old[i][j] = cp_time[i][j];
		}
		p++;
	    }
	}
}

static void
display_disk(base)
int base;
{
	int states[DISKSTATES], i;
	long dk_time[DK_NDRIVE], dk_wds[DK_NDRIVE], dk_cp_time[CPUSTATES];
	static long old_dk_time[DK_NDRIVE], old_dk_wds[DK_NDRIVE];
	static double oldtime;
	double newtime, elapsedtime;

	/* compute elapsed time since last call */
	kernel_get(nl[N_CP_TIME].n_value, (char *)dk_cp_time, 
			sizeof(dk_cp_time), "dk_cp_time");
	newtime=0.0;
	for(i=0;i<CPUSTATES;i++) newtime += (double)dk_cp_time[i];
	if(newtime<=0.0) newtime=1.0;
	newtime /= (double)hz;
	elapsedtime = newtime - oldtime;
	oldtime = newtime;

	kernel_get(nl[N_DK_WDS].n_value, (char *)dk_wds, sizeof(dk_wds), 
			"dk_wds");
	kernel_get(nl[N_DK_TIME].n_value, (char *)dk_time, sizeof(dk_time), 
			"dk_time");

	for(i=0; i<ndisk; i++){
		int nstates=0;
		double activetime, transfersize, diskrate, transfertime;
		double seektime, idletime;

		activetime = (double)(dk_time[i]-old_dk_time[i])/(double)hz;
		diskrate = (double)dk_bps[i]*elapsedtime;
		transfersize = (double)(DK_WDS_SIZE*(dk_wds[i]-old_dk_wds[i]));
		if(diskrate > 0.0){
			transfertime = transfersize/diskrate;
			if(transfertime>1.0) transfertime=1.0;
		} else {
			transfertime = 0.0;
		}
		transfertime *= activetime;

		seektime = activetime - transfertime;
		if(seektime<0.0) seektime=0.0;

		idletime = elapsedtime - (transfertime + seektime);
		if(idletime<0.0) idletime=0.0;

		states[nstates++] = (long)(hz*idletime*elapsedtime);
		states[nstates++] = (long)(hz*seektime*elapsedtime);
		states[nstates++] = (long)(hz*transfertime*elapsedtime);

		draw_bar(i+base, states, nstates);

		old_dk_wds[i]=dk_wds[i];
		old_dk_time[i]=dk_time[i];
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
#ifdef OMNI
	if(omniflag) {
		display_omni(n);
		n+=nomni;
	}
#endif /* OMNI */
	if(diskflag) {
		display_disk(n);
		n+=ndisk;
	}
}
