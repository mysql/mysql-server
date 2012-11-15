/* Copyright (c) 2002, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include <my_global.h>
#include <m_string.h>
#include <stdlib.h>
#include <mysys_err.h>
#include <my_getopt.h>
#include <errno.h>
#include <m_string.h>
#include "my_default.h"
#include <m_ctype.h>

typedef void (*init_func_p)(const struct my_option *option, void *variable,
                            longlong value);

static void default_reporter(enum loglevel level, const char *format, ...);
my_error_reporter my_getopt_error_reporter= &default_reporter;

static int findopt(char *, uint, const struct my_option **, const char **);
my_bool getopt_compare_strings(const char *, const char *, uint);
static longlong getopt_ll(char *arg, const struct my_option *optp, int *err);
static ulonglong getopt_ull(char *, const struct my_option *, int *);
static double getopt_double(char *arg, const struct my_option *optp, int *err);
static void init_variables(const struct my_option *, init_func_p);
static void init_one_value(const struct my_option *, void *, longlong);
static void fini_one_value(const struct my_option *, void *, longlong);
static int setval(const struct my_option *, void *, char *, my_bool);
static char *check_struct_option(char *cur_arg, char *key_name);
static void print_cmdline_password_warning();

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
char *enabled_my_option= (char*) "1";

/* 
   This is a flag that can be set in client programs. 0 means that
   my_getopt will not print error messages, but the client should do
   it by itself
*/

my_bool my_getopt_print_errors= 1;

/* 
   This is a flag that can be set in client programs. 1 means that
   my_getopt will skip over options it does not know how to handle.
*/

my_bool my_getopt_skip_unknown= 0;

static void default_reporter(enum loglevel level,
                             const char *format, ...)
{
  va_list args;
  va_start(args, format);
  if (level == WARNING_LEVEL)
    fprintf(stderr, "%s", "Warning: ");
  else if (level == INFORMATION_LEVEL)
    fprintf(stderr, "%s", "Info: ");
  vfprintf(stderr, format, args);
  va_end(args);
  fputc('\n', stderr);
  fflush(stderr);
}

static my_getopt_value getopt_get_addr;

void my_getopt_register_get_addr(my_getopt_value func_addr)
{
  getopt_get_addr= func_addr;
}


/**
  Wrapper around my_handle_options() for interface compatibility.

  @param argc [in, out]      Command line options (count)
  @param argv [in, out]      Command line options (values)
  @param longopts [in]       Descriptor of all valid options
  @param get_one_option [in] Optional callback function to process each option,
                             can be NULL.

  @return Error in case of ambiguous or unknown options,
          0 on success.
*/
int handle_options(int *argc, char ***argv,
		   const struct my_option *longopts,
                   my_get_one_option get_one_option)
{
  return my_handle_options(argc, argv, longopts, get_one_option, NULL);
}

/**
  Handle command line options.
  Sort options.
  Put options first, until special end of options (--),
  or until the end of argv. Parse options, check that the given option
  matches with one of the options in struct 'my_option'.
  Check that option was given an argument if it requires one
  Call the optional 'get_one_option()' function once for each option.

  Note that handle_options() can be invoked multiple times to
  parse a command line in several steps.
  In this case, use the global flag @c my_getopt_skip_unknown to indicate
  that options unknown in the current step should be preserved in the
  command line for later parsing in subsequent steps.

  For 'long' options (--a_long_option), @c my_getopt_skip_unknown is
  fully supported. Command line parameters such as:
  - "--a_long_option"
  - "--a_long_option=value"
  - "--a_long_option value"
  will be preserved as is when the option is not known.

  For 'short' options (-S), support for @c my_getopt_skip_unknown
  comes with some limitation, because several short options
  can also be specified together in the same command line argument,
  as in "-XYZ".

  The first use case supported is: all short options are declared.
  handle_options() will be able to interpret "-XYZ" as one of:
  - an unknown X option
  - "-X -Y -Z", three short options with no arguments
  - "-X -YZ", where Y is a short option with argument Z
  - "-XYZ", where X is a short option with argument YZ
  based on the full short options specifications.

  The second use case supported is: no short option is declared.
  handle_options() will reject "-XYZ" as unknown, to be parsed later.

  The use case that is explicitly not supported is to provide
  only a partial list of short options to handle_options().
  This function can not be expected to extract some option Y
  in the middle of the string "-XYZ" in these conditions,
  without knowing if X will be declared an option later.

  Note that this limitation only impacts parsing of several
  short options from the same command line argument,
  as in "mysqld -anW5".
  When each short option is properly separated out in the command line
  argument, for example in "mysqld -a -n -w5", the code would actually
  work even with partial options specs given at each stage.

  @param [in, out] argc      command line options (count)
  @param [in, out] argv      command line options (values)
  @param [in] longopts       descriptor of all valid options
  @param [in] get_one_option optional callback function to process each option,
                             can be NULL.
  @param [in] command_list   NULL-terminated list of strings (commands) which
                             (if set) is looked up for all non-option strings
                             found while parsing the command line parameters.
                             The parsing terminates if a match is found. At
                             exit, argv [out] would contain all the remaining
                             unparsed options along with the matched command.

  @return error in case of ambiguous or unknown options,
          0 on success.
*/
int my_handle_options(int *argc, char ***argv,
                      const struct my_option *longopts,
                      my_get_one_option get_one_option,
                      const char **command_list)
{
  uint UNINIT_VAR(opt_found), argvpos= 0, length;
  my_bool end_of_options= 0, must_be_var, set_maximum_value,
          option_is_loose;
  char **pos, **pos_end, *optend, *opt_str, key_name[FN_REFLEN];
  const char *UNINIT_VAR(prev_found);
  const struct my_option *optp;
  void *value;
  int error, i;
  my_bool is_cmdline_arg= 1;

  /* handle_options() assumes arg0 (program name) always exists */
  DBUG_ASSERT(argc && *argc >= 1);
  DBUG_ASSERT(argv && *argv);
  (*argc)--; /* Skip the program name */
  (*argv)++; /*      --- || ----      */
  init_variables(longopts, init_one_value);

  /*
    Search for args_separator, if found, then the first part of the
    arguments are loaded from configs
  */
  for (pos= *argv, pos_end=pos+ *argc; pos != pos_end ; pos++)
  {
    if (my_getopt_is_args_separator(*pos))
    {
      is_cmdline_arg= 0;
      break;
    }
  }

  for (pos= *argv, pos_end=pos+ *argc; pos != pos_end ; pos++)
  {
    char **first= pos;
    char *cur_arg= *pos;
    opt_found= 0;
    if (!is_cmdline_arg && (my_getopt_is_args_separator(cur_arg)))
    {
      is_cmdline_arg= 1;

      /* save the separator too if skip unkown options  */
      if (my_getopt_skip_unknown)
        (*argv)[argvpos++]= cur_arg;
      else
        (*argc)--;
      continue;
    }
    if (cur_arg[0] == '-' && cur_arg[1] && !end_of_options) /* must be opt */
    {
      char *argument=    0;
      must_be_var=       0;
      set_maximum_value= 0;
      option_is_loose=   0;

      cur_arg++;		/* skip '-' */
      if (*cur_arg == '-')      /* check for long option, */
      {
        if (!*++cur_arg)	/* skip the double dash */
        {
          /* '--' means end of options, look no further */
          end_of_options= 1;
          (*argc)--;
          continue;
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
		opt_str+= special_opt_prefix_lengths[i] + 1;
                length-= special_opt_prefix_lengths[i] + 1;
		if (i == OPT_LOOSE)
		  option_is_loose= 1;
		if ((opt_found= findopt(opt_str, length, &optp, &prev_found)))
		{
		  if (opt_found > 1)
		  {
		    if (my_getopt_print_errors)
                      my_getopt_error_reporter(ERROR_LEVEL,
                                               "%s: ambiguous option '--%s-%s' (--%s-%s)",
                                               my_progname, special_opt_prefix[i],
                                               opt_str, special_opt_prefix[i],
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
		      enabled_my_option : disabled_my_option;
		    break;
		  case OPT_ENABLE:
		    optend= (optend && *optend == '0' && !(*(optend + 1))) ?
                      disabled_my_option : enabled_my_option;
		    break;
		  case OPT_MAXIMUM:
		    set_maximum_value= 1;
		    must_be_var= 1;
		    break;
		  }
		  break; /* break from the inner loop, main loop continues */
		}
                i= -1; /* restart the loop */
	      }
	    }
	  }
	  if (!opt_found)
	  {
            if (my_getopt_skip_unknown)
            {
              /* Preserve all the components of this unknown option. */
              do {
                (*argv)[argvpos++]= *first++;
              } while (first <= pos);
              continue;
            }
	    if (must_be_var)
	    {
	      if (my_getopt_print_errors)
                my_getopt_error_reporter(option_is_loose ? 
                                           WARNING_LEVEL : ERROR_LEVEL,
                                         "%s: unknown variable '%s'",
                                         my_progname, cur_arg);
	      if (!option_is_loose)
		return EXIT_UNKNOWN_VARIABLE;
	    }
	    else
	    {
	      if (my_getopt_print_errors)
                my_getopt_error_reporter(option_is_loose ? 
                                           WARNING_LEVEL : ERROR_LEVEL,
                                         "%s: unknown option '--%s'", 
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
                                       "%s: variable prefix '%s' is not unique",
                                       my_progname, opt_str);
	    return EXIT_VAR_PREFIX_NOT_UNIQUE;
	  }
	  else
	  {
	    if (my_getopt_print_errors)
              my_getopt_error_reporter(ERROR_LEVEL,
                                       "%s: ambiguous option '--%s' (%s, %s)",
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
        error= 0;
	value= optp->var_type & GET_ASK_ADDR ?
	  (*getopt_get_addr)(key_name, (uint) strlen(key_name), optp, &error) :
          optp->value;
        if (error)
          return error;

	if (optp->arg_type == NO_ARG)
	{
	  /*
	    Due to historical reasons GET_BOOL var_types still accepts arguments
	    despite the NO_ARG arg_type attribute. This can seems a bit unintuitive
	    and care should be taken when refactoring this code.
	  */
	  if (optend && (optp->var_type & GET_TYPE_MASK) != GET_BOOL)
	  {
	    if (my_getopt_print_errors)
              my_getopt_error_reporter(ERROR_LEVEL,
                                       "%s: option '--%s' cannot take an argument",
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
				       "%s: ignoring option '--%s' "
                                       "due to invalid value '%s'",
				       my_progname, optp->name, optend);
	      continue;
	    }
            if (get_one_option && get_one_option(optp->id, optp,
                               *((my_bool*) value) ?
                               enabled_my_option : disabled_my_option))
              return EXIT_ARGUMENT_INVALID;
	    continue;
	  }
	  argument= optend;
	}
	else if (optp->arg_type == REQUIRED_ARG && !optend)
	{
	  /* Check if there are more arguments after this one,
       Note: options loaded from config file that requires value
       should always be in the form '--option=value'.
    */
	  if (!is_cmdline_arg || !*++pos)
	  {
	    if (my_getopt_print_errors)
              my_getopt_error_reporter(ERROR_LEVEL,
                                       "%s: option '--%s' requires an argument",
                                       my_progname, optp->name);
	    return EXIT_ARGUMENT_REQUIRED;
	  }
	  argument= *pos;
	  (*argc)--;
	}
	else
	  argument= optend;

        if (optp->var_type == GET_PASSWORD && is_cmdline_arg && argument)
          print_cmdline_password_warning();
      }
      else  /* must be short option */
      {
	for (optend= cur_arg; *optend; optend++)
	{
	  opt_found= 0;
	  for (optp= longopts; optp->name; optp++)
	  {
	    if (optp->id && optp->id == (int) (uchar) *optend)
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
                if (get_one_option && get_one_option(optp->id, optp, argument))
                  return EXIT_UNSPECIFIED_ERROR;
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
                  if (optp->var_type == GET_PASSWORD && is_cmdline_arg)
                    print_cmdline_password_warning();
		}
		else
		{
                  if (optp->arg_type == OPT_ARG)
                  {
                    if (optp->var_type == GET_BOOL)
                      *((my_bool*) optp->value)= (my_bool) 1;
                    if (get_one_option && get_one_option(optp->id, optp, argument))
                      return EXIT_UNSPECIFIED_ERROR;
                    continue;
                  }
		  /* Check if there are more arguments after this one */
		  if (!pos[1])
		  {
                    if (my_getopt_print_errors)
                      my_getopt_error_reporter(ERROR_LEVEL,
                                               "%s: option '-%c' requires an argument",
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
		return error;
              if (get_one_option && get_one_option(optp->id, optp, argument))
                return EXIT_UNSPECIFIED_ERROR;
	      break;
	    }
	  }
	  if (!opt_found)
	  {
            if (my_getopt_skip_unknown)
            {
              /*
                We are currently parsing a single argv[] argument
                of the form "-XYZ".
                One or the argument found (say Y) is not an option.
                Hack the string "-XYZ" to make a "-YZ" substring in it,
                and push that to the output as an unrecognized parameter.
              */
              DBUG_ASSERT(optend > *pos);
              DBUG_ASSERT(optend >= cur_arg);
              DBUG_ASSERT(optend <= *pos + strlen(*pos));
              DBUG_ASSERT(*optend);
              optend--;
              optend[0]= '-'; /* replace 'X' or '-' by '-' */
              (*argv)[argvpos++]= optend;
              /*
                Do not continue to parse at the current "-XYZ" argument,
                skip to the next argv[] argument instead.
              */
              optend= (char*) " ";
            }
            else
            {
              if (my_getopt_print_errors)
                my_getopt_error_reporter(ERROR_LEVEL,
                                         "%s: unknown option '-%c'",
                                         my_progname, *optend);
              return EXIT_UNKNOWN_OPTION;
            }
	  }
	}
        if (opt_found)
          (*argc)--; /* option handled (short), decrease argument count */
	continue;
      }
      if ((error= setval(optp, value, argument, set_maximum_value)))
	return error;
      if (get_one_option && get_one_option(optp->id, optp, argument))
        return EXIT_UNSPECIFIED_ERROR;

      (*argc)--; /* option handled (long), decrease argument count */
    }
    else /* non-option found */
    {
      if (command_list)
      {
        while (* command_list)
        {
          if (!strcmp(*command_list, cur_arg))
          {
            /* Match found. */
            (*argv)[argvpos ++]= cur_arg;

            /* Copy rest of the un-parsed elements & return. */
            while ((++ pos) != pos_end)
              (*argv)[argvpos ++]= *pos;
            goto done;
          }
          command_list ++;
        }
      }
      (*argv)[argvpos ++]= cur_arg;
    }
  }

done:
  /*
    Destroy the first, already handled option, so that programs that look
    for arguments in 'argv', without checking 'argc', know when to stop.
    Items in argv, before the destroyed one, are all non-option -arguments
    to the program, yet to be (possibly) handled.
  */
  (*argv)[argvpos]= 0;
  return 0;
}


/**
 * This function should be called to print a warning message
 * if password string is specified on the command line.
 */

static void print_cmdline_password_warning()
{
  static my_bool password_warning_announced= FALSE;

  if (!password_warning_announced)
  {
    fprintf(stderr, "Warning: Using a password on the command line "
            "interface can be insecure.\n");
    (void) fflush(stderr);
    password_warning_announced= TRUE;
  }
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


/**
   Parse a boolean command line argument

   "ON", "TRUE" and "1" will return true,
   other values will return false.

   @param[in] argument The value argument
   @return boolean value
*/
static my_bool get_bool_argument(const struct my_option *opts,
                                 const char *argument)
{
  if (!my_strcasecmp(&my_charset_latin1, argument, "true") ||
      !my_strcasecmp(&my_charset_latin1, argument, "on") ||
      !my_strcasecmp(&my_charset_latin1, argument, "1"))
    return 1;
  else if (!my_strcasecmp(&my_charset_latin1, argument, "false") ||
      !my_strcasecmp(&my_charset_latin1, argument, "off") ||
      !my_strcasecmp(&my_charset_latin1, argument, "0"))
    return 0;
  my_getopt_error_reporter(WARNING_LEVEL,
      "option '%s': boolean value '%s' wasn't recognized. Set to OFF.",
      opts->name, argument);
  return 0;
}

/*
  function: setval

  Arguments: opts, argument
  Will set the option value to given value
*/

static int setval(const struct my_option *opts, void *value, char *argument,
		  my_bool set_maximum_value)
{
  int err= 0, res= 0;

  if (!argument)
    argument= enabled_my_option;

  if (value)
  {
    if (set_maximum_value && !(value= opts->u_max_value))
    {
      my_getopt_error_reporter(ERROR_LEVEL,
                               "%s: Maximum value of '%s' cannot be set",
                               my_progname, opts->name);
      return EXIT_NO_PTR_TO_VARIABLE;
    }

    switch ((opts->var_type & GET_TYPE_MASK)) {
    case GET_BOOL: /* If argument differs from 0, enable option, else disable */
      *((my_bool*) value)= get_bool_argument(opts, argument);
      break;
    case GET_INT:
      *((int*) value)= (int) getopt_ll(argument, opts, &err);
      break;
    case GET_UINT:
      *((uint*) value)= (uint) getopt_ull(argument, opts, &err);
      break;
    case GET_LONG:
      *((long*) value)= (long) getopt_ll(argument, opts, &err);
      break;
    case GET_ULONG:
      *((long*) value)= (long) getopt_ull(argument, opts, &err);
      break;
    case GET_LL:
      *((longlong*) value)= getopt_ll(argument, opts, &err);
      break;
    case GET_ULL:
      *((ulonglong*) value)= getopt_ull(argument, opts, &err);
      break;
    case GET_DOUBLE:
      *((double*) value)= getopt_double(argument, opts, &err);
      break;
    case GET_STR:
    case GET_PASSWORD:
      if (argument == enabled_my_option)
        break; /* string options don't use this default of "1" */
      *((char**) value)= argument;
      break;
    case GET_STR_ALLOC:
      if (argument == enabled_my_option)
        break; /* string options don't use this default of "1" */
      my_free(*((char**) value));
      if (!(*((char**) value)= my_strdup(argument, MYF(MY_WME))))
      {
        res= EXIT_OUT_OF_MEMORY;
        goto ret;
      };
      break;
    case GET_ENUM:
      {
        int type= find_type(argument, opts->typelib, FIND_TYPE_BASIC);
        if (type == 0)
        {
          /*
            Accept an integer representation of the enumerated item.
          */
          char *endptr;
          ulong arg= strtoul(argument, &endptr, 10);
          if (*endptr || arg >= opts->typelib->count)
          {
            res= EXIT_ARGUMENT_INVALID;
            goto ret;
          }
          *(ulong*)value= arg;
        }
        else if (type < 0)
        {
          res= EXIT_AMBIGUOUS_OPTION;
          goto ret;
        }
        else
          *(ulong*)value= type - 1;
      }
      break;
    case GET_SET:
      *((ulonglong*)value)= find_typeset(argument, opts->typelib, &err);
      if (err)
      {
        /* Accept an integer representation of the set */
        char *endptr;
        ulonglong arg= (ulonglong) strtol(argument, &endptr, 10);
        if (*endptr || (arg >> 1) >= (1ULL << (opts->typelib->count-1)))
        {
          res= EXIT_ARGUMENT_INVALID;
          goto ret;
        };
        *(ulonglong*)value= arg;
        err= 0;
      }
      break;
    case GET_FLAGSET:
      {
        char *error;
        uint error_len;

        *((ulonglong*)value)=
              find_set_from_flags(opts->typelib, opts->typelib->count, 
                                  *(ulonglong *)value, opts->def_value,
                                  argument, strlen(argument),
                                  &error, &error_len);
        if (error)
        {
          res= EXIT_ARGUMENT_INVALID;
          goto ret;
        };
      }
      break;
    case GET_NO_ARG: /* get_one_option has taken care of the value already */
    default:         /* dummy default to avoid compiler warnings */
      break;
    }
    if (err)
    {
      res= EXIT_UNKNOWN_SUFFIX;
      goto ret;
    };
  }
  return 0;

ret:
  my_getopt_error_reporter(ERROR_LEVEL,
                           "%s: Error while setting value '%s' to '%s'",
                           my_progname, argument, opts->name);
  return res;
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
		   const char **ffname)
{
  uint count;
  const struct my_option *opt= *opt_res;

  for (count= 0; opt->name; opt++)
  {
    if (!getopt_compare_strings(opt->name, optpat, length)) /* match found */
    {
      (*opt_res)= opt;
      if (!opt->name[length])		/* Exact match */
	return 1;
      if (!count)
      {
        /* We only need to know one prev */
	count= 1;
	*ffname= opt->name;
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

my_bool getopt_compare_strings(const char *s, const char *t,
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

static longlong eval_num_suffix(char *argument, int *error, char *option_name)
{
  char *endchar;
  longlong num;
  
  *error= 0;
  errno= 0;
  num= strtoll(argument, &endchar, 10);
  if (errno == ERANGE)
  {
    my_getopt_error_reporter(ERROR_LEVEL,
                             "Incorrect integer value: '%s'", argument);
    *error= 1;
    return 0;
  }
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
  longlong num=eval_num_suffix(arg, err, (char*) optp->name);
  return getopt_ll_limit_value(num, optp, NULL);
}


/**
  Maximum possible value for an integer GET_* variable type
  @param  var_type  type of integer variable (GET_*)
  @returns  maximum possible value for this type
 */
ulonglong max_of_int_range(int var_type)
{
  switch (var_type)
  {
  case GET_INT:
    return INT_MAX;
  case GET_LONG:
    return LONG_MAX;
  case GET_LL:
    return LONGLONG_MAX;
  case GET_UINT:
    return UINT_MAX;
  case GET_ULONG:
    return ULONG_MAX;
  case GET_ULL:
    return ULONGLONG_MAX;
  default:
    DBUG_ASSERT(0);
    return 0;
  }
}


/*
  function: getopt_ll_limit_value

  Applies min/max/block_size to a numeric value of an option.
  Returns "fixed" value.
*/

longlong getopt_ll_limit_value(longlong num, const struct my_option *optp,
                               my_bool *fix)
{
  longlong old= num;
  my_bool adjusted= FALSE;
  char buf1[255], buf2[255];
  ulonglong block_size= (optp->block_size ? (ulonglong) optp->block_size : 1L);
  const longlong max_of_type=
    (longlong)max_of_int_range(optp->var_type & GET_TYPE_MASK);

  if (num > 0 && ((ulonglong) num > (ulonglong) optp->max_value) &&
      optp->max_value) /* if max value is not set -> no upper limit */
  {
    num= (ulonglong) optp->max_value;
    adjusted= TRUE;
  }

  if (num > max_of_type)
  {
    num= max_of_type;
    adjusted= TRUE;
  }

  num= (num / block_size);
  num= (longlong) (num * block_size);

  if (num < optp->min_value)
  {
    num= optp->min_value;
    if (old < optp->min_value)
      adjusted= TRUE;
  }

  if (fix)
    *fix= old != num;
  else if (adjusted)
    my_getopt_error_reporter(WARNING_LEVEL,
                             "option '%s': signed value %s adjusted to %s",
                             optp->name, llstr(old, buf1), llstr(num, buf2));
  return num;
}

static inline my_bool is_negative_num(char* num)
{
  while (my_isspace(&my_charset_latin1, *num)) 
    num++;
  
  return (*num == '-');
}

/*
  function: getopt_ull

  This is the same as getopt_ll, but is meant for unsigned long long
  values.
*/

static ulonglong getopt_ull(char *arg, const struct my_option *optp, int *err)
{
  ulonglong num;

  /*
    Bug #14683107
    eval_num_suffix uses strtoll, strtoull also seems to have the same
    behaviour return 0xffffffffffffffff irrespective of the signedness
    specification. Hence '-' is checked in the input and num is set to 0 if
    it is present to force it to the lowest possible unsigned value.
  */
  if (arg == NULL || is_negative_num(arg) == TRUE)
    num= 0;
  else
    num= eval_num_suffix(arg, err, (char*) optp->name);
  
  return getopt_ull_limit_value(num, optp, NULL);
}


ulonglong getopt_ull_limit_value(ulonglong num, const struct my_option *optp,
                                 my_bool *fix)
{
  my_bool adjusted= FALSE;
  ulonglong old= num;
  char buf1[255], buf2[255];
  const ulonglong max_of_type=
    max_of_int_range(optp->var_type & GET_TYPE_MASK);

  if ((ulonglong) num > (ulonglong) optp->max_value &&
      optp->max_value) /* if max value is not set -> no upper limit */
  {
    num= (ulonglong) optp->max_value;
    adjusted= TRUE;
  }

  if (num > max_of_type)
  {
    num= max_of_type;
    adjusted= TRUE;
  }

  if (optp->block_size > 1)
  {
    num/= (ulonglong) optp->block_size;
    num*= (ulonglong) optp->block_size;
  }

  if (num < (ulonglong) optp->min_value)
  {
    num= (ulonglong) optp->min_value;
    if (old < (ulonglong) optp->min_value)
      adjusted= TRUE;
  }

  if (fix)
    *fix= old != num;
  else if (adjusted)
    my_getopt_error_reporter(WARNING_LEVEL,
                             "option '%s': unsigned value %s adjusted to %s",
                             optp->name, ullstr(old, buf1), ullstr(num, buf2));

  return num;
}

double getopt_double_limit_value(double num, const struct my_option *optp,
                                 my_bool *fix)
{
  my_bool adjusted= FALSE;
  double old= num;
  if (optp->max_value && num > (double) optp->max_value)
  {
    num= (double) optp->max_value;
    adjusted= TRUE;
  }
  if (num < (double) optp->min_value)
  {
    num= (double) optp->min_value;
    adjusted= TRUE;
  }
  if (fix)
    *fix= adjusted;
  else if (adjusted)
    my_getopt_error_reporter(WARNING_LEVEL,
                             "option '%s': value %g adjusted to %g",
                             optp->name, old, num);
  return num;
}

/*
  Get double value withing ranges

  Evaluates and returns the value that user gave as an argument to a variable.

  RETURN
    decimal value of arg

    In case of an error, prints an error message and sets *err to
    EXIT_ARGUMENT_INVALID.  Otherwise err is not touched
*/

static double getopt_double(char *arg, const struct my_option *optp, int *err)
{
  double num;
  int error;
  char *end= arg + 1000;                     /* Big enough as *arg is \0 terminated */
  num= my_strtod(arg, &end, &error);
  if (end[0] != 0 || error)
  {
    my_getopt_error_reporter(ERROR_LEVEL,
            "Invalid decimal value for option '%s'\n", optp->name);
    *err= EXIT_ARGUMENT_INVALID;
    return 0.0;
  }
  return getopt_double_limit_value(num, optp, NULL);
}

/*
  Init one value to it's default values

  SYNOPSIS
    init_one_value()
    option		Option to initialize
    value		Pointer to variable
*/

static void init_one_value(const struct my_option *option, void *variable,
			   longlong value)
{
  DBUG_ENTER("init_one_value");
  switch ((option->var_type & GET_TYPE_MASK)) {
  case GET_BOOL:
    *((my_bool*) variable)= (my_bool) value;
    break;
  case GET_INT:
    *((int*) variable)= (int) getopt_ll_limit_value((int) value, option, NULL);
    break;
  case GET_ENUM:
    *((ulong*) variable)= (ulong) value;
    break;
  case GET_UINT:
    *((uint*) variable)= (uint) getopt_ull_limit_value((uint) value, option, NULL);
    break;
  case GET_LONG:
    *((long*) variable)= (long) getopt_ll_limit_value((long) value, option, NULL);
    break;
  case GET_ULONG:
    *((ulong*) variable)= (ulong) getopt_ull_limit_value((ulong) value, option, NULL);
    break;
  case GET_LL:
    *((longlong*) variable)= (longlong) getopt_ll_limit_value((longlong) value, option, NULL);
    break;
  case GET_ULL:
    *((ulonglong*) variable)= (ulonglong) getopt_ull_limit_value((ulonglong) value, option, NULL);
    break;
  case GET_SET:
  case GET_FLAGSET:
    *((ulonglong*) variable)= (ulonglong) value;
    break;
  case GET_DOUBLE:
    *((double*) variable)= ulonglong2double(value);
    break;
  case GET_STR:
  case GET_PASSWORD:
    /*
      Do not clear variable value if it has no default value.
      The default value may already be set.
      NOTE: To avoid compiler warnings, we first cast longlong to intptr,
      so that the value has the same size as a pointer.
    */
    if ((char*) (intptr) value)
      *((char**) variable)= (char*) (intptr) value;
    break;
  case GET_STR_ALLOC:
    /*
      Do not clear variable value if it has no default value.
      The default value may already be set.
      NOTE: To avoid compiler warnings, we first cast longlong to intptr,
      so that the value has the same size as a pointer.
    */
    if ((char*) (intptr) value)
    {
      char **pstr= (char **) variable;
      my_free(*pstr);
      *pstr= my_strdup((char*) (intptr) value, MYF(MY_WME));
    }
    break;
  default: /* dummy default to avoid compiler warnings */
    break;
  }
  DBUG_VOID_RETURN;
}


/*
  Init one value to it's default values

  SYNOPSIS
    init_one_value()
    option		Option to initialize
    value		Pointer to variable
*/

static void fini_one_value(const struct my_option *option, void *variable,
			   longlong value __attribute__ ((unused)))
{
  DBUG_ENTER("fini_one_value");
  switch ((option->var_type & GET_TYPE_MASK)) {
  case GET_STR_ALLOC:
    my_free(*((char**) variable));
    *((char**) variable)= NULL;
    break;
  default: /* dummy default to avoid compiler warnings */
    break;
  }
  DBUG_VOID_RETURN;
}


void my_cleanup_options(const struct my_option *options)
{
  init_variables(options, fini_one_value);
}


/* 
  initialize all variables to their default values

  SYNOPSIS
    init_variables()
    options		Array of options

  NOTES
    We will initialize the value that is pointed to by options->value.
    If the value is of type GET_ASK_ADDR, we will ask for the address
    for a value and initialize.
*/

static void init_variables(const struct my_option *options,
                           init_func_p init_one_value)
{
  DBUG_ENTER("init_variables");
  for (; options->name; options++)
  {
    void *value;
    DBUG_PRINT("options", ("name: '%s'", options->name));
    /*
      We must set u_max_value first as for some variables
      options->u_max_value == options->value and in this case we want to
      set the value to default value.
    */
    if (options->u_max_value)
      init_one_value(options, options->u_max_value, options->max_value);
    value= (options->var_type & GET_ASK_ADDR ?
		  (*getopt_get_addr)("", 0, options, 0) : options->value);
    if (value)
      init_one_value(options, value, options->def_value);
  }
  DBUG_VOID_RETURN;
}

/** Prints variable or option name, replacing _ with - */
static uint print_name(const struct my_option *optp)
{
  const char *s= optp->name;
  for (;*s;s++)
    putchar(*s == '_' ? '-' : *s);
  return s - optp->name;
}

/*
  function: my_print_options

  Print help for all options and variables.
*/

void my_print_help(const struct my_option *options)
{
  uint col, name_space= 22, comment_space= 57;
  const char *line_end;
  const struct my_option *optp;

  for (optp= options; optp->name; optp++)
  {
    if (optp->id && optp->id < 256)
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
      printf("--");
      col+= 2 + print_name(optp);
      if (optp->arg_type == NO_ARG ||
	  (optp->var_type & GET_TYPE_MASK) == GET_BOOL)
      {
	putchar(' ');
	col++;
      }
      else if ((optp->var_type & GET_TYPE_MASK) == GET_STR       ||
               (optp->var_type & GET_TYPE_MASK) == GET_PASSWORD  ||
               (optp->var_type & GET_TYPE_MASK) == GET_STR_ALLOC ||
               (optp->var_type & GET_TYPE_MASK) == GET_ENUM      ||
               (optp->var_type & GET_TYPE_MASK) == GET_SET       ||
               (optp->var_type & GET_TYPE_MASK) == GET_FLAGSET    )
      {
	printf("%s=name%s ", optp->arg_type == OPT_ARG ? "[" : "",
	       optp->arg_type == OPT_ARG ? "]" : "");
	col+= (optp->arg_type == OPT_ARG) ? 8 : 6;
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
	for (line_end= comment + comment_space; *line_end != ' '; line_end--)
        {}
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
    if ((optp->var_type & GET_TYPE_MASK) == GET_BOOL)
    {
      if (optp->def_value != 0)
      {
        printf("%*s(Defaults to on; use --skip-", name_space, "");
        print_name(optp);
        printf(" to disable.)\n");
      }
    }
  }
}


/*
  function: my_print_options

  Print variables.
*/

void my_print_variables(const struct my_option *options)
{
  uint name_space= 34, length, nr;
  ulonglong llvalue;
  char buff[255];
  const struct my_option *optp;

  for (optp= options; optp->name; optp++)
  {
    length= strlen(optp->name)+1;
    if (length > name_space)
      name_space= length;
  }

  printf("\nVariables (--variable-name=value)\n");
  printf("%-*s%s", name_space, "and boolean options {FALSE|TRUE}",
         "Value (after reading options)\n");
  for (length=1; length < 75; length++)
    putchar(length == name_space ? ' ' : '-');
  putchar('\n');
  
  for (optp= options; optp->name; optp++)
  {
    void *value= (optp->var_type & GET_ASK_ADDR ?
		  (*getopt_get_addr)("", 0, optp, 0) : optp->value);
    if (value)
    {
      length= print_name(optp);
      for (; length < name_space; length++)
	putchar(' ');
      switch ((optp->var_type & GET_TYPE_MASK)) {
      case GET_SET:
        if (!(llvalue= *(ulonglong*) value))
	  printf("%s\n", "");
	else
        for (nr= 0; llvalue && nr < optp->typelib->count; nr++, llvalue >>=1)
	{
	  if (llvalue & 1)
            printf( llvalue > 1 ? "%s," : "%s\n", get_type(optp->typelib, nr));
	}
	break;
      case GET_FLAGSET:
        llvalue= *(ulonglong*) value;
        for (nr= 0; llvalue && nr < optp->typelib->count; nr++, llvalue >>=1)
	{
          printf("%s%s=", (nr ? "," : ""), get_type(optp->typelib, nr));
	  printf(llvalue & 1 ? "on" : "off");
	}
        printf("\n");
	break;
      case GET_ENUM:
        printf("%s\n", get_type(optp->typelib, *(ulong*) value));
	break;
      case GET_STR:
      case GET_PASSWORD:
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
	printf("%ld\n", *((long*) value));
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
      case GET_DOUBLE:
	printf("%g\n", *(double*) value);
	break;
      case GET_NO_ARG:
	printf("(No default value)\n");
	break;
      default:
	printf("(Disabled)\n");
	break;
      }
    }
  }
}
