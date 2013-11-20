/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#ident "$Id$"

/**
  \file errors.c
  \brief Error handling
 
  The error handling routines for ydb
*/

#include <toku_portability.h>
#include <stdio.h>
#include <stdarg.h>

#include "ydb-internal.h"

/** Checks whether the environment has panicked */
int toku_env_is_panicked(DB_ENV *dbenv /**< The environment to check */) {
    if (dbenv==0) return 0;
    return dbenv->i->is_panicked;
}

/* Prints an error message to a file specified by env (or stderr),
   preceded by the environment's error prefix. */
static void toku__ydb_error_file(const DB_ENV *env, bool use_stderr, 
                                  char errmsg[]) {
    /* Determine the error file to use */
    FILE *CAST_FROM_VOIDP(efile, env->i->errfile);
    if (efile==NULL && env->i->errcall==0 && use_stderr) efile = stderr;

    /* Print out on a file */
    if (efile) {
        if (env->i->errpfx) fprintf(efile, "%s: ", env->i->errpfx);
	fprintf(efile, "%s", errmsg);
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
                              bool include_stderrstring, 
                              bool use_stderr_if_nothing_else, 
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
    toku__ydb_error_file(env, use_stderr_if_nothing_else, buf);
}

/** Handle all the error cases (but don't do the default thing.) 
    \param dbenv  The environment that is subject to errors
    \param error  The error code
    \param fmt    The format string for additional variable arguments to
                  be printed   */
int toku_ydb_do_error (const DB_ENV *dbenv, int error, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    toku_ydb_error_all_cases(dbenv, error, false, false, fmt, ap);
    va_end(ap);
    return error;
}

/** Handle errors on an environment, 
    \param dbenv  The environment that is subject to errors
    \param error  The error code
    \param fmt    The format string for additional variable arguments to
                  be printed   */
void toku_env_err(const DB_ENV * env, int error, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    toku_ydb_error_all_cases(env, error, false, true, fmt, ap);
    va_end(ap);
}
