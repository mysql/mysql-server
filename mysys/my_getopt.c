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

#include <my_global.h>
#include <m_string.h>
#include <stdlib.h>
#include <my_sys.h>
#include <mysys_err.h>
#include <my_getopt.h>

static void default_reporter(enum loglevel level, const char *format, ...);
my_error_reporter my_getopt_error_reporter= &default_reporter;

static int findopt(char *optpat, uint length,
		   const struct my_option **opt_res,
		   char **ffname);
my_bool getopt_compare_strings(const char *s,
			       const char *t,
			       uint length);
static longlong getopt_ll(char *arg, const struct my_option *optp, int *err);
static ulonglong getopt_ull(char *arg, const struct my_option *optp,
			    int *err);
static void init_variables(const struct my_option *options);
static int setval(const struct my_option *opts, gptr *value, char *argument,
		  my_bool set_maximum_value);
static char *check_struct_option(char *cur_arg, char *key_name);

/*
  The following three variables belong to same group and the number and
  order of their arguments must correspond to each other.
*/
static const char *special_opt_prefix[]=
{"skip", "disable", "enable", "maximum", "loose", 0};
static const uint special_opt_prefix_lengths[]=
{ 4,      7,         6,        7,         5,      0};
enum enum_special_opt
{ OPT_SKIP, OPT_DISABLE, OPT_ENABLE, OPT_MAXIMUM, OPT_LOOSE};

char *disabled_my_option= (char*) "0";

/* 
   This is a flag that can be set in client programs. 0 means that
   my_getopt will not print error messages, but the client should do
   it by itself
*/

my_bool my_getopt_print_errors= 1;

static void default_reporter(enum loglevel level __attribute__((unused)),
                             const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
}

/* 
  function: handle_options

  Sort options; put options first, until special end of options (--), or
  until end of argv. Parse options; check that the given option matches with
  one of the options in struct 'my_option', return error in case of ambiguous
  or unknown option. Check that option was given an argument if it requires
  one. Call function 'get_one_option()' once for each option.
*/

static gptr* (*getopt_get_addr)(const char *, uint, const struct my_option *);

void my_getopt_register_get_addr(gptr* (*func_addr)(const char *, uint,
						    const struct my_option *))
{
  getopt_get_addr= func_addr;
}

int handle_options(int *argc, char ***argv, 
		   const struct my_option *longopts,
                   my_get_one_option get_one_option)
{
  uint opt_found, argvpos= 0, length, i;
  my_bool end_of_options= 0, must_be_var, set_maximum_value,
          option_is_loose;
  char **pos, **pos_end, *optend, *prev_found,
       *opt_str, key_name[FN_REFLEN];
  const struct my_option *optp;
  gptr *value;
  int error;

  LINT_INIT(opt_found);
  (*argc)--; /* Skip the program name */
  (*argv)++; /*      --- || ----      */
  init_variables(longopts);

  for (pos= *argv, pos_end=pos+ *argc; pos != pos_end ; pos++)
  {
    char *cur_arg= *pos;
    if (cur_arg[0] == '-' && cur_arg[1] && !end_of_options) /* must be opt */
    {
      char *argument=    0;
      must_be_var=       0;
      set_maximum_value= 0;
      option_is_loose=   0;

      cur_arg++;		/* skip '-' */
      if (*cur_arg == '-' || *cur_arg == 'O') /* check for long option, */
      {                                       /* --set-variable, or -O  */
	if (*cur_arg == 'O')
	{
	  must_be_var= 1;

	  if (!(*++cur_arg))	/* If not -Ovar=# */
	  {
	    /* the argument must be in next argv */
	    if (!*++pos)
	    {
	      if (my_getopt_print_errors)
                my_getopt_error_reporter(ERROR_LEVEL,
                                         "%s: Option '-O' requires an argument\n",
                                         my_progname);
	      return EXIT_ARGUMENT_REQUIRED;
	    }
	    cur_arg= *pos;
	    (*argc)--;
	  }
	}
	else if (!getopt_compare_strings(cur_arg, "-set-variable", 13))
	{
	  must_be_var= 1;
	  if (cur_arg[13] == '=')
	  {
	    cur_arg+= 14;
	    if (!*cur_arg)
	    {
	      if (my_getopt_print_errors)
                my_getopt_error_reporter(ERROR_LEVEL,
                                         "%s: Option '--set-variable' requires an argument\n",
                                         my_progname);
	      return EXIT_ARGUMENT_REQUIRED;
	    }
	  }
	  else if (cur_arg[14]) /* garbage, or another option. break out */
	    must_be_var= 0;
	  else
	  {
	    /* the argument must be in next argv */
	    if (!*++pos)
	    {
	      if (my_getopt_print_errors)
                my_getopt_error_reporter(ERROR_LEVEL,
                                         "%s: Option '--set-variable' requires an argument\n",
                                         my_progname);
	      return EXIT_ARGUMENT_REQUIRED;
	    }
	    cur_arg= *pos;
	    (*argc)--;
	  }
	}
	else if (!must_be_var)
	{
	  if (!*++cur_arg)	/* skip the double dash */
	  {
	    /* '--' means end of options, look no further */
	    end_of_options= 1;
	    (*argc)--;
	    continue;
	  }
	}
	opt_str= check_struct_option(cur_arg, key_name);
	optend= strcend(opt_str, '=');
	length= (uint) (optend - opt_str);
	if (*optend == '=')
	  optend++;
	else
	  optend= 0;

	/*
	  Find first the right option. Return error in case of an ambiguous,
	  or unknown option
	*/
	optp= longopts;
	if (!(opt_found= findopt(opt_str, length, &optp, &prev_found)))
	{
	  /*
	    Didn't find any matching option. Let's see if someone called
	    option with a special option prefix
	  */
	  if (!must_be_var)
	  {
	    if (optend)
	      must_be_var= 1; /* option is followed by an argument */
	    for (i= 0; special_opt_prefix[i]; i++)
	    {
	      if (!getopt_compare_strings(special_opt_prefix[i], opt_str,
					  special_opt_prefix_lengths[i]) &&
		  (opt_str[special_opt_prefix_lengths[i]] == '-' ||
		   opt_str[special_opt_prefix_lengths[i]] == '_'))
	      {
		/*
		  We were called with a special prefix, we can reuse opt_found
		*/
		opt_str+= (special_opt_prefix_lengths[i] + 1);
		if (i == OPT_LOOSE)
		  option_is_loose= 1;
		if ((opt_found= findopt(opt_str, length -
					(special_opt_prefix_lengths[i] + 1),
					&optp, &prev_found)))
		{
		  if (opt_found > 1)
		  {
		    if (my_getopt_print_errors)
                      my_getopt_error_reporter(ERROR_LEVEL,
                                               "%s: ambiguous option '--%s-%s' (--%s-%s)\n",
                                               my_progname, special_opt_prefix[i],
                                               cur_arg, special_opt_prefix[i],
                                               prev_found);
		    return EXIT_AMBIGUOUS_OPTION;
		  }
		  switch (i) {
		  case OPT_SKIP:
		  case OPT_DISABLE: /* fall through */
		    /*
		      double negation is actually enable again,
		      for example: --skip-option=0 -> option = TRUE
		    */
		    optend= (optend && *optend == '0' && !(*(optend + 1))) ?
		      (char*) "1" : disabled_my_option;
		    break;
		  case OPT_ENABLE:
		    optend= (optend && *optend == '0' && !(*(optend + 1))) ?
 		      disabled_my_option : (char*) "1";
		    break;
		  case OPT_MAXIMUM:
		    set_maximum_value= 1;
		    must_be_var= 1;
		    break;
		  }
		  break; /* break from the inner loop, main loop continues */
		}
	      }
	    }
	  }
	  if (!opt_found)
	  {
	    if (must_be_var)
	    {
	      if (my_getopt_print_errors)
                my_getopt_error_reporter(option_is_loose ? 
                                           WARNING_LEVEL : ERROR_LEVEL,
                                         "%s: unknown variable '%s'\n",
                                         my_progname, cur_arg);
	      if (!option_is_loose)
		return EXIT_UNKNOWN_VARIABLE;
	    }
	    else
	    {
	      if (my_getopt_print_errors)
                my_getopt_error_reporter(option_is_loose ? 
                                           WARNING_LEVEL : ERROR_LEVEL,
                                         "%s: unknown option '--%s'\n", 
                                         my_progname, cur_arg);
	      if (!option_is_loose)
		return EXIT_UNKNOWN_OPTION;
	    }
	    if (option_is_loose)
	    {
	      (*argc)--;
	      continue;
	    }
	  }
	}
	if (opt_found > 1)
	{
	  if (must_be_var)
	  {
	    if (my_getopt_print_errors)
              my_getopt_error_reporter(ERROR_LEVEL,
                                       "%s: variable prefix '%s' is not unique\n",
                                       my_progname, opt_str);
	    return EXIT_VAR_PREFIX_NOT_UNIQUE;
	  }
	  else
	  {
	    if (my_getopt_print_errors)
              my_getopt_error_reporter(ERROR_LEVEL,
                                       "%s: ambiguous option '--%s' (%s, %s)\n",
                                       my_progname, opt_str, prev_found, 
                                       optp->name);
	    return EXIT_AMBIGUOUS_OPTION;
	  }
	}
	if ((optp->var_type & GET_TYPE_MASK) == GET_DISABLED)
	{
	  if (my_getopt_print_errors)
	    fprintf(stderr,
		    "%s: %s: Option '%s' used, but is disabled\n", my_progname,
		    option_is_loose ? "WARNING" : "ERROR", opt_str);
	  if (option_is_loose)
	  {
	    (*argc)--;
	    continue;
	  }
	  return EXIT_OPTION_DISABLED;
	}
	if (must_be_var && (optp->var_type & GET_TYPE_MASK) == GET_NO_ARG)
	{
	  if (my_getopt_print_errors)
            my_getopt_error_reporter(ERROR_LEVEL, 
                                     "%s: option '%s' cannot take an argument\n",
                                     my_progname, optp->name);
	  return EXIT_NO_ARGUMENT_ALLOWED;
	}
	value= optp->var_type & GET_ASK_ADDR ?
	  (*getopt_get_addr)(key_name, (uint) strlen(key_name), optp) : optp->value;
  
	if (optp->arg_type == NO_ARG)
	{
	  if (optend && (optp->var_type & GET_TYPE_MASK) != GET_BOOL)
	  {
	    if (my_getopt_print_errors)
              my_getopt_error_reporter(ERROR_LEVEL,
                                       "%s: option '--%s' cannot take an argument\n",
                                       my_progname, optp->name);
	    return EXIT_NO_ARGUMENT_ALLOWED;
	  }
	  if ((optp->var_type & GET_TYPE_MASK) == GET_BOOL)
	  {
	    /*
	      Set bool to 1 if no argument or if the user has used
	      --enable-'option-name'.
	      *optend was set to '0' if one used --disable-option
	      */
	    (*argc)--;
	    if (!optend || *optend == '1' ||
		!my_strcasecmp(&my_charset_latin1, optend, "true"))
	      *((my_bool*) value)= (my_bool) 1;
	    else if (*optend == '0' ||
		     !my_strcasecmp(&my_charset_latin1, optend, "false"))
	      *((my_bool*) value)= (my_bool) 0;
	    else
	    {
	      my_getopt_error_reporter(WARNING_LEVEL,
				       "%s: ignoring option '--%s' due to \
invalid value '%s'\n",
				       my_progname, optp->name, optend);
	      continue;
	    }
	    get_one_option(optp->id, optp,
			   *((my_bool*) value) ?
			   (char*) "1" : disabled_my_option);
	    continue;
	  }
	  argument= optend;
	}
	else if (optp->arg_type == OPT_ARG &&
		 (optp->var_type & GET_TYPE_MASK) == GET_BOOL)
	{
	  if (optend == disabled_my_option)
	    *((my_bool*) value)= (my_bool) 0;
	  else
	  {
	    if (!optend) /* No argument -> enable option */
	      *((my_bool*) value)= (my_bool) 1;
            else
              argument= optend;
	  }
	}
	else if (optp->arg_type == REQUIRED_ARG && !optend)
	{
	  /* Check if there are more arguments after this one */
	  if (!*++pos)
	  {
	    if (my_getopt_print_errors)
              my_getopt_error_reporter(ERROR_LEVEL,
                                       "%s: option '--%s' requires an argument\n",
                                       my_progname, optp->name);
	    return EXIT_ARGUMENT_REQUIRED;
	  }
	  argument= *pos;
	  (*argc)--;
	}
	else
	  argument= optend;
      }
      else  /* must be short option */
      {
	for (optend= cur_arg; *optend; optend++)
	{
	  opt_found= 0;
	  for (optp= longopts; optp->id; optp++)
	  {
	    if (optp->id == (int) (uchar) *optend)
	    {
	      /* Option recognized. Find next what to do with it */
	      opt_found= 1;
	      if ((optp->var_type & GET_TYPE_MASK) == GET_DISABLED)
	      {
		if (my_getopt_print_errors)
		  fprintf(stderr,
			  "%s: ERROR: Option '-%c' used, but is disabled\n",
			  my_progname, optp->id);
		return EXIT_OPTION_DISABLED;
	      }
	      if ((optp->var_type & GET_TYPE_MASK) == GET_BOOL &&
		  optp->arg_type == NO_ARG)
	      {
		*((my_bool*) optp->value)= (my_bool) 1;
		get_one_option(optp->id, optp, argument);
		continue;
	      }
	      else if (optp->arg_type == REQUIRED_ARG ||
		       optp->arg_type == OPT_ARG)
	      {
		if (*(optend + 1))
		{
		  /* The rest of the option is option argument */
		  argument= optend + 1;
		  /* This is in effect a jump out of the outer loop */
		  optend= (char*) " ";
		}
		else
		{
                  if (optp->arg_type == OPT_ARG)
                  {
                    if (optp->var_type == GET_BOOL)
                      *((my_bool*) optp->value)= (my_bool) 1;
                    get_one_option(optp->id, optp, argument);
                    continue;
                  }
		  /* Check if there are more arguments after this one */
		  if (!pos[1])
		  {
                    if (my_getopt_print_errors)
                      my_getopt_error_reporter(ERROR_LEVEL,
                                               "%s: option '-%c' requires an argument\n",
                                               my_progname, optp->id);
                    return EXIT_ARGUMENT_REQUIRED;
		  }
		  argument= *++pos;
		  (*argc)--;
		  /* the other loop will break, because *optend + 1 == 0 */
		}
	      }
	      if ((error= setval(optp, optp->value, argument,
				 set_maximum_value)))
	      {
                my_getopt_error_reporter(ERROR_LEVEL,
                                         "%s: Error while setting value '%s' to '%s'\n",
                                         my_progname, argument, optp->name);
		return error;
	      }
	      get_one_option(optp->id, optp, argument);
	      break;
	    }
	  }
	  if (!opt_found)
	  {
	    if (my_getopt_print_errors)
              my_getopt_error_reporter(ERROR_LEVEL,
                                       "%s: unknown option '-%c'\n", 
                                       my_progname, *optend);
	    return EXIT_UNKNOWN_OPTION;
	  }
	}
	(*argc)--; /* option handled (short), decrease argument count */
	continue;
      }
      if ((error= setval(optp, value, argument, set_maximum_value)))
      {
        my_getopt_error_reporter(ERROR_LEVEL,
                                 "%s: Error while setting value '%s' to '%s'\n",
                                 my_progname, argument, optp->name);
	return error;
      }
      get_one_option(optp->id, optp, argument);

      (*argc)--; /* option handled (short or long), decrease argument count */
    }
    else /* non-option found */
      (*argv)[argvpos++]= cur_arg;
  }
  /*
    Destroy the first, already handled option, so that programs that look
    for arguments in 'argv', without checking 'argc', know when to stop.
    Items in argv, before the destroyed one, are all non-option -arguments
    to the program, yet to be (possibly) handled.
  */
  (*argv)[argvpos]= 0;
  return 0;
}


/*
  function: check_struct_option

  Arguments: Current argument under processing from argv and a variable
  where to store the possible key name.

  Return value: In case option is a struct option, returns a pointer to
  the current argument at the position where the struct option (key_name)
  ends, the next character after the dot. In case argument is not a struct
  option, returns a pointer to the argument.

  key_name will hold the name of the key, or 0 if not found.
*/

static char *check_struct_option(char *cur_arg, char *key_name)
{
  char *ptr, *end;

  ptr= strcend(cur_arg + 1, '.'); /* Skip the first character */
  end= strcend(cur_arg, '=');

  /* 
     If the first dot is after an equal sign, then it is part
     of a variable value and the option is not a struct option.
     Also, if the last character in the string before the ending
     NULL, or the character right before equal sign is the first
     dot found, the option is not a struct option.
  */
  if (end - ptr > 1)
  {
    uint len= (uint) (ptr - cur_arg);
    set_if_smaller(len, FN_REFLEN-1);
    strmake(key_name, cur_arg, len);
    return ++ptr;
  }
  else
  {
    key_name[0]= 0;
    return cur_arg;
  }
}

/*
  function: setval

  Arguments: opts, argument
  Will set the option value to given value
*/

static int setval(const struct my_option *opts, gptr *value, char *argument,
		  my_bool set_maximum_value)
{
  int err= 0;

  if (value && argument)
  {
    gptr *result_pos= ((set_maximum_value) ?
		       opts->u_max_value : value);

    if (!result_pos)
      return EXIT_NO_PTR_TO_VARIABLE;

    switch ((opts->var_type & GET_TYPE_MASK)) {
    case GET_BOOL: /* If argument differs from 0, enable option, else disable */
      *((my_bool*) result_pos)= (my_bool) atoi(argument) != 0;
      break;
    case GET_INT:
    case GET_UINT:           /* fall through */
      *((int*) result_pos)= (int) getopt_ll(argument, opts, &err);
      break;
    case GET_LONG:
    case GET_ULONG:          /* fall through */
      *((long*) result_pos)= (long) getopt_ll(argument, opts, &err);
      break;
    case GET_LL:
      *((longlong*) result_pos)= getopt_ll(argument, opts, &err);
      break;
    case GET_ULL:
      *((ulonglong*) result_pos)= getopt_ull(argument, opts, &err);
      break;
    case GET_STR:
      *((char**) result_pos)= argument;
      break;
    case GET_STR_ALLOC:
      if ((*((char**) result_pos)))
	my_free((*(char**) result_pos), MYF(MY_WME | MY_FAE));
      if (!(*((char**) result_pos)= my_strdup(argument, MYF(MY_WME))))
	return EXIT_OUT_OF_MEMORY;
      break;
    default:    /* dummy default to avoid compiler warnings */
      break;
    }
    if (err)
      return EXIT_UNKNOWN_SUFFIX;
  }
  return 0;
}


/* 
  Find option

  SYNOPSIS
    findopt()
    optpat	Prefix of option to find (with - or _)
    length	Length of optpat
    opt_res	Options
    ffname	Place for pointer to first found name

  IMPLEMENTATION
    Go through all options in the my_option struct. Return number
    of options found that match the pattern and in the argument
    list the option found, if any. In case of ambiguous option, store
    the name in ffname argument

    RETURN
    0    No matching options
    #   Number of matching options
        ffname points to first matching option
*/

static int findopt(char *optpat, uint length,
		   const struct my_option **opt_res,
		   char **ffname)
{
  uint count;
  struct my_option *opt= (struct my_option *) *opt_res;

  for (count= 0; opt->name; opt++)
  {
    if (!getopt_compare_strings(opt->name, optpat, length)) /* match found */
    {
      (*opt_res)= opt;
      if (!opt->name[length])		/* Exact match */
	return 1;
      if (!count)
      {
	count= 1;
	*ffname= (char *) opt->name;	/* We only need to know one prev */
      }
      else if (strcmp(*ffname, opt->name))
      {
	/*
	  The above test is to not count same option twice
	  (see mysql.cc, option "help")
	*/
	count++;
      }
    }
  }
  return count;
}


/* 
  function: compare_strings

  Works like strncmp, other than 1.) considers '-' and '_' the same.
  2.) Returns -1 if strings differ, 0 if they are equal
*/

my_bool getopt_compare_strings(register const char *s, register const char *t,
			       uint length)
{
  char const *end= s + length;
  for (;s != end ; s++, t++)
  {
    if ((*s != '-' ? *s : '_') != (*t != '-' ? *t : '_'))
      return 1;
  }
  return 0;
}

/*
  function: eval_num_suffix

  Transforms a number with a suffix to real number. Suffix can
  be k|K for kilo, m|M for mega or g|G for giga.
*/

static longlong eval_num_suffix (char *argument, int *error, char *option_name)
{
  char *endchar;
  longlong num;
  
  *error= 0;
  num= strtoll(argument, &endchar, 10);
  if (*endchar == 'k' || *endchar == 'K')
    num*= 1024L;
  else if (*endchar == 'm' || *endchar == 'M')
    num*= 1024L * 1024L;
  else if (*endchar == 'g' || *endchar == 'G')
    num*= 1024L * 1024L * 1024L;
  else if (*endchar)
  {
    fprintf(stderr,
	    "Unknown suffix '%c' used for variable '%s' (value '%s')\n",
	    *endchar, option_name, argument);
    *error= 1;
    return 0;
  }
  return num;
}

/* 
  function: getopt_ll

  Evaluates and returns the value that user gave as an argument
  to a variable. Recognizes (case insensitive) K as KILO, M as MEGA
  and G as GIGA bytes. Some values must be in certain blocks, as
  defined in the given my_option struct, this function will check
  that those values are honored.
  In case of an error, set error value in *err.
*/

static longlong getopt_ll(char *arg, const struct my_option *optp, int *err)
{
  longlong num;
  ulonglong block_size= (optp->block_size ? (ulonglong) optp->block_size : 1L);
  
  num= eval_num_suffix(arg, err, (char*) optp->name);
  if (num > 0 && (ulonglong) num > (ulonglong) (ulong) optp->max_value &&
      optp->max_value) /* if max value is not set -> no upper limit */
    num= (longlong) (ulong) optp->max_value;
  num= ((num - (longlong) optp->sub_size) / block_size);
  num= (longlong) (num * block_size);
  return max(num, optp->min_value);
}

/*
  function: getopt_ull

  This is the same as getopt_ll, but is meant for unsigned long long
  values.
*/

static ulonglong getopt_ull(char *arg, const struct my_option *optp, int *err)
{
  ulonglong num;

  num= eval_num_suffix(arg, err, (char*) optp->name);  
  return getopt_ull_limit_value(num, optp);
}


ulonglong getopt_ull_limit_value(ulonglong num, const struct my_option *optp)
{
  if ((ulonglong) num > (ulonglong) optp->max_value &&
      optp->max_value) /* if max value is not set -> no upper limit */
    num= (ulonglong) optp->max_value;
  if (optp->block_size > 1)
  {
    num/= (ulonglong) optp->block_size;
    num*= (ulonglong) optp->block_size;
  }
  if (num < (ulonglong) optp->min_value)
    num= (ulonglong) optp->min_value;
  return num;
}


/*
  Init one value to it's default values

  SYNOPSIS
    init_one_value()
    option		Option to initialize
    value		Pointer to variable
*/

static void init_one_value(const struct my_option *option, gptr *variable,
			   longlong value)
{
  switch ((option->var_type & GET_TYPE_MASK)) {
  case GET_BOOL:
    *((my_bool*) variable)= (my_bool) value;
    break;
  case GET_INT:
    *((int*) variable)= (int) value;
    break;
  case GET_UINT:
    *((uint*) variable)= (uint) value;
    break;
  case GET_LONG:
    *((long*) variable)= (long) value;
    break;
  case GET_ULONG:
    *((ulong*) variable)= (ulong) value;
    break;
  case GET_LL:
    *((longlong*) variable)= (longlong) value;
    break;
  case GET_ULL:
    *((ulonglong*) variable)=  (ulonglong) value;
    break;
  default: /* dummy default to avoid compiler warnings */
    break;
  }
}


/* 
  initialize all variables to their default values

  SYNOPSIS
    init_variables()
    options		Array of options

  NOTES
    We will initialize the value that is pointed to by options->value.
    If the value is of type GET_ASK_ADDR, we will also ask for the address
    for a value and initialize.
*/

static void init_variables(const struct my_option *options)
{
  for (; options->name; options++)
  {
    gptr *variable;
    /*
      We must set u_max_value first as for some variables
      options->u_max_value == options->value and in this case we want to
      set the value to default value.
    */
    if (options->u_max_value)
      init_one_value(options, options->u_max_value, options->max_value);
    if (options->value)
      init_one_value(options, options->value, options->def_value);
    if (options->var_type & GET_ASK_ADDR &&
	(variable= (*getopt_get_addr)("", 0, options)))
      init_one_value(options, variable, options->def_value);
  }
}


/*
  function: my_print_options

  Print help for all options and variables.
*/

#include <help_start.h>

void my_print_help(const struct my_option *options)
{
  uint col, name_space= 22, comment_space= 57;
  const char *line_end;
  const struct my_option *optp;

  for (optp= options; optp->id; optp++)
  {
    if (optp->id < 256)
    {
      printf("  -%c%s", optp->id, strlen(optp->name) ? ", " : "  ");
      col= 6;
    }
    else
    {
      printf("  ");
      col= 2;
    }
    if (strlen(optp->name))
    {
      printf("--%s", optp->name);
      col+= 2 + (uint) strlen(optp->name);
      if ((optp->var_type & GET_TYPE_MASK) == GET_STR ||
	  (optp->var_type & GET_TYPE_MASK) == GET_STR_ALLOC)
      {
	printf("%s=name%s ", optp->arg_type == OPT_ARG ? "[" : "",
	       optp->arg_type == OPT_ARG ? "]" : "");
	col+= (optp->arg_type == OPT_ARG) ? 8 : 6;
      }
      else if ((optp->var_type & GET_TYPE_MASK) == GET_NO_ARG ||
	       (optp->var_type & GET_TYPE_MASK) == GET_BOOL)
      {
	putchar(' ');
	col++;
      }
      else
      {
	printf("%s=#%s ", optp->arg_type == OPT_ARG ? "[" : "",
	       optp->arg_type == OPT_ARG ? "]" : "");
	col+= (optp->arg_type == OPT_ARG) ? 5 : 3;
      }
      if (col > name_space && optp->comment && *optp->comment)
      {
	putchar('\n');
	col= 0;
      }
    }
    for (; col < name_space; col++)
      putchar(' ');
    if (optp->comment && *optp->comment)
    {
      const char *comment= optp->comment, *end= strend(comment);

      while ((uint) (end - comment) > comment_space)
      {
	for (line_end= comment + comment_space; *line_end != ' '; line_end--);
	for (; comment != line_end; comment++)
	  putchar(*comment);
	comment++; /* skip the space, as a newline will take it's place now */
	putchar('\n');
	for (col= 0; col < name_space; col++)
	  putchar(' ');
      }
      printf("%s", comment);
    }
    putchar('\n');
  }
}


/*
  function: my_print_options

  Print variables.
*/

void my_print_variables(const struct my_option *options)
{
  uint name_space= 34, length;
  char buff[255];
  const struct my_option *optp;

  printf("\nVariables (--variable-name=value)\n");
  printf("and boolean options {FALSE|TRUE}  Value (after reading options)\n");
  printf("--------------------------------- -----------------------------\n");
  for (optp= options; optp->id; optp++)
  {
    gptr *value= (optp->var_type & GET_ASK_ADDR ?
		  (*getopt_get_addr)("", 0, optp) : optp->value);
    if (value)
    {
      printf("%s", optp->name);
      length= (uint) strlen(optp->name);
      for (; length < name_space; length++)
	putchar(' ');
      switch ((optp->var_type & GET_TYPE_MASK)) {
      case GET_STR:
      case GET_STR_ALLOC:                    /* fall through */
	printf("%s\n", *((char**) value) ? *((char**) value) :
	       "(No default value)");
	break;
      case GET_BOOL:
	printf("%s\n", *((my_bool*) value) ? "TRUE" : "FALSE");
	break;
      case GET_INT:
	printf("%d\n", *((int*) value));
	break;
      case GET_UINT:
	printf("%d\n", *((uint*) value));
	break;
      case GET_LONG:
	printf("%lu\n", *((long*) value));
	break;
      case GET_ULONG:
	printf("%lu\n", *((ulong*) value));
	break;
      case GET_LL:
	printf("%s\n", llstr(*((longlong*) value), buff));
	break;
      case GET_ULL:
	longlong2str(*((ulonglong*) value), buff, 10);
	printf("%s\n", buff);
	break;
      default:
	printf("(Disabled)\n");
	break;
      }
    }
  }
}

#include <help_end.h>
