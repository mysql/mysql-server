/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* Defines for netware compatible with MySQL */

/* required headers */
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <screen.h>
#include <limits.h>
#include <nks/synch.h>
#include <nks/thread.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <nks/errno.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#include <nks/time.h>
#include <pthread.h>
#include <termios.h>

/* required adjustments */
#undef HAVE_READDIR_R
#undef HAVE_RWLOCK_INIT
#undef HAVE_SCHED_H
#undef HAVE_SYS_MMAN_H
#undef HAVE_SYNCH_H
#undef HAVE_CRYPT
#define HAVE_PTHREAD_ATTR_SETSTACKSIZE 1
#define HAVE_PTHREAD_SIGMASK 1
#define HAVE_PTHREAD_YIELD_ZERO_ARG 1
#define HAVE_BROKEN_REALPATH 1

/* no case sensitivity */
#define FN_NO_CASE_SENCE 1

/* the thread alarm is not used */
#define DONT_USE_THR_ALARM 1

/* signals do not interrupt sockets */
#define SIGNALS_DONT_BREAK_READ 1

/* signal by closing the sockets */
#define SIGNAL_WITH_VIO_CLOSE 1

/* default directory information */
#define	DEFAULT_MYSQL_HOME    "sys:/mysql"
#define PACKAGE               "mysql"
#define DEFAULT_BASEDIR       "sys:/"
#define SHAREDIR              "share/"
#define DEFAULT_CHARSET_HOME  "sys:/mysql/"
#define DATADIR               "data/"

/* 64-bit file system calls */
#define SIZEOF_OFF_T          8
#define off_t                 off64_t
#define chsize                chsize64
#define ftruncate             ftruncate64
#define lseek                 lseek64
#define pread                 pread64
#define pwrite                pwrite64
#define tell                  tell64

/* do not use the extended time in LibC sys\stat.h */
#define _POSIX_SOURCE

/* Some macros for portability */

#define set_timespec(ABSTIME,SEC) { (ABSTIME).tv_sec=(SEC); (ABSTIME).tv_nsec=0; }
