/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/


/**
  @file

  @brief
  Get hostname for an IP.

    Hostnames are checked with reverse name lookup and
    checked that they doesn't resemble an ip.
*/

#include "mysql_priv.h"
#include "hash_filo.h"
#include <m_ctype.h>
#ifdef	__cplusplus
extern "C" {					// Because of SCO 3.2V4.2
#endif
#if !defined( __WIN__)
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#include <netdb.h>
#include <sys/utsname.h>
#endif // __WIN__
#ifdef	__cplusplus
}
#endif


class host_entry :public hash_filo_element
{
public:
  char	 ip[sizeof(((struct in_addr *) 0)->s_addr)];
  uint	 errors;
  char	 *hostname;
};

static hash_filo *hostname_cache;
static pthread_mutex_t LOCK_hostname;

void hostname_cache_refresh()
{
  hostname_cache->clear();
}

bool hostname_cache_init()
{
  host_entry tmp;
  uint offset= (uint) ((char*) (&tmp.ip) - (char*) &tmp);
  if (!(hostname_cache=new hash_filo(HOST_CACHE_SIZE, offset,
				     sizeof(struct in_addr),NULL,
				     (hash_free_key) free,
				     &my_charset_bin)))
    return 1;
  hostname_cache->clear();
  (void) pthread_mutex_init(&LOCK_hostname,MY_MUTEX_INIT_SLOW);
  return 0;
}

void hostname_cache_free()
{
  if (hostname_cache)
  {
    (void) pthread_mutex_destroy(&LOCK_hostname);
    delete hostname_cache;
    hostname_cache= 0;
  }
}


static void add_hostname(struct in_addr *in,const char *name)
{
  if (!(specialflag & SPECIAL_NO_HOST_CACHE))
  {
    VOID(pthread_mutex_lock(&hostname_cache->lock));
    host_entry *entry;
    if (!(entry=(host_entry*) hostname_cache->search((uchar*) &in->s_addr,0)))
    {
      uint length=name ? (uint) strlen(name) : 0;

      if ((entry=(host_entry*) malloc(sizeof(host_entry)+length+1)))
      {
	char *new_name;
	memcpy_fixed(&entry->ip, &in->s_addr, sizeof(in->s_addr));
	if (length)
	  memcpy(new_name= (char *) (entry+1), name, length+1);
	else
	  new_name=0;
	entry->hostname=new_name;
	entry->errors=0;
	(void) hostname_cache->add(entry);
      }
    }
    VOID(pthread_mutex_unlock(&hostname_cache->lock));
  }
}


inline void add_wrong_ip(struct in_addr *in)
{
  add_hostname(in,NullS);
}

void inc_host_errors(struct in_addr *in)
{
  VOID(pthread_mutex_lock(&hostname_cache->lock));
  host_entry *entry;
  if ((entry=(host_entry*) hostname_cache->search((uchar*) &in->s_addr,0)))
    entry->errors++;
  VOID(pthread_mutex_unlock(&hostname_cache->lock));
}

void reset_host_errors(struct in_addr *in)
{
  VOID(pthread_mutex_lock(&hostname_cache->lock));
  host_entry *entry;
  if ((entry=(host_entry*) hostname_cache->search((uchar*) &in->s_addr,0)))
    entry->errors=0;
  VOID(pthread_mutex_unlock(&hostname_cache->lock));
}

/* Deal with systems that don't defined INADDR_LOOPBACK */
#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK 0x7f000001UL
#endif

char * ip_to_hostname(struct in_addr *in, uint *errors)
{
  uint i;
  host_entry *entry;
  DBUG_ENTER("ip_to_hostname");
  *errors=0;

  /* We always treat the loopback address as "localhost". */
  if (in->s_addr == htonl(INADDR_LOOPBACK))   // is expanded inline by gcc
    DBUG_RETURN((char *)my_localhost);

  /* Check first if we have name in cache */
  if (!(specialflag & SPECIAL_NO_HOST_CACHE))
  {
    VOID(pthread_mutex_lock(&hostname_cache->lock));
    if ((entry=(host_entry*) hostname_cache->search((uchar*) &in->s_addr,0)))
    {
      char *name;
      if (!entry->hostname)
	name=0;					// Don't allow connection
      else
	name=my_strdup(entry->hostname,MYF(0));
      *errors= entry->errors;
      VOID(pthread_mutex_unlock(&hostname_cache->lock));
      DBUG_RETURN(name);
    }
    VOID(pthread_mutex_unlock(&hostname_cache->lock));
  }

  struct hostent *hp, *check;
  char *name;
  LINT_INIT(check);
#if defined(HAVE_GETHOSTBYADDR_R) && defined(HAVE_SOLARIS_STYLE_GETHOST)
  char buff[GETHOSTBYADDR_BUFF_SIZE],buff2[GETHOSTBYNAME_BUFF_SIZE];
  int tmp_errno;
  struct hostent tmp_hostent, tmp_hostent2;
#ifdef HAVE_purify
  bzero(buff,sizeof(buff));		// Bug in purify
#endif
  if (!(hp=gethostbyaddr_r((char*) in,sizeof(*in),
			   AF_INET,
			   &tmp_hostent,buff,sizeof(buff),&tmp_errno)))
  {
    DBUG_PRINT("error",("gethostbyaddr_r returned %d",tmp_errno));
    DBUG_RETURN(0);
  }
  if (!(check=my_gethostbyname_r(hp->h_name,&tmp_hostent2,buff2,sizeof(buff2),
				 &tmp_errno)))
  {
    DBUG_PRINT("error",("gethostbyname_r returned %d",tmp_errno));
    /*
      Don't cache responses when the DSN server is down, as otherwise
      transient DNS failure may leave any number of clients (those
      that attempted to connect during the outage) unable to connect
      indefinitely.
    */
    if (tmp_errno == HOST_NOT_FOUND || tmp_errno == NO_DATA)
      add_wrong_ip(in);
    my_gethostbyname_r_free();
    DBUG_RETURN(0);
  }
  if (!hp->h_name[0])
  {
    DBUG_PRINT("error",("Got an empty hostname"));
    add_wrong_ip(in);
    my_gethostbyname_r_free();
    DBUG_RETURN(0);				// Don't allow empty hostnames
  }
  if (!(name=my_strdup(hp->h_name,MYF(0))))
  {
    my_gethostbyname_r_free();
    DBUG_RETURN(0);				// out of memory
  }
  my_gethostbyname_r_free();
#else

  DBUG_EXECUTE_IF("addr_fake_ipv4",
                  {
                    const char* fake_host= "santa.claus.ipv4.example.com";
                    name=my_strdup(fake_host, MYF(0));
                    add_hostname(in,name);
                    DBUG_RETURN(name);
                  };);

  VOID(pthread_mutex_lock(&LOCK_hostname));
  if (!(hp=gethostbyaddr((char*) in,sizeof(*in), AF_INET)))
  {
    VOID(pthread_mutex_unlock(&LOCK_hostname));
    DBUG_PRINT("error",("gethostbyaddr returned %d",errno));

    if (errno == HOST_NOT_FOUND || errno == NO_DATA)
      goto add_wrong_ip_and_return;
    /* Failure, don't cache responce */
    DBUG_RETURN(0);
  }
  if (!hp->h_name[0])				// Don't allow empty hostnames
  {
    VOID(pthread_mutex_unlock(&LOCK_hostname));
    DBUG_PRINT("error",("Got an empty hostname"));
    goto add_wrong_ip_and_return;
  }
  if (!(name=my_strdup(hp->h_name,MYF(0))))
  {
    VOID(pthread_mutex_unlock(&LOCK_hostname));
    DBUG_RETURN(0);				// out of memory
  }
  check=gethostbyname(name);
  VOID(pthread_mutex_unlock(&LOCK_hostname));
  if (!check)
  {
    DBUG_PRINT("error",("gethostbyname returned %d",errno));
    my_free(name,MYF(0));
    DBUG_RETURN(0);
  }
#endif

  /* Don't accept hostnames that starts with digits because they may be
     false ip:s */
  if (my_isdigit(&my_charset_latin1,name[0]))
  {
    char *pos;
    for (pos= name+1 ; my_isdigit(&my_charset_latin1,*pos); pos++) ;
    if (*pos == '.')
    {
      DBUG_PRINT("error",("mysqld doesn't accept hostnames that starts with a number followed by a '.'"));
      my_free(name,MYF(0));
      goto add_wrong_ip_and_return;
    }
  }

  /* Check that 'gethostbyname' returned the used ip */
  for (i=0; check->h_addr_list[i]; i++)
  {
    if (*(uint32*)(check->h_addr_list)[i] == in->s_addr)
    {
      add_hostname(in,name);
      DBUG_RETURN(name);
    }
  }
  DBUG_PRINT("error",("Couldn't verify hostname with gethostbyname"));
  my_free(name,MYF(0));

add_wrong_ip_and_return:
  add_wrong_ip(in);
  DBUG_RETURN(0);
}
