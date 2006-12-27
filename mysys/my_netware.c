/* Copyright (C) 2003 MySQL AB

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

/*
  Functions specific to netware
*/

#include <mysys_priv.h>
#ifdef __NETWARE__
  #include <string.h>
  #include <library.h>

/*
  PMUserLicenseRequest is an API exported by the polimgr.nlm
  (loaded by the NetWare OS when it comes up) for use by other
  NLM-based NetWare products/services.
  PMUserLicenseRequest provides a couple of functions:
  1) it will optionally request a User license or ensure that
  one already exists for the specified User in userInfo
  2) it utilizes the NetWare usage metering service to
  record usage information about your product/service.
*/

long PMMeteredUsageRequest
(
 /*
   NDS distinguished name or IP address or ??.  asciiz string, e.g.
   ".CN=Admin.O=this.T=MYTREE."
 */
 char *userInfo,
 long infoType,                /* see defined values */
 /*
   string used to identify the calling service, used to index the
   metered info e.g. "iPrint"
 */
 char *serviceID,
 char tranAddrType,            /* type of address that follows */
 char *tranAddr,               /* ptr to a 10-byte array */
 long flags,                   /* see defined values */
 /* NLS error code, if any.  NULL input is okay */
 long *licRequestErrCode,
 /*  meter service error code, if any.  NULL input is okay */
 long *storeMeterInfoErrCode,
 /*
   error code from NLSMeter if
   storeMeterInfoErrCode == PM_LICREQ_NLSMETERERROR.
   NULL input is okay
 */
 long *NLSMeterErrCode
);

typedef long(*PMUR)(const char*, long, const char*, char,
        const char*, long, long*, long*, long*);

/* infoType */
/* indicates that the info in the userInfo param is an NDS user */
#define PM_USERINFO_TYPE_NDS      1
/* indicates that the info in the userInfo param is NOT an NDS user */
#define PM_USERINFO_TYPE_ADDRESS  2

/* Flags */

/*
  Tells the service that it should not check to see if the NDS user
  contained in the userInfo param has a NetWare User License - just
  record metering information; this is ignored if infoType !=
  PM_USERINFO_TYPE_NDS
*/

#define PM_FLAGS_METER_ONLY         0x0000001

/*
  Indicates that the values in the userInfo and serviceID parameters
  are unicode strings, so that the metering service bypasses
  converting these to unicode (again)
*/
#define PM_LICREQ_ALREADY_UNICODE   0x0000002
/*
  Useful only if infoType is PM_USERINFO_TYPE_NDS - indicates a "no
  stop" policy of the calling service
*/
#define PM_LICREQ_ALWAYS_METER      0x0000004


/*
  net Address Types - system-defined types of net addresses that can
  be used in the tranAddrType field
*/

#define NLS_TRAN_TYPE_IPX     0x00000001    /* An IPX address */
#define NLS_TRAN_TYPE_IP      0x00000008    /* An IP address */
#define NLS_ADDR_TYPE_MAC     0x000000F1    /* a MAC address */

/*
  Net Address Sizes - lengths that correspond to the tranAddrType
  field (just fyi)
*/
#define NLS_IPX_ADDR_SIZE   10    /* the size of an IPX address */
#define NLS_IP_ADDR_SIZE    4     /* the size of an IP address */
#define NLS_MAC_ADDR_SIZE   6     /* the size of a MAC address */


void netware_reg_user(const char *ip, const char *user,
		      const char *application)
{
  PMUR usage_request;
  long licRequestErrCode      = 0;
  long storeMeterInfoErrCode  = 0;
  long nlsMeterErrCode        = 0;

  /* import the symbol */
  usage_request= ((PMUR)ImportPublicObject(getnlmhandle(),
					   "PMMeteredUsageRequest"));
  if (usage_request != NULL)
  {
    unsigned long iaddr;
    char addr[NLS_IPX_ADDR_SIZE];

    /* create address */
    iaddr = htonl(inet_addr(ip));
    bzero(addr, NLS_IPX_ADDR_SIZE);
    memcpy(addr, &iaddr, NLS_IP_ADDR_SIZE);

    /* call to NLS */
    usage_request(user,
		  PM_USERINFO_TYPE_ADDRESS,
		  application,
		  NLS_TRAN_TYPE_IP,
		  addr,
		  PM_FLAGS_METER_ONLY,
		  &licRequestErrCode,
		  &storeMeterInfoErrCode,
		  &nlsMeterErrCode);
    /* release symbol */
    UnImportPublicObject(getnlmhandle(), "PMMeteredUsageRequest");
  }
}
#endif /* __NETWARE__ */
