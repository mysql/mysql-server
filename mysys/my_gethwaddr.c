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

/* get hardware address for an interface */
/* if there are many available, any non-zero one can be used */

#include "mysys_priv.h"
#include <m_string.h>

#ifndef MAIN
static my_bool memcpy_and_test(uchar *to, uchar *from, uint len)
{
  uint i, res=1;

  for (i=0; i < len; i++)
    if ((*to++= *from++))
      res=0;
  return res;
}

#ifdef __FreeBSD__

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
  int i, res=1, mib[6]={CTL_NET, AF_ROUTE, 0, AF_LINK, NET_RT_IFLIST, 0};

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
      addr=LLADDR(sdl);
      res=memcpy_and_test(to, addr, ETHER_ADDR_LEN);
    }
  }

err:
  return res;
}

#elif __linux__

#include <net/if.h>
#include <sys/ioctl.h>
#include <net/ethernet.h>

my_bool my_gethwaddr(uchar *to)
{
  int fd, res=1;
  struct ifreq ifr;

  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0)
    goto err;

  bzero(&ifr, sizeof(ifr));
  strnmov(ifr.ifr_name, "eth0", sizeof(ifr.ifr_name) - 1);

  do {
    if (ioctl(fd, SIOCGIFHWADDR, &ifr) >= 0)
      res=memcpy_and_test(to, (uchar *)&ifr.ifr_hwaddr.sa_data, ETHER_ADDR_LEN);
  } while (res && (errno == 0 || errno == ENODEV) && ifr.ifr_name[3]++ < '6');

  close(fd);
err:
  return res;
}

#else
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
  for (i=0; i < sizeof(mac); i++)
  {
    if (i) printf(":");
    printf("%02x", mac[i]);
  }
  printf("\n");
  return 0;
}
#endif

