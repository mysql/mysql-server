/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: shqueue.h,v 11.6 2000/11/14 20:20:28 bostic Exp $
 */
#ifndef	_SYS_SHQUEUE_H_
#define	_SYS_SHQUEUE_H_

/*
 * This file defines three types of data structures: lists, tail queues, and
 * circular queues, similarly to the include file <sys/queue.h>.
 *
 * The difference is that this set of macros can be used for structures that
 * reside in shared memory that may be mapped at different addresses in each
 * process.  In most cases, the macros for shared structures exactly mirror
 * the normal macros, although the macro calls require an additional type
 * parameter, only used by the HEAD and ENTRY macros of the standard macros.
 *
 * For details on the use of these macros, see the queue(3) manual page.
 */

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Shared list definitions.
 */
#define	SH_LIST_HEAD(name)						\
struct name {								\
	ssize_t slh_first;	/* first element */			\
}

#define	SH_LIST_ENTRY							\
struct {								\
	ssize_t sle_next;	/* relative offset next element */	\
	ssize_t sle_prev;	/* relative offset of prev element */	\
}

/*
 * Shared list functions.  Since we use relative offsets for pointers,
 * 0 is a valid offset.  Therefore, we use -1 to indicate end of list.
 * The macros ending in "P" return pointers without checking for end
 * of list, the others check for end of list and evaluate to either a
 * pointer or NULL.
 */

#define	SH_LIST_FIRSTP(head, type)					\
	((struct type *)(((u_int8_t *)(head)) + (head)->slh_first))

#define	SH_LIST_FIRST(head, type)					\
	((head)->slh_first == -1 ? NULL :				\
	((struct type *)(((u_int8_t *)(head)) + (head)->slh_first)))

#define	SH_LIST_NEXTP(elm, field, type)					\
	((struct type *)(((u_int8_t *)(elm)) + (elm)->field.sle_next))

#define	SH_LIST_NEXT(elm, field, type)					\
	((elm)->field.sle_next == -1 ? NULL :				\
	((struct type *)(((u_int8_t *)(elm)) + (elm)->field.sle_next)))

#define	SH_LIST_PREV(elm, field)					\
	((ssize_t *)(((u_int8_t *)(elm)) + (elm)->field.sle_prev))

#define	SH_PTR_TO_OFF(src, dest)					\
	((ssize_t)(((u_int8_t *)(dest)) - ((u_int8_t *)(src))))

/*
 * Take the element's next pointer and calculate what the corresponding
 * Prev pointer should be -- basically it is the negation plus the offset
 * of the next field in the structure.
 */
#define	SH_LIST_NEXT_TO_PREV(elm, field)				\
	(-(elm)->field.sle_next + SH_PTR_TO_OFF(elm, &(elm)->field.sle_next))

#define	SH_LIST_INIT(head) (head)->slh_first = -1

#define	SH_LIST_INSERT_AFTER(listelm, elm, field, type) do {		\
	if ((listelm)->field.sle_next != -1) {				\
		(elm)->field.sle_next = SH_PTR_TO_OFF(elm,		\
		    SH_LIST_NEXTP(listelm, field, type));		\
		SH_LIST_NEXTP(listelm, field, type)->field.sle_prev =	\
			SH_LIST_NEXT_TO_PREV(elm, field);		\
	} else								\
		(elm)->field.sle_next = -1;				\
	(listelm)->field.sle_next = SH_PTR_TO_OFF(listelm, elm);	\
	(elm)->field.sle_prev = SH_LIST_NEXT_TO_PREV(listelm, field);	\
} while (0)

#define	SH_LIST_INSERT_HEAD(head, elm, field, type) do {		\
	if ((head)->slh_first != -1) {					\
		(elm)->field.sle_next =				\
		    (head)->slh_first - SH_PTR_TO_OFF(head, elm);	\
		SH_LIST_FIRSTP(head, type)->field.sle_prev =		\
			SH_LIST_NEXT_TO_PREV(elm, field);		\
	} else								\
		(elm)->field.sle_next = -1;				\
	(head)->slh_first = SH_PTR_TO_OFF(head, elm);			\
	(elm)->field.sle_prev = SH_PTR_TO_OFF(elm, &(head)->slh_first);	\
} while (0)

#define	SH_LIST_REMOVE(elm, field, type) do {				\
	if ((elm)->field.sle_next != -1) {				\
		SH_LIST_NEXTP(elm, field, type)->field.sle_prev =	\
			(elm)->field.sle_prev - (elm)->field.sle_next;	\
		*SH_LIST_PREV(elm, field) += (elm)->field.sle_next;	\
	} else								\
		*SH_LIST_PREV(elm, field) = -1;				\
} while (0)

/*
 * Shared tail queue definitions.
 */
#define	SH_TAILQ_HEAD(name)						\
struct name {								\
	ssize_t stqh_first;	/* relative offset of first element */	\
	ssize_t stqh_last;	/* relative offset of last's next */	\
}

#define	SH_TAILQ_ENTRY							\
struct {								\
	ssize_t stqe_next;	/* relative offset of next element */	\
	ssize_t stqe_prev;	/* relative offset of prev's next */	\
}

/*
 * Shared tail queue functions.
 */
#define	SH_TAILQ_FIRSTP(head, type)					\
	((struct type *)((u_int8_t *)(head) + (head)->stqh_first))

#define	SH_TAILQ_FIRST(head, type)					\
	((head)->stqh_first == -1 ? NULL : SH_TAILQ_FIRSTP(head, type))

#define	SH_TAILQ_NEXTP(elm, field, type)				\
	((struct type *)((u_int8_t *)(elm) + (elm)->field.stqe_next))

#define	SH_TAILQ_NEXT(elm, field, type)					\
	((elm)->field.stqe_next == -1 ? NULL : SH_TAILQ_NEXTP(elm, field, type))

#define	SH_TAILQ_PREVP(elm, field)					\
	((ssize_t *)((u_int8_t *)(elm) + (elm)->field.stqe_prev))

#define	SH_TAILQ_LAST(head)						\
	((ssize_t *)(((u_int8_t *)(head)) + (head)->stqh_last))

#define	SH_TAILQ_NEXT_TO_PREV(elm, field)				\
	(-(elm)->field.stqe_next + SH_PTR_TO_OFF(elm, &(elm)->field.stqe_next))

#define	SH_TAILQ_INIT(head) {						\
	(head)->stqh_first = -1;					\
	(head)->stqh_last = SH_PTR_TO_OFF(head, &(head)->stqh_first);	\
}

#define	SH_TAILQ_INSERT_HEAD(head, elm, field, type) do {		\
	if ((head)->stqh_first != -1) {					\
		(elm)->field.stqe_next =				\
		    (head)->stqh_first - SH_PTR_TO_OFF(head, elm);	\
		SH_TAILQ_FIRSTP(head, type)->field.stqe_prev =		\
			SH_TAILQ_NEXT_TO_PREV(elm, field);		\
	} else {							\
		(elm)->field.stqe_next = -1;				\
		(head)->stqh_last =					\
		    SH_PTR_TO_OFF(head, &(elm)->field.stqe_next);	\
	}								\
	(head)->stqh_first = SH_PTR_TO_OFF(head, elm);			\
	(elm)->field.stqe_prev =					\
	    SH_PTR_TO_OFF(elm, &(head)->stqh_first);			\
} while (0)

#define	SH_TAILQ_INSERT_TAIL(head, elm, field) do {			\
	(elm)->field.stqe_next = -1;					\
	(elm)->field.stqe_prev =					\
	    -SH_PTR_TO_OFF(head, elm) + (head)->stqh_last;		\
	if ((head)->stqh_last ==					\
	    SH_PTR_TO_OFF((head), &(head)->stqh_first))			\
		(head)->stqh_first = SH_PTR_TO_OFF(head, elm);		\
	else								\
		*SH_TAILQ_LAST(head) = -(head)->stqh_last +		\
		    SH_PTR_TO_OFF((elm), &(elm)->field.stqe_next) +	\
		    SH_PTR_TO_OFF(head, elm);				\
	(head)->stqh_last =						\
	    SH_PTR_TO_OFF(head, &((elm)->field.stqe_next));		\
} while (0)

#define	SH_TAILQ_INSERT_AFTER(head, listelm, elm, field, type) do {	\
	if ((listelm)->field.stqe_next != -1) {				\
		(elm)->field.stqe_next = (listelm)->field.stqe_next -	\
		    SH_PTR_TO_OFF(listelm, elm);			\
		SH_TAILQ_NEXTP(listelm, field, type)->field.stqe_prev =	\
		    SH_TAILQ_NEXT_TO_PREV(elm, field);			\
	} else {							\
		(elm)->field.stqe_next = -1;				\
		(head)->stqh_last =					\
		    SH_PTR_TO_OFF(head, &elm->field.stqe_next);		\
	}								\
	(listelm)->field.stqe_next = SH_PTR_TO_OFF(listelm, elm);	\
	(elm)->field.stqe_prev = SH_TAILQ_NEXT_TO_PREV(listelm, field);	\
} while (0)

#define	SH_TAILQ_REMOVE(head, elm, field, type) do {			\
	if ((elm)->field.stqe_next != -1) {				\
		SH_TAILQ_NEXTP(elm, field, type)->field.stqe_prev =	\
		    (elm)->field.stqe_prev +				\
		    SH_PTR_TO_OFF(SH_TAILQ_NEXTP(elm,			\
		    field, type), elm);					\
		*SH_TAILQ_PREVP(elm, field) += elm->field.stqe_next;	\
	} else {							\
		(head)->stqh_last = (elm)->field.stqe_prev +		\
			SH_PTR_TO_OFF(head, elm);			\
		*SH_TAILQ_PREVP(elm, field) = -1;			\
	}								\
} while (0)

/*
 * Shared circular queue definitions.
 */
#define	SH_CIRCLEQ_HEAD(name)						\
struct name {								\
	ssize_t scqh_first;		/* first element */		\
	ssize_t scqh_last;		/* last element */		\
}

#define	SH_CIRCLEQ_ENTRY						\
struct {								\
	ssize_t scqe_next;		/* next element */		\
	ssize_t scqe_prev;		/* previous element */		\
}

/*
 * Shared circular queue functions.
 */
#define	SH_CIRCLEQ_FIRSTP(head, type)					\
	((struct type *)(((u_int8_t *)(head)) + (head)->scqh_first))

#define	SH_CIRCLEQ_FIRST(head, type)					\
	((head)->scqh_first == -1 ?					\
	(void *)head : SH_CIRCLEQ_FIRSTP(head, type))

#define	SH_CIRCLEQ_LASTP(head, type)					\
	((struct type *)(((u_int8_t *)(head)) + (head)->scqh_last))

#define	SH_CIRCLEQ_LAST(head, type)					\
	((head)->scqh_last == -1 ? (void *)head : SH_CIRCLEQ_LASTP(head, type))

#define	SH_CIRCLEQ_NEXTP(elm, field, type)				\
	((struct type *)(((u_int8_t *)(elm)) + (elm)->field.scqe_next))

#define	SH_CIRCLEQ_NEXT(head, elm, field, type)				\
	((elm)->field.scqe_next == SH_PTR_TO_OFF(elm, head) ?		\
	    (void *)head : SH_CIRCLEQ_NEXTP(elm, field, type))

#define	SH_CIRCLEQ_PREVP(elm, field, type)				\
	((struct type *)(((u_int8_t *)(elm)) + (elm)->field.scqe_prev))

#define	SH_CIRCLEQ_PREV(head, elm, field, type)				\
	((elm)->field.scqe_prev == SH_PTR_TO_OFF(elm, head) ?		\
	    (void *)head : SH_CIRCLEQ_PREVP(elm, field, type))

#define	SH_CIRCLEQ_INIT(head) {						\
	(head)->scqh_first = 0;						\
	(head)->scqh_last = 0;						\
}

#define	SH_CIRCLEQ_INSERT_AFTER(head, listelm, elm, field, type) do {	\
	(elm)->field.scqe_prev = SH_PTR_TO_OFF(elm, listelm);		\
	(elm)->field.scqe_next = (listelm)->field.scqe_next +		\
	    (elm)->field.scqe_prev;					\
	if (SH_CIRCLEQ_NEXTP(listelm, field, type) == (void *)head)	\
		(head)->scqh_last = SH_PTR_TO_OFF(head, elm);		\
	else								\
		SH_CIRCLEQ_NEXTP(listelm,				\
		    field, type)->field.scqe_prev =			\
		    SH_PTR_TO_OFF(SH_CIRCLEQ_NEXTP(listelm,		\
		    field, type), elm);					\
	(listelm)->field.scqe_next = -(elm)->field.scqe_prev;		\
} while (0)

#define	SH_CIRCLEQ_INSERT_BEFORE(head, listelm, elm, field, type) do {	\
	(elm)->field.scqe_next = SH_PTR_TO_OFF(elm, listelm);		\
	(elm)->field.scqe_prev = (elm)->field.scqe_next -		\
		SH_CIRCLEQ_PREVP(listelm, field, type)->field.scqe_next;\
	if (SH_CIRCLEQ_PREVP(listelm, field, type) == (void *)(head))	\
		(head)->scqh_first = SH_PTR_TO_OFF(head, elm);		\
	else								\
		SH_CIRCLEQ_PREVP(listelm,				\
		    field, type)->field.scqe_next =			\
		    SH_PTR_TO_OFF(SH_CIRCLEQ_PREVP(listelm,		\
		    field, type), elm);					\
	(listelm)->field.scqe_prev = -(elm)->field.scqe_next;		\
} while (0)

#define	SH_CIRCLEQ_INSERT_HEAD(head, elm, field, type) do {		\
	(elm)->field.scqe_prev = SH_PTR_TO_OFF(elm, head);		\
	(elm)->field.scqe_next = (head)->scqh_first +			\
		(elm)->field.scqe_prev;					\
	if ((head)->scqh_last == 0)					\
		(head)->scqh_last = -(elm)->field.scqe_prev;		\
	else								\
		SH_CIRCLEQ_FIRSTP(head, type)->field.scqe_prev =	\
		    SH_PTR_TO_OFF(SH_CIRCLEQ_FIRSTP(head, type), elm);	\
	(head)->scqh_first = -(elm)->field.scqe_prev;			\
} while (0)

#define	SH_CIRCLEQ_INSERT_TAIL(head, elm, field, type) do {		\
	(elm)->field.scqe_next = SH_PTR_TO_OFF(elm, head);		\
	(elm)->field.scqe_prev = (head)->scqh_last +			\
	    (elm)->field.scqe_next;					\
	if ((head)->scqh_first == 0)					\
		(head)->scqh_first = -(elm)->field.scqe_next;		\
	else								\
		SH_CIRCLEQ_LASTP(head, type)->field.scqe_next =	\
		    SH_PTR_TO_OFF(SH_CIRCLEQ_LASTP(head, type), elm);	\
	(head)->scqh_last = -(elm)->field.scqe_next;			\
} while (0)

#define	SH_CIRCLEQ_REMOVE(head, elm, field, type) do {			\
	if (SH_CIRCLEQ_NEXTP(elm, field, type) == (void *)(head))	\
		(head)->scqh_last += (elm)->field.scqe_prev;		\
	else								\
		SH_CIRCLEQ_NEXTP(elm, field, type)->field.scqe_prev +=	\
		    (elm)->field.scqe_prev;				\
	if (SH_CIRCLEQ_PREVP(elm, field, type) == (void *)(head))	\
		(head)->scqh_first += (elm)->field.scqe_next;		\
	else								\
		SH_CIRCLEQ_PREVP(elm, field, type)->field.scqe_next +=	\
		    (elm)->field.scqe_next;				\
} while (0)

#if defined(__cplusplus)
}
#endif

#endif	/* !_SYS_SHQUEUE_H_ */
