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

/* Resolves IP's to hostname and hostnames to IP's */

#define RESOLVE_VERSION "2.0"
 
#include <my_global.h>
#include <m_ctype.h>
#include <my_net.h>
#include <my_sys.h>
#include <m_string.h>
#include <netdb.h>
#include <getopt.h>

#if !defined(_AIX) && !defined(HAVE_UNIXWARE7_THREADS) && !defined(HAVE_UNIXWARE7_POSIX) && !defined(h_errno)
extern int h_errno;
#endif

static int silent=0;

static struct option long_options[] =
{
  {"help",       no_argument,        0, '?'},
  {"info",       no_argument,        0, 'I'},
  {"silent",     no_argument,        0, 's'},
  {"version",    no_argument,        0, 'V'},
  {0, 0, 0, 0}
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
  printf("\n\
  -?, --help     Displays this help and exits.\n\
  -I, --info     Synonym for the above.\n\
  -s, --silent   Be more silent.\n\
  -V, --version  Displays version information and exits.\n");
}

/*static my_string load_default_groups[]= { "resolveip","client",0 }; */

static int get_options(int *argc,char ***argv)
{
  int c,option_index;

  /*  load_defaults("my",load_default_groups,argc,argv); */
  while ((c=getopt_long(*argc,*argv,"?IsV",
			long_options, &option_index)) != EOF)
  {
    switch (c) {
    case 's':
      silent=1;
      break;
    case 'V': print_version(); exit(0);
    case 'I':
    case '?':
      usage();
      exit(0);
    default:
      fprintf(stderr,"%s: Illegal option character '%c'\n",
	      my_progname,opterr);
      return(1);
      break;
    }
  }
  (*argc)-=optind;
  (*argv)+=optind;
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
  u_long taddr;
  char *ip,**q;
  int error=0;

  MY_INIT(argv[0]);

  if (get_options(&argc,&argv))
    exit(1);

  while (argc--)
  {
    ip = *argv++;    

    if (isdigit(ip[0]))
    {
      taddr = inet_addr(ip);
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
	  for (q = hpaddr->h_aliases; *q != 0; q++)
	    (void) printf(", %s", *q);
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



