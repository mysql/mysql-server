/*
 * First, the stuff that ends up in the outside-world include file
 = typedef off_t regoff_t;
 = typedef struct {
 = 	int re_magic;
 = 	size_t re_nsub;		// number of parenthesized subexpressions
 = 	const char *re_endp;	// end pointer for REG_PEND
 = 	struct re_guts *re_g;	// none of your business :-)
 = } regex_t;
 = typedef struct {
 = 	regoff_t rm_so;		// start of match
 = 	regoff_t rm_eo;		// end of match
 = } regmatch_t;
 */
/*
 * internals of regex_t
 */
#ifndef	__regex2_h__
#define __regex2_h__

#define	MAGIC1	((('r'^0200)<<8) | 'e')

/*
 * The internal representation is a *strip*, a sequence of
 * operators ending with an endmarker.  (Some terminology etc. is a
 * historical relic of earlier versions which used multiple strips.)
 * Certain oddities in the representation are there to permit running
 * the machinery backwards; in particular, any deviation from sequential
 * flow must be marked at both its source and its destination.  Some
 * fine points:
 *
 * - OPLUS_ and O_PLUS are *inside* the loop they create.
 * - OQUEST_ and O_QUEST are *outside* the bypass they create.
 * - OCH_ and O_CH are *outside* the multi-way branch they create, while
 *   OOR1 and OOR2 are respectively the end and the beginning of one of
 *   the branches.  Note that there is an implicit OOR2 following OCH_
 *   and an implicit OOR1 preceding O_CH.
 *
 * In state representations, an operator's bit is on to signify a state
 * immediately *preceding* "execution" of that operator.
 */
typedef unsigned long sop;	/* strip operator */
typedef long sopno;
#define	OPRMASK	0xf8000000
#define	OPDMASK	0x07ffffff
#define	OPSHIFT	((unsigned long)27)
#define	OP(n)	((unsigned long) ((n)&OPRMASK))
#define	OPND(n)	((unsigned long) ((n)&OPDMASK))
#define	SOP(op, opnd)	(unsigned long) ((op)|(opnd))
#define OSHIFT(A) ((unsigned long) (A) << OPSHIFT)
/* operators			   meaning	operand			*/
/*						(back, fwd are offsets)	*/
#define	OEND	OSHIFT(1)	/* endmarker	-			*/
#define	OCHAR	OSHIFT(2)	/* character	unsigned char		*/
#define	OBOL	OSHIFT(3)	/* left anchor	-			*/
#define	OEOL	OSHIFT(4)	/* right anchor	-			*/
#define	OANY	OSHIFT(5)	/* .		-			*/
#define	OANYOF	OSHIFT(6)	/* [...]	set number		*/
#define	OBACK_	OSHIFT(7)	/* begin \d	paren number		*/
#define	O_BACK	OSHIFT(8)	/* end \d	paren number		*/
#define	OPLUS_	OSHIFT(9)	/* + prefix	fwd to suffix		*/
#define	O_PLUS	OSHIFT(10)	/* + suffix	back to prefix		*/
#define	OQUEST_	OSHIFT(11)	/* ? prefix	fwd to suffix		*/
#define	O_QUEST	OSHIFT(12)	/* ? suffix	back to prefix		*/
#define	OLPAREN	OSHIFT(13)	/* (		fwd to )		*/
#define	ORPAREN	OSHIFT(14)	/* )		back to (		*/
#define	OCH_	OSHIFT(15)	/* begin choice	fwd to OOR2		*/
#define	OOR1	OSHIFT(16)	/* | pt. 1	back to OOR1 or OCH_	*/
#define	OOR2	OSHIFT(17)	/* | pt. 2	fwd to OOR2 or O_CH	*/
#define	O_CH	OSHIFT(18)	/* end choice	back to OOR1		*/
#define	OBOW	OSHIFT(19)	/* begin word	-			*/
#define	OEOW	OSHIFT(20)	/* end word	-			*/

/*
 * Structure for [] character-set representation.  Character sets are
 * done as bit vectors, grouped 8 to a byte vector for compactness.
 * The individual set therefore has both a pointer to the byte vector
 * and a mask to pick out the relevant bit of each byte.  A hash code
 * simplifies testing whether two sets could be identical.
 *
 * This will get trickier for multicharacter collating elements.  As
 * preliminary hooks for dealing with such things, we also carry along
 * a string of multi-character elements, and decide the size of the
 * vectors at run time.
 */
#ifdef __WIN__
typedef unsigned char uch ;
#endif

typedef struct {
	uch *ptr;		/* -> uch [csetsize] */
	uch mask;		/* bit within array */
	uch hash;		/* hash code */
	size_t smultis;
	char *multis;		/* -> char[smulti]  ab\0cd\0ef\0\0 */
} cset;
/* note that CHadd and CHsub are unsafe, and CHIN doesn't yield 0/1 */
#define	CHadd(cs, c)	((cs)->ptr[(uch)(c)] |= (cs)->mask, (cs)->hash += (uch) (c))
#define	CHsub(cs, c)	((cs)->ptr[(uch)(c)] &= ~(cs)->mask, (cs)->hash -= (uch)(c))
#define	CHIN(cs, c)	((cs)->ptr[(uch)(c)] & (cs)->mask)
#define	MCadd(p, cs, cp)	mcadd(p, cs, cp)	/* regcomp() internal fns */
#define	MCsub(p, cs, cp)	mcsub(p, cs, cp)
#define	MCin(p, cs, cp)	mcin(p, cs, cp)

/* stuff for character categories */
typedef unsigned char cat_t;

/*
 * main compiled-expression structure
 */
struct re_guts {
	int magic;
#		define	MAGIC2	((('R'^0200)<<8)|'E')
	sop *strip;		/* malloced area for strip */
	int csetsize;		/* number of bits in a cset vector */
	int ncsets;		/* number of csets in use */
	cset *sets;		/* -> cset [ncsets] */
	uch *setbits;		/* -> uch[csetsize][ncsets/CHAR_BIT] */
	int cflags;		/* copy of regcomp() cflags argument */
	sopno nstates;		/* = number of sops */
	sopno firststate;	/* the initial OEND (normally 0) */
	sopno laststate;	/* the final OEND */
	int iflags;		/* internal flags */
#		define	USEBOL	01	/* used ^ */
#		define	USEEOL	02	/* used $ */
#		define	BAD	04	/* something wrong */
	int nbol;		/* number of ^ used */
	int neol;		/* number of $ used */
	int ncategories;	/* how many character categories */
	cat_t *categories;	/* ->catspace[-CHAR_MIN] */
	char *must;		/* match must contain this string */
	int mlen;		/* length of must */
	size_t nsub;		/* copy of re_nsub */
	int backrefs;		/* does it use back references? */
	sopno nplus;		/* how deep does it nest +s? */
	/* catspace must be last */
	cat_t catspace[1];	/* actually [NC] */
};

/* misc utilities */
#undef OUT			/* May be defined in windows */
#define	OUT	(CHAR_MAX+1)	/* a non-character value */
#define	ISWORD(s,c)	(my_isalnum(s,c) || (c) == '_')

#endif /* __regex2_h__ */
