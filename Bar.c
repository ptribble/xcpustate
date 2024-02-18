/* #include <X11/copyright.h> */

/* $XConsortium: Bar.c,v 1.2 88/10/25 17:40:25 swick Exp $ */
/* Copyright	Massachusetts Institute of Technology	1987, 1988 */

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include "array.h"
#include "BarP.h"

static Dimension def_dimension	= (Dimension)~0;
static XtOrientation def_orient = XtorientHorizontal;
static Boolean def_true = True;

#define DEF_LENGTH  ((Dimension) 200)
#define DEF_THICKNESS	((Dimension) 15)

/*
 * !! should provide converters for Arrays of some supported types, and
 * for fillstyles
 */
static XtResource resources[] = {
#define offset(field) XtOffset(BarWidget, bar.field)
#define Offset(field) XtOffset(BarWidget, field)
    /* {name, class, type, size, offset, default_type, default_addr}, */
    /*
     * We define these core class values to set defaults, so we can
     * initialize correctly
     */
    {XtNwidth, XtCWidth, XtRDimension, sizeof(Dimension),
	Offset(core.width), XtRDimension, (caddr_t)&def_dimension},
    {XtNheight, XtCHeight, XtRDimension, sizeof(Dimension),
	Offset(core.height), XtRDimension, (caddr_t)&def_dimension},
    /* Private bar widget resources */
    {XtNlength, XtCLength, XtRDimension, sizeof(Dimension),
	offset(length), XtRDimension, (caddr_t) &def_dimension},
    {XtNthickness, XtCThickness, XtRDimension, sizeof(Dimension),
	offset(thickness), XtRDimension, (caddr_t) &def_dimension},
    {XtNorientation, XtCOrientation, XtROrientation, sizeof(XtOrientation),
	offset(orientation), XtROrientation, (caddr_t) &def_orient},
    {XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
	offset(foreground), XtRString, XtDefaultForeground},
    {XtNfillStyle, XtCFillStyle, XtRInt, sizeof(int),
	offset(fillstyle), XtRInt, (caddr_t) 0},
    {XtNpixmaps, XtCPixmaps, XtRArray, sizeof(Array *),
	offset(pixmaps), XtRArray, (caddr_t) NULL},
#undef offset
#undef Offset
};

static void ClassInitialize();
static void Initialize();
static void Resize();
static void Redisplay();

BarClassRec barClassRec = {
  { /* core fields */
    /* superclass		*/	(WidgetClass) &widgetClassRec,
    /* class_name		*/	"Bar",
    /* widget_size		*/	sizeof(BarRec),
    /* class_initialize		*/	ClassInitialize,
    /* class_part_initialize	*/	NULL,
    /* class_inited		*/	FALSE,
    /* initialize		*/	Initialize,
    /* initialize_hook		*/	NULL,
    /* realize			*/	XtInheritRealize,
    /* actions			*/	NULL,
    /* num_actions		*/	0,
    /* resources		*/	resources,
    /* num_resources		*/	XtNumber(resources),
    /* xrm_class		*/	NULLQUARK,
    /* compress_motion		*/	TRUE,
    /* compress_exposure	*/	TRUE,
    /* compress_enterleave	*/	TRUE,
    /* visible_interest		*/	FALSE,
    /* destroy			*/	NULL,
    /* resize			*/	Resize,
    /* expose			*/	Redisplay,
    /* set_values		*/	NULL,
    /* set_values_hook		*/	NULL,
    /* set_values_almost	*/	XtInheritSetValuesAlmost,
    /* get_values_hook		*/	NULL,
    /* accept_focus		*/	NULL,
    /* version			*/	XtVersion,
    /* callback_private		*/	NULL,
    /* tm_table			*/	NULL,
    /* query_geometry		*/	XtInheritQueryGeometry,
    /* display_accelerator	*/	XtInheritDisplayAccelerator,
    /* extension		*/	NULL
  },
  { /* Bar class fields */
    /* extension		*/	NULL
  }
};

WidgetClass barWidgetClass = (WidgetClass)&barClassRec;

static void
ClassInitialize()
{
    XtAddConverter( XtRString, XtROrientation, XmuCvtStringToOrientation,
		    NULL, (Cardinal)0 );
}

static void
SetDimensions(w)
BarWidget w;
{
    if (w->bar.orientation == XtorientVertical) {
	w->bar.length = w->core.height;
	w->bar.thickness = w->core.width;
    }
    else {
	w->bar.length = w->core.width;
	w->bar.thickness = w->core.height;
    }
}

/* 
 * We can't use XtGetGC, since we're going to write to the GC, so we use
 * a simple caching scheme to share a gc between Bar widgets of the same
 * fillstyle on the same display.  The first Bar widget in gets to be
 * the GC we cache.  For most applications, all Bars will be on one
 * display and use the same fillStyle, we hope.
 */
static GC gc_cache;
static Display *dpy_cache;
static int fstyle_cache;

/* ARGSUSED */
static void
Initialize(request, new)
Widget request;		/* what the client asked for */
Widget new;		/* what we're going to give him */
{
    BarWidget w = (BarWidget) new;
    XGCValues gcv;
    Display *dpy = XtDisplay(new);

    if (dpy_cache == NULL || w->bar.fillstyle != fstyle_cache ||
	dpy_cache != dpy) {
	gcv.fill_style = w->bar.fillstyle;
	gcv.foreground = w->bar.foreground;
	gcv.background = w->core.background_pixel;
	w->bar.gc = XCreateGC(dpy, DefaultRootWindow(dpy),
			      GCFillStyle | GCForeground | GCBackground, &gcv);
	if (dpy_cache == NULL) {
	    dpy_cache = dpy;
	    fstyle_cache = w->bar.fillstyle;
	    gc_cache = w->bar.gc;
	}
    } else {
	w->bar.gc = gc_cache;
    }
    /* Width and height override length and thickness */
    if (w->bar.length == def_dimension)
	w->bar.length = DEF_LENGTH;
	
    if (w->bar.thickness == def_dimension)
	w->bar.thickness = DEF_THICKNESS;
	
    if (w->core.width == def_dimension)
	w->core.width = (w->bar.orientation == XtorientVertical)
	    ? w->bar.thickness : w->bar.length;

    if (w->core.height == def_dimension)
	w->core.height = (w->bar.orientation == XtorientHorizontal)
	    ? w->bar.thickness : w->bar.length;
    w->bar.values = NULL;
    w->bar.nvalues = 0;

    SetDimensions(w);
}

/* ARGSUSED */
static void
Resize(gw)
Widget gw;
{
    SetDimensions((BarWidget)gw);
    Redisplay(gw, (XEvent*)NULL, (Region)NULL);
}

/* ARGSUSED */
static void
Redisplay(gw, event, region)
Widget gw;
XEvent *event;
Region region;
{
    BarWidget w = (BarWidget) gw;
    int x, y;
    unsigned int width, height;
    int maxvalue, i;
    int vertical = (w->bar.orientation == XtorientVertical);;

    if (! XtIsRealized(gw))
	return;
    for(maxvalue = 0, i = 0; i < w->bar.nvalues; i++)
	maxvalue += w->bar.values[i];
    if(maxvalue <= 0)
	return;
    x = y = 0;
    if(vertical)
	width = w->bar.thickness;
    else
	height = w->bar.thickness;
    if (w->bar.nvalues > w->bar.pixmaps->nitems) {
	XtWarning("Too many values to represent in a bar");
	return;
    }
    for(i = 0; i < w->bar.nvalues; i++) {
	unsigned int rectlen = (w->bar.length * w->bar.values[i] +
				maxvalue / 2) / (unsigned int)maxvalue;
	if (rectlen == 0)
	    continue;
	if (i == w->bar.nvalues - 1) {
	    if (vertical)
		rectlen = w->core.height - y;
	    else
		rectlen = w->core.width - x;
	}
	switch (w->bar.fillstyle) {
	case FillSolid:
	    XSetForeground(XtDisplay(gw), w->bar.gc,
			   ((Pixel *) w->bar.pixmaps->items)[i]);
	    break;
	case FillTiled:
	    XSetTile(XtDisplay(gw), w->bar.gc,
			   ((Pixmap *) w->bar.pixmaps->items)[i]);
	    break;
	case FillStippled:
	case FillOpaqueStippled:
	    XSetStipple(XtDisplay(gw), w->bar.gc,
			   ((Pixmap *) w->bar.pixmaps->items)[i]);
	    break;
	}
	if(vertical) {
	    XFillRectangle(XtDisplay(gw), XtWindow(gw), w->bar.gc,
	     x, y, width, rectlen);
	    y += rectlen;
	} else {
	    XFillRectangle(XtDisplay(gw), XtWindow(gw), w->bar.gc,
	     x, y, rectlen, height);
	    x += rectlen;
	}
    }
}

/* !! We really should make the widget support Xt{Get,Set}Values fully */
/* Public routines. */
void
SetBarValues(gw, values, nvalues)
Widget gw;
int *values;
int nvalues;
{
    BarWidget w = (BarWidget) gw;
    int i;

    if (nvalues > w->bar.pixmaps->nitems) {
#define ERRFMT "Tried to set %d values for bar - maximum is %d\n"
	char errbuf[sizeof(ERRFMT) + 32];
	(void) sprintf(errbuf, ERRFMT, nvalues, w->bar.pixmaps->nitems);
#undef ERRFMT
	XtWarning(errbuf);
	nvalues = w->bar.pixmaps->nitems;
    }
    if (nvalues <= 0) {
#define ERRFMT "Tried to set %d values for bar - it must be > 0\n"
	char errbuf[sizeof(ERRFMT) + 32];
	(void) sprintf(errbuf, ERRFMT, nvalues);
	XtWarning(errbuf);
#undef ERRFMT
	return;
    }
    if (w->bar.values == NULL || w->bar.nvalues < nvalues) {
	if (w->bar.values) XtFree((char *) w->bar.values);
	w->bar.values = (int *) XtMalloc(nvalues * sizeof(int));
    }
    w->bar.nvalues = nvalues;
    for(i = 0; i < nvalues; i++)
	w->bar.values[i] = values[i];
    Redisplay( gw, (XEvent*)NULL, (Region)NULL );
}
