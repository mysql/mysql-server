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

#include <my_config.h>
#include <my_global.h>
#include <m_string.h>
#include <stdlib.h>
#include <my_getopt.h>
#include <assert.h>

static int findopt (char *optpat, uint length,
		    const struct my_option **opt_res,
		    char **ffname);
static my_bool compare_strings (register const char *s, register const char *t,
				uint length);
static longlong getopt_ll (char *arg, const struct my_option *optp, int *err);
static void init_variables(const struct my_option *options);


#define DISABLE_OPTION_COUNT      2

static const char *special_opt_prefix[]=
{"skip", "disable", "enable", "maximum", 0};


/* Return error values from handle_options */

#define ERR_UNKNOWN_OPTION        1
#define ERR_AMBIGUOUS_OPTION      2
#define ERR_NO_ARGUMENT_ALLOWED   3
#define ERR_ARGUMENT_REQUIRED     4
#define ERR_VAR_PREFIX_NOT_UNIQUE 5
#define ERR_UNKNOWN_VARIABLE      6
#define ERR_MUST_BE_VARIABLE      7
#define ERR_UNKNOWN_SUFFIX        8
#define ERR_NO_PTR_TO_VARIABLE	  9


/* 
  function: handle_options

  Sort options; put options first, until special end of options (--), or
  until end of argv. Parse options; check that the given option matches with
  one of the options in struct 'my_option', return error in case of ambiguous
  or unknown option. Check that option was given an argument if it requires
  one. Call function 'get_one_option()' once for each option.
*/

int handle_options(int *argc, char ***argv, 
		   const struct my_option *longopts, 
		   my_bool (*get_one_option)(int,
					     const struct my_option *,
					     char *))
{
  uint opt_found, argvpos= 0, length, spec_len, i;
  int err;
  my_bool end_of_options= 0, must_be_var, set_maximum_value, special_used;
  char *progname= *(*argv), **pos, *optend, *prev_found;
  const struct my_option *optp;

  LINT_INIT(opt_found);
  (*argc)--; /* Skip the program name */
  (*argv)++; /*      --- || ----      */
  init_variables(longopts);
  for (pos= *argv; *pos; pos++)
  {
    char *cur_arg= *pos;
    if (cur_arg[0] == '-' && cur_arg[1] && !end_of_options) /* must be opt */
    {
      char *argument=    0;
      must_be_var=       0;
      set_maximum_value= 0;
      special_used=      0;

      cur_arg++;		/* skip '-' */
      if (*cur_arg == 'O')
      {
	must_be_var= 1;

	if (!(*++cur_arg))	/* If not -Ovar=# */
	{
	  /* the argument must be in next argv */
	  if (!*++pos)
	  {
	    fprintf(stderr, "%s: Option '-O' requires an argument\n",
		    progname);
	    return ERR_ARGUMENT_REQUIRED;
	  }
	  cur_arg= *pos;
	  (*argc)--;
	}
      }
      else if (*cur_arg == '-') /* check for long option, or --set-variable */
      {
	if (!compare_strings(cur_arg, "-set-variable", 13))
	{
	  must_be_var= 1;
	  if (cur_arg[13] == '=')
	  {
	    cur_arg+= 14;
	    if (!*cur_arg)
	    {
	      fprintf(stderr,
		      "%s: Option '--set-variable' requires an argument\n",
		      progname);
	      return ERR_ARGUMENT_REQUIRED;
	    }
	  }
	  else if (cur_arg[14]) /* garbage, or another option. break out */
	    must_be_var= 0;
	  else
	  {
	    /* the argument must be in next argv */
	    if (!*++pos)
	    {
	      fprintf(stderr,
		      "%s: Option '--set-variable' requires an argument\n",
		      progname);
	      return ERR_ARGUMENT_REQUIRED;
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
	optend= strcend(cur_arg, '=');
	length= optend - cur_arg;
	if (*optend == '=')
	  optend++;
	else
	  optend=0;

	/*
	  Find first the right option. Return error in case of an ambiguous,
	  or unknown option
	*/
	optp= longopts;
	if (!(opt_found= findopt(cur_arg, length, &optp, &prev_found)))
	{
	  /*
	    Didn't find any matching option. Let's see if someone called
	    option with a special option prefix
	  */
	  if (!must_be_var)
	  {
	    if (optend)
	      must_be_var= 1;
	    for (i= 0; special_opt_prefix[i]; i++)
	    {
	      spec_len= strlen(special_opt_prefix[i]);
	      if (!compare_strings(special_opt_prefix[i], cur_arg, spec_len) &&
		  cur_arg[spec_len] == '-')
	      {
		/*
		  We were called with a special prefix, we can reuse opt_found
		*/
		special_used= 1;
		cur_arg += (spec_len + 1);
		if ((opt_found= findopt(cur_arg, length - (spec_len + 1),
					&optp, &prev_found)))
		{
		  if (opt_found > 1)
		  {
		    fprintf(stderr,
			    "%s: ambiguous option '--%s-%s' (--%s-%s)\n",
			    progname, special_opt_prefix[i], cur_arg,
			    special_opt_prefix[i], prev_found);
		    return ERR_AMBIGUOUS_OPTION;
		  }
		  if (i < DISABLE_OPTION_COUNT)
		    optend= (char*) "0";
		  else if (!compare_strings(special_opt_prefix[i],"enable",6))
		    optend= (char*) "1";
		  else if (!compare_strings(special_opt_prefix[i],"maximum",7))
		  {
		    set_maximum_value= 1;
		    must_be_var= 1;
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
	      fprintf(stderr,
		      "%s: unknown variable '%s'\n", progname, cur_arg);
	      return ERR_UNKNOWN_VARIABLE;
	    }
	    else
	    {
	      fprintf(stderr,
		      "%s: unknown option '--%s'\n", progname, cur_arg);
	      return ERR_UNKNOWN_OPTION;
	    }
	  }
	}
	if (opt_found > 1)
	{
	  if (must_be_var)
	  {
	    fprintf(stderr, "%s: variable prefix '%s' is not unique\n",
		    progname, cur_arg);
	    return ERR_VAR_PREFIX_NOT_UNIQUE;
	  }
	  else
	  {
	    fprintf(stderr, "%s: ambiguous option '--%s' (%s, %s)\n",
		    progname, cur_arg, prev_found, optp->name);
	    return ERR_AMBIGUOUS_OPTION;
	  }
	}
	if (must_be_var && !optp->value)
	{
	  fprintf(stderr, "%s: the argument '%s' is not an variable\n",
		  progname, *pos);
	  return ERR_MUST_BE_VARIABLE;
	}
	if (optp->arg_type == NO_ARG && optend && !special_used)
	{
	  fprintf(stderr, "%s: option '--%s' cannot take an argument\n",
		  progname, optp->name);
	  return ERR_NO_ARGUMENT_ALLOWED;
	}
	else if (optp->arg_type == REQUIRED_ARG && !optend)
	{
	  /* Check if there are more arguments after this one */
	  if (!*++pos)
	  {
	    fprintf(stderr, "%s: option '--%s' requires an argument\n",
		    progname, optp->name);
	    return ERR_ARGUMENT_REQUIRED;
	  }
	  argument= *pos;
	  (*argc)--;
	}
	else
	  argument= optend;
      }
      else  /* must be short option */
      {
	for (optend= cur_arg; *optend; optend++, opt_found= 0)
	{
	  for (optp= longopts; optp->id; optp++)
	  {
	    if (optp->id == (int) (uchar) *optend)
	    {
	      /* Option recognized. Find next what to do with it */
	      opt_found= 1;
	      if (optp->arg_type == REQUIRED_ARG || optp->arg_type == OPT_ARG)
	      {
		if (*(optend + 1))
		{
		  // The rest of the option is option argument
		  argument= optend + 1;
		  // This is in effect a jump out of the outer loop
		  optend= (char*) " ";
		}
		else if (optp->arg_type == REQUIRED_ARG)
		{
		  /* Check if there are more arguments after this one */
		  if (!*++pos)
		  {
		    fprintf(stderr, "%s: option '-%c' requires an argument\n",
			    progname, optp->id);
		    return ERR_ARGUMENT_REQUIRED;
		  }
		  argument= *pos;
		  (*argc)--;
		  /* the other loop will break, because *optend + 1 == 0 */
		}
	      }
	      get_one_option(optp->id, optp, argument);
	      break;
	    }
	  }
	  if (!opt_found)
	  {
	    fprintf(stderr,
		    "%s: unknown option '-%c'\n", progname, *cur_arg);
	    return ERR_UNKNOWN_OPTION;
	  }
	}
	(*argc)--; /* option handled (short), decrease argument count */
	continue;
      }
      if (optp->value)
      {
	gptr *result_pos= (set_maximum_value) ?
	  optp->u_max_value : optp->value;
	if (!result_pos)
	{
	  fprintf(stderr,
		  "%s: Can't set a value for %s\n", progname, optp->name);
	  return ERR_NO_PTR_TO_VARIABLE;
	}
	if (optp->var_type == GET_LONG)
	 *((long*) result_pos)= (long) getopt_ll(argument, optp, &err);
	else if (optp->var_type == GET_LL)
	  *((longlong*) result_pos)=   getopt_ll(argument, optp, &err);
	else if (optp->var_type == GET_STR)
	  *((char**) result_pos)= argument;
	if (err)
	  return ERR_UNKNOWN_SUFFIX;
      }
      else
	get_one_option(optp->id, optp, argument);

      (*argc)--; /* option handled (short or long), decrease argument count */
    }
    else /* non-option found */
      (*argv)[argvpos++]= cur_arg;
  }
  return 0;
}


/* 
  function: findopt

  Arguments: opt_pattern, length of opt_pattern, opt_struct, first found
  name (ffname)

  Go through all options in the my_option struct. Return number
  of options found that match the pattern and in the argument
  list the option found, if any. In case of ambiguous option, store
  the name in ffname argument
*/

static int findopt (char *optpat, uint length,
		    const struct my_option **opt_res,
		    char **ffname)
{
  int count;
  struct my_option *opt= (struct my_option *) *opt_res;

  for (count= 0; opt->name; opt++)
  {
    if (!compare_strings(opt->name, optpat, length)) /* match found */
    {
      (*opt_res)= opt;
      if (!count)
	*ffname= (char *) opt->name;     /* we only need to know one prev */
      if (length == strlen(opt->name))   /* exact match */
	return 1;
      count++;
    }
  }
  return count;
}


/* 
  function: compare_strings

  Works like strncmp, other than 1.) considers '-' and '_' the same.
  2.) Returns -1 if strings differ, 0 if they are equal
*/

static my_bool compare_strings(register const char *s, register const char *t,
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
  function: getopt_ll

  Evaluates and returns the value that user gave as an argument
  to a variable. Recognizes (case insensitive) K as KILO, M as MEGA
  and G as GIGA bytes. Some values must be in certain blocks, as
  defined in the given my_option struct, this function will check
  that those values are honored.
  In case of an error, set error value in *err.
*/

static longlong getopt_ll (char *arg, const struct my_option *optp, int *err)
{
  char *endchar;
  longlong num;
  
  *err= 0;
  num= strtoll(arg, &endchar, 10);
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
	    *endchar, optp->name, arg);
    *err= 1;
  }
  if (num < (longlong) optp->min_value)
    num= (longlong) optp->min_value;
  else if (num > 0 && (ulonglong) num > (ulonglong) (ulong) optp->max_value
	   && optp->max_value) // if max value is not set -> no upper limit
    num= (longlong) (ulong) optp->max_value;
  num= ((num - (longlong) optp->sub_size) / (optp->block_size ?
					     (ulonglong) optp->block_size :
					     1L));
  return (longlong) (num * (optp->block_size ? (ulonglong) optp->block_size :
			    1L));
}


/* 
  function: init_variables

  initialize all variables to their default values
*/

static void init_variables(const struct my_option *options)
{
  for (; options->name; options++)
  {
    if (options->value)
    {
      if (options->var_type == GET_LONG)
	*((long*) options->u_max_value)= *((long*) options->value)=
	  options->def_value;
      else if (options->var_type == GET_LL)
	*((longlong*) options->u_max_value)= *((longlong*) options->value)=
	  options->def_value;
    }
  }
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

  for (optp= options; optp->id; optp++)
  {
    if (optp->id < 256)
    {
      printf("  -%c, ", optp->id);
      col= 6;
    }
    else
    {
      printf("  ");
      col= 2;
    }
    printf("--%s", optp->name);
    col+= 2 + strlen(optp->name);
    if (optp->var_type == GET_STR)
    {
      printf("=name ");
      col+= 6;
    }
    else if (optp->var_type == GET_NO_ARG)
    {
      putchar(' ');
      col++;
    }
    else
    {
      printf("=# ");
      col+= 3;
    }
    if (col > name_space)
    {
      putchar('\n');
      col= 0;
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
	comment++; // skip the space, as a newline will take it's place now
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

  printf("Variables (--variable-name=value) Default value\n");
  printf("--------------------------------- -------------\n");
  for (optp= options; optp->id; optp++)
  {
    if (optp->value)
    {
      printf("%s", optp->name);
      length= strlen(optp->name);
      for (; length < name_space; length++)
	putchar(' ');
      if (optp->var_type == GET_STR)
      {
	if (!optp->def_value && !*((char**) optp->value))
	  printf("(No default value)\n");
	else
	  printf("%s\n", *((char**) optp->value));
      }
      else if (optp->var_type == GET_LONG)
      {
	if (!optp->def_value && !*((long*) optp->value))
	  printf("(No default value)\n");
	else
	  printf("%lu\n", *((long*) optp->value));
      }
      else
      {
	if (!optp->def_value && !*((longlong*) optp->value))
	  printf("(No default value)\n");
	else
	  printf("%s\n", llstr(*((longlong*) optp->value), buff));
      }
    }
  }
}
