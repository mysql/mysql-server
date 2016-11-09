/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "gcs_xcom_networking.h"
#include "gcs_xcom_utils.h"
#include "gcs_group_identifier.h"
#include "gcs_logging.h"
#include <algorithm>
#include "sock_probe.h"
#include<bitset>
#include<set>

#ifdef WIN32
#include "sock_probe_win32.c"

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
#include "sock_probe_ix.c"

/* These functions are only used on Unixes. To avoid warnings of
   unused functions when building XCom we put them here only. */

/* Return the sockaddr of the netmask of interface #count. */
static bool_t refresh_addr(sock_probe *s, int count, unsigned long request)
{
  struct ifreq *ifrecc;
  idx_check_ret(count, number_of_interfaces(s), 0) ifrecc= s->ifrp[count];
  if(s->tmp_socket == INVALID_SOCKET)
    return 0;

  return (ioctl(s->tmp_socket, request, (char *)ifrecc) >= 0);
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

bool
get_ipv4_local_addresses(std::map<std::string, int>& addr_to_cidr_bits,
                         bool filter_out_inactive)
{
  std::string localhost= "127.0.0.1";
  struct addrinfo *addr= caching_getaddrinfo(localhost.c_str());
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

        if (!inet_ntop(AF_INET, inaddr, sname, sizeof(sname)) ||
            !inet_ntop(AF_INET, inmask, smask, sizeof(smask)))
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
    if ((ip.compare(0, 8, "192.168.") == 0 && cidr >= 16) ||
        (ip.compare(0, 7, "172.16.") == 0 && cidr >= 12) ||
        (ip.compare(0, 3, "10.") == 0 && cidr >= 8) ||
        (ip.compare("127.0.0.1") == 0 ))
    {
      out.insert(std::make_pair(ip, cidr));
    }
  }
  return false;
}

bool
get_ipv4_addr_from_hostname(const std::string& host, std::string& ip)
{
  char cip[INET6_ADDRSTRLEN];
  struct addrinfo *addrinf= NULL;

  checked_getaddrinfo(host.c_str(), 0, NULL, &addrinf);
  if (!inet_ntop(AF_INET,  &((struct sockaddr_in *)addrinf->ai_addr)->sin_addr,
                 cip, sizeof(cip)))
  {
    if (addrinf)
      freeaddrinfo(addrinf);
    return true;
  }

  ip.assign(cip);
  if (addrinf)
    freeaddrinfo(addrinf);

  return false;
}


/**
  Given the address as a string, gets the IP encoded as
  an integer.
 */
static
bool string_to_sockaddr(const std::string& addr, struct sockaddr_storage *sa)
{
  sa->ss_family= AF_INET;
  if (inet_pton(AF_INET, addr.c_str(), &(((struct sockaddr_in *)sa)->sin_addr) ) == 1)
    return false;

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
  socklen_t addr_size= sizeof(struct sockaddr_storage);
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
  socklen_t addr_size= sizeof(struct sockaddr_storage);
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

/* purecov: begin deadcode */
std::string
Gcs_ip_whitelist::to_string() const
{
  std::map< std::vector<unsigned char>, std::vector<unsigned char> >::const_iterator wl_it;
  std::stringstream ss;

  for (wl_it= m_ip_whitelist.begin();
       wl_it != m_ip_whitelist.end();
       wl_it++)
  {
    char saddr[INET6_ADDRSTRLEN];
    unsigned long netbits= 0;
    std::vector<unsigned char> vmask= wl_it->second;
    std::vector<unsigned char> vip= wl_it->first;
    const unsigned char *ip= &vip[0];
    const unsigned char *mask= &vmask[0];
    const char *ret;
    saddr[0]= '\0';

    // try IPv6 first
    if (vip.size() > 4)
      ret= inet_ntop(AF_INET6, (struct in6_addr *) ip, saddr, sizeof(saddr));
    else
      ret= inet_ntop(AF_INET, (struct in_addr *) ip, saddr, sizeof(saddr));

    if (!ret)
      continue;

    for(unsigned int i=0 ; i < vmask.size(); i++)
    {
      unsigned long int tmp= 0;
      memcpy(&tmp, mask, 1); // count per octet
      std::bitset<8> netmask(tmp);
      netbits+= netmask.count();
      mask++;
    }

    ss << saddr  << "/" << netbits << ",";
  }

  std::string res= ss.str();
  res.erase(res.end() - 1);
  return res;

}
/* purecov: end */

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

    // verify that this is a valid IPv4 or IPv6 address
    is_valid_ip= !string_to_sockaddr(ip, &sa);

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
  while(std::getline(list_ss, list_entry, ','))
  {
    std::stringstream entry_ss(list_entry);
    std::string ip, mask;

    std::getline(entry_ss, ip, '/');
    std::getline(entry_ss, mask, '/');

    add_address(ip, mask);
  }

  // make sure that we always allow connections from localhost
  // so that we are able to connect to our embedded xcom

  // add IPv4 localhost addresses if needed
  if (!add_address("127.0.0.1", "32"))
  {
    MYSQL_GCS_LOG_WARN("Automatically adding IPv4 localhost address to the "
                       "whitelist. It is mandatory that it is added.");
  }

  return false;
}

bool
Gcs_ip_whitelist::add_address(std::string addr, std::string mask)
{
  struct sockaddr_storage sa;
  unsigned char *sock;
  int netmask_len= 0;
  int netbits= 0;
  std::vector<unsigned char> ssock;
  std::vector<unsigned char> smask;

  // zero the memory area
  memset(&sa, 0, sizeof(struct sockaddr_storage));
  smask.insert(smask.begin(), smask.size(), 0);
  ssock.insert(ssock.begin(), ssock.size(), 0);

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

  if (m_ip_whitelist.find(ssock) == m_ip_whitelist.end())
  {
    smask.resize(netmask_len, 0);
    for(int octet= 0, bits= netbits; octet < netmask_len && bits > 0; octet++, bits -= 8)
    {
      if (bits > 0)
        smask[octet]= static_cast<char>(0xff << (bits > 8 ? 0 : (8-bits)));
      else
        smask[octet]= 0x00;
    }

    m_ip_whitelist.insert(std::make_pair (ssock, smask));

    return false;
  }

  return true;
}

bool
Gcs_ip_whitelist::do_check_block(struct sockaddr_storage *sa) const
{
  bool block= true;
  unsigned char *buf;
  std::map< std::vector<unsigned char>, std::vector<unsigned char> >::const_iterator wl_it;
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
    unsigned int octet;
    const std::vector<unsigned char> range= (*wl_it).first;
    const std::vector<unsigned char> netmask= (*wl_it).second;

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
