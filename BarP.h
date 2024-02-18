/* $XConsortium: BarP.h,v 1.2 88/10/25 17:37:59 swick Exp $ */
/* Copyright	Massachusetts Institute of Technology	1987, 1988 */

#ifndef _BarP_h
#define _BarP_h

#include <X11/Xmu/Xmu.h>
#include "Bar.h"

/* define unique representation types not found in <X11/StringDefs.h> */

#define MAXGRAY 10

typedef struct {
    caddr_t	extension;	/* C compiler needs one field at least */
} BarClassPart;

typedef struct _BarClassRec {
    CoreClassPart	core_class;
    BarClassPart	bar_class;
} BarClassRec;

extern BarClassRec barClassRec;

typedef struct {
    /* private state */
    int *values;		/* Array of values displayed in bar */
    int nvalues;		/* Number of elements in values */
    Boolean	  paintTile;	/* if true, use tiling instead of stippling */
    XtOrientation orientation;	/* XtorientHorizontal or XtorientVertical */
    Dimension	  length;	/* either height or width */
    Dimension	  thickness;	/* either width or height */
    int		  fillstyle;	/* fillstyle to use for XFillRectangle */
    Pixel	  foreground;	/* foreground colour */
    Array	  *pixmaps;	/* pixels/tiles/stipples to use */
    GC		  gc;		/* a gc */
} BarPart;

typedef struct _BarRec {
    CorePart	core;
    BarPart	bar;
} BarRec;

#endif  /* _BarP_h */
