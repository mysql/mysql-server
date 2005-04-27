/* Copyright (C) 2003 MySQL AB

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

/*
 * Copyright (c) 1997, 1999 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

/* $KTH: getarg.h,v 1.9 2000/09/01 21:25:55 lha Exp $ */

#ifndef __GETARG_H__
#define __GETARG_H__

#include <ndb_global.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { 
    arg_integer, 
    arg_string, 
    arg_flag, 
    arg_negative_flag, 
    arg_strings,
    arg_double,
    arg_collect,
    arg_counter
} arg_type;

struct getargs{
    const char *long_name;
    char short_name;
    arg_type type;
    void *value;
    const char *help;
    const char *arg_help;
};

enum {
    ARG_ERR_NO_MATCH  = 1,
    ARG_ERR_BAD_ARG,
    ARG_ERR_NO_ARG
};

typedef struct getarg_strings {
    int num_strings;
    char **strings;
} getarg_strings;

typedef int (*getarg_collect_func)(int short_opt,
				   int argc,
				   const char **argv,
				   int *optind,
				   int *optarg,
				   void *data);

typedef struct getarg_collect_info {
    getarg_collect_func func;
    void *data;
} getarg_collect_info;

int getarg(struct getargs *args, size_t num_args, 
	   int argc, const char **argv, int *optind);

void arg_printusage (struct getargs *args,
		     size_t num_args,
		     const char *progname,
		     const char *extra_string);
#ifdef __cplusplus
}
#endif

#endif /* __GETARG_H__ */
