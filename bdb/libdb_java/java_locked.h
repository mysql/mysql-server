/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: java_locked.h,v 11.9 2000/10/25 19:54:55 dda Exp $
 */

#ifndef _JAVA_LOCKED_H_
#define	_JAVA_LOCKED_H_

/*
 * Used internally by LockedDBT constructor.
 */
typedef enum _OpKind {
	inOp,     /* setting data in database (passing data in) */
	outOp,    /* getting data from database to user memory */
	inOutOp   /* both getting/setting data */
} OpKind;

/*
 *
 * Declaration of JDBT
 *
 * A JDBT object exists during a
 * single native call to the DB API.  Its constructor's job is
 * to temporarily convert any java array found in the DBT_JAVAINFO
 * to actual bytes in memory that remain locked in place.  These
 * bytes are used during the call to the underlying DB C layer,
 * and are released and/or copied back by the destructor.
 * Thus, a LockedDBT must be declared as a stack object to
 * function properly.
 */
typedef struct _jdbt
{
	/* these are accessed externally to ldbt_ functions */
	DBT_JAVAINFO *dbt;
	unsigned int java_array_len_;

	/* these are for used internally by ldbt_ functions */
	jobject obj_;
	jbyte *java_data_;
	jbyte *before_data_;
	int has_error_;
	int do_realloc_;
	OpKind kind_;
} JDBT;

extern int jdbt_lock(JDBT *, JNIEnv *jnienv, jobject obj, OpKind kind);
extern void jdbt_unlock(JDBT *, JNIEnv *jnienv); /* this unlocks and frees the memory */
extern int jdbt_realloc(JDBT *, JNIEnv *jnienv); /* returns 1 if reallocation took place */

/****************************************************************
 *
 * Declaration of JSTR
 *
 * A JSTR exists temporarily to convert a java jstring object
 * to a char *.  Because the memory for the char * string is
 * managed by the JVM, it must be released when we are done
 * looking at it.  Typically, jstr_lock() is called at the
 * beginning of a function for each jstring object, and jstr_unlock
 * is called at the end of each function for each JSTR.
 */
typedef struct _jstr
{
	/* this accessed externally to jstr_ functions */
	const char *string;

	/* this is used internally by jstr_ functions */
	jstring jstr_;
} JSTR;

extern int jstr_lock(JSTR *, JNIEnv *jnienv, jstring jstr);
extern void jstr_unlock(JSTR *, JNIEnv *jnienv);  /* this unlocks and frees mem */

/****************************************************************
 *
 * Declaration of class LockedStrarray
 *
 * Given a java jobjectArray object (that must be a String[]),
 * we extract the individual strings and build a const char **
 * When the LockedStrarray object is destroyed, the individual
 * strings are released.
 */
typedef struct _jstrarray
{
	/* this accessed externally to jstrarray_ functions */
	const char **array;

	/* this is used internally by jstrarray_ functions */
	jobjectArray arr_;
} JSTRARRAY;

extern int jstrarray_lock(JSTRARRAY *, JNIEnv *jnienv, jobjectArray arr);
extern void jstrarray_unlock(JSTRARRAY *, JNIEnv *jnienv);  /* this unlocks and frees mem */

#endif /* !_JAVA_LOCKED_H_ */
