/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* Resolves IP's to hostname and hostnames to IP's */

#define RESOLVE_VERSION "2.3"

#include <my_global.h>
#include <m_ctype.h>
#include <my_sys.h>
#include <m_string.h>
#include <sys/types.h>
#include <sys/socket.h>
#ifndef HAVE_BROKEN_NETINET_INCLUDES
#include <netinet/in.h>
#endif
#include <arpa/inet.h>
#include <netdb.h>
#include <my_net.h>
#include <my_getopt.h>

#if !defined(_AIX) && !defined(h_errno)
extern int h_errno;
#endif

static my_bool silent;

static struct my_option my_long_options[] =
{
  {"help", '?', "Displays this help and exits.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"info", 'I', "Synonym for --help.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"silent", 's', "Be more silent.", (uchar**) &silent, (uchar**) &silent,
   0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
  {"version", 'V', "Displays version information and exits.",
   0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};


static void print_version(void)
{
  printf("%s Ver %s, for %s (%s)\n",my_progname,RESOLVE_VERSION,
	 SYSTEM_TYPE,MACHINE_TYPE);
}


static void usage(void)
{
  print_version();
  puts("This software comes with ABSOLUTELY NO WARRANTY. This is free software,\nand you are welcome to modify and redistribute it under the GPL license\n");
  puts("Get hostname based on IP-address or IP-address based on hostname.\n");
  printf("Usage: %s [OPTIONS] hostname or IP-address\n",my_progname);
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}


static my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument __attribute__((unused)))
{
  switch (optid) {
  case 'V': print_version(); exit(0);
  case 'I':
  case '?':
    usage();
    exit(0);
  }
  return 0;
}

/*static char * load_default_groups[]= { "resolveip","client",0 }; */

static int get_options(int *argc,char ***argv)
{
  int ho_error;

  if ((ho_error=handle_options(argc, argv, my_long_options, get_one_option)))
    exit(ho_error);

  if (*argc == 0)
  {
    usage();
    return 1;
  }
  return 0;
} /* get_options */



int main(int argc, char **argv)
{
  struct hostent *hpaddr;
  in_addr_t taddr;
  char *ip,**q;
  int error=0;

  MY_INIT(argv[0]);

  if (get_options(&argc,&argv))
    exit(1);

  while (argc--)
  {
    struct in_addr addr;
    ip = *argv++;    

    /* Not compatible with IPv6!  Probably should use getnameinfo(). */
    if (inet_aton(ip, &addr) != 0)
    {
      taddr= addr.s_addr;
      if (taddr == htonl(INADDR_BROADCAST))
      {	
	puts("Broadcast");
	continue;
      }
      if (taddr == htonl(INADDR_ANY)) 
      {
	if (!taddr) 
	  puts("Null-IP-Addr");
	else
	  puts("Old-Bcast");
	continue;
      }

      hpaddr = gethostbyaddr((char*) &(taddr), sizeof(struct in_addr),AF_INET);
      if (hpaddr) 
      {
	if (silent)
	  puts(hpaddr->h_name);
	else
	{
	  printf ("Host name of %s is %s", ip,hpaddr->h_name);
#ifndef __NETWARE__
	  /* this information is not available on NetWare */
	  for (q = hpaddr->h_aliases; *q != 0; q++)
	    (void) printf(", %s", *q);
#endif /* __NETWARE__ */
	  puts("");
	}
      }
      else
      {
	error=2;
	fprintf(stderr,"%s: Unable to find hostname for '%s'\n",my_progname,
		ip);
      }
    }
    else
    {
      hpaddr = gethostbyname(ip);
      if (!hpaddr)
      {
	const char *err;
	fprintf(stderr,"%s: Unable to find hostid for '%s'",my_progname,ip);
	switch (h_errno) {
	case HOST_NOT_FOUND: err="host not found"; break;
	case TRY_AGAIN: err="try again"; break;
	case NO_RECOVERY: err="no recovery"; break;
	case NO_DATA: err="no_data"; break;
	default: err=0;
	}
	if (err)
	  fprintf(stderr,": %s\n",err);
	else
	  fprintf(stderr,"\n");
	error=2;
      }
      else if (silent)
      {
	struct in_addr in;
	memcpy((char*) &in.s_addr, (char*) *hpaddr->h_addr_list,
	       sizeof (in.s_addr));
	puts(inet_ntoa(in));
      }
      else
      {
	char **p;
	for (p = hpaddr->h_addr_list; *p != 0; p++)
	{
	  struct in_addr in;
	  memcpy(&in.s_addr, *p, sizeof (in.s_addr));
	  printf ("IP address of %s is %s\n",ip,inet_ntoa(in));
	}
      }
    }
  }
  exit(error);
}
