/*****************************************************************************

Copyright (c) 1998, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/eval0proc.h
 Executes SQL stored procedures and their control structures

 Created 1/20/1998 Heikki Tuuri
 *******************************************************/

#ifndef eval0proc_h
#define eval0proc_h

#include "pars0pars.h"
#include "pars0sym.h"
#include "que0types.h"
#include "univ.i"

/** Performs an execution step of a procedure node.
 @return query thread to run next or NULL */
static inline que_thr_t *proc_step(que_thr_t *thr); /*!< in: query thread */
/** Performs an execution step of an if-statement node.
 @return query thread to run next or NULL */
que_thr_t *if_step(que_thr_t *thr); /*!< in: query thread */
/** Performs an execution step of a while-statement node.
 @return query thread to run next or NULL */
que_thr_t *while_step(que_thr_t *thr); /*!< in: query thread */
/** Performs an execution step of a for-loop node.
 @return query thread to run next or NULL */
que_thr_t *for_step(que_thr_t *thr); /*!< in: query thread */
/** Performs an execution step of an assignment statement node.
 @return query thread to run next or NULL */
que_thr_t *assign_step(que_thr_t *thr); /*!< in: query thread */
/** Performs an execution step of a procedure call node.
 @return query thread to run next or NULL */
static inline que_thr_t *proc_eval_step(
    que_thr_t *thr); /*!< in: query thread */
/** Performs an execution step of an exit statement node.
 @return query thread to run next or NULL */
que_thr_t *exit_step(que_thr_t *thr); /*!< in: query thread */
/** Performs an execution step of a return-statement node.
 @return query thread to run next or NULL */
que_thr_t *return_step(que_thr_t *thr); /*!< in: query thread */

#include "eval0proc.ic"

#endif
