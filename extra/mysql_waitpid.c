/* Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* Wait until a program dies */

#include <my_global.h>
#include <m_string.h>
#include <my_sys.h>
#include <my_getopt.h>
#include <signal.h>
#include <errno.h>

static const char *VER= "1.1";
static char *progname;
static my_bool verbose;

void usage(void);

static struct my_option my_long_options[] =
{
  {"help", '?', "Display this help and exit.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0,
   0, 0, 0, 0, 0},
  {"help", 'I', "Synonym for -?.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0,
   0, 0, 0, 0, 0},
  {"verbose", 'v',
   "Be more verbose. Give a warning, if kill can't handle signal 0.", 
   &verbose, &verbose, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Print version information and exit.", 0, 0, 0,
   GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument __attribute__((unused)))
{
  switch(optid) {
  case 'V':
    printf("%s version %s by Jani Tolonen\n", progname, VER);
    exit(-1);
  case 'I':
  case '?':
    usage();
  }
  return 0;
}
 

int main(int argc, char *argv[])
{
  int pid= 0, t= 0, sig= 0;

  progname= argv[0];

  if (handle_options(&argc, &argv, my_long_options, get_one_option))
    exit(-1);
  if (!argv[0] || !argv[1] || (pid= atoi(argv[0])) <= 0 ||
      (t= atoi(argv[1])) <= 0)
    usage();
  for (; t > 0; t--)
  {
    if (kill((pid_t) pid, sig))
    {
      if (errno == EINVAL)
      {
	if (verbose)
	  printf("WARNING: kill couldn't handle signal 0, using signal 1.\n");
	sig= 1;
	t++;
	continue;
      }
      return 0;
    }
    sleep(1);
  }
  return 1;
}

void usage(void)
{
  printf("%s version %s by Jani Tolonen\n\n", progname, VER);
  printf("usage: %s [options] #pid #time\n\n", progname);
  printf("Description: Waits for a program, which program id is #pid, to\n");
  printf("terminate within #time seconds. If the program terminates within\n");
  printf("this time, or if the #pid no longer exists, value 0 is returned.\n");
  printf("Otherwise 1 is returned. Both #pid and #time must be positive\n");
  printf("integer arguments.\n\n");
  printf("Options:\n");
  my_print_help(my_long_options);
  exit(-1);
}
