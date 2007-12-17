/******************************************************
The transaction lock system global types

(c) 1996 Innobase Oy

Created 5/7/1996 Heikki Tuuri
*******************************************************/

#ifndef lock0types_h
#define lock0types_h

#define lock_t ib_lock_t
typedef struct lock_struct	lock_t;
typedef struct lock_sys_struct	lock_sys_t;

/* Basic lock modes */
enum lock_mode {
	LOCK_IS = 0,	/* intention shared */
	LOCK_IX,	/* intention exclusive */
	LOCK_S,		/* shared */
	LOCK_X,		/* exclusive */
	LOCK_AUTO_INC,	/* locks the auto-inc counter of a table
			in an exclusive mode */
	LOCK_NONE,	/* this is used elsewhere to note consistent read */
	LOCK_NUM = LOCK_NONE/* number of lock modes */
};

#endif
