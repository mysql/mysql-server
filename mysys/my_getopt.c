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

static int sortopt (int *argc, char ***argv);
static int findopt (char *optpat, uint length,
		    const struct my_option **opt_res,
		    char **ffname);

#define DISABLE_OPTION_COUNT 2

static char *special_opt_prefix[] = {"skip", "disable", "enable", 0};


/* function: handle_options
   Sort options; put options first, until special end of options (--), or
   until end of argv. Parse options; check that the given option matches with
   one of the options in struct 'my_option', return error in case of ambiguous
   or unknown option. Check that option was given an argument if it requires
   one. Call function 'get_one_option()' once for each option.
*/
extern int handle_options (int *argc, char ***argv, 
			   const struct my_option *longopts, 
			   my_bool (*get_one_option)(int,
						     const struct my_option *,
						     char *))
{
  uint opt_found, argvpos = 0, length, spec_len, i;
  my_bool end_of_options = 0, must_be_var = 0;
  char *progname = *(*argv), **pos, *optend, *prev_found;
  const struct my_option *optp;

  (*argc)--; 
  (*argv)++; 
  for (pos = *argv; *pos; pos++)
  {
    char *cur_arg= *pos;
    if (*cur_arg == '-' && *(cur_arg + 1) && !end_of_options) // must be opt.
    {
      char *argument = 0;
      must_be_var= 0;

      // check for long option, or --set-variable (-O)
      if (*(cur_arg + 1) == '-' || *(cur_arg + 1) == 'O')
      {
	if (*(cur_arg + 1) == 'O' || !strncmp(cur_arg, "--set-variable", 14))
	{
	  must_be_var= 1;

	  if (*(cur_arg + 1) == 'O')
	  {
	    cur_arg+= 2;
	    if (!(*cur_arg))
	    {
	      // the argument must be in next argv
	      if (!(*(pos + 1)))
	      {
		fprintf(stderr, "%s: Option '-O' requires an argument\n",
			progname);
		return 4;
	      }
	      pos++;
	      cur_arg= *pos;
	      (*argc)--;
	    }
	  }
	  else // Option argument begins with string '--set-variable'
	  {
	    cur_arg+= 14;
	    if (*cur_arg == '=')
	    {
	      cur_arg++;
	      if (!(*cur_arg))
	      {
		fprintf(stderr,
			"%s: Option '--set-variable' requires an argument\n",
			progname);
		return 4;
	      }
	    }
	    else if (*cur_arg) // garbage, or another option. break out
	    {
	      cur_arg-= 14;
	      must_be_var= 0;
	    }
	    else
	    {
	      // the argument must be in next argv
	      if (!(*(pos + 1)))
	      {
		fprintf(stderr,
			"%s: Option '--set-variable' requires an argument\n",
			progname);
		return 4;
	      }
	      pos++;
	      cur_arg= *pos;
	      (*argc)--;
	    }
	  }
	}
	else if (!must_be_var)
	{
	  if (!*(cur_arg + 2))  // '--' means end of options, look no further
	  {
	    end_of_options = 1;
	    (*argc)--;
	    continue;
	  }
	  cur_arg+= 2;          // skip the double dash
	}
	for (optend = cur_arg; *optend && *optend != '='; optend++) ;
	length = optend - cur_arg;
	/*
	  Find first the right option. Return error in case of an ambiguous,
	  or unknown option
	*/
	optp = longopts;
	if (!(opt_found = findopt(cur_arg, length, &optp, &prev_found)))
	{
	  /*
	    Didn't find any matching option. Let's see if someone called
	    option with a special option prefix
	  */
	  if (*optend != '=' && !must_be_var)
	  {
	    for (i = 0; special_opt_prefix[i]; i++)
	    {
	      spec_len = strlen(special_opt_prefix[i]);
	      if (!strncmp(special_opt_prefix[i], cur_arg, spec_len) &&
		  cur_arg[spec_len] == '-')
	      {
		// We were called with a special prefix, we can reuse opt_found
		cur_arg += (spec_len + 1);
		if ((opt_found = findopt(cur_arg, length - (spec_len + 1),
					 &optp, &prev_found)))
		{
		  if (opt_found > 1)
		  {
		    fprintf(stderr,
			    "%s: ambiguous option '--%s-%s' (--%s-%s)\n",
			    progname, special_opt_prefix[i], cur_arg,
			    special_opt_prefix[i], prev_found);
		    return 2;
		  }
		  if (i < DISABLE_OPTION_COUNT)
		    optend= "=0";
		  else                       // enable
		    optend= "=1";
		  break; // note break from the inner loop, main loop continues
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
	      return 7;
	    }
	    else
	    {
	      fprintf(stderr,
		      "%s: unknown option '--%s'\n", progname, cur_arg);
	      return 1;
	    }
	  }
	}
	if (opt_found > 1)
	{
	  if (must_be_var)
	  {
	    fprintf(stderr, "%s: variable prefix '%s' is not unique\n",
		    progname, cur_arg);
	    return 6;
	  }
	  else
	  {
	    fprintf(stderr, "%s: ambiguous option '--%s' (%s, %s)\n",
		    progname, cur_arg, prev_found, optp->name);
	    return 2;
	  }
	}
	if (must_be_var && !optp->changeable_var)
	{
	  fprintf(stderr, "%s: the argument to -O must be a variable\n",
		  progname);
	  return 8;
	}
	if (optp->arg_type == NO_ARG && *optend == '=')
	{
	  fprintf(stderr, "%s: option '--%s' cannot take an argument\n",
		  progname, optp->name);
	  return 3;
	}
	else if (optp->arg_type == REQUIRED_ARG && !*optend)
	{
	  /* Check if there are more arguments after this one */
	  if (!(*(pos + 1)))
	  {
	    fprintf(stderr, "%s: option '--%s' requires an argument\n",
		    progname, optp->name);
	    return 4;
	  }
	  pos++;
	  argument = *pos;
	  (*argc)--;
	}
	else if (*optend == '=')
	  argument = *(optend + 1) ? optend + 1 : "";
      }
      else  // must be short option
      {
	my_bool skip;
	for (skip = 0, optend = (cur_arg + 1); *optend && !skip; optend++)
	{
	  for (optp = longopts; optp->id ; optp++)
	  {
	    if (optp->id == (int) (uchar) *optend)
	    {
	      /* Option recognized. Find next what to do with it */
	      if (optp->arg_type == REQUIRED_ARG || optp->arg_type == OPT_ARG)
	      {
		if (*(optend + 1))
		{
		  argument = (optend + 1);
		  /*
		    The rest of the option is option argument
		    This is in effect a jump out of this loop
		  */
		  skip = 1;
		}
		else if (optp->arg_type == REQUIRED_ARG)
		{
		  /* Check if there are more arguments after this one */
		  if (!(*(pos + 1)))
		  {
		    fprintf(stderr, "%s: option '-%c' requires an argument\n",
			    progname, optp->id);
		    return 4;
		  }
		  pos++;
		  argument = *pos;
		  (*argc)--;
		}
	      }
	      else if (*(optend + 1)) // we are hitting many options in 1 argv
		get_one_option(optp->id, optp, 0);
	      break;
	    }
	  }
	}
      }
      get_one_option(optp->id, optp, argument);
      (*argc)--; // option handled (short or long), decrease argument count
    }
    else // non-option found
      (*argv)[argvpos++] = cur_arg;
  }
  return 0;
}

/* function: findopt
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

  for (count = 0; opt->id; opt++)
  {
    if (!strncmp(opt->name, optpat, length)) // match found
    {
      (*opt_res) = opt;
      if (!count)
	*ffname = (char *) opt->name;    // we only need to know one prev
      if (length == strlen(opt->name))   // exact match
	return 1;
      count++;
    }
  }
  return count;
}
