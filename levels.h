/* Levels of bar intensity */

#define MAXLEVELS 10 

/* mapping of grays to levels; intended to maximize contrast */
static int level_mapping[MAXLEVELS][MAXLEVELS] = {
	{ 9 },
	{ 0, 9 },
	{ 0, 4, 9 },
	{ 0, 3, 6, 9 },
	{ 0, 2, 4, 7, 9 },
	{ 0, 1, 3, 5, 7, 9 },
	{ 0, 1, 3, 5, 7, 8, 9 },
	{ 0, 1, 2, 3, 5, 7, 8, 9 },
	{ 0, 1, 2, 3, 5, 6, 7, 8, 9 },
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 },
};

/* Gray bitmaps */

static char gray0_bits[] = {
   0x00, 0x00, 0x00};
static char gray1_bits[] = {
   0x00, 0x02, 0x00};
static char gray2_bits[] = {
   0x00, 0x03, 0x00};
static char gray3_bits[] = {
   0x00, 0x03, 0x02};
static char gray4_bits[] = {
   0x00, 0x07, 0x02};
static char gray5_bits[] = {
   0x04, 0x07, 0x02};
static char gray6_bits[] = {
   0x04, 0x07, 0x03};
static char gray7_bits[] = {
   0x05, 0x07, 0x03};
static char gray8_bits[] = {
   0x05, 0x07, 0x07};
static char gray9_bits[] = {
   ~0x0, ~0x0, ~0x0};

static char *gray_bits[] = {
    gray0_bits,
    gray1_bits,
    gray2_bits,
    gray3_bits,
    gray4_bits,
    gray5_bits,
    gray6_bits,
    gray7_bits,
    gray8_bits,
    gray9_bits,
};

/* Gray colors */
static char graycolornames[] = 
"gray99,gray90,gray81,gray72,gray63,gray54,gray45,gray36,gray27,gray18";

/* Default non-gray colors */
static char colornames[] = 
"blue,green,yellow,red,cyan,magenta,purple,maroon,navyblue,black";
