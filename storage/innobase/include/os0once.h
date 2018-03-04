/*****************************************************************************

Copyright (c) 2014, 2017, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/**************************************************//**
@file include/os0once.h
A class that aids executing a given function exactly once in a multi-threaded
environment.

Created Feb 20, 2014 Vasil Dimov
*******************************************************/

#ifndef os0once_h
#define os0once_h

#include "univ.i"

#include "os0atomic.h"
#include "ut0ut.h"

/** Execute a given function exactly once in a multi-threaded environment
or wait for the function to be executed by another thread.

Example usage:
First the user must create a control variable of type os_once::state_t and
assign it os_once::NEVER_DONE.
Then the user must pass this variable, together with a function to be
executed to os_once::do_or_wait_for_done().

Multiple threads can call os_once::do_or_wait_for_done() simultaneously with
the same (os_once::state_t) control variable. The provided function will be
called exactly once and when os_once::do_or_wait_for_done() returns then this
function has completed execution, by this or another thread. In other words
os_once::do_or_wait_for_done() will either execute the provided function or
will wait for its execution to complete if it is already called by another
thread or will do nothing if the function has already completed its execution
earlier.

This mimics pthread_once(3), but unfortunatelly pthread_once(3) does not
support passing arguments to the init_routine() function. We should use
std::call_once() when we start compiling with C++11 enabled. */
class os_once {
public:
	/** Control variables' state type */
	typedef ib_uint32_t	state_t;

	/** Not yet executed. */
	static const state_t	NEVER_DONE = 0;

	/** Currently being executed by this or another thread. */
	static const state_t	IN_PROGRESS = 1;

	/** Finished execution. */
	static const state_t	DONE = 2;

	/** Call a given function or wait its execution to complete if it is
	already called by another thread.
	@param[in,out]	state		control variable
	@param[in]	do_func		function to call
	@param[in,out]	do_func_arg	an argument to pass to do_func(). */
	static
	void
	do_or_wait_for_done(
		volatile state_t*	state,
		void			(*do_func)(void*),
		void*			do_func_arg)
	{
		/* Avoid calling os_compare_and_swap_uint32() in the most
		common case. */
		if (*state == DONE) {
			return;
		}

		if (os_compare_and_swap_uint32(state,
					       NEVER_DONE, IN_PROGRESS)) {
			/* We are the first. Call the function. */

			do_func(do_func_arg);

			const bool	swapped = os_compare_and_swap_uint32(
				state, IN_PROGRESS, DONE);

			ut_a(swapped);
		} else {
			/* The state is not NEVER_DONE, so either it is
			IN_PROGRESS (somebody is calling the function right
			now or DONE (it has already been called and completed).
			Wait for it to become DONE. */
			for (;;) {
				const state_t	s = *state;

				switch (s) {
				case DONE:
					return;
				case IN_PROGRESS:
					break;
				case NEVER_DONE:
					/* fall through */
				default:
					ut_error;
				}

#ifndef UNIV_HOTBACKUP
				UT_RELAX_CPU();
#endif /* !UNIV_HOTBACKUP */
			}
		}
	}
};

#endif /* os0once_h */
