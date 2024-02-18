/*
 *  Any system dependent file must implement these routines. 
 */

/* 
 * num_bars() is called at the beginning to inquire how many bars are needed. 
 */
extern int num_bars(); 

/* 
 * version() is called by -version, prints to stdout the version, and any
 * compile-time limits, e.g. 
 *	printf("MyOS: maxcpu=%d, maxdisk=%d\n", MAXCPU, MAXDISK);
 */
extern void version(); 

/*
 * bar_items() indicates how many levels each bar has.  For most machines, 
 * each CPU bar will have the same stuff.  But one can, for instance, display 
 * one sort of thing on one bar, another on others, etc.
 */
extern void bar_items(/* int nbars, int items[] */);

/* 
 * label_bars() is called after num_bars to ask for the bar names.
 */
extern char **label_bars(/* int nbars */);

/* 
 *  init_bars() is called after the bars are created to perform any machine 
 *  dependent initializations.
 */
extern void init_bars(/* int nbars */);
	
/* 
 *  display_bars() is called every interval to compute and display the
 *  bars. It should call draw_bar() with the bar number, the array of
 *  integer values to display in the bar, and the number of values in
 *  the array.
 */
extern void display_bars(/* int nbars */);
