/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

#ifndef _my_getopt_h
#define _my_getopt_h

C_MODE_START

#define GET_NO_ARG     1
#define GET_BOOL       2
#define GET_INT        3
#define GET_UINT       4
#define GET_LONG       5
#define GET_ULONG      6
#define GET_LL         7
#define GET_ULL        8
#define GET_STR        9
#define GET_STR_ALLOC 10
#define GET_DISABLED  11

#define GET_ASK_ADDR	 128
#define GET_TYPE_MASK	 127

enum get_opt_arg_type { NO_ARG, OPT_ARG, REQUIRED_ARG };

struct my_option
{
  const char *name;                     /* Name of the option */
  int        id;                        /* unique id or short option */
  const char *comment;                  /* option comment, for autom. --help */
  gptr       *value;                    /* The variable value */
  gptr       *u_max_value;              /* The user def. max variable value */
  const char **str_values;              /* Pointer to possible values */
  ulong     var_type;
  enum get_opt_arg_type arg_type;
  longlong   def_value;                 /* Default value */
  longlong   min_value;                 /* Min allowed value */
  longlong   max_value;                 /* Max allowed value */
  longlong   sub_size;                  /* Subtract this from given value */
  long       block_size;                /* Value should be a mult. of this */
  int        app_type;                  /* To be used by an application */
};

typedef my_bool (* my_get_one_option) (int, const struct my_option *, char * );
typedef void (* my_error_reporter) (enum loglevel level, const char *format, ... );

extern char *disabled_my_option;
extern my_bool my_getopt_print_errors;
extern my_error_reporter my_getopt_error_reporter;

extern int handle_options (int *argc, char ***argv, 
			   const struct my_option *longopts, my_get_one_option);
extern void my_print_help(const struct my_option *options);
extern void my_print_variables(const struct my_option *options);
extern void my_getopt_register_get_addr(gptr* (*func_addr)(const char *, uint,
							   const struct my_option *));

ulonglong getopt_ull_limit_value(ulonglong num, const struct my_option *optp);
my_bool getopt_compare_strings(const char *s, const char *t, uint length);

C_MODE_END

#endif /* _my_getopt_h */

