/*
   Copyright (c) 2004, 2010, Oracle and/or its affiliates
   Copyright (c) 2011, Monty Program Ab

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

/* get hardware address for an interface */
/* if there are many available, any non-zero one can be used */

#include "mysys_priv.h"
#include <m_string.h>

#ifndef MAIN

static my_bool memcpy_and_test(uchar *to, uchar *from, uint len)
{
  uint i, res= 1;

  for (i= 0; i < len; i++)
    if ((*to++= *from++))
      res= 0;
  return res;
}

#if defined(__APPLE__) || defined(__FreeBSD__)
#include <net/ethernet.h>
#include <sys/sysctl.h>
#include <net/route.h>
#include <net/if.h>
#include <net/if_dl.h>

my_bool my_gethwaddr(uchar *to)
{
  size_t len;
  uchar  *buf, *next, *end, *addr;
  struct if_msghdr *ifm;
  struct sockaddr_dl *sdl;
  int res= 1, mib[6]= {CTL_NET, AF_ROUTE, 0, AF_LINK, NET_RT_IFLIST, 0};

  if (sysctl(mib, 6, NULL, &len, NULL, 0) == -1)
    goto err;
  if (!(buf = alloca(len)))
    goto err;
  if (sysctl(mib, 6, buf, &len, NULL, 0) < 0)
    goto err;

  end = buf + len;

  for (next = buf ; res && next < end ; next += ifm->ifm_msglen)
  {
    ifm = (struct if_msghdr *)next;
    if (ifm->ifm_type == RTM_IFINFO)
    {
      sdl = (struct sockaddr_dl *)(ifm + 1);
      addr= (uchar *)LLADDR(sdl);
      res= memcpy_and_test(to, addr, ETHER_ADDR_LEN);
    }
  }

err:
  return res;
}

#elif defined(__linux__) || defined(__sun__)
#include <net/if.h>
#include <sys/ioctl.h>
#include <net/if_arp.h>
#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif

#define ETHER_ADDR_LEN 6

my_bool my_gethwaddr(uchar *to)
{
  int fd, res= 1;
  struct ifreq ifr[32];
  struct ifconf ifc;

  ifc.ifc_req= ifr;
  ifc.ifc_len= sizeof(ifr);

  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0)
    goto err;

  if (ioctl(fd, SIOCGIFCONF, (char*)&ifc) >= 0)
  {
    uint i;
    for (i= 0; res && i < ifc.ifc_len / sizeof(ifr[0]); i++)
    {
#ifdef __linux__
      if (ioctl(fd, SIOCGIFHWADDR, &ifr[i]) >= 0)
        res= memcpy_and_test(to, (uchar *)&ifr[i].ifr_hwaddr.sa_data,
                             ETHER_ADDR_LEN);
#else
      /*
        A bug in OpenSolaris used to prevent non-root from getting a mac address:
        {no url. Oracle killed the old OpenSolaris bug database}

        Thus, we'll use an alternative method and extract the address from the
        arp table.
      */
      struct arpreq arpr;
      arpr.arp_pa= ifr[i].ifr_addr;

      if (ioctl(fd, SIOCGARP, (char*)&arpr) >= 0)
        res= memcpy_and_test(to, (uchar *)&arpr.arp_ha.sa_data,
                             ETHER_ADDR_LEN);
#endif
    }
  }

  close(fd);
err:
  return res;
}

#elif defined(_WIN32)
#include <winsock2.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")

#define ETHER_ADDR_LEN 6

my_bool my_gethwaddr(uchar *to)
{
  my_bool res= 1;

  IP_ADAPTER_INFO *info= NULL;
  ULONG info_len= 0;

  if (GetAdaptersInfo(info, &info_len) != ERROR_BUFFER_OVERFLOW)
    goto err;

  info= (IP_ADAPTER_INFO *)alloca(info_len);

  if (GetAdaptersInfo(info, &info_len) != NO_ERROR)
    goto err;

  while (info && res)
  {
    if (info->Type == MIB_IF_TYPE_ETHERNET &&
        info->AddressLength == ETHER_ADDR_LEN)
    {
      res= memcpy_and_test(to, info->Address, ETHER_ADDR_LEN);
    }
    info = info->Next;
  }

err:
  return res;
}

#else /* unsupported system */
/* just fail */
my_bool my_gethwaddr(uchar *to __attribute__((unused)))
{
  return 1;
}
#endif

#else /* MAIN */
int main(int argc __attribute__((unused)),char **argv)
{
  uchar mac[6];
  uint i;
  MY_INIT(argv[0]);
  if (my_gethwaddr(mac))
  {
    printf("my_gethwaddr failed with errno %d\n", errno);
    exit(1);
  }
  for (i= 0; i < sizeof(mac); i++)
  {
    if (i) printf(":");
    printf("%02x", mac[i]);
  }
  printf("\n");
  return 0;
}
#endif

