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

struct my_optarg
{
  char *arg;         /* option argument */
  int  pos;          /* next element in ARGV */
  int  verbose;      /* 0 = inhibit warnings of unrecognized options */
  int  unrecognized; /* position of the unrecognized option */
};


enum get_opt_var_type { GET_NO_ARG, GET_LONG, GET_LL, GET_STR };
enum get_opt_arg_type { NO_ARG, OPT_ARG, REQUIRED_ARG };

struct my_option
{
  const char *name;                     /* Name of the option */
  const char *comment;                  /* option comment, for autom. --help */
  gptr       *value;                    /* The variable value */
  gptr       *u_max_value;              /* The user def. max variable value */
  const char **str_values;              /* Pointer to possible values */
  enum get_opt_var_type var_type;
  enum get_opt_arg_type arg_type;
  int        id;                        /* unique id or short option */
  longlong   def_value;                 /* Default value */
  longlong   min_value;                 /* Min allowed value */
  longlong   max_value;                 /* Max allowed value */
  longlong   sub_size;                  /* Subtract this from given value */
  long       block_size;                /* Value should be a mult. of this */
  int        app_type;                  /* To be used by an application */
};

extern int handle_options (int *argc, char ***argv, 
			   const struct my_option *longopts, 
			   my_bool (*get_one_option)(int,
						     const struct my_option *,
						     char *));
extern void my_print_help(const struct my_option *options);
extern void my_print_variables(const struct my_option *options);
