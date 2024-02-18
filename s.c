/*
 * for each type of machine, stick the appropriate ifdef's in here and define
 * s_included.  
 */
#ifdef sgi
# ifdef mips
#  include "s-irix.c"
#  define s_included
# endif
#endif

#ifdef CRAY1
# include "s-unicos.c"
# define s_included
#endif

#if defined(MACH)
# include "s-mach.c"
# define s_included
#endif

#ifdef kap
# include "s-solbourne.c"
# define s_included
#endif

#ifdef ultrix
# include "s-ultrix.c"
# define s_included
#endif

#ifdef GOULD_NP1
# include "s-gould-np1.c"
# define s_included
#endif

#ifdef SUNOS4
# include "s-sunos4.c"
# define s_included
#endif

#ifdef SUNOS5
# include "s-sunos5.c"
# define s_included
#endif

#ifdef NCR
# include "s-ncr.c"
# define  s_included
#endif

#ifdef __bsdi__
# include "s-bsdi.c"
# define  s_included
#endif

#if defined(__FreeBSD__) || defined(__NetBSD__) 
# include "s-bsd44.c"
# define  s_included
#endif

#if defined(__OpenBSD__) 
#include <sys/param.h>
#if OpenBSD <= 200111
#  include "s-bsd44.c"
# else
#  include "s-openbsd.c"
# endif
# define  s_included
#endif

#if defined( SVR4 ) && !defined( SUNOS5 ) && !defined( IRIX )
# include "s-svr4.c"
# define  s_included
#endif

#ifdef _AIX
# include "s-aix.c"
# define  s_included
#endif

#if !defined(s_included) && defined(BSD)
# include "s-bsd.c"
#endif

#ifdef linux
# include "s-linux.c"
# define  s_included
#endif

#ifdef __CYGWIN__
# include "s-cygwin.c"
# define  s_included
#endif

#ifndef s_included
  /* "nothing" mode: stub functions */
  int num_bars(){ return 0; }
  void bar_items(){}
  char **label_bars(){}
  void init_bars(){}
  void display_bars(){}
#endif
/* Do not add anything after this line */
