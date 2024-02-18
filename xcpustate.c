/*
*  Displays CPU state distribution
*/
#ifndef lint
static char rcsid[] = "$Id: xcpustate.c,v 1.32 2004/05/30 01:41:03 jdd Exp $";
#endif /*lint*/

#include <stdio.h>

#include <X11/Xos.h>
#include <X11/StringDefs.h>
#include <X11/Intrinsic.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Label.h>
#include <X11/Xmu/Xmu.h>
#include "array.h"
#include "Bar.h"
#include "s.h"
#include "levels.h"
#include "patchlevel.h"
#include <stdlib.h>

#define MAXSTR 1024
#define MAXAVG 1024
#define MAXBARS 256
#ifndef INTERVAL_MIN
#define INTERVAL_MIN 0.000
#endif /* INTERVAL_MIN */
#ifndef DEF_INTERVAL
#define DEF_INTERVAL    1.000
#endif /* DEF_INTERVAL */
#ifndef DEF_FONT
#define DEF_FONT        XtDefaultFont
#endif /* DEF_FONT */

#define NOTSET minusTwo

char *progname;

typedef enum {
	grayscale,	
	color,
	tile,
	stipple,
	autodetect
} FillType; 

static float defaultInterval = (1.0*DEF_INTERVAL);
static int one = 1;
static int minusOne = -1;
static int minusTwo = -2;
static Boolean def_true = True, def_false= False;
static Colormap colormap;
static Pixel colors[MAXLEVELS];
static FillType filltype;
int cpuflag, diskflag, sep_wait;
char *kernelSymbols;
int kmem_can_map;
int avg;
#ifdef OMNI
int omniflag;
#endif /* OMNI */
#ifdef RSTAT
char *rstathost;
#endif /* RSTAT */

static struct _resource {
	XFontStruct *font;
	char *colorspec;
	int count;
	float interval;
	int shortenTo;
	Pixel fg, bg;
	Boolean rv;
	char *filltypestr;
	int cpuflag, diskflag, sep_wait, version;
	char *kernelSymbols;
	int   kmem_can_map;
	int avg;
#ifdef OMNI
	int omniflag;
#endif /* OMNI */
#ifdef RSTAT
	char *rstathost;
#endif /* RSTAT */
} resource;

#define offset(field)   XtOffsetOf(struct _resource, field)

extern char *strtok();

static int allocstyles(), parsecolors();

static void setup_WMPROTOCOL(), close_window();

#ifdef RSTAT
extern int rstat_num_bars();
extern char **rstat_label_bars();
extern void rstat_init_bars();
extern void rstat_display_bars();

#define numBars() ((NULL==rstathost)?num_bars():rstat_num_bars())
#define labelBars(n) ((NULL==rstathost)?label_bars(n):rstat_label_bars(n))
#define initBars(n) if(NULL==rstathost)init_bars(n);else rstat_init_bars(n)
#define displayBars(n) if(NULL==rstathost)display_bars(n);else rstat_display_bars(n)
#define barItems(n,ni) if(NULL==rstathost)bar_items(n,ni);else rstat_bar_items(n,ni)

#else /* RSTAT */

#define numBars() num_bars()
#define labelBars(n) label_bars(n)
#define initBars(n) init_bars(n)
#define displayBars(n) display_bars(n)
#define barItems(n,ni) bar_items(n,ni) 

#endif /* RSTAT */

/* Application Resources - no particular widget */
static XtResource application_resources[] = {
    {"interval", "Interval", XtRFloat, sizeof(float),
            offset(interval), XtRFloat, (caddr_t) &defaultInterval},
    {"count", "Count", XtRInt, sizeof(int),
            offset(count), XtRInt, (caddr_t) &minusOne},
    {"shorten", "Shorten", XtRInt, sizeof(int),
            offset(shortenTo), XtRInt, (caddr_t) &NOTSET},
    {XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
            offset(fg), XtRString, XtDefaultForeground},
    {XtNbackground, XtCBackground, XtRPixel, sizeof(Pixel),
            offset(bg), XtRString, XtDefaultBackground},
    {XtNreverseVideo, XtCReverseVideo, XtRBoolean, sizeof(Boolean),
            offset(rv), XtRBoolean, (caddr_t) &def_false},
    {"font", XtCFont, XtRFontStruct, sizeof(XFontStruct *),
            offset(font), XtRString, DEF_FONT},
    {"colors", "Colors", XtRString, sizeof(char *),
            offset(colorspec), XtRString, (caddr_t)NULL},
    {"filltype", "Filltype", XtRString, sizeof(char *),
            offset(filltypestr), XtRString, (caddr_t)"auto"},
    {"cpu", "Cpu", XtRBoolean, sizeof(Boolean),
            offset(cpuflag), XtRImmediate, (XtPointer)True},
    {"disk", "Disk", XtRBoolean, sizeof(Boolean),
            offset(diskflag), XtRImmediate, (XtPointer)False},
    {"wait", "Wait", XtRBoolean, sizeof(Boolean),
            offset(sep_wait), XtRImmediate, (XtPointer)False},
    {"version", "Version", XtRBoolean, sizeof(Boolean),
            offset(version), XtRImmediate, (XtPointer)False},
    {"kernel", "Kernel", XtRString,  sizeof(char *),
            offset(kernelSymbols), XtRString, (caddr_t) NULL},
    {"mmap", "Mmap", XtRBoolean, sizeof(Boolean),
            offset(kmem_can_map), XtRInt, (caddr_t)&def_true},
    {"avg", "Avg", XtRInt, sizeof(int),
            offset(avg), XtRInt, (caddr_t)&one},
#ifdef OMNI
    {"omni", "Omni", XtRBoolean, sizeof(Boolean),
            offset(omniflag), XtRImmediate, (XtPointer)False},
#endif /* OMNI */
#ifdef RSTAT
    {"host", "Host", XtRString, sizeof(char *),
            offset(rstathost), XtRString, (caddr_t)NULL},
#endif /* RSTAT */
};

/*
*  Command line options table. The command line is parsed for these,
*  and it sets/overrides the appropriate values in the resource
*  database
*/
static XrmOptionDescRec optionDescList[] = {
    {"-interval",   ".interval",    XrmoptionSepArg,    (caddr_t) NULL},
    {"-count",      ".count",       XrmoptionSepArg,    (caddr_t) NULL},
    {"-shorten",    ".shorten",     XrmoptionSepArg,    (caddr_t) NULL},
    {"-cpu",        ".cpu",         XrmoptionNoArg,     (caddr_t) "True"},
    {"-nocpu",      ".cpu",         XrmoptionNoArg,     (caddr_t) "False"},
    {"-disk",       ".disk",        XrmoptionNoArg,     (caddr_t) "True"},
    {"-nodisk",     ".disk",        XrmoptionNoArg,     (caddr_t) "False"},
    {"-wait",       ".wait",        XrmoptionNoArg,     (caddr_t) "True"},
    {"-nowait",     ".wait",        XrmoptionNoArg,     (caddr_t) "False"},
    {"-host",       ".host",        XrmoptionSepArg,    (caddr_t) NULL},
    {"-colors",     ".colors",      XrmoptionSepArg,    (caddr_t) NULL},
    {"-filltype",   ".filltype",    XrmoptionSepArg,    (caddr_t) NULL},
    {"-kernel",     ".kernel",      XrmoptionSepArg,    (caddr_t) NULL},
    {"-mmap",       ".mmap",        XrmoptionNoArg,     (caddr_t) "True"},
    {"-nommap",     ".mmap",        XrmoptionNoArg,     (caddr_t) "False"},
    {"-version",    ".version",     XrmoptionNoArg,     (caddr_t) "True"},
    {"-avg",        ".avg",         XrmoptionSepArg,    (caddr_t) NULL},
#ifdef OMNI
    {"-omni",       ".omni",        XrmoptionNoArg,     (caddr_t) "True"},
    {"-noomni",     ".omni",        XrmoptionNoArg,     (caddr_t) "False"},
#endif /* OMNI */
};

/*
*  DON'T CHANGE THE ORDER OF THE ARGS IN THE VARIOUS ARG STRUCTS. IF
*  YOU WANT TO ADD STUFF, ADD IT AT THE END OF THE STRUCT, BECAUSE WE
*  REFER TO SOME OF THE ELEMENTS BY POSITION IN THE CODE.
*/
/* No spacing between the widgets on the Form */
static Arg form_args[] = {
    {XtNdefaultDistance, (XtArgVal) 0},
};

static Arg subform_args[] = {
    {XtNfromVert, (XtArgVal) 0},
    {XtNdefaultDistance, (XtArgVal) 0},
    {XtNborderWidth, (XtArgVal) 0},
    {XtNtop, (XtArgVal) XtRubber},
    {XtNbottom, (XtArgVal) XtRubber}, /* ChainBottom causes strange resize */
    {XtNright, (XtArgVal) XtChainRight},
    {XtNleft, (XtArgVal) XtChainLeft},
};

static Arg bar_args[] = {
    {XtNfromHoriz, (XtArgVal) 0},
    {XtNpixmaps, (XtArgVal) 0},
    {XtNfillStyle, (XtArgVal) 0},
    {XtNorientation, (XtArgVal) XtorientHorizontal},
    {XtNborderWidth, (XtArgVal) 1},
    {XtNlength, (XtArgVal) ((Dimension) 200)},
    {XtNthickness, (XtArgVal) ((Dimension) 20)},
    {XtNtop, (XtArgVal) XtChainTop},
    {XtNbottom, (XtArgVal) XtChainBottom},
    {XtNright, (XtArgVal) XtChainRight},
    {XtNleft, (XtArgVal) XtChainLeft},
    {XtNvertDistance, (XtArgVal) 0},
    {XtNhorizDistance, (XtArgVal) 0},
    {XtNresizable, (XtArgVal) TRUE},
};

static Arg label_args[] = {
    {XtNlabel, (XtArgVal) 0},
    {XtNjustify, (XtArgVal) XtJustifyLeft},
    {XtNborderWidth, (XtArgVal) 0},
    {XtNtop, (XtArgVal) XtChainTop},
    {XtNbottom, (XtArgVal) XtChainTop},
    {XtNright, (XtArgVal) XtChainLeft},
    {XtNleft, (XtArgVal) XtChainLeft},
    {XtNvertDistance, (XtArgVal) 0},
    {XtNhorizDistance, (XtArgVal) 0},
    {XtNresizable, (XtArgVal) FALSE},
    {XtNwidth, (XtArgVal) 1},
    {XtNfont, (XtArgVal) 0},
};

void
usage()
{
        /* Ugly! */
    (void) fprintf(stderr, 
"\
Usage: %s [Xt options] [-count iterations]\n\
\t[-interval delay_seconds][-shorten components][-cpu]\n\
\t[-nocpu] [-disk] [-nodisk] [-wait] [-nowait] %s%s\n\
\t[-filltype auto|grayscale|color|tile|stipple ]\n\
\t[-colors colorname[,colorname[,...]]]\n\
\t[-avg numintervals]\n\
\t[-kernel kernelname ] [-mmap] [-nommap]\n",
progname,
#ifdef OMNI
" [-omni] [-noomni]",
#else  /* OMNI */
"",
#endif /* OMNI */
#ifdef RSTAT
" [-host hostname]"
#else  /* RSTAT */
""
#endif /* RSTAT */
);
    exit(-1);
}

char *
xmalloc(n)
unsigned int n;
{
    return XtMalloc(n);
}

static int nbars;
static Widget *bar;
static char **barnames;
    
/* ARGSUSED */
static void
update_display(closure, id)
XtPointer closure;
XtIntervalId *id;
{
    unsigned long timeout;
    displayBars(nbars);
    
    if (resource.count > 0) {
        if (--resource.count == 0)
            return;
    }
    timeout=(unsigned long)(resource.interval * 1000.0);
    (void) XtAddTimeOut(timeout, update_display, (XtPointer) NULL);
}

void main(argc, argv)
int argc;
char **argv;
{
    int i, maxlabellen=0, *nbaritems, fstyle;
    Widget topLevel, form, label, subform = NULL;
    Array *arrp;
    Display *dpy;
    int scr;

#ifndef index
    extern char *index(/* const char *, char */);
#endif

    if ((progname = index(argv[0], '/')) == NULL)
        progname = argv[0];
    else
        progname++;

    topLevel = XtInitialize(progname, "CPUStateMonitor",
                            optionDescList, XtNumber(optionDescList),
                            &argc, argv);
    dpy=XtDisplay(topLevel);
    scr=DefaultScreen(dpy);

    XtGetApplicationResources(topLevel, (XtPointer) &resource,
			      application_resources,
                              XtNumber(application_resources), 
                              (ArgList)NULL, (Cardinal)0 );

    cpuflag = resource.cpuflag;
    diskflag = resource.diskflag;
    sep_wait = resource.sep_wait;
    kernelSymbols = resource.kernelSymbols;
    kmem_can_map = resource.kmem_can_map;
    avg = resource.avg; if(avg<1) avg=1; if(avg>MAXAVG) avg=MAXAVG;
#ifdef OMNI
    omniflag = resource.omniflag;
#endif /* OMNI */
#ifdef RSTAT
    rstathost = resource.rstathost;
#endif /* RSTAT */

    if(resource.version) { 
	printf("%s\n", patchlevel);
	version();
#ifdef RSTAT
	rstat_version();
#endif /* RSTAT */
        exit(0);
    }
    if(argc>1) usage();

    if(0==strcmp(resource.filltypestr, "auto")){
	if(DefaultDepth(dpy, scr)>1){
	    VisualID vid;
	    XVisualInfo vinfo, *vinfo_p;
	    int count;

	    vid=XVisualIDFromVisual(DefaultVisual(dpy, scr));
	    vinfo.visualid = vid;
	    vinfo_p = XGetVisualInfo(dpy, VisualIDMask, &vinfo, &count);
	    if(1!=count){
	        (void) fprintf(stderr, 
				"VisualID doesn't map to one visual???\n"); 
		filltype=grayscale; /* lowest common denominator */
	    } else {
		if((GrayScale==vinfo_p->class || StaticGray==vinfo_p->class)){
		    filltype=grayscale;
		} else {
		    filltype=color;
		}
	    }
	} else {
	    filltype=tile;
	}
    } else if(0==strcmp(resource.filltypestr, "grayscale")){
	filltype=grayscale;
    } else if (0==strcmp(resource.filltypestr, "color")){
	filltype=color;
    } else if (0==strcmp(resource.filltypestr, "stipple")){
	filltype=stipple;
    } else if (0==strcmp(resource.filltypestr, "tile")){
	filltype=tile;
    } else {
	if(NULL!=resource.filltypestr) {
		fprintf(stderr, 
			"%s: Unknown filltype %s, using \"tile\".\n", progname,
			resource.filltypestr);
	}
	filltype=tile;
    }

    if(color==filltype || grayscale==filltype){
        colormap = DefaultColormap(XtDisplay(topLevel), 
                        DefaultScreen(XtDisplay(topLevel)));
        if(color==filltype) {
            (void)parsecolors(topLevel,colormap,colornames,resource.colorspec,colors);
        } else if(grayscale==filltype){ 
            (void)parsecolors(topLevel,colormap,graycolornames,NULL,colors);
        }
    }

    if(cpuflag) cpuflag=1;
    if(diskflag) diskflag=1;
    if(sep_wait) sep_wait=1;
    if(kmem_can_map) kmem_can_map=1;
#ifdef OMNI
    if(omniflag) omniflag=1;
#endif /* OMNI */

    if(NULL==resource.font) {
        (void) fprintf(stderr, "Can't find font '%s'\n", DEF_FONT);
        exit(-1);
    }

    /* Make sure the default interval isn't less than the minimum. */
    if(defaultInterval<INTERVAL_MIN){
	if(resource.interval==defaultInterval){
		resource.interval=INTERVAL_MIN;
	}
	defaultInterval=INTERVAL_MIN;
    }
    if(resource.interval < INTERVAL_MIN){
	(void) fprintf(stderr, 
"The minimum interval is '%.4f'; '%.4f' is too small.  Using '%.4f'\n", 
		(1.0*INTERVAL_MIN), resource.interval, defaultInterval);
	resource.interval=defaultInterval;
    }

    nbars = numBars();
    if (0==nbars) {
        (void) fprintf(stderr, "Nothing to do. Exiting.\n");
        exit(-1);
    }

    bar = (Widget *) xmalloc(nbars * sizeof(Widget));
    barnames = labelBars(nbars);
    nbaritems = (int *) xmalloc(nbars * sizeof(int));
    barItems(nbars, nbaritems);
    arrp = (Array *) xmalloc(nbars * sizeof(Array));
    fstyle = allocstyles(topLevel, nbars, nbaritems, arrp);

    /* find width of biggest label */
    for(i=0;i<nbars;i++){
        int labellen;
        labellen=XTextWidth(resource.font, barnames[i], strlen(barnames[i]));
        if(labellen>maxlabellen) maxlabellen=labellen;
    }

    form = XtCreateManagedWidget("form", formWidgetClass, topLevel,
                                 form_args, XtNumber(form_args));
    
    for(i = 0; i < nbars; i++) {
#define FORMNAMEFMT     "form%d"
        char formname[sizeof(FORMNAMEFMT) + 32];
#define BARNAMEFMT      "bar%d"
        char barname[sizeof(BARNAMEFMT) + 32];

        if (i > 0)
            subform_args[0].value = (XtArgVal) subform;
        (void) sprintf(formname, FORMNAMEFMT, i);
        subform = XtCreateManagedWidget(formname, formWidgetClass, form,
                                 subform_args, XtNumber(subform_args));
        label_args[0].value = (XtArgVal) barnames[i];
	if (resource.shortenTo >= 0 || resource.shortenTo == NOTSET) {
		label_args[10].value = (XtArgVal) (6+maxlabellen);
	} else {
		label_args[10].value = (XtArgVal) 1;
	}
        label_args[11].value = (XtArgVal) resource.font;
	label = XtCreateManagedWidget(barnames[i], labelWidgetClass,
			 subform, label_args, XtNumber(label_args));
        (void) sprintf(barname, BARNAMEFMT, i);
        bar_args[0].value = (XtArgVal) label;
        bar_args[1].value = (XtArgVal) (arrp + i);
        bar_args[2].value = (XtArgVal) fstyle;
        bar[i] = XtCreateManagedWidget(barname, barWidgetClass, subform,
                                    bar_args, XtNumber(bar_args));
    }
    XtRealizeWidget(topLevel);
    setup_WMPROTOCOL(topLevel, "");

    initBars(nbars);
    

    displayBars(nbars);  /* First display is always rubbish */

    /* let's look at rubbish for only 0.01 second, regardless of interval */
    (void) XtAddTimeOut((unsigned long) (10), update_display, 
			(XtPointer)NULL);

    XtMainLoop();
}

void
draw_bar(i, states, nstates)
int i;
int states[];
int nstates;
{
	static int sa[MAXBARS][MAXAVG][MAXLEVELS];
	static int avged[MAXLEVELS];
	static int pos=0, notfirsttime[MAXBARS];
	int j, k, sum=0;
	
	pos++; if(pos>=avg) pos=0;

	for(j=0;j<nstates;j++) {
		if(notfirsttime[i]) {
			/* 
			 * If this is the first time for this bar, the numbers
			 * are likely to be total nonsense, so don't keep them
			 * around for later.  Otherwise the bars may be
			 * totally wrong for avg number of intervals.
			 */
			sa[i][pos][j]=states[j];
		}
		avged[j]=0;
		for(k=0;k<avg;k++){
			avged[j] += sa[i][k][j];
		}
		/* avged[j] = avged[j]/avg; */
	}
    	SetBarValues((Widget) bar[i], avged, nstates); 
	notfirsttime[i]=1;
}

void
shorten(hname)
char *hname;
{
    if (resource.shortenTo >= 0) {
        int i;
        char *cp = hname;
        
        for(i = 0; i < resource.shortenTo; i++) {
            cp = index(++cp, '.');
            if (cp == NULL)
                break;
        }
        if (cp)
            *cp = '\0';
    }
}

static void
allocbarstyle(w, arrp, n)
Widget w;
Array *arrp;
int n;
{
#define newpixmap     XCreatePixmapFromBitmapData
      Display *dpy = XtDisplay(w);
      Window root = DefaultRootWindow(dpy);
      Pixmap *pp;
      int i, igray;

      pp = (Pixmap *) xmalloc(n * sizeof(Pixmap));

      arrp->nitems = n;
      arrp->itemtype = sizeof(Pixmap);
      arrp->items = (caddr_t) pp;

      for(igray = level_mapping[n-1][0], i=0; 
                        i<n; 
                        i++, igray=level_mapping[n-1][i]) {
              if (tile==filltype) {
                      unsigned depth = DefaultDepthOfScreen(XtScreen(w));

                      pp[i] = newpixmap(dpy, root, (char *)gray_bits[igray],
                       (unsigned) 3, (unsigned) 3, resource.fg, resource.bg, depth);
              } else {
                      Pixel black = 1;
                      Pixel white = 0;
                      unsigned depth = 1;

                      pp[i] = newpixmap(dpy, root, (char *)gray_bits[igray],
                       (unsigned) 3, (unsigned) 3, black, white, depth);
              }
      }
}

static void
allocbarcolor(w, arrp, n)
Widget w;
Array *arrp;
int n;
{
        arrp->nitems = n;
        arrp->itemtype = sizeof(Pixel);
        if(grayscale == filltype) {
                int i, igray;
                Pixel *px;
                
                px = (Pixel *) xmalloc(n * sizeof(Pixel));
                for(igray = level_mapping[n-1][0], i=0;
                                i<n;
                                i++, igray=level_mapping[n-1][i]) {
                        px[i] = colors[igray];
                }
                arrp->items= (caddr_t) px;
        } else {
                arrp->items = (caddr_t) colors;
        } 
}

static int
allocstyles(w, nbars, nbaritems, arrp)
Widget w;
int nbars;
int nbaritems[];
Array arrp[]; /* nbars elements */
{
        int i;

        if(grayscale==filltype || color==filltype){
                /* Fillsolid */
                for(i = 0; i < nbars; i++)
                        allocbarcolor(w, &arrp[i], nbaritems[i]);
                return (FillSolid);
        } else {
                /* Tiled or stippled */
                for(i = 0; i < nbars; i++)
                      allocbarstyle(w, &arrp[i], nbaritems[i]);
                return ((tile==filltype) ?  FillTiled : FillOpaqueStippled);
        }
}

static int
parsecolors(w, cmap, names, newnames, parr)
Widget w;
Colormap cmap;
char *names;     /* List of color names separated by commas */
char *newnames;  /* As names, "." is a placeholder. Used to override names */
Pixel *parr;     /* Points to MAXLEVELS array of pixels */
{
        char nm[MAXSTR], nnm[MAXSTR], *cnames[MAXLEVELS], *cp;
        int i, retval=0;

        (void)strncpy(nm, names, MAXSTR-1);
        if(NULL!=newnames) (void)strncpy(nnm, newnames, MAXSTR-1);

        /* parse names */
        for(cp=strtok(nm,","),i=0; NULL!=cp; cp=strtok(NULL,","),i++){
                if(i>=MAXLEVELS) break;
                cnames[i]=cp;
        }

        /* override names with newnames */
        if(NULL!=newnames){
                for(cp=strtok(nnm,","),i=0; NULL!=cp; cp=strtok(NULL,","),i++){
                        if(i>=MAXLEVELS) break;
                        if('.'!=*cp) cnames[i]=cp;
                }
        }

        /* allocate colors */
        for(i=0;i<MAXLEVELS;i++){
                XColor scolor, ecolor;
                if(XAllocNamedColor(XtDisplay(w), cmap,
                        cnames[i], &scolor, &ecolor)) {
                                parr[i]=scolor.pixel;
                } else {
                        fprintf(stderr,
                          "Can't find color %s, using foreground color.\n",
                          cnames[i]);
                        parr[i]=resource.fg;
                        retval++;
                }
        }
        return(retval);
}



static Atom protocol[1];
static XtActionsRec actionTable[] =
{ /* action for window manager close window protocol message */
  { "WMclose_window",		close_window },
};

#define WMPROTOCOL_TRANSLATIONS  "<Message>WM_PROTOCOLS: WMclose_window()\n"

static void
close_window(w, ev, params, nparams)
  Widget   *w;
  XEvent   *ev;
  String   *params;
  Cardinal *nparams;
{
  exit(0);
}


static void
setup_WMPROTOCOL(w, acttype)
  Widget  w;
  char   *acttype;
{
  static int init = 0;

  if (!init++)
    {
      XtAppAddActions(XtWidgetToApplicationContext(w),
		      actionTable, XtNumber(actionTable));
    }

  protocol[0] = XInternAtom ( XtDisplay(w) , "WM_DELETE_WINDOW" , False ) ;
  XSetWMProtocols ( XtDisplay(w) , XtWindow(w) , protocol , 1 ) ;

  XtOverrideTranslations(w, XtParseTranslationTable(WMPROTOCOL_TRANSLATIONS));
}
