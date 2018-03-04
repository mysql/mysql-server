/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <algorithm>
#include <bitset>
#include <set>
#include <errno.h>

#ifndef _WIN32
#include <netdb.h>
#endif

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_group_identifier.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_logging_system.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_networking.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_utils.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/sock_probe.h"

#if defined(_WIN32)
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/sock_probe_win32.c"

/* Return the sockaddr of interface #count. */
static sockaddr get_if_addr(sock_probe *s, int count, int *error)
{
  *error= 0;
  return get_sockaddr(s, count);
}

static sockaddr get_if_netmask(sock_probe *s, int count, int *error)
{
  *error= 0;
  idx_check_fail(count, number_of_interfaces(s)) return s->interfaceInfo[count].iiNetmask.Address;
}

static std::string get_if_name(sock_probe *s, int count, int *error)
{
  return "";
}

#else
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/sock_probe_ix.c"

/* These functions are only used on Unixes. To avoid warnings of
   unused functions when building XCom we put them here only. */

/* Return the sockaddr of the netmask of interface #count. */
static bool_t refresh_addr(sock_probe *s, int count, unsigned long request)
{
  struct ifreq *ifrecc;
  idx_check_ret(count, number_of_interfaces(s), 0) ifrecc= s->ifrp[count];
  if(s->tmp_socket == INVALID_SOCKET)
    return 0;

#ifdef IOCTL_INT_REQUEST  // On sunos ioctl's second argument is defined as int
  return static_cast<bool_t>(ioctl(s->tmp_socket, static_cast<int>(request),
                                   (char *)ifrecc) >= 0);
#else
  return static_cast<bool_t>(ioctl(s->tmp_socket, request,
                                   (char *)ifrecc) >= 0);
#endif
}

/* Return the sockaddr with netmask of interface #count. */
static sockaddr get_if_addr(sock_probe *s, int count, int *error)
{
  if (!refresh_addr(s, count, SIOCGIFADDR))
    *error= 1;
  return get_sockaddr(s, count);
}

/* Return the sockaddr with address of interface #count. */
static sockaddr get_if_netmask(sock_probe *s, int count, int *error)
{
  if (!refresh_addr(s, count, SIOCGIFNETMASK))
    *error= 1;
  return get_sockaddr(s, count);
}

static std::string get_if_name(sock_probe *s, int count, int *error)
{
  struct ifreq *ifrecc;
  idx_check_ret(count, number_of_interfaces(s), 0) ifrecc= s->ifrp[count];
#ifdef HAVE_STRUCT_IFREQ_IFR_NAME
  std::string res= ifrecc->ifr_name;
#else
  std::string res= ifrecc->ifr_ifrn.ifrn_name;
#endif /* HAVE_STRUCT_IFREQ_IFR_NAME */
  *error= 0;
  return res;
}
#endif

/**
 Determines if a given address is an IP localhost address

 @param[in] address a reference to a string containing the address to test

 @return true if localhost, false otherwise.
 */
static bool is_address_localhost(const std::string &address)
{
  std::string lower_address(address);

  std::transform(lower_address.begin(), lower_address.end(),
                 lower_address.begin(), ::tolower);

  return (strcmp(lower_address.c_str(), "127.0.0.1/32") == 0) ||
         (strcmp(lower_address.c_str(), "localhost/32") == 0);
}

bool
get_ipv4_local_addresses(std::map<std::string, int>& addr_to_cidr_bits,
                         bool filter_out_inactive)
{
  std::string localhost= "127.0.0.1";
  struct addrinfo *addr= xcom_caching_getaddrinfo(localhost.c_str());
  bool no_addresses_collected= true;

  while (addr)
  {
    if (addr->ai_socktype != SOCK_STREAM && addr->ai_socktype != 0)
    {
      addr= addr->ai_next;
      continue;
    }

    sock_probe *s= (sock_probe*)calloc(1, sizeof(sock_probe));

    if (init_sock_probe(s)<0)
    {
      free(s);
      continue;
    }

    for (int j= 0; j < number_of_interfaces(s); j++)
    {
      if (!filter_out_inactive || is_if_running(s, j))
      {
        char sname[INET6_ADDRSTRLEN];
        char smask[INET6_ADDRSTRLEN];
        struct in_addr *inaddr= NULL;
        struct in_addr *inmask= NULL;
        int ip_error= 0, mask_error= 0;

        sockaddr ip= get_if_addr(s, j, &ip_error);
        sockaddr netmask= get_if_netmask(s, j, &mask_error);
        if (ip_error || mask_error)
        {
          int error= 0;
          std::string if_name= get_if_name(s, j, &error);

          if(error)
            if_name= "";

          MYSQL_GCS_LOG_INFO("Unable to probe network interface \"" <<
                             (if_name.size() > 0 ? if_name : "<unknown>") <<
                             "\" for IP and netmask information. Skipping!");
          continue;
        }
        inaddr= &((struct sockaddr_in*)&ip)->sin_addr;
        inmask= &((struct sockaddr_in*)&netmask)->sin_addr;

        // byte order does not matter, only how many bits are set does
        std::bitset<sizeof(unsigned long) * 8> prefix(inmask->s_addr);

        sname[0]= smask[0]= '\0';

        if (!inet_ntop(AF_INET, inaddr, sname,
                       static_cast<socklen_t>(sizeof(sname))) ||
            !inet_ntop(AF_INET, inmask, smask,
                       static_cast<socklen_t>(sizeof(smask))))
        {
          int error= 0;
          std::string if_name= get_if_name(s, j, &error);

          if(error)
            if_name= "";

          MYSQL_GCS_LOG_INFO("Unable to probe network interface \"" <<
                             (if_name.size() > 0 ? if_name : "<unknown>") <<
                             "\" for IP and netmask information. Skipping!");
          continue;
        }

        addr_to_cidr_bits.insert(std::make_pair(sname, prefix.count()));

        no_addresses_collected= false;
      }
    }
    addr= addr->ai_next;
    delete_sock_probe(s);
  }

  if (no_addresses_collected)
  {
    MYSQL_GCS_LOG_WARN("Unable to probe any network interface "
                       "for IP and netmask information. No addresses "
                       "collected!");
  }

  return no_addresses_collected;
}

/**
 This function gets all private network addresses and their
 subnet masks as a string. IPv4 only.
 */
bool
get_ipv4_local_private_addresses(std::map<std::string, int>& out,
                                 bool filter_out_inactive)
{
  std::map<std::string, int> addr_to_cidr;
  std::map<std::string, int>::iterator it;
  get_ipv4_local_addresses(addr_to_cidr, filter_out_inactive);

  for (it= addr_to_cidr.begin(); it != addr_to_cidr.end(); it++)
  {
    std::string ip= it->first;
    int cidr= it->second;

    int part1, part2, part3, part4;
    sscanf(ip.c_str(), "%d.%d.%d.%d", &part1, &part2, &part3, &part4);

    if ((part1 == 192 && part2 == 168 && cidr >= 16) ||
        (part1 == 172 && part2 >= 16 && part2 <= 31 && cidr >= 12) ||
        (part1 == 10 && cidr >= 8) ||
        (part1 == 127 && part2 == 0 && part3 == 0 && part4 == 1 ))
    {
      out.insert(std::make_pair(ip, cidr));
    }
  }

  return false;
}

bool
resolve_ip_addr_from_hostname(std::string name, std::string& ip)
{
  int res= true;
  char cip[INET6_ADDRSTRLEN];
  socklen_t cip_len= static_cast<socklen_t>(sizeof(cip));
  struct addrinfo *addrinf= NULL, hints;
  struct sockaddr *sa= NULL;
  void *in_addr= NULL;

  memset(&hints, 0, sizeof(hints));
  //For now, we will only support IPv4
  hints.ai_family = AF_INET;

  checked_getaddrinfo(name.c_str(), 0, &hints, &addrinf);
  if (!addrinf)
    goto end;

  sa= (struct sockaddr*) addrinf->ai_addr;

  switch(sa->sa_family)
  {
    case AF_INET:
      in_addr= &((struct sockaddr_in *)sa)->sin_addr;
      break;
    /* For now, we only support IPv4
    case AF_INET6:
      in_addr= &((struct sockaddr_in6 *)sa)->sin6_addr;
      break;
    */
    default:
      goto end;
  }

  if (!inet_ntop(sa->sa_family, in_addr, cip, cip_len))
    goto end;

  ip.assign(cip);
  res= false;

  end:
  if (addrinf)
    freeaddrinfo(addrinf);

  return res;
}

/**
  Given the address as a string, gets the IP encoded as
  an integer.
 */
bool
string_to_sockaddr(const std::string& addr, struct sockaddr_storage *sa)
{
  /**
    Try IPv4 first.
   */
  sa->ss_family= AF_INET;
  if (inet_pton(AF_INET, addr.c_str(), &(((struct sockaddr_in *)sa)->sin_addr)) == 1)
    return false;

  /**
    Try IPv6.
   */
  sa->ss_family= AF_INET6;
  if (inet_pton(AF_INET6, addr.c_str(), &(((struct sockaddr_in6 *)sa)->sin6_addr)) == 1)
    return false;

  return true;
}

/**
  Returns the address in unsigned integer.
 */
static bool
sock_descriptor_to_sockaddr(int fd, struct sockaddr_storage *sa)
{
  int res= 0;
  memset(sa, 0, sizeof (struct sockaddr_storage));
  socklen_t addr_size= static_cast<socklen_t>(sizeof(struct sockaddr_storage));
  if (!(res= getpeername(fd, (struct sockaddr *) sa, &addr_size)))
  {
    if (sa->ss_family != AF_INET && sa->ss_family != AF_INET6)
    {
      MYSQL_GCS_LOG_WARN("Connection is not from an IPv4 nor IPv6 address. "
                         "This is not supported. Refusing the connection!");
      res= 1;
    }
  }
  else
  {
    MYSQL_GCS_LOG_WARN("Unable to handle socket descriptor, therefore "
                       "refusing connection.");
  }
  return res ? true: false;
}

/**
  This function is a frontend function to inet_ntop.
  */
static bool
sock_descriptor_to_string(int fd, std::string &out)
{
  struct sockaddr_storage sa;
  socklen_t addr_size= static_cast<socklen_t>(sizeof(struct sockaddr_storage));
  char saddr[INET6_ADDRSTRLEN];

  // get the sockaddr struct
  sock_descriptor_to_sockaddr(fd, &sa);

  // try IPv4
  if (inet_ntop(AF_INET, &(((struct sockaddr_in *)&sa)->sin_addr),
                saddr, addr_size))
  {
    out= saddr;
    return false;
  }

  // try IPv6
  if (inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)&sa)->sin6_addr),
                saddr, addr_size))
  {
    out= saddr;
    return false;
  }

  // no go, return error
  return true;
}

const std::string Gcs_ip_whitelist::DEFAULT_WHITELIST=
  "127.0.0.1/32,10.0.0.0/8,172.16.0.0/12,192.168.0.0/16";


Gcs_ip_whitelist_entry::Gcs_ip_whitelist_entry(std::string addr,
                                               std::string mask) :
  m_addr(addr), m_mask(mask)
{
}

Gcs_ip_whitelist_entry_ip::
Gcs_ip_whitelist_entry_ip(std::string addr, std::string mask):
                                              Gcs_ip_whitelist_entry(addr, mask)
{
}

bool
Gcs_ip_whitelist_entry_ip::init_value()
{
  bool error = get_address_for_whitelist(get_addr(), get_mask(), m_value);

  return error;
}

std::pair<std::vector<unsigned char>, std::vector<unsigned char>>*
Gcs_ip_whitelist_entry_ip::get_value()
{
  return &m_value;
}

Gcs_ip_whitelist_entry_hostname::
Gcs_ip_whitelist_entry_hostname(std::string addr, std::string mask):
  Gcs_ip_whitelist_entry(addr, mask)
{
}

bool
Gcs_ip_whitelist_entry_hostname::init_value()
{
  return false;
}

std::pair<std::vector<unsigned char>, std::vector<unsigned char>>*
Gcs_ip_whitelist_entry_hostname::get_value()
{
  std::string ip;
  bool error = false;
  std::pair<std::vector<unsigned char>, std::vector<unsigned char>> value;

  if(resolve_ip_addr_from_hostname(get_addr(), ip))
  {
    MYSQL_GCS_LOG_WARN("Hostname " << get_addr().c_str() << " in Whitelist" <<
                       " configuration was not resolvable. Please check your" <<
                       " Whitelist configuration.");
    return NULL;
  }

  error = get_address_for_whitelist(ip, get_mask(), value);

  return error? NULL: new std::pair< std::vector<unsigned char>,
                                     std::vector<unsigned char> >
                                                    (value.first, value.second);
}

/* purecov: begin deadcode */
std::string
Gcs_ip_whitelist::to_string() const
{
  std::set< Gcs_ip_whitelist_entry* >::const_iterator wl_it;
  std::stringstream ss;

  for (wl_it= m_ip_whitelist.begin();
       wl_it != m_ip_whitelist.end();
       wl_it++)
  {
    ss << (*wl_it)->get_addr()  << "/" << (*wl_it)->get_mask() << ",";
  }

  std::string res= ss.str();
  res.erase(res.end() - 1);
  return res;
}
/* purecov: end */

/**
 Wrapper helper method for getnameinfo.

 Taken from VIO. If VIO ever becomes a server independent library, or we start
 using MySQL protocol, this should be removed.
 */
int gcs_getnameinfo(const struct sockaddr *sa,
                    char *hostname, size_t hostname_size,
                    char *port, size_t port_size,
                    int flags)
{
  int sa_length= 0;

  switch (sa->sa_family)
  {
  case AF_INET:
    sa_length= sizeof (struct sockaddr_in);
#ifdef HAVE_SOCKADDR_IN_SIN_LEN
    ((struct sockaddr_in *) sa)->sin_len= sa_length;
#endif /* HAVE_SOCKADDR_IN_SIN_LEN */
    break;

  case AF_INET6:
    sa_length= sizeof (struct sockaddr_in6);
# ifdef HAVE_SOCKADDR_IN6_SIN6_LEN
    ((struct sockaddr_in6 *) sa)->sin6_len= sa_length;
# endif /* HAVE_SOCKADDR_IN6_SIN6_LEN */
    break;
  }

  return getnameinfo(sa, sa_length,
                     hostname, hostname_size,
                     port, port_size,
                     flags);
}

/**
 Check if it contains an attempt of having an IP v4 address.
 It does not check for its validity, but only if it contains all authorized
 characters: numbers and dots.

 @return true if it exclusively contains all authorized characters.
 */
bool is_ipv4_address(const std::string& possible_ip)
{
  std::string::const_iterator it = possible_ip.begin();
  while (it != possible_ip.end() &&
         (isdigit(static_cast<unsigned char>(*it)) ||
          (*it) == '.'))
  {
    ++it;
  }
  return !possible_ip.empty() && it == possible_ip.end();
}

/**
 Check if it contains an attempt of having an IP v6 address.
 It does not check for its validity, but it checks if it contains : character

 @return true if it contains the : character.
 */
bool is_ipv6_address(const std::string& possible_ip)
{
  return !possible_ip.empty() &&
          possible_ip.find_first_of(':') != std::string::npos;
}

bool
Gcs_ip_whitelist::is_valid(const std::string& the_list) const
{
  // copy the string
  std::string whitelist= the_list;

  // remove trailing whitespaces
  whitelist.erase(std::remove(whitelist.begin(), whitelist.end(), ' '),
                  whitelist.end());

  std::stringstream list_ss(whitelist);
  std::string list_entry;

  // split list by commas
  while(std::getline(list_ss, list_entry, ','))
  {
    bool is_valid_ip= false;
    struct sockaddr_storage sa;
    unsigned int imask;
    std::stringstream entry_ss(list_entry);
    std::string ip, mask;

    // get ip and netmasks
    std::getline(entry_ss, ip, '/');
    std::getline(entry_ss, mask, '/');

    //Verify that this is a valid IPv4 or IPv6 address
    if(is_ipv4_address(ip) || is_ipv6_address(ip))
    {
      is_valid_ip= !string_to_sockaddr(ip, &sa);
    }
    else
    { //We won't check for hostname validity here.
      continue;
    }

    // convert the netbits from the mask to integer
    imask= (unsigned int) atoi(mask.c_str());

    // check if everything is valid
    if ((!is_valid_ip) ||                            // check for valid IP
        (!mask.empty() && !is_number(mask)) ||       // check that mask is a number
        (sa.ss_family == AF_INET6 && imask > 128) || // check that IPv6 mask is within range
        (sa.ss_family == AF_INET && imask > 32))     // check that IPv4 mask is within range
    {
      MYSQL_GCS_LOG_ERROR("Invalid IP or subnet mask in the whitelist: "
                          << ip << (mask.empty() ? "" : "/") <<
                          (mask.empty() ? "" : mask));
      return false;
    }
  }

  return true;
}

bool
Gcs_ip_whitelist::configure(const std::string& the_list)
{
  // copy the list
  std::string whitelist= the_list;
  m_original_list.assign(whitelist);

  // clear the list
  m_ip_whitelist.clear();

  // remove whitespaces
  whitelist.erase(std::remove(whitelist.begin(), whitelist.end(), ' '),
                  whitelist.end());

  std::stringstream list_ss(whitelist);
  std::string list_entry;

  // parse commas
  bool found_localhost_entry= false;
  while(std::getline(list_ss, list_entry, ','))
  {
    std::stringstream entry_ss(list_entry);
    std::string ip, mask;

    /**
      Check if the address is a localhost ipv4 address.
      Add it after if necessary.
    */
    if(!found_localhost_entry)
    {
      found_localhost_entry = is_address_localhost(entry_ss.str());
    }

    std::getline(entry_ss, ip, '/');
    std::getline(entry_ss, mask, '/');

    add_address(ip, mask);
  }

  // make sure that we always allow connections from localhost
  // so that we are able to connect to our embedded xcom

  // add IPv4 localhost addresses if needed
  if (!found_localhost_entry)
  {
    if(!add_address("127.0.0.1", "32"))
    {
      MYSQL_GCS_LOG_WARN("Automatically adding IPv4 localhost address to the "
                         "whitelist. It is mandatory that it is added.");
    }
    else
    {
      MYSQL_GCS_LOG_ERROR("Error adding IPv4 localhost address automatically"
                          " to the whitelist");
    }
  }

  return false;
}

bool
get_address_for_whitelist(std::string addr, std::string mask,
                          std::pair<std::vector<unsigned char>,
                                    std::vector<unsigned char>> &out_pair)
{
  struct sockaddr_storage sa;
  unsigned char *sock;
  size_t netmask_len= 0;
  int netbits= 0;
  std::vector<unsigned char> ssock;

  // zero the memory area
  memset(&sa, 0, sizeof(struct sockaddr_storage));

  // fill in the struct sockaddr
  if (string_to_sockaddr(addr, &sa))
    return true;

  switch(sa.ss_family)
  {
    case AF_INET:
      sock= (unsigned char *) &((struct sockaddr_in *) &sa)->sin_addr;
      ssock.assign(sock, sock+sizeof(struct in_addr));
      netmask_len= sizeof(struct in_addr);
      netbits= mask.empty() ? 32 : atoi(mask.c_str());
      break;

/* purecov: begin deadcode */
    case AF_INET6:
      sock= (unsigned char *) &((struct sockaddr_in6 *) &sa)->sin6_addr;
      ssock.assign(sock, sock+sizeof(struct in6_addr));
      netmask_len= sizeof(struct in6_addr);
      netbits= mask.empty() ? 128 : atoi(mask.c_str());
      break;
/* purecov: end */
    default:
      return true;
  }

  std::vector<unsigned char> smask;

  // Set the first netbits/8 BYTEs to 255.
  smask.resize(static_cast<size_t>(netbits/8), 0xff);

  if (smask.size() < netmask_len)
  {
    // Set the following netbits%8 BITs to 1.
    smask.push_back(static_cast<unsigned char>(0xff << (8-netbits%8)));
    // Set non-net part to 0
    smask.resize(netmask_len, 0);
  }

  out_pair= std::make_pair (ssock, smask);

  return false;
}

Gcs_ip_whitelist::~Gcs_ip_whitelist()
{
  std::set< Gcs_ip_whitelist_entry* >::const_iterator wl_it=
                                                         m_ip_whitelist.begin();
  while(wl_it != m_ip_whitelist.end())
  {
    delete (*wl_it);
    m_ip_whitelist.erase(wl_it++);
  }
}

bool
Gcs_ip_whitelist::add_address(std::string addr, std::string mask)
{
  Gcs_ip_whitelist_entry *addr_for_wl;
  struct sockaddr_storage sa;
  if(!string_to_sockaddr(addr, &sa))
  {
    addr_for_wl = new Gcs_ip_whitelist_entry_ip(addr, mask);
  }
  else
  {
    addr_for_wl = new Gcs_ip_whitelist_entry_hostname(addr, mask);
  }
  bool error = addr_for_wl->init_value();

  if(!error)
  {
    std::pair<std::set< Gcs_ip_whitelist_entry*,
            Gcs_ip_whitelist_entry_pointer_comparator>::iterator,
            bool> result;
    result= m_ip_whitelist.insert(addr_for_wl);

    error= !result.second;
  }

  return error;
}

bool
Gcs_ip_whitelist::do_check_block(struct sockaddr_storage *sa) const
{
  bool block= true;
  unsigned char *buf;
  std::set< Gcs_ip_whitelist_entry* >::const_iterator wl_it;
  std::pair< std::vector<unsigned char>, std::vector<unsigned char> > *wl_value;
  std::vector<unsigned char> ip;

/* purecov: begin deadcode */
  if (sa->ss_family == AF_INET6)
  {
    buf= (unsigned char*) &((struct sockaddr_in6 *)sa)->sin6_addr;
    ip.assign(buf, buf+sizeof(struct in6_addr));
  }
/* purecov: end */
  else if (sa->ss_family == AF_INET)
  {
    buf= (unsigned char*) &((struct sockaddr_in *)sa)->sin_addr;
    ip.assign(buf, buf+sizeof(struct in_addr));
  }
  else
    goto end;

  /*
   This check works like this:
   1. Check if the whitelist is empty.
      - if empty, return false
   2. If whitelist is not empty
      - for every ip and mask
        - check if bytes (octets) match in network byte order
   */

  if (m_ip_whitelist.empty())
    goto end;

  for (wl_it= m_ip_whitelist.begin();
       wl_it != m_ip_whitelist.end() && block;
       wl_it++)
  {
    wl_value= (*wl_it)->get_value();

    if(wl_value == NULL)
      continue;

    unsigned int octet;
    const std::vector<unsigned char> range= (*wl_value).first;
    const std::vector<unsigned char> netmask= (*wl_value).second;

    for (octet= 0; octet < range.size(); octet++)
    {
      unsigned char oct_in_ip= ip[octet];
      unsigned char oct_range_ip= range[octet];
      unsigned char oct_mask_ip= netmask[octet];

      // bail out on the first octet mismatch -- try next IP
      if ((block= (oct_in_ip & oct_mask_ip) != (oct_range_ip & oct_mask_ip)))
        break;
    }
  }

end:
  return block;
}

bool
Gcs_ip_whitelist::shall_block(int fd) const
{
  bool ret= true;
  if (fd > 0)
  {
    struct sockaddr_storage sa;
    if (sock_descriptor_to_sockaddr(fd, &sa))
    {
      MYSQL_GCS_LOG_WARN("Invalid IPv4/IPv6 address. "
                         "Refusing connection!");
      ret= true;
    }
    else
      ret= do_check_block(&sa);
  }

  if (ret)
  {
    std::string addr;
    sock_descriptor_to_string(fd, addr);
    MYSQL_GCS_LOG_WARN("Connection attempt from IP address "
                       << addr << " refused. Address is not in the "
                       "IP whitelist.");
  }
  return ret;
}

bool
Gcs_ip_whitelist::shall_block(const std::string& ip_addr) const
{
  bool ret= true;
  if (!ip_addr.empty())
  {
    struct sockaddr_storage sa;
    if (string_to_sockaddr(ip_addr, &sa))
    {
      MYSQL_GCS_LOG_WARN("Invalid IPv4/IPv6 address (" << ip_addr <<"). "
                         "Refusing connection!");
      ret= true;
    }
    else
      ret= do_check_block(&sa);
  }

  if (ret)
  {
    MYSQL_GCS_LOG_WARN("Connection attempt from IP address "
                       << ip_addr << " refused. Address is not in the "
                       "IP whitelist.");
  }
  return ret;
}
