/*
 * System dependent file for Cray Unicos >= 5.1 Systems
 * Walter D. Poxon, Cray Research, Inc., Eagan, MN  <wdp@cray.com>
 */

#include <nlist.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/unistd.h>
#include <sys/param.h>
#include <sys/pws.h>
#include <errno.h>

#define UNICOS "/unicos"
#define KMEM "/dev/kmem"

#define NSTATES 6
#define NCPUSTATES 3

int num_bars(void);
void bar_items(int nbars, int items[]);
char ** label_bars(int nbars);
void init_bars(int nbars);
void display_bars(int nbars);

static struct nlist nl[] = { {"cpuw"},{0} };
int kmem;
long bytes;
struct pw cpuw;

extern int sep_wait;
extern char *kernelSymbols;

extern char *xmalloc(/* int nbytes */);

union cpu_name {
	word wval;
	struct {
		uint : 32,
		c    : 8,
		p    : 8,
		u    : 8,
		no   : 8;
	} cval;
};

/* Called by -version */
void
version()
{
	printf("Cray UNICOS 5.1+: maxdisk=0\n");
}

/* Called at the beginning to inquire how many bars are needed. */
int
num_bars(void)
{
	bzero(&cpuw,sizeof(cpuw));

	if (nlist(kernelSymbols?kernelSymbols:UNICOS,nl) == -1) {
		perror("nlist");
		return 0;
	}

	bytes = nl[0].n_value;
	
	if ((kmem = open(KMEM, O_RDONLY)) <0) {
		perror("open");
		return 0;
	}

	if (lseek(kmem, bytes, SEEK_SET) != bytes) {
		perror("lseek");
		return 0;
	}
	

	if (read(kmem, &cpuw, sizeof(cpuw)) != sizeof(cpuw)){
		perror("read");
		return 0;
	}

	return cpuw.pw_ccpu;       /* Number of CPU's configured in UNICOS */
}


/*
 * Indicates how many levels each bar has.  For most machines, each bar will
 * have the same stuff.  But one can, for instance, display memory use on one
 * bar, processor levels on others, etc.
 */
void
bar_items(int nbars, int items[])
{
	int i;
	
	for (i = 0; i < nbars; i++)
		items[i] = NCPUSTATES;
}


char **
label_bars(int nbars)          /* Called after num_bars to ask for bar names */
{
	char **names;
	int i;
	extern char *strcpy(/* char *, const char * */);
	union cpu_name c;

	names = (char **) xmalloc(nbars * sizeof(char *));

	for (i = 0; i < nbars; i++) {
		char buf[8];
		c.wval = cpuw.pws[i].pw_cpu;
		sprintf(buf, "%c%c%c%c", c.cval.c, c.cval.p, c.cval.u,
			c.cval.no);
		names[i] = strcpy(xmalloc(strlen(buf)+1), buf);
	}

	return names;
}


/* 
 *  Called after the bars are created to perform any machine dependent
 *  initializations.
 */
void
init_bars(int nbars)
{
	display_bars(nbars);
}


/* 
 *  This procedure gets called every interval to compute and display the
 *  bars. It should call draw_bar() with the bar number, the array of
 *  integer values to display in the bar, and the number of values in
 *  the array.
 */

#define delta(cpustate) \
  ((int) (si[i]->cpu[(cpustate)] - last_si[i]->cpu[(cpustate)]))

void
display_bars(int nbars)
{
	int states[NSTATES];
	int nstates;
	int cpu;
	extern void draw_bar(/*int bar_num, int *states, int num_states*/);

	if (lseek(kmem, bytes, SEEK_SET) != bytes) {
		perror("lseek");
		return;
	}
	
    
	if (read(kmem, &cpuw, sizeof(cpuw)) != sizeof(cpuw)) {
		perror("read");
		return;
	}
	

	for (cpu=0; cpu < nbars; cpu++) {
		nstates = 0;

		if(sep_wait){
			states[nstates++] = cpuw.pws[cpu].pw_idlee;
			states[nstates++] = cpuw.pws[cpu].pw_syswe;
		} else {	
			states[nstates++] = cpuw.pws[cpu].pw_syswe
				+ cpuw.pws[cpu].pw_idlee;
		}
		states[nstates++] = cpuw.pws[cpu].pw_usere;
		states[nstates++] = cpuw.pws[cpu].pw_unixe;

		draw_bar(cpu, states, nstates);
	}
}
