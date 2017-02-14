/* character-class table */

#define CCLASS_ALNUM	0
#define CCLASS_ALPHA	1
#define CCLASS_BLANK	2
#define CCLASS_CNTRL	3
#define CCLASS_DIGIT	4
#define CCLASS_GRAPH	5
#define CCLASS_LOWER	6
#define CCLASS_PRINT	7
#define CCLASS_PUNCT	8
#define CCLASS_SPACE	9
#define CCLASS_UPPER	10
#define CCLASS_XDIGIT	11
#define CCLASS_LAST	12

extern struct cclass {
	const char *name;
	const char *chars;
	const char *multis;
	uint  mask;
} cclasses[];
