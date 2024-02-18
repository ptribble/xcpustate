/*
 * System dependent file for 4.4BSD derivatives.  Works on FreeBSD 2.x.
 * Should work on NetBSD 1.x and OpenBSD
 * Based on s-bsd.c by Chris Siebenmann <cks@sys.toronto.edu>
 */
/* David O'Brien, University of California at Davis <obrien@cs.ucdavis.edu> */
/*
 * Utilizes the kvm library avalible on BSD 4.4.
 * Binary should be sgid ``kmem''
 */

/* LINTLIBRARY */

#include <sys/param.h>
#if !(defined(BSD) && (BSD >= 199306)) 	/* ie. BSD 4.4 systems */
#error This is not a 4.4BSD-Lite dervived system.
#endif

#include <sys/dkstat.h>
#include <kvm.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>		/* for perror() */

#include <nlist.h>


extern char *xmalloc(/* int nbytes */);
extern void shorten(/* char *hname */);

extern char *kernelSymbols;
static kvm_t *kmem;			/* pointer into the kmem */

long	cp_time[CPUSTATES];
long	cp_old[CPUSTATES];

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
	printf("BSD4.4+: maxcpu=%d, maxdisk=0\n", NPROCS);
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
	int j;
	char errbuf[ 1024 ];

	/* if ((kmem = kvm_open(NULL, NULL, NULL, O_RDONLY, "kvm_open")) == NULL) { */
	if ((kmem = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, errbuf)) == NULL) {
		perror("/dev/kmem");
		fprintf(stderr, "%s\n", errbuf );
		exit(1);
	}
	/* get the list of symbols we want to access in the kernel */
	if (kvm_nlist(kmem, nl) == -1) 
		fprintf( stderr, "kvm error, %s\n", kvm_geterr(kmem));
		
	if (nl[0].n_type == 0)
		fprintf(stderr, "nlist failed\n");

	if ((j=kvm_read(kmem, nl[X_CP_TIME].n_value, (char *) cp_old,
		sizeof cp_old)) != sizeof cp_old) {
		perror("kvm_read,1");
		fprintf( stderr, "kvm_read returns %d\n", j );
		fprintf( stderr, "kvm error, %s\n", kvm_geterr(kmem));
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
	
	if ((i=kvm_read(kmem, nl[X_CP_TIME].n_value, (char *) cp_time,
		sizeof cp_time)) != sizeof cp_time)
	{
		perror("kvm_read,2");
		fprintf( stderr, "kvm_read returns %d\n", i );
		fprintf( stderr, "kvm error, %s\n", kvm_geterr(kmem));
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
