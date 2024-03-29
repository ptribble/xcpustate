%/*
% * Copyright (c) 1985, 1990, 1991 by Sun Microsystems, Inc.
% */

%/* from rstat.x */

/*
 * Gather statistics on remote machines
 */

#ifdef RPC_HDR
%
%#pragma ident	"@(#)rstat.x	1.2	92/07/14 SMI"
%
%#ifndef FSCALE
%/*
% * Scale factor for scaled integers used to count load averages.
% */
%#define FSHIFT  8               /* bits to right of fixed binary point */
%#define FSCALE  (1<<FSHIFT)
%
%#endif /* ndef FSCALE */

%#ifndef DST_NONE
%#include <sys/time.h>          /* The time struct defined below is meant to */
%#endif                         /* match struct timeval.                     */
%
%
%
%
%
%
#elif RPC_SVC
%
%/*
% *  Server side stub routines for the rstat daemon
% */
%
#elif RPC_CLNT
%
%/*
% *  Client side stub routines for the rstat daemon
% */
%
#elif RPC_XDR
%/*
% * XDR routines for the rstat daemon, rup and perfmeter.
% */
%
%/*
% * xdr_timeval was used in previous releases.
% */
%
%bool_t
%#ifdef __STDC__
%xdr_timeval(XDR *xdrs, struct timeval *tvp)
%#else /* K&R C */
%xdr_timeval(xdrs, tvp)
%	XDR *xdrs;
%	struct timeval *tvp;
%#endif /* K&R C */
%{
%	return (xdr_rstat_timeval(xdrs, tvp));
%}

%
#endif

const RSTAT_CPUSTATES = 4;
const RSTAT_DK_NDRIVE = 4;

/*
 * GMT since 0:00, January 1, 1970
 */
struct rstat_timeval {
	long tv_sec;	/* seconds */
	long tv_usec;	/* and microseconds */
};

struct statsvar {				/* RSTATVERS_VAR */
	int cp_time<>; 		/* variable number of CPU states */
	int dk_xfer<>;		/* variable number of disks */
	unsigned v_pgpgin;	/* these are cumulative sum */
	unsigned v_pgpgout;
	unsigned v_pswpin;
	unsigned v_pswpout;
	unsigned v_intr;
	int if_ipackets;
	int if_ierrors;
	int if_opackets;
	int if_oerrors;
	int if_collisions;
	unsigned v_swtch;
	long avenrun[3];
	rstat_timeval boottime;
	rstat_timeval curtime;
};

struct statstime {				/* RSTATVERS_TIME */
	int cp_time[RSTAT_CPUSTATES];
	int dk_xfer[RSTAT_DK_NDRIVE];
	unsigned int v_pgpgin;	/* these are cumulative sum */
	unsigned int v_pgpgout;
	unsigned int v_pswpin;
	unsigned int v_pswpout;
	unsigned int v_intr;
	int if_ipackets;
	int if_ierrors;
	int if_oerrors;
	int if_collisions;
	unsigned int v_swtch;
	long avenrun[3];
	rstat_timeval boottime;
	rstat_timeval curtime;
	int if_opackets;
};

struct statsswtch {                     /* RSTATVERS_SWTCH */
        int cp_time[RSTAT_CPUSTATES];
        int dk_xfer[RSTAT_DK_NDRIVE];
        unsigned int v_pgpgin;  /* these are cumulative sum */
        unsigned int v_pgpgout;
        unsigned int v_pswpin;
        unsigned int v_pswpout;
        unsigned int v_intr;
        int if_ipackets;
        int if_ierrors;
        int if_oerrors;
        int if_collisions;
        unsigned int v_swtch;
        unsigned int avenrun[3];/* scaled by FSCALE */
        rstat_timeval boottime;
        int if_opackets;
};

struct stats {                          /* RSTATVERS_ORIG */
        int cp_time[RSTAT_CPUSTATES];
        int dk_xfer[RSTAT_DK_NDRIVE];
        unsigned int v_pgpgin;  /* these are cumulative sum */
        unsigned int v_pgpgout;
        unsigned int v_pswpin;
        unsigned int v_pswpout;
        unsigned int v_intr;
        int if_ipackets;
        int if_ierrors;
        int if_oerrors;
        int if_collisions;
        int if_opackets;
};

program RSTATPROG {
        /*
         * Version 4 allows for variable number of disk and RSTAT_CPU states.
         */
	version RSTATVERS_VAR {
		statsvar
		RSTATPROC_STATS (void) = 1;
		unsigned int
		RSTATPROC_HAVEDISK (void) = 2;
	} = 4;
	/*
	 * Newest version includes current time and context switching info
	 */
	version RSTATVERS_TIME {
		statstime
		RSTATPROC_STATS(void) = 1;
		unsigned int
		RSTATPROC_HAVEDISK(void) = 2;
	} = 3;
        /*
         * Does not have current time
         */
        version RSTATVERS_SWTCH {
                statsswtch
                RSTATPROC_STATS(void) = 1;
  
                unsigned int
                RSTATPROC_HAVEDISK(void) = 2;
        } = 2;
        /*
         * Old version has no info about current time or context switching
         */
        version RSTATVERS_ORIG {
                stats
                RSTATPROC_STATS(void) = 1;
  
                unsigned int
                RSTATPROC_HAVEDISK(void) = 2;
        } = 1;
} = 100001;
