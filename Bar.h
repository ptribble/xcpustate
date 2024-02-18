/* $XConsortium: Bar.h,v 1.2 88/10/25 17:22:09 swick Exp $ */
/* Copyright	Massachusetts Institute of Technology	1987, 1988 */

#ifndef _Bar_h
#define _Bar_h

/****************************************************************
 *
 * Bar widget
 *
 ****************************************************************/

/* Resources:

 Name		     Class		RepType		Default Value
 ----		     -----		-------		-------------
 background	     Background		Pixel		XtDefaultBackground
 border		     BorderColor	Pixel		XtDefaultForeground
 borderWidth	     BorderWidth	Dimension	1
 destroyCallback     Callback		Pointer		NULL
 height		     Height		Dimension	0
 mappedWhenManaged   MappedWhenManaged	Boolean		True
 sensitive	     Sensitive		Boolean		True
 width		     Width		Dimension	0
 x		     Position		Position	0
 y		     Position		Position	0
 fillStyle	     FillStyle		Short		0
 pixmaps	     Pixmaps		Array		0

*/

/* declare specific BarWidget class and instance datatypes */

typedef struct _BarClassRec*	BarWidgetClass;
typedef struct _BarRec*		BarWidget;

/* declare barWidget name and class atoms */
/*
 * If FillStyle is solid, the array pixmaps contains the Pixels to use
 * for the display.  If FillStyle is FillTiled, the array contains the
 * Pixmaps to use for tiling, if FillStyle is FillStippled or
 * FillOpaqueStippled, then the array contains the Pixmaps to be used as
 * stipples.
 */
#define XtNfillStyle	"fillStyle"
#define XtCFillStyle	"FillStyle"
#define XtNpixmaps	"pixmaps"
#define XtCPixmaps	"Pixmaps"

/* declare the class constant */

extern WidgetClass barWidgetClass;

/* Public procedures */
extern void SetBarValues(/* BarWidget w, int *values, int nvalues */);

#endif  /* _Bar_h */
