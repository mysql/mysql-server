/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/* Install or upgrade MySQL server. By Sasha Pachev <sasha@mysql.com>
 */

#define INSTALL_VERSION "1.0"

#define DONT_USE_RAID
#include <global.h>
#include <m_ctype.h>
#include <my_sys.h>
#include <m_string.h>
#include <mysql_version.h>
#include <errno.h>
#include <getopt.h>

struct option long_options[] =
{
  {"help", no_argument, 0, 'h'},
  {"version", no_argument, 0, 'V'},
  {0, 0,0,0}
};

static void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s (%s)\n",my_progname,INSTALL_VERSION,
	 MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
}

static void usage()
{
  print_version();
  printf("MySQL AB, by Sasha Pachev\n");
  printf("This software comes with ABSOLUTELY NO WARRANTY\n\n");
  printf("Install or upgrade MySQL server.\n\n");
  printf("Usage: %s [OPTIONS] \n", my_progname);
  printf("\n\
  -?, --help               Display this help and exit.\n\
  -h, --host=...           Connect to host.\n\
  -V, --version            Output version information and exit.\n");
}






