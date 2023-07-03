/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <my_config.h>
#include <sql_common.h>
#include "dns_srv_data.h"

#ifdef HAVE_UNIX_DNS_SRV

#ifdef __FreeBSD__
#include <netinet/in.h>
#endif
#include <netdb.h>
#include <resolv.h>

// POSIX version

static bool get_dns_srv(Dns_srv_data &data, const char *dnsname, int &error) {
  struct __res_state state {};
  res_ninit(&state);
  unsigned char query_buffer[NS_PACKETSZ];
  bool ret = true;

  data.clear();

  int res = res_nsearch(&state, dnsname, ns_c_in, ns_t_srv, query_buffer,
                        sizeof(query_buffer));

  if (res >= 0) {
    ns_msg msg;
    ns_initparse(query_buffer, res, &msg);

    for (int x = 0; x < ns_msg_count(msg, ns_s_an); x++) {
      // Get next DNS SRV record and its data

      ns_rr rr;
      ns_parserr(&msg, ns_s_an, x, &rr);
      const unsigned char *srv_data = ns_rr_rdata(rr);

      // Read port, priority and weight.
      // Note: Each NS_GET16 call moves srv_data to next value

      uint16_t port, prio, weight;

      NS_GET16(prio, srv_data);
      NS_GET16(weight, srv_data);
      NS_GET16(port, srv_data);

      // Read host name

      char name_buffer[NS_MAXDNAME];

      dn_expand(ns_msg_base(msg), ns_msg_end(msg), srv_data, name_buffer,
                sizeof(name_buffer));
      data.add(name_buffer, port, prio, weight);
    }
    ret = false;
  } else {
    ret = true;
    error = h_errno;
  }

  res_nclose(&state);
  return ret;
}

#elif defined(HAVE_WIN32_DNS_SRV)
#include <windns.h>
#include <winsock2.h>

// Windows version

static bool get_dns_srv(Dns_srv_data &data, const char *dnsname, int &error) {
  DNS_STATUS status;                 // Return value of  DnsQuery_A() function.
  PDNS_RECORD pDnsRecord = nullptr;  // Pointer to DNS_RECORD structure.

  data.clear();
  status = DnsQuery(dnsname, DNS_TYPE_SRV, DNS_QUERY_STANDARD, nullptr,
                    &pDnsRecord, nullptr);

  if (status == ERROR_SUCCESS) {
    // Iterate over linked list of DNS records

    PDNS_RECORD pRecord = pDnsRecord;
    while (pRecord) {
      if (pRecord->wType == DNS_TYPE_SRV) {
        data.add(pRecord->Data.Srv.pNameTarget, pRecord->Data.Srv.wPort,
                 pRecord->Data.Srv.wPriority, pRecord->Data.Srv.wWeight);
      }
      pRecord = pRecord->pNext;
    }

    DnsRecordListFree(pDnsRecord, DnsFreeRecordListDeep);
  } else
    error = status;
  return status != ERROR_SUCCESS;
}
#else

#error "No DNS SRV Support detected for your OS. Consider adjusting Cmake."

#if 0
// dummy function returning an error in case it's not supported by the OS

static bool get_dns_srv(Dns_srv_data &data, const char *dnsname, int &error) {
  error = -1;  // set a special error code for not supported
  return true;
}
#endif
#endif

/**
  Connect to a server using a DNS SRV name

  See rfc2782 for what a DNS SRV is and how is one read

  @param mysql a MySQL handle to use
  @param dns_srv_name  the name of the DNS SRV resource to query. ANSI
  @param user  the user name to pass to @ref mysql_real_connect
  @param passwd the password to pass to @ref mysql_real_connect
  @param db the database to pass to @ref mysql_real_connect
  @param client_flag the client flag to pass to @ref mysql_real_connect

  @retval NULL an error has occurred
  @retval non-NULL the connected MySQL handle to use

  If the OS doesn't support it the function returns OS error -1.

  SRV FORMAT:
  _service._proto.name. TTL class SRV priority weight port target.

  Example:
  _sip._tcp.example.com. 86400 IN SRV 0 5 5060 sipserver.example.com.

  @sa mysql_real_connect
*/
MYSQL *STDCALL mysql_real_connect_dns_srv(MYSQL *mysql,
                                          const char *dns_srv_name,
                                          const char *user, const char *passwd,
                                          const char *db,
                                          unsigned long client_flag) {
  Dns_srv_data data;
  int err = 0;

  if (get_dns_srv(data, dns_srv_name, err)) {
    set_mysql_extended_error(mysql, CR_DNS_SRV_LOOKUP_FAILED, unknown_sqlstate,
                             ER_CLIENT(CR_DNS_SRV_LOOKUP_FAILED), err);
    return nullptr;
  }

  std::string host;
  uint port;
  while (!data.pop_next(host, port)) {
    MYSQL *ret =
        mysql_real_connect(mysql, host.c_str(), user, passwd, db, port, nullptr,
                           client_flag | CLIENT_REMEMBER_OPTIONS);
    if (ret) return ret;
  }
  return nullptr;
}
