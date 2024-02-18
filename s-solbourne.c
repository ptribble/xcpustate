/* System dependent file for Solbourne workstations running OS/MP.	*/
/*
 * Copyright 1990 Solbourne Computer, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Solbourne not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Solbourne Computer, Inc. makes no representations 
 * about the suitability of this software for any purpose.  It is provided 
 * "as is" without express or implied warranty.
 *
 * SOLBOURNE COMPUTER, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. 
 * IN NO EVENT SHALL SOLBOURNE COMPUTER, INC. BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  William Kucharski, Solbourne Computer, Inc.
 *          kucharsk@Solbourne.COM
 *	    ...!{boulder,sun,uunet}!stan!kucharsk
 */
/* LINTLIBRARY */

#include <sys/syscall.h>
#include <sys/param.h>
#include <sys/dk.h>
#include <nlist.h>

extern char *xmalloc(/* int nbytes */);

extern int open(), read();
extern long lseek();

extern char *kernelSymbols();

long	cp_time[MAXNCPUS][CPUSTATES];
long	cp_old[MAXNCPUS][CPUSTATES];
int	kmem;			/* file descriptor of /dev/kmem. */
struct nlist	nl[] = {
#define	X_CP_TIME	0
	{ "_cp_mp_time" },
	{ "" },
};

/* Called by -version */
void
version()
{
	printf("Solbourne OS/MP: maxcpus=%d, maxdisk=0\n", MAXNCPUS);
}

/* Called at the beginning to inquire how many bars are needed. */

int
num_bars()
{
	unsigned int	status;
	unsigned int	width;
	unsigned int	type;

	int	numcpus = 0;
	int	proc;

	if (syscall(SYS_getcpustatus, &status, &width, &type) < 0) {
		perror("getcpustatus");
		exit(1);
	}

	for (proc = 0; proc < width; proc++)
		if ((status >> proc) & 1)
			numcpus++;

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
    extern char *strcpy(/* char *, const char * */);

    char **names;
    int i;

    static char hname[MAXHOSTNAMELEN];

    if (gethostname(hname, MAXHOSTNAMELEN) < 0) {
        perror("gethostname");
        hname[0] = '\0';
    }

    names = (char **) xmalloc(nbars * sizeof(char *));
    if (nbars == 1) {
	    char buf[MAXHOSTNAMELEN + 3];
	    (void) sprintf(buf, "%s: ", hname);
	    names[0] = strcpy(xmalloc(strlen(buf) + 1), buf);
    } else {
    for(i = 0; i < nbars; i++) {
        char buf[MAXHOSTNAMELEN + 6];

		    /* 
		     * Use bar number % 10 in case someone someday can stuff
		     * more than 9 CPUs into a Solbourne box...
		     */

		    (void) sprintf(buf, "%s(%d): ", hname, (i % 10));
        names[i] = strcpy(xmalloc(strlen(buf) + 1), buf);
	    }
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
    
    if ((kmem = open("/dev/kmem", 0)) < 0) {
	    perror("/dev/kmem");
	    exit(1);
    }
    (void)nlist(kernelSymbols?kernelSymbols:"/vmunix", nl);
    if (lseek(kmem, (long) nl[X_CP_TIME].n_value, 0) !=
	(long) nl[X_CP_TIME].n_value)
	    perror("lseek");
    if (read(kmem, (char *) cp_old, sizeof(cp_old)) !=
	sizeof(cp_old))
	    perror("read");
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
	int	nstates;
	int	states[CPUSTATES];
	int	i;
	int	proc;

	extern void draw_bar(/*int bar_num, int *states, int num_states*/);
	
	if (lseek(kmem, (long) nl[X_CP_TIME].n_value, 0) !=
	    (long) nl[X_CP_TIME].n_value)
		perror("lseek");
	if (read(kmem, (char *) cp_time, sizeof(cp_time)) !=
	    sizeof(cp_time))
		perror("read");
	
#define delta(cpu, cpustate) ((int) (cp_time[cpu][(cpustate)] - \
	cp_old[cpu][(cpustate)]))

	for (proc = 0; proc < nbars; proc++) {
		nstates = 0;
		states[nstates++] = delta(proc, CP_IDLE);
		states[nstates++] = delta(proc, CP_USER);
		states[nstates++] = delta(proc, CP_NICE);
		states[nstates++] = delta(proc, CP_SYS);
		draw_bar(proc, states, nstates);
		for (i = 0; i < CPUSTATES; i ++)
			cp_old[proc][i] = cp_time[proc][i];
	}
}
