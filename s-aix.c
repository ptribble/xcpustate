/* 
 * AIX CPU support (including SMP for AIX 4.x) 
 * 
 * John DiMarco <jdd@cs.toronto.edu>, University of Toronto, CSLab
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/sysinfo.h>
#include <nlist.h>

extern char *kernelSymbols;
extern int sep_wait, cpuflag;

#ifndef MAXCPU
#define MAXCPU 64
#endif /* MAXCPU */

#ifndef KMEM
#define KMEM "/dev/kmem"
#endif /* KMEM */

#ifndef KSYMS
#define KSYMS "/unix"
#endif /* KSYMS */

#ifdef SMP
#define NPROCS MAXCPU
#define CPUINFO "cpuinfo"
#else
#define cpuinfo sysinfo
#define CPUINFO "sysinfo"
#define NPROCS 1
#endif

#define BUFFSIZE 64

int	ncpus;
struct cpuinfo cpu[MAXCPU];

long	cp_time[NPROCS][CPU_NTIMES];
long	cp_old [NPROCS][CPU_NTIMES];
int	kmem;			/* file descriptor for /dev/kmem. */
struct nlist	nl[] = {
#define N_NCPU		0
	{ "number_of_cpus" },
#define	N_CPUINFO	1
	{ CPUINFO },
	{ "" },
};

/* Called by -version */
void
version()
{
	printf("AIX: maxcpu=%d, maxdisk=0\n", NPROCS);
}

/* Called at the beginning to inquire how many bars are needed. */
int
num_bars()
{
	if(!cpuflag) return 0;
	if (0>(kmem = open(KMEM, O_RDONLY))){
		perror(KMEM);
		exit(1);
	}
	if (0>nlist(kernelSymbols?kernelSymbols:KSYMS, nl)){
		perror(kernelSymbols?kernelSymbols:KSYMS);
		exit(1);
	}
#if 1==NPROCS
	ncpus=1;
	return NPROCS;
	/* NOTREACHED */
#endif
	if ((off_t)nl[N_NCPU].n_value != 
	    lseek(kmem,(off_t)nl[N_NCPU].n_value, SEEK_SET)){
		perror("lseek _number_of_cpus");
		exit(1);
	}
	if (sizeof(ncpus) != read(kmem, (char *)&ncpus, sizeof(ncpus))){
		perror("read _number_of_cpus");
		exit(1);
	}
	return ncpus;
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

	for(i=0; i<nbars; i++){
		items[i] = CPU_NTIMES;
	}
}

/* Called after num_bars to ask for the bar names */
/* ARGSUSED */
char **
label_bars(nbars)
{
        char **names;
        int i;
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

	/* do cpu names */
	for(i=0; i<ncpus; i++) {
		(void) sprintf(buf, "%s%s%s%d", hname, 
				hname[0]?" ":"", cpname, i);
		names[i] = (char *)xmalloc(strlen(buf)+1);
		(void) strcpy(names[i], buf);
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
	int i,j;

	if ((off_t)nl[N_CPUINFO].n_value != 
	    lseek(kmem,(off_t)nl[N_CPUINFO].n_value, SEEK_SET)){
		perror("lseek _cpuinfo");
		exit(1);
	}
	if ((ncpus * sizeof(struct cpuinfo)) != 
	    read(kmem, (char *)cpu, ncpus * sizeof(struct cpuinfo))){
		perror("read _cpuinfo");
		exit(1);
	}
	for (i=0;i<ncpus;i++) {
		for(j=0;j<CPU_NTIMES;j++){
			cp_old[i][j]=cpu[i].cpu[j];
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
	int	states[CPU_NTIMES];
	int	nstates;
	int	i,j;

	if ((off_t)nl[N_CPUINFO].n_value != 
	    lseek(kmem,(off_t)nl[N_CPUINFO].n_value, SEEK_SET)){
		perror("lseek _cpuinfo");
		exit(1);
	}
	if ((ncpus * sizeof(struct cpuinfo)) != 
	    read(kmem, (char *)cpu, ncpus * sizeof(struct cpuinfo))){
		perror("read _cpuinfo");
		exit(1);
	}

#define delta(c) ((int) (cp_time[i][(c)] - cp_old[i][(c)]))

	for (i=0; i<ncpus; i++) {
		for(j=0;j<CPU_NTIMES;j++){
			cp_time[i][j]=cpu[i].cpu[j];
		}
		nstates = 0;
		if(sep_wait){
			states[nstates++] = delta(CPU_IDLE);
			states[nstates++] = delta(CPU_WAIT);
		} else {
			states[nstates++] = delta(CPU_IDLE) + delta(CPU_WAIT);
		}
		states[nstates++] = delta(CPU_USER);
		states[nstates++] = delta(CPU_KERNEL);
		draw_bar(i, states, nstates);
	}
	for (i=0; i<ncpus; i++){
		for (j=0; j<CPU_NTIMES; j++){
			cp_old[i][j] = cp_time[i][j];
		}
	}
}
