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


/*
  Get hostname for an IP.  Hostnames are checked with reverse name lookup and
  checked that they doesn't resemble an ip.
*/

#include "mysql_priv.h"
#include "hash_filo.h"
#include <m_ctype.h>
#ifdef	__cplusplus
extern "C" {					// Because of SCO 3.2V4.2
#endif
#ifndef __WIN__
#include <sys/resource.h>
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

void hostname_cache_refresh()
{
  hostname_cache->clear();
}

bool hostname_cache_init()
{
  if (!(hostname_cache=new hash_filo(HOST_CACHE_SIZE,offsetof(host_entry,ip),
				     sizeof(struct in_addr),NULL,
				     (void (*)(void*)) free)))
    return 1;
  hostname_cache->clear();
  return 0;
}

void hostname_cache_free()
{
  delete hostname_cache;
}

static void add_hostname(struct in_addr *in,const char *name)
{
  if (!(specialflag & SPECIAL_NO_HOST_CACHE))
  {
    VOID(pthread_mutex_lock(&hostname_cache->lock));
    host_entry *entry;
    if (!(entry=(host_entry*) hostname_cache->search((gptr) &in->s_addr,0)))
    {
      uint length=name ? strlen(name) : 0;

      if ((entry=(host_entry*) malloc(sizeof(host_entry)+length+1)))
      {
	char *new_name= (char *) (entry+1);
	memcpy_fixed(&entry->ip, &in->s_addr, sizeof(in->s_addr));
	memcpy(new_name, name, length); // Should work even if name == NULL
	new_name[length]=0;		// End of string
	entry->hostname=new_name;
	entry->errors=0;
	(void) hostname_cache->add(entry);
      }
    }
    VOID(pthread_mutex_unlock(&hostname_cache->lock));
  }
}


static inline void add_wrong_ip(struct in_addr *in)
{
  add_hostname(in,NullS);
}

void inc_host_errors(struct in_addr *in)
{
  VOID(pthread_mutex_lock(&hostname_cache->lock));
  host_entry *entry;
  if ((entry=(host_entry*) hostname_cache->search((gptr) &in->s_addr,0)))
    entry->errors++;
  VOID(pthread_mutex_unlock(&hostname_cache->lock));
}

void reset_host_errors(struct in_addr *in)
{
  VOID(pthread_mutex_lock(&hostname_cache->lock));
  host_entry *entry;
  if ((entry=(host_entry*) hostname_cache->search((gptr) &in->s_addr,0)))
    entry->errors=0;
  VOID(pthread_mutex_unlock(&hostname_cache->lock));
}


my_string ip_to_hostname(struct in_addr *in, uint *errors)
{
  host_entry *entry;
  DBUG_ENTER("ip_to_hostname");

  /* Check first if we have name in cache */
  *errors=0;
  if (!(specialflag & SPECIAL_NO_HOST_CACHE))
  {
    VOID(pthread_mutex_lock(&hostname_cache->lock));
    if ((entry=(host_entry*) hostname_cache->search((gptr) &in->s_addr,0)))
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
    return 0;
  }
  if (!(check=my_gethostbyname_r(hp->h_name,&tmp_hostent2,buff2,sizeof(buff2),
				 &tmp_errno)))
  {
    DBUG_PRINT("error",("gethostbyname_r returned %d",tmp_errno));
    add_wrong_ip(in);
    DBUG_RETURN(0);
  }
  if (!hp->h_name[0])
  {
    DBUG_PRINT("error",("Got an empty hostname"));
    add_wrong_ip(in);
    DBUG_RETURN(0);				// Don't allow empty hostnames
  }
  if (!(name=my_strdup(hp->h_name,MYF(0))))
    DBUG_RETURN(0);				// out of memory

#else
  VOID(pthread_mutex_lock(&hostname_cache->lock));
  if (!(hp=gethostbyaddr((char*) in,sizeof(*in), AF_INET)))
  {
    DBUG_PRINT("error",("gethostbyaddr returned %d",errno));
    VOID(pthread_mutex_unlock(&hostname_cache->lock));
    add_wrong_ip(in);
    DBUG_RETURN(0);
  }
  if (!hp->h_name[0])
  {
    VOID(pthread_mutex_unlock(&hostname_cache->lock));
    DBUG_PRINT("error",("Got an empty hostname"));
    add_wrong_ip(in);
    DBUG_RETURN(0);				// Don't allow empty hostnames
  }
  if (!(name=my_strdup(hp->h_name,MYF(0))))
    DBUG_RETURN(0);				// out of memory
  check=gethostbyname(name);
  VOID(pthread_mutex_unlock(&hostname_cache->lock));
  if (!check)
  {
    DBUG_PRINT("error",("gethostbyname returned %d",errno));
    my_free(name,MYF(0));
    DBUG_RETURN(0);
  }
#endif

  /* Don't accept hostnames that starts with digits because they may be
     false ip:s */
  if (isdigit(name[0]))
  {
    char *pos;
    for (pos= name+1 ; isdigit(*pos); pos++) ;
    if (*pos == '.')
    {
      DBUG_PRINT("error",("mysqld doesn't accept hostnames that starts with a number followed by a '.'"));
      my_free(name,MYF(0));
      add_wrong_ip(in);
      DBUG_RETURN(0);
    }
  }

  /* Check that 'gethostbyname' returned the used ip */
  for (uint i=0; check->h_addr_list[i]; i++)
  {
    if (*(uint32*)(check->h_addr_list)[i] == in->s_addr)
    {
      add_hostname(in,name);
      DBUG_RETURN(name);
    }
  }
  DBUG_PRINT("error",("Couldn't verify hostname with gethostbyname"));
  my_free(name,MYF(0));
  add_wrong_ip(in);
  DBUG_RETURN(0);
}
