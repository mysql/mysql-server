/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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

#include "handshake.h"

#include <mysql.h> // for MYSQL structure


/// Client-side context for authentication handshake

class Handshake_client: public Handshake
{
  /**
    Name of the server's service for which we authenticate.

    The service name is sent by server in the initial packet. If no
    service name is used, this member is @c NULL.
  */
  SEC_WCHAR  *m_service_name;

  /// Buffer for storing service name obtained from server.
  SEC_WCHAR   m_service_name_buf[MAX_SERVICE_NAME_LENGTH];

public:

  Handshake_client(const char *target, size_t len);
  ~Handshake_client();

  Blob  first_packet();
  Blob  process_data(const Blob&);
};


/**
  Create authentication handshake context for client.

  @param target    name of the target service with which we will authenticate
                   (can be NULL if not used)

  Some security packages (like Kerberos) require providing explicit name
  of the service with which a client wants to authenticate. The server-side
  authentication plugin sends this name in the greeting packet
  (see @c win_auth_handshake_{server,client}() functions).
*/

Handshake_client::Handshake_client(const char *target, size_t len)
: Handshake(SSP_NAME, CLIENT), m_service_name(NULL)
{
  if (!target || 0 == len)
    return;

  // Convert received UPN to internal WCHAR representation.

  m_service_name= utf8_to_wchar(target, &len);

  if (m_service_name)
    DBUG_PRINT("info", ("Using target service: %S\n", m_service_name));
  else
  {
    /*
      Note: we ignore errors here - m_target will be NULL, the target name
      will not be used and system will fall-back to NTLM authentication. But
      we leave trace in error log.
    */
    ERROR_LOG(WARNING, ("Could not decode UPN sent by the server"
                        "; target service name will not be used"
                        " and Kerberos authentication will not work"));
  }
}


Handshake_client::~Handshake_client()
{
  if (m_service_name)
    free(m_service_name);
}


/**
  Generate first packet to be sent to the server during packet exchange.

  This first packet should contain some data. In case of error a null blob
  is returned and @c error() gives non-zero error code.

  @return Data to be sent in the first packet or null blob in case of error.
*/

Blob Handshake_client::first_packet()
{
  SECURITY_STATUS ret;

  m_output.free();

  ret= InitializeSecurityContextW(
         &m_cred,
         NULL,                                 // partial context
         m_service_name,                       // service name
         ASC_REQ_ALLOCATE_MEMORY,              // requested attributes
         0,                                    // reserved
         SECURITY_NETWORK_DREP,                // data representation
         NULL,                                 // input data
         0,                                    // reserved
         &m_sctx,                              // context
         &m_output,                            // output data
         &m_atts,                              // attributes
         &m_expire);                           // expire date

  if (process_result(ret))
  {
    DBUG_PRINT("error",
               ("InitializeSecurityContext() failed with error %X", ret));
    return Blob();
  }

  return m_output.as_blob();
}


/**
  Process data sent by server.

  @param[in]  data  blob with data from server

  This method analyses data sent by server during authentication handshake.
  If client should continue packet exchange, this method returns data to
  be sent to the server next. If no more data needs to be exchanged, an
  empty blob is returned and @c is_complete() is @c true. In case of error
  an empty blob is returned and @c error() gives non-zero error code.

  @return Data to be sent to the server next or null blob if no more data
  needs to be exchanged or in case of error.
*/

Blob Handshake_client::process_data(const Blob &data)
{
  Security_buffer  input(data);
  SECURITY_STATUS  ret;

  m_output.free();

  ret= InitializeSecurityContextW(
         &m_cred,
         &m_sctx,                              // partial context
         m_service_name,                       // service name
         ASC_REQ_ALLOCATE_MEMORY,              // requested attributes
         0,                                    // reserved
         SECURITY_NETWORK_DREP,                // data representation
         &input,                               // input data
         0,                                    // reserved
         &m_sctx,                              // context
         &m_output,                            // output data
         &m_atts,                              // attributes
         &m_expire);                           // expire date

  if (process_result(ret))
  {
    DBUG_PRINT("error",
               ("InitializeSecurityContext() failed with error %X", ret));
    return Blob();
  }

  return m_output.as_blob();
}


/**********************************************************************/


/**
  Perform authentication handshake from client side.

  @param[in]  vio    pointer to @c MYSQL_PLUGIN_VIO instance to be used
                     for communication with the server
  @param[in]  mysql  pointer to a MySQL connection for which we authenticate

  After reading the initial packet from server, containing its UPN to be
  used as service name, client starts packet exchange by sending the first
  packet in this exchange. While handshake is not yet completed, client
  reads packets sent by the server and process them, possibly generating new
  data to be sent to the server.

  This function reports errors.

  @return 0 on success.
*/

int win_auth_handshake_client(MYSQL_PLUGIN_VIO *vio, MYSQL *mysql)
{
  /*
    Check if we should enable logging.
  */
  {
    const char *opt= getenv("AUTHENTICATION_WIN_LOG");
    int opt_val= opt ? atoi(opt) : 0;
    if (opt && !opt_val)
    {
      if (!strncasecmp("on", opt, 2))    opt_val= 1;
      if (!strncasecmp("yes", opt, 3))   opt_val= 1;
      if (!strncasecmp("true", opt, 4))  opt_val= 1;
      if (!strncasecmp("debug", opt, 5)) opt_val= 2;
      if (!strncasecmp("dbug", opt, 4))  opt_val= 2;
    }
    opt_auth_win_client_log= opt_val;
  }

  ERROR_LOG(INFO, ("Authentication handshake for account %s", mysql->user));

  // Create connection object.

  Connection con(vio);
  DBUG_ASSERT(!con.error());

  // Read initial packet from server containing service name.

  int ret;
  Blob service_name= con.read();

  if (con.error() || service_name.is_null())
  {
    ERROR_LOG(ERROR, ("Error reading initial packet"));
    return CR_ERROR;
  }
  DBUG_PRINT("info", ("Got initial packet of length %d", service_name.len()));

  // Create authentication handsake context using the given service name.

  Handshake_client hndshk(service_name[0] ? (char *)service_name.ptr() : NULL,
                          service_name.len());
  if (hndshk.error())
  {
    ERROR_LOG(ERROR, ("Could not create authentication handshake context"));
    return CR_ERROR;
  }

  /*
    The following packet exchange always starts with a packet sent by
    the client. Send this first packet now.
  */

  {
    Blob packet= hndshk.first_packet();
    if (hndshk.error() || packet.is_null())
    {
      ERROR_LOG(ERROR, ("Could not generate first packet"));
      return CR_ERROR;
    }
    DBUG_PRINT("info", ("Sending first packet of length %d", packet.len()));

    ret= con.write(packet);
    if (ret)
    {
      ERROR_LOG(ERROR, ("Error writing first packet"));
      return CR_ERROR;
    }
    DBUG_PRINT("info", ("First packet sent"));
  }

  DBUG_ASSERT(!hndshk.error());

  /*
    If handshake is not yet complete and client expects a reply, 
    read and process packets from server until handshake is complete. 
  */
  if (!hndshk.is_complete())
  {
    if (hndshk.packet_processing_loop(con))
      return CR_ERROR;
  }

  DBUG_ASSERT(!hndshk.error() && hndshk.is_complete());

  return CR_OK;
}
