/* ==== wrapper.c ============================================================
 * Copyright (c) 1994 by Chris Provenzano, proven@mit.edu
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *  This product includes software developed by Chris Provenzano.
 * 4. The name of Chris Provenzano may not be used to endorse or promote 
 *	  products derived from this software without specific prior written
 *	  permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CHRIS PROVENZANO ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL CHRIS PROVENZANO BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 *
 * Description : Wrapper functions for syscalls that only need errno redirected
 *
 *	1.4x 94/07/23 proven
 *		-Started coding this file.
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif

#include "config.h"
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread/posix.h>

/* ==========================================================================
 * link()
 */
int link(const char * name1, const char * name2)
{
	int ret;

	if ((ret = machdep_sys_link(name1, name2)) < OK) {
		SET_ERRNO(-ret);
	}
	return(ret);

}

/* ==========================================================================
 * unlink()
 */
int unlink(const char * path)
{
	int ret;

	if ((ret = machdep_sys_unlink(path)) < OK) {
		SET_ERRNO(-ret);
	}
	return(ret);

}

/* ==========================================================================
 * chdir()
 */
int chdir(const char * path)
{
	int ret;

	if ((ret = machdep_sys_chdir(path)) < OK) {
		SET_ERRNO(-ret);
	}
	return(ret);

}

/* ==========================================================================
 * chmod()
 */
int chmod(const char * path, mode_t mode)
{
	int ret;

	if ((ret = machdep_sys_chmod(path, mode)) < OK) {
		SET_ERRNO(-ret);
	}
	return(ret);

}

/* ==========================================================================
 * chown()
 */
int chown(const char * path, uid_t owner, gid_t group)
{
	int ret;

	if ((ret = machdep_sys_chown(path, owner, group)) < OK) {
		SET_ERRNO(-ret);
	}
	return(ret);

}

/* ==========================================================================
 * rename()
 */
int rename(const char * name1, const char * name2)
{
	int ret;

	if ((ret = machdep_sys_rename(name1, name2)) < OK) {
		SET_ERRNO(-ret);
	}
	return(ret);

}


/* ==========================================================================
 * chroot()
 */

#ifdef HAVE_SYSCALL_CHROOT
int chroot(const char * name)
{
	int ret;

	if ((ret = machdep_sys_chroot(name)) < OK) {
		SET_ERRNO(-ret);
	}
	return(ret);

}
#endif
