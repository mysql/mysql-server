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

#define MANAGER_PWGEN_VERSION "1.0"

#include <my_global.h>
#include <m_ctype.h>
#include <my_sys.h>
#include <m_string.h>
#include <mysql_version.h>
#include <errno.h>
#include <getopt.h>
#include <md5.h>

const char* outfile=0,*user="root";

struct option long_options[] =
{
  {"output-file",required_argument,0,'o'},
  {"user",required_argument,0,'u'},
  {"help",no_argument,0,'?'},
  {"version",no_argument,0,'V'},
  {0,0,0,0}
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
  printf("-?,--help     Display this message and exit\n\
-V,--version    Display version info\n\
-u,--user=      Put given user in the password file\n\
-o,--output-file= Write the output to the file with the given name\n");
}

int parse_args(int argc, char** argv)
{
  int c,option_index=0;
  while ((c=getopt_long(argc,argv,"?Vu:o:",long_options,&option_index))
	 != EOF)
  {
    switch (c)
    {
    case 'o':
      outfile=optarg;
      break;
    case 'u':
      user=optarg;
      break;
    case '?':
      usage();
      exit(0);
    case 'V':
      print_version();
      exit(0);
    default:
      usage();
      exit(1);
    }
  }
  return 0;
}

void get_pass(char* pw, int len)
{
  FILE* fp;
  char* pw_end=pw+len;
/* /dev/random is more secure than  rand() because the seed is easy to
 predict, so we resort to rand() only if /dev/random is not available */
  if ((fp=fopen("/dev/random","r")))
  {
    fread(pw,len,1,fp);
    fclose(fp);
    while (pw<pw_end)
    {
      *pw++='a'+((uint)*pw % 26);
    }
  }
  else
  {
    srand(time(NULL));
    while (pw<pw_end)
      *pw++='a'+((uint)rand() % 26);
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
  my_MD5Update(&context,pw,sizeof(pw)-1);
  my_MD5Final(digest,&context);
  fprintf(fp,"%s:",user);
  for (i=0;i<sizeof(digest);i++)
    fprintf(fp,"%02x",digest[i]);
  fprintf(fp,"\n");
  fclose(fp);
  printf("%s\n",pw);
  return 0;
}
