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

#define MANAGER_PWGEN_VERSION "1.4"

#include <my_global.h>
#include <m_ctype.h>
#include <my_sys.h>
#include <m_string.h>
#include <mysql_version.h>
#include <errno.h>
#include <my_getopt.h>
#include <md5.h>

const char* outfile=0,*user="root";

static struct my_option my_long_options[] =
{
  {"output-file", 'o', "Write the output to the file with the given name.",
   (gptr*) &outfile, (gptr*) &outfile, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0,
   0, 0},
  {"user", 'u', "Put given user in the password file.", (gptr*) &user,
   (gptr*) &user, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
  {"help", '?', "Display this message and exit.", 0, 0, 0, GET_NO_ARG, NO_ARG,
   0, 0, 0, 0, 0, 0},
  {"version", 'V', "Display version info.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0,
   0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static void die(const char* fmt, ...)
{
  va_list args;
  DBUG_ENTER("die");
  va_start(args, fmt);
  if (fmt)
  {
    fprintf(stderr, "%s: ", my_progname);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    fflush(stderr);
  }
  va_end(args);
  exit(1);
}

static void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s (%s)\n",my_progname,
	 MANAGER_PWGEN_VERSION,
	 MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
}

void usage()
{
  print_version();
  printf("MySQL AB, by Sasha\n");
  printf("This software comes with ABSOLUTELY NO WARRANTY\n\n");
  printf("Generates a password file to be used by mysqltest.\n\n");
  printf("Usage: %s [OPTIONS]\n", my_progname);
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}

static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument __attribute__((unused)))
{
  switch (optid) {
  case '?':
    usage();
    exit(0);
  case 'V':
    print_version();
    exit(0);
  }
  return 0;
}


int parse_args(int argc, char** argv)
{
  int ho_error;

  if ((ho_error=handle_options(&argc, &argv, my_long_options, get_one_option)))
    exit(ho_error);

  return 0;
}

void get_pass(char* pw, int len)
{
  FILE* fp;
  char* pw_end=pw+len;
  /*
    /dev/random is more secure than  rand() because the seed is easy to
    predict, so we resort to rand() only if /dev/random is not available
  */
  if ((fp=fopen("/dev/random","r")))
  {
    fread(pw,len,1,fp);
    fclose(fp);
    while (pw<pw_end)
    {
      char tmp= 'a'+((uint)*pw % 26);
      *pw++= tmp;
    }
  }
  else
  {
    srand(time(NULL));
    while (pw<pw_end)
    {
      char tmp= 'a'+((uint)*pw % 26);
      *pw++= tmp;
    }
  }
  *pw_end=0;
}


int main(int argc, char** argv)
{
  FILE* fp;
  my_MD5_CTX context;
  uchar digest[16];
  char pw[17];
  uint i;

  MY_INIT(argv[0]);
  parse_args(argc,argv);
  if (!outfile)
    die("Missing --output-file");

  if (!(fp=fopen(outfile,"w")))
    die("Could not open '%s'(errno=%d)",outfile,errno);
  get_pass(pw,sizeof(pw)-1);
  my_MD5Init(&context);
  my_MD5Update(&context,(uchar*) pw,sizeof(pw)-1);
  my_MD5Final(digest,&context);
  fprintf(fp,"%s:",user);
  for (i=0;i<sizeof(digest);i++)
    fprintf(fp,"%02x",digest[i]);
  fprintf(fp,"\n");
  fclose(fp);
  printf("%s\n",pw);
  return 0;
}
