/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997-2002
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: java_locked.h,v 11.18 2002/05/07 16:12:42 dda Exp $
 */

#ifndef _JAVA_LOCKED_H_
#define	_JAVA_LOCKED_H_

/*
 * Used as argument to locked_dbt_get().
 */
typedef enum _OpKind {
	inOp,		/* setting data in database (passing data in) */
	outOp,		/* getting data from database to user memory */
	inOutOp		/* both getting/setting data */
} OpKind;

/*
 * LOCKED_DBT
 *
 * A stack variable LOCKED_DBT should be declared for each Dbt used in a
 * native call to the DB API.  Before the DBT can be used, locked_dbt_get()
 * must be called to temporarily convert any java array found in the
 * Dbt (which has a pointer to a DBT_JAVAINFO struct) to actual bytes
 * in memory that remain locked in place.  These bytes are used during
 * the call to the DB C API, and are released and/or copied back when
 * locked_dbt_put is called.
 */
typedef struct _locked_dbt
{
	/* these are accessed externally to locked_dbt_ functions */
	DBT_JAVAINFO *javainfo;
	unsigned int java_array_len;
	jobject jdbt;

	/* these are for used internally by locked_dbt_ functions */
	jbyte *java_data;
	jbyte *before_data;
	OpKind kind;

#define	LOCKED_ERROR		0x01	/* error occurred */
#define	LOCKED_CREATE_DATA	0x02	/* must create data on the fly */
#define	LOCKED_REALLOC_NONNULL	0x04	/* DB_DBT_REALLOC flag, nonnull data */
	u_int32_t flags;
} LOCKED_DBT;

/* Fill the LOCKED_DBT struct and lock the Java byte array */
extern int locked_dbt_get(LOCKED_DBT *, JNIEnv *, DB_ENV *, jobject, OpKind);

/* unlock the Java byte array */
extern void locked_dbt_put(LOCKED_DBT *, JNIEnv *, DB_ENV *);

/* realloc the Java byte array */
extern int locked_dbt_realloc(LOCKED_DBT *, JNIEnv *, DB_ENV *);

/*
 * LOCKED_STRING
 *
 * A LOCKED_STRING exists temporarily to convert a java jstring object
 * to a char *.  Because the memory for the char * string is
 * managed by the JVM, it must be released when we are done
 * looking at it.  Typically, locked_string_get() is called at the
 * beginning of a function for each jstring object, and locked_string_put
 * is called at the end of each function for each LOCKED_STRING.
 */
typedef struct _locked_string
{
	/* this accessed externally to locked_string_ functions */
	const char *string;

	/* this is used internally by locked_string_ functions */
	jstring jstr;
} LOCKED_STRING;

extern int locked_string_get(LOCKED_STRING *, JNIEnv *jnienv, jstring jstr);
extern void locked_string_put(LOCKED_STRING *, JNIEnv *jnienv);  /* this unlocks and frees mem */

#endif /* !_JAVA_LOCKED_H_ */
