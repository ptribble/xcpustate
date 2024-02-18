/*
 * System dependent file for Linux.   Includes SMP support.
 *
 * Kumsup Lee <klee@ima.umn.edu>
 * John DiMarco <jdd@cs.toronto.edu>,  University of Toronto, CSLab
 * Greg Nakhimovsky <greg.nakhimovsky@sun.com> May 2004
 *   - fixed a bug in display_bars() for SMP
 *   - added Linux support for "-disk"
 * Greg Nakhimovsky <greg.nakhimovsky@sun.com> April 2006
 *   - added disk stats for the >= 2.6 Linux kernel using /proc/diskstats
 */

/* LINTLIBRARY */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>

/* 
 * NR_CPUS in <linux/threads.h> sets the maximum number of CPUs
 * this version of Linux can handle.  CONFIG_SMP needs to be set first,
 * else NR_CPUS will be one. In old Linux versions, NR_CPUS used
 * to be in <nlist.h>.  We use this number to size the CPU state
 * data structures.
 */
#define CONFIG_SMP
#include <linux/threads.h>
/* if NR_CPUS is undefined or 1, need to set it to a more reasonable number */
#define DEFAULT_MAXCPUS 8
#ifndef NR_CPUS
#define NR_CPUS DEFAULT_MAXCPUS
#endif /* NR_CPUS */
#if NR_CPUS == 1 
#define NR_CPUS DEFAULT_MAXCPUS
#endif /* NR_CPUS */
#define CPUSTATES 4   /* idle, user, nice, system */
static long	cp_time[NR_CPUS+1][CPUSTATES];
static long	cp_old[NR_CPUS+1][CPUSTATES];
static int maxcpus=NR_CPUS;

/* 
 * DK_MAX_DISK in <linux/kernel_stat.h> sets the maximum number of disks
 * this kernel keeps statistics on. Unfortunately, including this
 * file with CONFIG_SMP set causes unwanted #include dependancies, so unset
 * it first.  Warning: this resets NR_CPUS to 1; use maxcpus.
 */
#undef CONFIG_SMP 
#include <linux/kernel_stat.h>
#ifndef DK_MAX_DISK
#define DK_MAX_DISK 16	/* pick a reasonable default if not set */
#endif /* DK_MAX_DISK */

#ifndef STATFILE
#define STATFILE "/proc/stat"
#define STATFILE_26 "/proc/diskstats"
#endif /* STATFILE */

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif /* MAXHOSTNAMELEN */

/* 
 * MAXLINELEN is the maximum line length in STATFILE.  All the
 * disks are on one line in STATFILE, which thus could be very long if 
 * DK_MAX_DISK is large, so set it to a minimum size and bump it up if 
 * DK_MAX_DISK warrants.
 */
#define MAXLINELEN DK_MAX_DISK*128
#if MAXLINELEN < 1024
#define MAXLINELEN 1024
#endif /* MAXLINELEN < 1024 */

#define DISKSTATES 3  /* idle, io_reads, io_writes */

extern void shorten(/* char *hname */);
extern void draw_bar(/*int bar_num, int *states, int num_states*/);

extern int cpuflag;
extern int diskflag;

#define WHITESPACE "	 "
#define CPU "cpu"
#define DISK "disk_io:"

extern char *xmalloc(/* int nbytes */);

static char line[MAXLINELEN];

static int numcpus=0;
static int numdisks=0;
static FILE *proc_stat;
static FILE *proc_diskstats;
static unsigned int d; /* dummy */
static char devicename[10];
static int ret;
static unsigned int v_major, v_index;

/* Flag if we are running >= 2.6 kernel with disk stats in /proc/diskstats */
static int diskstats_26=0;
/*
 * Consider it a real disk if a line in /proc/diskstats:
 * 1) Has 14 fields, and
 * 2) Device name does not start with "ram", and
 * 3) Device name does not end with a digit (like "md0")
 */
#define REAL_DISK26 (ret == 14 && strncmp(devicename, "ram", 3) != 0 && \
                    isalpha(*(devicename+strlen(devicename)-1)))

/* Called by -version */
void
version()
{
	printf("Linux: maxcpu=%d, maxdisk=%d\n", maxcpus, DK_MAX_DISK);
}

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

	int foundcpu=0;  /* we found a line beginning with "cpu " */
	char *p;

	/* flag if we are running >= 2.6 Linux kernel; count disks */
	if(0!=(proc_diskstats=fopen(STATFILE_26, "r"))) {
	  diskstats_26 = 1;
	  if(diskflag) {
	    while(NULL!=fgets(line, sizeof(line)-1, proc_diskstats)){	
  	      ret = sscanf(line, "%d%d%s%d%d%d%d%d%d%d%d%d%d%d", 
		    &d, &d, devicename, &d, &d, &d, &d, &d, &d, &d, &d, &d, &d, &d);
	      if(REAL_DISK26)
		numdisks++;
	    }
	  }
	  (void)fclose(proc_diskstats);
	}

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
		else if(diskflag && !diskstats_26 &&
		        0==strncmp(line, DISK, strlen(DISK))){
		  p = &line[strlen(DISK)];
		  while(NULL!=(p=strchr(p, ':'))) {
	      	    numdisks++;
		    p++;
		  }
		  goto DONE;
		}
	}
	DONE:
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
	if(numcpus>(maxcpus+1)) numcpus=maxcpus+1;
	if(numdisks>DK_MAX_DISK) numdisks=DK_MAX_DISK;
	return(numcpus+numdisks);
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

    for(i = 0; i < numcpus; i++,n++)
        items[i] = CPUSTATES;

    if(diskflag){
        for(i = 0; i < numdisks; i++,n++){
            items[n] = DISKSTATES;
        }
    }

}


/* Called after num_bars to ask for the bar names */
/* ARGSUSED */
char **
label_bars(nbars)
{
    static char **names;
    int i;
    static char hname[MAXHOSTNAMELEN+1];
    char buf[MAXHOSTNAMELEN + 1 + 32];
    unsigned int done, pos;

    hname[MAXHOSTNAMELEN] = '\0';
    if (gethostname(hname, MAXHOSTNAMELEN) < 0) {
	perror("gethostname");
	*hname = '\0';
    }
    shorten(hname);
    names=(char **) xmalloc(nbars * sizeof(char *));
    for(i=0; i < numcpus; i++) {
#define CPUNAME "%s %d"

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

    if(diskflag) {
      if(diskstats_26) {
	if((proc_diskstats=fopen(STATFILE_26, "r")) == NULL ) {
           perror(STATFILE_26);
	   exit(1);
	}
	i = numcpus;
	while(NULL!=fgets(line, sizeof(line)-1, proc_diskstats)){	
	  ret = sscanf(line, "%d%d%s%d%d%d%d%d%d%d%d%d%d%d", 
			&v_major, &v_index, devicename, 
			&d, &d, &d, &d, &d, &d, &d, &d, &d, &d, &d);
	  if(REAL_DISK26) {
            (void) sprintf(buf, "disk %s %d-%d", devicename, v_major, v_index);
            names[i++] = strcpy(xmalloc(strlen(buf) + 1), buf);
	  }
	}
	(void)fclose(proc_diskstats);
      }
      else {
	if((proc_stat=fopen(STATFILE,"r")) == NULL ) {
           perror(STATFILE);
	   exit(1);
	}
	/* Get the /proc/stat line with the current disk data */
	done = 0;
	while(!done && NULL!=fgets(line, sizeof(line)-1, proc_stat)){
          if(0==strncmp(line, DISK, strlen(DISK)))
          done = 1;
	}
	(void)fclose(proc_stat);
	pos = 9;
	for(i=numcpus; i < numcpus+numdisks; i++) {
          sscanf(line+pos, "(%u,%u):(%*u,%*u,%*u,%*u,%*u) ",
             &v_major, &v_index);
	  pos += strcspn(line + pos, " ") + 1;
          (void) sprintf(buf, "disk %d-%d", v_major, v_index);
          names[i] = strcpy(xmalloc(strlen(buf) + 1), buf);
      }
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
		   /* There is no need to read the entire line here */
	    }
#endif /* SHOWAVG */
            for (i=0; i < nbars; i++) {
                fscanf(proc_stat, "%s %ld %ld %ld %ld \n",
		       dummy, &cp_old[i][0], &cp_old[i][1], &cp_old[i][2],
		       &cp_old[i][3]);
		   /* There is no need to read the entire line here */
	    }
	}
	(void)fclose(proc_stat);
}

/* ARGSUSED */
void
display_cpus(void)
{
	int	states[CPUSTATES];
	int	nstates;
	int	i, done;
	char dummy[256];
	extern void draw_bar(/*int bar_num, int *states, int num_states*/);

        if((proc_stat=fopen(STATFILE,"r")) == NULL ) {
                perror(STATFILE);
        } else {
#ifndef SHOWAVG
	    if(numcpus>1){
		/* skip the first ("cpu") line */
		   fscanf(proc_stat, "%s %ld %ld %ld %ld",
                       dummy, &cp_time[0][0], &cp_time[0][1], &cp_time[0][2],
                       &cp_time[0][3]);
		/* SMP bug fix: skip all chars before newline */
		while ((getc(proc_stat)) != '\n')
		  ;
	    }
#endif /* SHOWAVG */
            for (i=0; i < numcpus; i++)
	    {
                fscanf(proc_stat, "%s %ld %ld %ld %ld",
                       dummy, &cp_time[i][0], &cp_time[i][1], &cp_time[i][2],
                       &cp_time[i][3]);
                /* SMP bug fix: skip all chars before newline */
                while ((getc(proc_stat)) != '\n')
                  ;
	    }
	    if(diskflag && !diskstats_26) {
	        /* get the /proc/stat line with the current disk io data */
		done = 0;
	    	while(!done && NULL!=fgets(line, sizeof(line)-1, proc_stat)){
		  if(0==strncmp(line, DISK, strlen(DISK)))
		    done = 1;
		}
            }
        }
	(void)fclose(proc_stat);

	for (i=0; i < numcpus; i++) {
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

static void
display_disk(int base)
{
  /* Uses disk_io in /proc/stat for 2.4 or /proc/diskstats for 2.6 kernel */
  /* Assume that disks may not be registered or unregistered dynamically */
  int i, idisk, pos=9;
  static unsigned int io_reads[DK_MAX_DISK][2], io_writes[DK_MAX_DISK][2];
  static unsigned int io_ops_max[DK_MAX_DISK];
  static unsigned int curr=0;
  static unsigned int doit[DK_MAX_DISK]; /* initialized to zeros */
  int states[DISKSTATES];

  /* Read disks I/O statistics */
  if(diskstats_26) {
    if((proc_diskstats=fopen(STATFILE_26, "r")) == NULL ) {
      perror(STATFILE_26);
      exit(1);
    }
    idisk = 0;
    while(NULL!=fgets(line, sizeof(line)-1, proc_diskstats)){
      ret = sscanf(line, "%d%d%s%d%d%d%d%d%d%d%d%d%d%d", 
              &v_major, &v_index, devicename, 
 	      &io_reads[idisk][curr], &d, &d, &d, &io_writes[idisk][curr],
	      &d, &d, &d, &d, &d, &d);
      if(REAL_DISK26) {
	states[1] = abs(io_reads[idisk][curr] - io_reads[idisk][!curr]);   /* current io_reads */
	states[2] = abs(io_writes[idisk][curr] - io_writes[idisk][!curr]); /* current io_writes */
	if(doit[idisk] && (states[1]+states[2]) > io_ops_max[idisk])
	  io_ops_max[idisk] = states[1]+states[2];
	states[0] = abs(io_ops_max[idisk] - states[1] - states[2]); /* max - current */
	if(states[0] == 0) /* to prevent garbage from showing */
	  states[0] = 1;
	if(!doit[idisk]) { /* show full "idle" for the first time */
	  states[0] = 1;
	  states[1] = 0;
	  states[2] = 0;
	  io_ops_max[idisk] = 1;
	}
	/*
	printf("DEBUG: disk %d,%d: Calling draw_bar() for disk %d with states: %d,%d,%d\n",
	  v_major, v_index, idisk, states[0], states[1], states[2]);
	*/
	draw_bar(numcpus+idisk, states, 3);
	doit[idisk] = 1;
	/* prepare for the next call */
	idisk++;
      }
    }
    (void)fclose(proc_diskstats);
    curr ^= 1;
  }
  else {
    for (i=numcpus; i < numcpus+numdisks; i++) {
      idisk = i - numcpus;
      sscanf(line+pos, "(%u,%u):(%*u,%u,%*u,%u,%*u) ",
	     &v_major, &v_index, &io_reads[idisk][curr], &io_writes[idisk][curr]);
      pos += strcspn(line + pos, " ") + 1;
      states[1] = abs(io_reads[idisk][curr] - io_reads[idisk][!curr]);   /* current io_reads */
      states[2] = abs(io_writes[idisk][curr] - io_writes[idisk][!curr]); /* current io_writes */
      if(doit[idisk] && (states[1]+states[2]) > io_ops_max[idisk])
	io_ops_max[idisk] = states[1]+states[2];
      states[0] = abs(io_ops_max[idisk] - states[1] - states[2]); /* max - current */
      if(states[0] == 0) /* to prevent garbage from showing */
	states[0] = 1;
      if(!doit[idisk]) { /* show full "idle" for the first time */
	states[0] = 1;
	states[1] = 0;
	states[2] = 0;
	io_ops_max[idisk] = 1;
      }
      /*
      printf("DEBUG: disk %d,%d: Calling draw_bar with %d,%d,%d\n",
	v_major, v_index, states[0], states[1], states[2]);
      */
      draw_bar(i, states, 3);
      doit[idisk] = 1;
    }
    curr ^= 1;  /* prepare for the next call */
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
display_bars(int nbars)
{	
	if(cpuflag)
		display_cpus();
	if(diskflag)
		display_disk(numcpus);

}

