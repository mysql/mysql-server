/* Copyright (C) 2000-2006 MySQL AB

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
#include <sys/utsname.h>
#endif // __WIN__
#ifdef	__cplusplus
}
#endif


class host_entry :public hash_filo_element
{
public:
  char	 ip[sizeof(struct sockaddr_storage)];
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
				     sizeof(struct sockaddr_storage),NULL,
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


static void add_hostname(struct sockaddr_storage *in,const char *name)
{
  if (!(specialflag & SPECIAL_NO_HOST_CACHE))
  {
    VOID(pthread_mutex_lock(&hostname_cache->lock));
    host_entry *entry;
    if (!(entry=(host_entry*) hostname_cache->search((uchar*) in,0)))
    {
      uint length=name ? (uint) strlen(name) : 0;

      if ((entry=(host_entry*) malloc(sizeof(host_entry)+length+1)))
      {
	char *new_name;
	memcpy_fixed(&entry->ip, in, sizeof(struct sockaddr_storage));
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


inline void add_wrong_ip(struct sockaddr_storage *in)
{
  add_hostname(in, NullS);
}

void inc_host_errors(struct sockaddr_storage *in)
{
  VOID(pthread_mutex_lock(&hostname_cache->lock));
  host_entry *entry;
  if ((entry=(host_entry*) hostname_cache->search((uchar*) in,0)))
    entry->errors++;
  VOID(pthread_mutex_unlock(&hostname_cache->lock));
}

void reset_host_errors(struct sockaddr_storage *in)
{
  VOID(pthread_mutex_lock(&hostname_cache->lock));
  host_entry *entry;
  if ((entry=(host_entry*) hostname_cache->search((uchar*) in,0)))
    entry->errors=0;
  VOID(pthread_mutex_unlock(&hostname_cache->lock));
}


char * ip_to_hostname(struct sockaddr_storage *in, int addrLen, uint *errors)
{
  char *name= NULL;

  struct addrinfo hints,*res_lst= NULL,*t_res;
  int gxi_error;
  char hostname_buff[NI_MAXHOST];

  host_entry *entry;
  DBUG_ENTER("ip_to_hostname");
  *errors=0;

  /* Historical comparison for 127.0.0.1 */
  gxi_error= getnameinfo((struct sockaddr *)in, addrLen,
                         hostname_buff, NI_MAXHOST,
                         NULL, 0, NI_NUMERICHOST);
  if (gxi_error)
  {
    DBUG_PRINT("error",("getnameinfo returned %d", gxi_error));
    DBUG_RETURN(0);
  }
  DBUG_PRINT("info",("resolved: %s", hostname_buff));
 
  /* The next three compares are to solve historical solutions with localhost */  
  if (!memcmp(hostname_buff, "127.0.0.1", sizeof("127.0.0.1")))
  {
    DBUG_RETURN((char *)my_localhost);
  }
  if (!memcmp(hostname_buff, "::ffff:127.0.0.1", sizeof("::ffff:127.0.0.1")))
  {
    DBUG_RETURN((char *)my_localhost);
  }
  if (!memcmp(hostname_buff, "::1", sizeof("::1")))
  {
    DBUG_RETURN((char *)my_localhost);
  }
  
  /* Check first if we have name in cache */
  if (!(specialflag & SPECIAL_NO_HOST_CACHE))
  {
    VOID(pthread_mutex_lock(&hostname_cache->lock));
    if ((entry=(host_entry*) hostname_cache->search((uchar*) in,0)))
    {
      if (entry->hostname)
	name=my_strdup(entry->hostname,MYF(0));
      else
        name= NULL;

      DBUG_PRINT("info",("cached data %s", name ? name : "null" ));
      *errors= entry->errors;
      VOID(pthread_mutex_unlock(&hostname_cache->lock));
      DBUG_RETURN(name);
    }
    VOID(pthread_mutex_unlock(&hostname_cache->lock));
  }

  if (!(name= my_strdup(hostname_buff,MYF(0))))
  {
    DBUG_PRINT("error",("out of memory"));
    DBUG_RETURN(0);
  }

  /* Don't accept hostnames that starts with digits because they may be
     false ip:s */
  if (my_isdigit(&my_charset_latin1,name[0]))
  {
    char *pos;
    for (pos= name+1 ; my_isdigit(&my_charset_latin1,*pos); pos++) ;
    if (*pos == '.')
    {
      DBUG_PRINT("error",("mysqld doesn't accept hostnames that starts with a number followed by a '.'"));
      goto add_wrong_ip_and_return;
    }
  }
  DBUG_PRINT("info",("resolved: %s",name));
  
  bzero(&hints, sizeof (struct addrinfo));
  hints.ai_flags= AI_PASSIVE;
  hints.ai_socktype= SOCK_STREAM;  
  hints.ai_family= AF_UNSPEC;

  gxi_error= getaddrinfo(hostname_buff, NULL, &hints, &res_lst);
  if (gxi_error != 0)
  {
    /*
      Don't cache responses when the DNS server is down, as otherwise
      transient DNS failure may leave any number of clients (those
      that attempted to connect during the outage) unable to connect
      indefinitely.
    */
    DBUG_PRINT("error",("getaddrinfo returned %d", gxi_error));
#ifdef EAI_NODATA
    if (gxi_error == EAI_NODATA )
#else
    if (gxi_error == EAI_NONAME )
#endif
      add_wrong_ip(in);

    if (res_lst)
      freeaddrinfo(res_lst);

    my_free(name, MYF(0));
    DBUG_RETURN(0);
  }

  /* Check that 'getaddrinfo' returned the used ip */
  for (t_res= res_lst; t_res; t_res=t_res->ai_next)
  {
    if (!memcmp(&(t_res->ai_addr), in,
                sizeof(struct sockaddr_storage) ) )
    {
      add_hostname(in,name);
      freeaddrinfo(res_lst);
      DBUG_RETURN(name);
    }
  }
  
  freeaddrinfo(res_lst);
  DBUG_PRINT("error",("Couldn't verify hostname with getaddrinfo"));

add_wrong_ip_and_return:
  my_free(name,MYF(0));
  add_wrong_ip(in);
  DBUG_RETURN(0);
}
