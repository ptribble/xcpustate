#ifndef array_h_
#define array_h_

/*
 * Structure in which to store counted arrays of fixed size objects.
 * The items element is a pointer to a block of memory, presumably
 * holding nitems.  The user should cast it to the proper type; as with
 * a union, it is the users responsibility to store and retrieve the
 * same type of information -- the itemtype field can be used to store
 * type codes for validation and checking, or the size of the objects.
 */
typedef struct {
	int itemtype;
	int nitems;
	caddr_t items;
} Array;

#define XtNarray	"array"
#define XtRArray	"Array"
#define XtCArray	"Array"

#endif /* array_h_ */ /* Do not add anything after this line */
