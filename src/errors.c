/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/**
  \file errors.c
  \brief Error handling
 
  The error handling routines for ydb
*/

#include <stdio.h>
#include <stdarg.h>

#include "ydb-internal.h"

/** Checks whether the environment has panicked */
int toku_env_is_panicked(DB_ENV *dbenv /**< The environment to check */) {
    if (dbenv==0) return 0;
    return dbenv->i->is_panicked || toku_logger_panicked(dbenv->i->logger);
}


/* Prints an error message to a file specified by env (or stderr),
   preceded by the environment's error prefix. */
static void __toku_ydb_error_file(const DB_ENV *env, BOOL use_stderr, 
                                  char errmsg[]) {
    /* Determine the error file to use */
    FILE *efile=env->i->errfile;
    if (efile==NULL && env->i->errcall==0 && use_stderr) efile = stderr;

    /* Print out on a file */
    if (efile) {
        if (env->i->errpfx) fprintf(efile, "%s: ", env->i->errpfx);
	fprintf(efile, ": %s", errmsg);
    }
}


/**  

     Prints out environment errors, adjusting to a variety of options 
     and formats. 
     The printout format can be controlled to print the following optional 
     messages:
     - The environment error message prefix
     - User-supplied prefix obtained by printing ap with the
       fmt string
     - The standard db error string
     The print out takes place via errcall (if set), errfile (if set),
     or stderr if neither is set (and the user so toggles the printout).
     Both errcall and errfile can be set.
     The error message is truncated to approximately 4,000 characters.

     \param env   The environment that the error refers to. 
     \param error The error code
     \param include_stderrstring Controls whether the standard db error 
                  string should be included in the print out
     \param use_stderr_if_nothing_else Toggles the use of stderr.
     \param fmt   Output format for optional prefix arguments (must be NULL
                  if the prefix is empty)
     \param ap    Optional prefix
*/
void toku_ydb_error_all_cases(const DB_ENV * env, 
                              int error, 
                              BOOL include_stderrstring, 
                              BOOL use_stderr_if_nothing_else, 
                              const char *fmt, va_list ap) {
    /* Construct the error message */
    char buf [4000];
    int count=0;
    if (fmt) count=vsnprintf(buf, sizeof(buf), fmt, ap);
    if (include_stderrstring) {
        count+=snprintf(&buf[count], sizeof(buf)-count, ": %s", 
                        db_strerror(error));
    }

    /* Print via errcall */
    if (env->i->errcall) env->i->errcall(env, env->i->errpfx, buf);

    /* Print out on a file */
    __toku_ydb_error_file(env, use_stderr_if_nothing_else, buf);
}

int toku_ydb_do_error (const DB_ENV *dbenv, int error, const char *string, ...)
                       __attribute__((__format__(__printf__, 3, 4)));


/** Handle all the error cases (but don't do the default thing.) 
    \param dbenv  The environment that is subject to errors
    \param error  The error code
    \param fmt    The format string for additional variable arguments to
                  be printed   */
int toku_ydb_do_error (const DB_ENV *dbenv, int error, const char *fmt, ...) {
    if (toku_logger_panicked(dbenv->i->logger)) dbenv->i->is_panicked=1;
    va_list ap;
    va_start(ap, fmt);
    toku_ydb_error_all_cases(dbenv, error, TRUE, FALSE, fmt, ap);
    va_end(ap);
    return error;
}

void toku_locked_env_err(const DB_ENV * env, int error, const char *fmt, ...)
                           __attribute__((__format__(__printf__, 3, 4)));


/** Handle errors on an environment, guarded by the ydb lock 
    \param dbenv  The environment that is subject to errors
    \param error  The error code
    \param fmt    The format string for additional variable arguments to
                  be printed   */
void toku_locked_env_err(const DB_ENV * env, int error, const char *fmt, ...) {
    toku_ydb_lock();
    va_list ap;
    va_start(ap, fmt);
    toku_ydb_error_all_cases(env, error, TRUE, TRUE, fmt, ap);
    va_end(ap);
    toku_ydb_unlock();
}


/** Barf out where ydb is and what it is doing */
void toku_ydb_barf() {
    fprintf(stderr, "YDB: BARF %s:%d in %s\n", __FILE__, __LINE__, __func__); 
}

/** Prints a note with the point where it was generated
    \param fmt The format string for the note to be printed */
void toku_ydb_notef(const char *fmt, ...) {
    fprintf(stderr, "YDB: Note %s:%d in %s, ", __FILE__, __LINE__, __func__); 
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

