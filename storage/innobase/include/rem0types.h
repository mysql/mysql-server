/************************************************************************
Record manager global types

(c) 1994-1996 Innobase Oy

Created 5/30/1994 Heikki Tuuri
*************************************************************************/

#ifndef rem0types_h
#define rem0types_h

/* We define the physical record simply as an array of bytes */
typedef byte	rec_t;

/* Maximum values for various fields (for non-blob tuples) */
#define REC_MAX_N_FIELDS	(1024 - 1)
#define REC_MAX_HEAP_NO		(2 * 8192 - 1)
#define REC_MAX_N_OWNED		(16 - 1)

#endif
