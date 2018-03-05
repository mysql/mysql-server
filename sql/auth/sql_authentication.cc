/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "sql_base.h"                   /* close_mysql_tables */
#include "sql_parse.h"                  /* check_access */
#include "log.h"                        /* sql_print_warning, query_logger */
#include <sql_common.h>                 /* mpvio_info */
#include "sql_connect.h"                /* thd_init_client_charset */
                                        /* get_or_create_user_conn */
                                        /* check_for_max_user_connections */
                                        /* release_user_connection */
#include "hostname.h"                   /* Host_errors, inc_host_errors */
#include "password.h"                 // my_make_scrambled_password
#include "sql_db.h"                     /* mysql_change_db */
#include "connection_handler_manager.h"
#include "crypt_genhash_impl.h"         /* generate_user_salt */
#include <mysql/plugin_validate_password.h> /* validate_password plugin */
#include <mysql/service_my_plugin_log.h>
#include "sys_vars.h"
#include <fstream>                      /* std::fstream */
#include <string>                       /* std::string */
#include <algorithm>                    /* for_each */
#include <stdexcept>                    /* Exception handling */
#include <vector>                       /* std::vector */
#include <stdint.h>

#if defined(HAVE_OPENSSL) && !defined(HAVE_YASSL)
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#endif /* HAVE OPENSSL && !HAVE_YASSL */

#include "auth_internal.h"
#include "sql_auth_cache.h"
#include "sql_authentication.h"
#include "tztime.h"
#include "sql_time.h"
#include <mutex_lock.h>

/****************************************************************************
   AUTHENTICATION CODE
   including initial connect handshake, invoking appropriate plugins,
   client-server plugin negotiation, COM_CHANGE_USER, and native
   MySQL authentication plugins.
****************************************************************************/

LEX_CSTRING native_password_plugin_name= {
  C_STRING_WITH_LEN("mysql_native_password")
};
  
LEX_CSTRING sha256_password_plugin_name= {
  C_STRING_WITH_LEN("sha256_password")
};

LEX_CSTRING validate_password_plugin_name= {
  C_STRING_WITH_LEN("validate_password")
};

LEX_CSTRING default_auth_plugin_name;

plugin_ref native_password_plugin;

my_bool disconnect_on_expired_password= TRUE;

/** Size of the header fields of an authentication packet. */
#define AUTH_PACKET_HEADER_SIZE_PROTO_41    32
#define AUTH_PACKET_HEADER_SIZE_PROTO_40    5  

#if defined(HAVE_OPENSSL)
#define MAX_CIPHER_LENGTH 1024
#define SHA256_PASSWORD_MAX_PASSWORD_LENGTH MAX_PLAINTEXT_LENGTH
#if !defined(HAVE_YASSL)
#define AUTH_DEFAULT_RSA_PRIVATE_KEY "private_key.pem"
#define AUTH_DEFAULT_RSA_PUBLIC_KEY "public_key.pem"

#define DEFAULT_SSL_CLIENT_CERT "client-cert.pem"
#define DEFAULT_SSL_CLIENT_KEY  "client-key.pem"

#define MAX_CN_NAME_LENGTH 64

my_bool opt_auto_generate_certs= TRUE;

char *auth_rsa_private_key_path;
char *auth_rsa_public_key_path;
my_bool auth_rsa_auto_generate_rsa_keys= TRUE;

static bool do_auto_rsa_keys_generation();
static Rsa_authentication_keys g_rsa_keys;

#endif /* HAVE_YASSL */
#endif /* HAVE_OPENSSL */

bool
Thd_charset_adapter::init_client_charset(uint cs_number)
{
  if (thd_init_client_charset(thd, cs_number))
    return true;
  thd->update_charset();
  return thd->is_error();
}


const CHARSET_INFO *
Thd_charset_adapter::charset()
{
  return thd->charset();
}

#if defined(HAVE_OPENSSL)
#ifndef HAVE_YASSL

/**
  @brief Set key file path

  @param  key[in]            Points to either auth_rsa_private_key_path or
                             auth_rsa_public_key_path.
  @param  key_file_path[out] Stores value of actual key file path.

*/
void
Rsa_authentication_keys::get_key_file_path(char *key, String *key_file_path)
{
  /*
     If a fully qualified path is entered use that, else assume the keys are 
     stored in the data directory.
   */
  if (strchr(key, FN_LIBCHAR) != NULL
#ifdef _WIN32
      || strchr(key, FN_LIBCHAR2) != NULL
#endif
     )
    key_file_path->set_quick(key, strlen(key), system_charset_info);
  else
  {
    key_file_path->append(mysql_real_data_home, strlen(mysql_real_data_home));
    if ((*key_file_path)[key_file_path->length()] != FN_LIBCHAR)
      key_file_path->append(FN_LIBCHAR);
    key_file_path->append(key);
  }
}


/**
  @brief Read a key file and store its value in RSA structure

  @param  key_ptr[out]         Address of pointer to RSA. This is set to
                               point to a non null value if key is correctly
                               read.
  @param  is_priv_key[in]      Whether we are reading private key or public
                               key.
  @param  key_text_buffer[out] To store key file content of public key.

  @return Error status
    @retval false              Success : Either both keys are read or none
                               are.
    @retval true               Failure : An appropriate error is raised.
*/
bool
Rsa_authentication_keys::read_key_file(RSA **key_ptr,
                                       bool is_priv_key,
                                       char **key_text_buffer)
{
  String key_file_path;
  char *key;
  const char *key_type;
  FILE *key_file= NULL;

  key= is_priv_key ? auth_rsa_private_key_path : auth_rsa_public_key_path;
  key_type= is_priv_key ? "private" : "public";
  *key_ptr= NULL;

  get_key_file_path(key, &key_file_path);

  /*
     Check for existance of private key/public key file.
  */
  if ((key_file= fopen(key_file_path.c_ptr(), "r")) == NULL)
  {
    sql_print_information("RSA %s key file not found: %s."
                          " Some authentication plugins will not work.",
                          key_type, key_file_path.c_ptr());
  }
  else
  {
      *key_ptr= is_priv_key ? PEM_read_RSAPrivateKey(key_file, 0, 0, 0) :
                              PEM_read_RSA_PUBKEY(key_file, 0, 0, 0);

    if (!(*key_ptr))
    {
      char error_buf[MYSQL_ERRMSG_SIZE];
      ERR_error_string_n(ERR_get_error(), error_buf, MYSQL_ERRMSG_SIZE);
      sql_print_error("Failure to parse RSA %s key (file exists): %s:"
                      " %s", key_type, key_file_path.c_ptr(), error_buf);

      /*
        Call ERR_clear_error() just in case there are more than 1 entry in the
        OpenSSL thread's error queue.
      */
      ERR_clear_error();

      return true;
    }

    /* For public key, read key file content into a char buffer. */
    bool read_error= false;
    if (!is_priv_key)
    {
      int filesize;
      fseek(key_file, 0, SEEK_END);
      filesize= ftell(key_file);
      fseek(key_file, 0, SEEK_SET);
      *key_text_buffer= new char[filesize+1];
      int items_read= fread(*key_text_buffer, filesize, 1, key_file);
      read_error= items_read != 1;
      if (read_error)
      {
        char errbuf[MYSQL_ERRMSG_SIZE];
        sql_print_error("Failure to read key file: %s",
                        my_strerror(errbuf, MYSQL_ERRMSG_SIZE, my_errno()));
      }
      (*key_text_buffer)[filesize]= '\0';
    }
    fclose(key_file);
    return read_error;
  }
  return false;
}


Rsa_authentication_keys::Rsa_authentication_keys()
{
  m_cipher_len= 0;
  m_private_key= 0;
  m_public_key= 0;
  m_pem_public_key= 0;
}
  

void
Rsa_authentication_keys::free_memory()
{
  if (m_private_key)
    RSA_free(m_private_key);

  if (m_public_key)
  {
    RSA_free(m_public_key);
    m_cipher_len= 0;
  }

  if (m_pem_public_key)
    delete [] m_pem_public_key;
}


void *
Rsa_authentication_keys::allocate_pem_buffer(size_t buffer_len)
{
  m_pem_public_key= new char[buffer_len];
  return m_pem_public_key;
}


int
Rsa_authentication_keys::get_cipher_length()
{
  return (m_cipher_len= RSA_size(m_public_key));
}


/**
  @brief Read RSA private key and public key from file and store them
         in m_private_key and m_public_key. Also, read public key in
         text format and store it in m_pem_public_key.

  @return Error status
    @retval false        Success : Either both keys are read or none are.
    @retval true         Failure : An appropriate error is raised.
*/
bool
Rsa_authentication_keys::read_rsa_keys()
{
  RSA *rsa_private_key_ptr= NULL;
  RSA *rsa_public_key_ptr= NULL;
  char *pub_key_buff= NULL; 

  if ((strlen(auth_rsa_private_key_path) == 0) &&
      (strlen(auth_rsa_public_key_path) == 0))
  {
    sql_print_information("RSA key files not found."
                          " Some authentication plugins will not work.");
    return false;
  }

  /*
    Read private key in RSA format.
  */
  if (read_key_file(&rsa_private_key_ptr, true, NULL))
      return true;
  
  /*
    Read public key in RSA format.
  */
  if (read_key_file(&rsa_public_key_ptr, false, &pub_key_buff))
  {
    if (rsa_private_key_ptr)
      RSA_free(rsa_private_key_ptr);
    return true;
  }

  /*
     If both key files are read successfully then assign values to following
     members of the class
     1. m_pem_public_key
     2. m_private_key
     3. m_public_key

     Else clean up.
   */
  if (rsa_private_key_ptr && rsa_public_key_ptr)
  {
    size_t buff_len= strlen(pub_key_buff);
    char *pem_file_buffer= (char *)allocate_pem_buffer(buff_len + 1);
    strncpy(pem_file_buffer, pub_key_buff, buff_len);
    pem_file_buffer[buff_len]= '\0';

    m_private_key= rsa_private_key_ptr;
    m_public_key= rsa_public_key_ptr;

    delete [] pub_key_buff; 
  }
  else
  {
    if (rsa_private_key_ptr)
      RSA_free(rsa_private_key_ptr);

    if (rsa_public_key_ptr)
    {
      delete [] pub_key_buff; 
      RSA_free(rsa_public_key_ptr);
    }
  }
  return false;
}

#endif /* HAVE_YASSL */
#endif /* HAVE_OPENSSL */

/**
 Initialize default authentication plugin based on command line options or
 configuration file settings.
 
 @param plugin_name Name of the plugin
 @param plugin_name_length Length of the string
*/

int set_default_auth_plugin(char *plugin_name, size_t plugin_name_length)
{
  default_auth_plugin_name.str= plugin_name;
  default_auth_plugin_name.length= plugin_name_length;

  optimize_plugin_compare_by_pointer(&default_auth_plugin_name);

#if defined(HAVE_OPENSSL)
  if (default_auth_plugin_name.str == sha256_password_plugin_name.str)
  {
    /*
      Adjust default password algorithm to fit the default authentication
      method.
    */
    global_system_variables.old_passwords= 2;
  }
  else
#endif /* HAVE_OPENSSL */
  if (default_auth_plugin_name.str != native_password_plugin_name.str)
    return 1;

  return 0;
}


void optimize_plugin_compare_by_pointer(LEX_CSTRING *plugin_name)
{
#if defined(HAVE_OPENSSL)
  if (my_strcasecmp(system_charset_info, sha256_password_plugin_name.str,
        plugin_name->str) == 0)
  {
    plugin_name->str= sha256_password_plugin_name.str;
    plugin_name->length= sha256_password_plugin_name.length;
  }
  else
#endif
    if (my_strcasecmp(system_charset_info, native_password_plugin_name.str,
          plugin_name->str) == 0)
    {
      plugin_name->str= native_password_plugin_name.str;
      plugin_name->length= native_password_plugin_name.length;
  }
}


bool auth_plugin_is_built_in(const char *plugin_name)
{
 return (plugin_name == native_password_plugin_name.str
#if defined(HAVE_OPENSSL)
         || plugin_name == sha256_password_plugin_name.str
#endif
         );
}


/**
  Only the plugins that are known to use the mysql.user table 
  to store their passwords support password expiration atm.
  TODO: create a service and extend the plugin API to support
  password expiration for external plugins.

  @retval      false  expiration not supported
  @retval      true   expiration supported
*/
bool auth_plugin_supports_expiration(const char *plugin_name)
{
 return (!plugin_name || !*plugin_name ||
         plugin_name == native_password_plugin_name.str
#if defined(HAVE_OPENSSL)
         || plugin_name == sha256_password_plugin_name.str
#endif
         );
}


/* few defines to have less ifdef's in the code below */
#ifdef EMBEDDED_LIBRARY
#undef HAVE_OPENSSL
#ifdef NO_EMBEDDED_ACCESS_CHECKS
#define initialized 0
#endif /* NO_EMBEDDED_ACCESS_CHECKS */
#endif /* EMBEDDED_LIBRARY */
#ifndef HAVE_OPENSSL
#define ssl_acceptor_fd 0
#define sslaccept(A,B,C) 1
#endif /* HAVE_OPENSSL */

/**
  a helper function to report an access denied error in all the proper places
*/
static void login_failed_error(MPVIO_EXT *mpvio, int passwd_used)
{
  THD *thd= current_thd;
  if (passwd_used == 2)
  {
    my_error(ER_ACCESS_DENIED_NO_PASSWORD_ERROR, MYF(0),
             mpvio->auth_info.user_name,
             mpvio->auth_info.host_or_ip);
    query_logger.general_log_print(thd, COM_CONNECT,
                                   ER(ER_ACCESS_DENIED_NO_PASSWORD_ERROR),
                                   mpvio->auth_info.user_name,
                                   mpvio->auth_info.host_or_ip);
    /*
      Log access denied messages to the error log when log-warnings = 2
      so that the overhead of the general query log is not required to track
      failed connections.
    */
    sql_print_information(ER(ER_ACCESS_DENIED_NO_PASSWORD_ERROR),
                          mpvio->auth_info.user_name,
                          mpvio->auth_info.host_or_ip);
  }
  else
  {
    my_error(ER_ACCESS_DENIED_ERROR, MYF(0),
             mpvio->auth_info.user_name,
             mpvio->auth_info.host_or_ip,
             passwd_used ? ER(ER_YES) : ER(ER_NO));
    query_logger.general_log_print(thd, COM_CONNECT, ER(ER_ACCESS_DENIED_ERROR),
                                   mpvio->auth_info.user_name,
                                   mpvio->auth_info.host_or_ip,
                                   passwd_used ? ER(ER_YES) : ER(ER_NO));
    /*
      Log access denied messages to the error log when log-warnings = 2
      so that the overhead of the general query log is not required to track
      failed connections.
    */
    sql_print_information(ER(ER_ACCESS_DENIED_ERROR),
                          mpvio->auth_info.user_name,
                          mpvio->auth_info.host_or_ip,
                          passwd_used ? ER(ER_YES) : ER(ER_NO));
  }
}


/**
  sends a server handshake initialization packet, the very first packet
  after the connection was established

  Packet format:

    Bytes       Content
    -----       ----
    1           protocol version (always 10)
    n           server version string, \0-terminated
    4           thread id
    8           first 8 bytes of the plugin provided data (scramble)
    1           \0 byte, terminating the first part of a scramble
    2           server capabilities (two lower bytes)
    1           server character set
    2           server status
    2           server capabilities (two upper bytes)
    1           length of the scramble
    10          reserved, always 0
    n           rest of the plugin provided data (at least 12 bytes)
    1           \0 byte, terminating the second part of a scramble

  @retval 0 ok
  @retval 1 error
*/
static bool send_server_handshake_packet(MPVIO_EXT *mpvio,
                                         const char *data, uint data_len)
{
  DBUG_ASSERT(mpvio->status == MPVIO_EXT::FAILURE);
  DBUG_ASSERT(data_len <= 255);
  Protocol_classic *protocol= mpvio->protocol;

  char *buff= (char *) my_alloca(1 + SERVER_VERSION_LENGTH + data_len + 64);
  char scramble_buf[SCRAMBLE_LENGTH];
  char *end= buff;

  DBUG_ENTER("send_server_handshake_packet");
  *end++= protocol_version;

  protocol->set_client_capabilities(CLIENT_BASIC_FLAGS);

  if (opt_using_transactions)
    protocol->add_client_capability(CLIENT_TRANSACTIONS);

  protocol->add_client_capability(CAN_CLIENT_COMPRESS);

  if (ssl_acceptor_fd)
  {
    protocol->add_client_capability(CLIENT_SSL);
    protocol->add_client_capability(CLIENT_SSL_VERIFY_SERVER_CERT);
  }

  if (data_len)
  {
    mpvio->cached_server_packet.pkt= (char*) memdup_root(mpvio->mem_root, 
                                                         data, data_len);
    mpvio->cached_server_packet.pkt_len= data_len;
  }

  if (data_len < SCRAMBLE_LENGTH)
  {
    if (data_len)
    {
      /*
        the first packet *must* have at least 20 bytes of a scramble.
        if a plugin provided less, we pad it to 20 with zeros
      */
      memcpy(scramble_buf, data, data_len);
      memset(scramble_buf + data_len, 0, SCRAMBLE_LENGTH - data_len);
      data= scramble_buf;
    }
    else
    {
      /*
        if the default plugin does not provide the data for the scramble at
        all, we generate a scramble internally anyway, just in case the
        user account (that will be known only later) uses a
        native_password_plugin (which needs a scramble). If we don't send a
        scramble now - wasting 20 bytes in the packet -
        native_password_plugin will have to send it in a separate packet,
        adding one more round trip.
      */
      generate_user_salt(mpvio->scramble, SCRAMBLE_LENGTH + 1);
      data= mpvio->scramble;
    }
    data_len= SCRAMBLE_LENGTH;
  }

  end= my_stpnmov(end, server_version, SERVER_VERSION_LENGTH) + 1;

  DBUG_ASSERT(sizeof(my_thread_id) == 4);
  int4store((uchar*) end, mpvio->thread_id);
  end+= 4;

  /*
    Old clients does not understand long scrambles, but can ignore packet
    tail: that's why first part of the scramble is placed here, and second
    part at the end of packet.
  */
  end= (char*) memcpy(end, data, AUTH_PLUGIN_DATA_PART_1_LENGTH);
  end+= AUTH_PLUGIN_DATA_PART_1_LENGTH;
  *end++= 0;
 
  int2store(end, static_cast<uint16>(protocol->get_client_capabilities()));
  /* write server characteristics: up to 16 bytes allowed */
  end[2]= (char) default_charset_info->number;
  int2store(end + 3, mpvio->server_status[0]);
  int2store(end + 5, protocol->get_client_capabilities() >> 16);
  end[7]= data_len;
  DBUG_EXECUTE_IF("poison_srv_handshake_scramble_len", end[7]= -100;);
  memset(end + 8, 0, 10);
  end+= 18;
  /* write scramble tail */
  end= (char*) memcpy(end, data + AUTH_PLUGIN_DATA_PART_1_LENGTH,
                      data_len - AUTH_PLUGIN_DATA_PART_1_LENGTH);
  end+= data_len - AUTH_PLUGIN_DATA_PART_1_LENGTH;
  end= strmake(end, plugin_name(mpvio->plugin)->str,
                    plugin_name(mpvio->plugin)->length);

  int res= protocol->write((uchar*) buff, (size_t) (end - buff + 1)) ||
           protocol->flush_net();
  DBUG_RETURN (res);
}


/**
  sends a "change plugin" packet, requesting a client to restart authentication
  using a different authentication plugin

  Packet format:
   
    Bytes       Content
    -----       ----
    1           byte with the value 254
    n           client plugin to use, \0-terminated
    n           plugin provided data

  @retval 0 ok
  @retval 1 error
*/
static bool send_plugin_request_packet(MPVIO_EXT *mpvio,
                                       const uchar *data, uint data_len)
{
  DBUG_ASSERT(mpvio->packets_written == 1);
  DBUG_ASSERT(mpvio->packets_read == 1);
  static uchar switch_plugin_request_buf[]= { 254 };

  DBUG_ENTER("send_plugin_request_packet");
  mpvio->status= MPVIO_EXT::FAILURE; // the status is no longer RESTART

  const char *client_auth_plugin=
    ((st_mysql_auth *) (plugin_decl(mpvio->plugin)->info))->client_auth_plugin;

  DBUG_ASSERT(client_auth_plugin);

  /*
    If we're dealing with an older client we can't just send a change plugin
    packet to re-initiate the authentication handshake, because the client 
    won't understand it. The good thing is that we don't need to : the old client
    expects us to just check the user credentials here, which we can do by just reading
    the cached data that are placed there by parse_com_change_user_packet() 
    In this case we just do nothing and behave as if normal authentication
    should continue.
  */
  if (!(mpvio->protocol->has_client_capability(CLIENT_PLUGIN_AUTH)))
  {
    DBUG_PRINT("info", ("old client sent a COM_CHANGE_USER"));
    DBUG_ASSERT(mpvio->cached_client_reply.pkt);
    /* get the status back so the read can process the cached result */
    mpvio->status= MPVIO_EXT::RESTART; 
    DBUG_RETURN(0);
  }

  DBUG_PRINT("info", ("requesting client to use the %s plugin", 
                      client_auth_plugin));
  DBUG_RETURN(net_write_command(mpvio->protocol->get_net(),
                                switch_plugin_request_buf[0],
                                (uchar*) client_auth_plugin,
                                strlen(client_auth_plugin) + 1,
                                (uchar*) data, data_len));
}



#ifndef NO_EMBEDDED_ACCESS_CHECKS


/* Return true if there is no users that can match the given host */

bool acl_check_host(const char *host, const char *ip)
{
  mysql_mutex_lock(&acl_cache->lock);
  if (allow_all_hosts)
  {
    
    mysql_mutex_unlock(&acl_cache->lock);
    return 0;
  }

  if ((host && my_hash_search(&acl_check_hosts,(uchar*) host,strlen(host))) ||
      (ip && my_hash_search(&acl_check_hosts,(uchar*) ip, strlen(ip))))
  {
    mysql_mutex_unlock(&acl_cache->lock);
    return 0;                                   // Found host
  }
  for (ACL_HOST_AND_IP *acl= acl_wild_hosts->begin();
       acl != acl_wild_hosts->end(); ++acl)
  {
    if (acl->compare_hostname(host, ip))
    {
      mysql_mutex_unlock(&acl_cache->lock);
      return 0;                                 // Host ok
    }
  }
  mysql_mutex_unlock(&acl_cache->lock);
  if (ip != NULL)
  {
    /* Increment HOST_CACHE.COUNT_HOST_ACL_ERRORS. */
    Host_errors errors;
    errors.m_host_acl= 1;
    inc_host_errors(ip, &errors);
  }
  return 1;                                     // Host is not allowed
}


/**
  When authentication is attempted using an unknown username a dummy user
  account with no authentication capabilites is assigned to the connection.
  This is done increase the cost of enumerating user accounts based on
  authentication protocol.
*/

ACL_USER *decoy_user(const LEX_STRING &username,
                      MEM_ROOT *mem)
{
  ACL_USER *user= (ACL_USER *) alloc_root(mem, sizeof(ACL_USER));
  user->can_authenticate= false;
  user->user= strdup_root(mem, username.str);
  user->user[username.length]= '\0';
  user->auth_string= empty_lex_str;
  user->ssl_cipher= empty_c_string;
  user->x509_issuer= empty_c_string;
  user->x509_subject= empty_c_string;
  user->salt_len= 0;
  user->password_last_changed.time_type= MYSQL_TIMESTAMP_ERROR;
  user->password_lifetime= 0;
  user->use_default_password_lifetime= true;
  user->account_locked= false;

  /*
    For now the common default account is used. Improvements might involve
    mapping a consistent hash of a username to a range of plugins.
  */
  user->plugin= default_auth_plugin_name;
  return user;
}


/**
   Finds acl entry in user database for authentication purposes.
   
   Finds a user and copies it into mpvio. Reports an authentication
   failure if a user is not found.

   @note find_acl_user is not the same, because it doesn't take into
   account the case when user is not empty, but acl_user->user is empty

   @retval 0    found
   @retval 1    not found
*/
static bool find_mpvio_user(MPVIO_EXT *mpvio)
{
  DBUG_ENTER("find_mpvio_user");
  DBUG_PRINT("info", ("entry: %s", mpvio->auth_info.user_name));
  DBUG_ASSERT(mpvio->acl_user == 0);
  mysql_mutex_lock(&acl_cache->lock);
  for (ACL_USER *acl_user_tmp= acl_users->begin();
       acl_user_tmp != acl_users->end(); ++acl_user_tmp)
  {
    if ((!acl_user_tmp->user || 
         !strcmp(mpvio->auth_info.user_name, acl_user_tmp->user)) &&
        acl_user_tmp->host.compare_hostname(mpvio->host, mpvio->ip))
    {
      mpvio->acl_user= acl_user_tmp->copy(mpvio->mem_root);

      /*
        When setting mpvio->acl_user_plugin we can save memory allocation if
        this is a built in plugin.
      */
      if (auth_plugin_is_built_in(acl_user_tmp->plugin.str))
        mpvio->acl_user_plugin= mpvio->acl_user->plugin;
      else
        make_lex_string_root(mpvio->mem_root, 
                             &mpvio->acl_user_plugin, 
                             acl_user_tmp->plugin.str, 
                             acl_user_tmp->plugin.length, 0);
      break;
    }
  }
  mysql_mutex_unlock(&acl_cache->lock);

  if (!mpvio->acl_user)
  {
    /*
      Pretend the user exists; let the plugin decide how to handle
      bad credentials.
    */
    LEX_STRING usr= { mpvio->auth_info.user_name,
                      mpvio->auth_info.user_name_length };
    mpvio->acl_user= decoy_user(usr, mpvio->mem_root);
    mpvio->acl_user_plugin= mpvio->acl_user->plugin;
  }

  if (my_strcasecmp(system_charset_info, mpvio->acl_user->plugin.str,
                    native_password_plugin_name.str) != 0 &&
      !(mpvio->protocol->has_client_capability(CLIENT_PLUGIN_AUTH)))
  {
    /* user account requires non-default plugin and the client is too old */
    DBUG_ASSERT(my_strcasecmp(system_charset_info, mpvio->acl_user->plugin.str,
                              native_password_plugin_name.str));
    my_error(ER_NOT_SUPPORTED_AUTH_MODE, MYF(0));
    query_logger.general_log_print(current_thd, COM_CONNECT,
                                   ER(ER_NOT_SUPPORTED_AUTH_MODE));
    DBUG_RETURN (1);
  }

  mpvio->auth_info.auth_string= mpvio->acl_user->auth_string.str;
  mpvio->auth_info.auth_string_length= 
    (unsigned long) mpvio->acl_user->auth_string.length;
  strmake(mpvio->auth_info.authenticated_as, mpvio->acl_user->user ?
          mpvio->acl_user->user : "", USERNAME_LENGTH);
  DBUG_PRINT("info", ("exit: user=%s, auth_string=%s, authenticated as=%s"
                      ", plugin=%s",
                      mpvio->auth_info.user_name,
                      mpvio->auth_info.auth_string,
                      mpvio->auth_info.authenticated_as,
                      mpvio->acl_user->plugin.str));
  DBUG_RETURN(0);
}


static bool
read_client_connect_attrs(char **ptr, size_t *max_bytes_available,
                          const CHARSET_INFO *from_cs)
{
  size_t length, length_length;
  char *ptr_save;
  /* not enough bytes to hold the length */
  if (*max_bytes_available < 1)
    return true;

  /* read the length */
  ptr_save= *ptr;
  length= static_cast<size_t>(net_field_length_ll((uchar **) ptr));
  length_length= *ptr - ptr_save;
  if (*max_bytes_available < length_length)
    return true;

  *max_bytes_available-= length_length;

  /* length says there're more data than can fit into the packet */
  if (length > *max_bytes_available)
    return true;

  /* impose an artificial length limit of 64k */
  if (length > 65535)
    return true;

#ifdef HAVE_PSI_THREAD_INTERFACE
  if (PSI_THREAD_CALL(set_thread_connect_attrs)(*ptr, length, from_cs))
    sql_print_warning("Connection attributes of length %lu were truncated",
                      (unsigned long) length);
#endif /* HAVE_PSI_THREAD_INTERFACE */
  return false;
}


static bool acl_check_ssl(THD *thd, const ACL_USER *acl_user)
{
#if defined(HAVE_OPENSSL)
  Vio *vio= thd->get_protocol_classic()->get_vio();
  SSL *ssl= (SSL*) vio->ssl_arg;
  X509 *cert;
#endif /* HAVE_OPENSSL */

  /*
    At this point we know that user is allowed to connect
    from given host by given username/password pair. Now
    we check if SSL is required, if user is using SSL and
    if X509 certificate attributes are OK
  */
  switch (acl_user->ssl_type) {
  case SSL_TYPE_NOT_SPECIFIED:                  // Impossible
  case SSL_TYPE_NONE:                           // SSL is not required
    return 0;
#if defined(HAVE_OPENSSL)
  case SSL_TYPE_ANY:                            // Any kind of SSL is ok
    return vio_type(vio) != VIO_TYPE_SSL;
  case SSL_TYPE_X509: /* Client should have any valid certificate. */
    /*
      Connections with non-valid certificates are dropped already
      in sslaccept() anyway, so we do not check validity here.

      We need to check for absence of SSL because without SSL
      we should reject connection.
    */
    if (vio_type(vio) == VIO_TYPE_SSL &&
        SSL_get_verify_result(ssl) == X509_V_OK &&
        (cert= SSL_get_peer_certificate(ssl)))
    {
      X509_free(cert);
      return 0;
    }
    return 1;
  case SSL_TYPE_SPECIFIED: /* Client should have specified attrib */
    /* If a cipher name is specified, we compare it to actual cipher in use. */
    if (vio_type(vio) != VIO_TYPE_SSL ||
        SSL_get_verify_result(ssl) != X509_V_OK)
      return 1;
    if (acl_user->ssl_cipher)
    {
      DBUG_PRINT("info", ("comparing ciphers: '%s' and '%s'",
                         acl_user->ssl_cipher, SSL_get_cipher(ssl)));
      if (strcmp(acl_user->ssl_cipher, SSL_get_cipher(ssl)))
      {
        sql_print_information("X509 ciphers mismatch: should be '%s' but is '%s'",
                              acl_user->ssl_cipher, SSL_get_cipher(ssl));
        return 1;
      }
    }
    /* Prepare certificate (if exists) */
    if (!(cert= SSL_get_peer_certificate(ssl)))
      return 1;
    /* If X509 issuer is specified, we check it... */
    if (acl_user->x509_issuer)
    {
      char *ptr= X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
      DBUG_PRINT("info", ("comparing issuers: '%s' and '%s'",
                         acl_user->x509_issuer, ptr));
      if (strcmp(acl_user->x509_issuer, ptr))
      {
        sql_print_information("X509 issuer mismatch: should be '%s' "
                              "but is '%s'", acl_user->x509_issuer, ptr);
        OPENSSL_free(ptr);
        X509_free(cert);
        return 1;
      }
      OPENSSL_free(ptr);
    }
    /* X509 subject is specified, we check it .. */
    if (acl_user->x509_subject)
    {
      char *ptr= X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
      DBUG_PRINT("info", ("comparing subjects: '%s' and '%s'",
                         acl_user->x509_subject, ptr));
      if (strcmp(acl_user->x509_subject, ptr))
      {
        sql_print_information("X509 subject mismatch: should be '%s' but is '%s'",
                          acl_user->x509_subject, ptr);
        OPENSSL_free(ptr);
        X509_free(cert);
        return 1;
      }
      OPENSSL_free(ptr);
    }
    X509_free(cert);
    return 0;
#else  /* HAVE_OPENSSL */
  default:
    /*
      If we don't have SSL but SSL is required for this user the 
      authentication should fail.
    */
    return 1;
#endif /* HAVE_OPENSSL */
  }
  return 1;
}


/**

  Check if server has valid public key/private key
  pair for RSA communication.

  @return
    @retval false RSA support is available
    @retval true RSA support is not available
*/
bool rsa_auth_status()
{
#if !defined(HAVE_OPENSSL) || defined(HAVE_YASSL)
  return false;
#else
  return (!g_rsa_keys.get_private_key() || !g_rsa_keys.get_public_key());
#endif /* !HAVE_OPENSSL || HAVE_YASSL */
}


#endif /* NO_EMBEDDED_ACCESS_CHECKS */


/* the packet format is described in send_change_user_packet() */
static bool parse_com_change_user_packet(MPVIO_EXT *mpvio, size_t packet_length)
{
  Protocol_classic *protocol = mpvio->protocol;
  char *user= (char*) protocol->get_net()->read_pos;
  char *end= user + packet_length;
  /* Safe because there is always a trailing \0 at the end of the packet */
  char *passwd= strend(user) + 1;
  size_t user_len= passwd - user - 1;
  char *db= passwd;
  char db_buff[NAME_LEN + 1];                 // buffer to store db in utf8
  char user_buff[USERNAME_LENGTH + 1];        // buffer to store user in utf8
  uint dummy_errors;

  DBUG_ENTER ("parse_com_change_user_packet");
  if (passwd >= end)
  {
    my_message(ER_UNKNOWN_COM_ERROR, ER(ER_UNKNOWN_COM_ERROR), MYF(0));
    DBUG_RETURN (1);
  }

  /*
    Clients send the size (1 byte) + string (not null-terminated).

    Cast *passwd to an unsigned char, so that it doesn't extend the sign for
    *passwd > 127 and become 2**32-127+ after casting to uint.
  */
  size_t passwd_len= (uchar) (*passwd++);

  db+= passwd_len + 1;
  /*
    Database name is always NUL-terminated, so in case of empty database
    the packet must contain at least the trailing '\0'.
  */
  if (db >= end)
  {
    my_message(ER_UNKNOWN_COM_ERROR, ER(ER_UNKNOWN_COM_ERROR), MYF(0));
    DBUG_RETURN (1);
  }

  size_t db_len= strlen(db);

  char *ptr= db + db_len + 1;

  if (ptr + 1 < end)
  {
    if (mpvio->charset_adapter->init_client_charset(uint2korr(ptr)))
      DBUG_RETURN(1);
  }

  /* Convert database and user names to utf8 */
  db_len= copy_and_convert(db_buff, sizeof(db_buff) - 1, system_charset_info,
                           db, db_len, mpvio->charset_adapter->charset(),
                           &dummy_errors);
  db_buff[db_len]= 0;

  user_len= copy_and_convert(user_buff, sizeof(user_buff) - 1,
                                  system_charset_info, user, user_len,
                                  mpvio->charset_adapter->charset(),
                                  &dummy_errors);
  user_buff[user_len]= 0;

  /* we should not free mpvio->user here: it's saved by dispatch_command() */
  if (!(mpvio->auth_info.user_name= my_strndup(key_memory_MPVIO_EXT_auth_info,
                                               user_buff, user_len, MYF(MY_WME))))
    DBUG_RETURN(1);
  mpvio->auth_info.user_name_length= user_len;

  if (make_lex_string_root(mpvio->mem_root, 
                           &mpvio->db, db_buff, db_len, 0) == 0)
    DBUG_RETURN(1); /* The error is set by make_lex_string(). */

  if (!initialized)
  {
    // if mysqld's been started with --skip-grant-tables option
    strmake(mpvio->auth_info.authenticated_as, 
            mpvio->auth_info.user_name, USERNAME_LENGTH);

    mpvio->status= MPVIO_EXT::SUCCESS;
    DBUG_RETURN(0);
  }

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (find_mpvio_user(mpvio))
  {
    DBUG_RETURN(1);
  }

  const char *client_plugin;
  if (protocol->has_client_capability(CLIENT_PLUGIN_AUTH))
  {
    client_plugin= ptr + 2;
    if (client_plugin >= end)
    {
      my_message(ER_UNKNOWN_COM_ERROR, ER(ER_UNKNOWN_COM_ERROR), MYF(0));
      DBUG_RETURN(1);
    }
  }
  else
    client_plugin= native_password_plugin_name.str;

  size_t bytes_remaining_in_packet= end - ptr;

  if (protocol->has_client_capability(CLIENT_CONNECT_ATTRS) &&
      read_client_connect_attrs(&ptr, &bytes_remaining_in_packet,
                                mpvio->charset_adapter->charset()))
    DBUG_RETURN(MY_TEST(packet_error));

  DBUG_PRINT("info", ("client_plugin=%s, restart", client_plugin));
  /* 
    Remember the data part of the packet, to present it to plugin in 
    read_packet() 
  */
  mpvio->cached_client_reply.pkt= passwd;
  mpvio->cached_client_reply.pkt_len= passwd_len;
  mpvio->cached_client_reply.plugin= client_plugin;
  mpvio->status= MPVIO_EXT::RESTART;
#endif /* NO_EMBEDDED_ACCESS_CHECKS */

  DBUG_RETURN (0);
}


#ifndef EMBEDDED_LIBRARY

/** Get a string according to the protocol of the underlying buffer. */
typedef char * (*get_proto_string_func_t) (char **, size_t *, size_t *);

/**
  Get a string formatted according to the 4.1 version of the MySQL protocol.

  @param buffer[in, out]    Pointer to the user-supplied buffer to be scanned.
  @param max_bytes_available[in, out]  Limit the bytes to scan.
  @param string_length[out] The number of characters scanned not including
                            the null character.

  @remark Strings are always null character terminated in this version of the
          protocol.

  @remark The string_length does not include the terminating null character.
          However, after the call, the buffer is increased by string_length+1
          bytes, beyond the null character if there still available bytes to
          scan.

  @return pointer to beginning of the string scanned.
    @retval NULL The buffer content is malformed
*/

static
char *get_41_protocol_string(char **buffer,
                             size_t *max_bytes_available,
                             size_t *string_length)
{
  char *str= (char *)memchr(*buffer, '\0', *max_bytes_available);

  if (str == NULL)
    return NULL;

  *string_length= (size_t)(str - *buffer);
  *max_bytes_available-= *string_length + 1;
  str= *buffer;
  *buffer += *string_length + 1;

  return str;
}


/**
  Get a string formatted according to the 4.0 version of the MySQL protocol.

  @param buffer[in, out]    Pointer to the user-supplied buffer to be scanned.
  @param max_bytes_available[in, out]  Limit the bytes to scan.
  @param string_length[out] The number of characters scanned not including
                            the null character.

  @remark If there are not enough bytes left after the current position of
          the buffer to satisfy the current string, the string is considered
          to be empty and a pointer to empty_c_string is returned.

  @remark A string at the end of the packet is not null terminated.

  @return Pointer to beginning of the string scanned, or a pointer to a empty
          string.
*/
static
char *get_40_protocol_string(char **buffer,
                             size_t *max_bytes_available,
                             size_t *string_length)
{
  char *str;
  size_t len;

  /* No bytes to scan left, treat string as empty. */
  if ((*max_bytes_available) == 0)
  {
    *string_length= 0;
    return empty_c_string;
  }

  str= (char *) memchr(*buffer, '\0', *max_bytes_available);

  /*
    If the string was not null terminated by the client,
    the remainder of the packet is the string. Otherwise,
    advance the buffer past the end of the null terminated
    string.
  */
  if (str == NULL)
    len= *string_length= *max_bytes_available;
  else
    len= (*string_length= (size_t)(str - *buffer)) + 1;

  str= *buffer;
  *buffer+= len;
  *max_bytes_available-= len;

  return str;
}

/**
  Get a length encoded string from a user-supplied buffer.

  @param buffer[in, out] The buffer to scan; updates position after scan.
  @param max_bytes_available[in, out] Limit the number of bytes to scan
  @param string_length[out] Number of characters scanned

  @remark In case the length is zero, then the total size of the string is
    considered to be 1 byte; the size byte.

  @return pointer to first byte after the header in buffer.
    @retval NULL The buffer content is malformed
*/

static
char *get_56_lenc_string(char **buffer,
                         size_t *max_bytes_available,
                         size_t *string_length)
{
  static char empty_string[1]= { '\0' };
  char *begin= *buffer;
  uchar *pos= (uchar *)begin;
  size_t required_length= 9;


  if (*max_bytes_available == 0)
    return NULL;

  /*
    If the length encoded string has the length 0
    the total size of the string is only one byte long (the size byte)
  */
  if (*begin == 0)
  {
    *string_length= 0;
    --*max_bytes_available;
    ++*buffer;
    /*
      Return a pointer to the \0 character so the return value will be
      an empty string.
    */
    return empty_string;
  }

  /* Make sure we have enough bytes available for net_field_length_ll */
  DBUG_EXECUTE_IF("buffer_too_short_3",
                  *pos= 252; *max_bytes_available= 2;
  );
  DBUG_EXECUTE_IF("buffer_too_short_4",
                  *pos= 253; *max_bytes_available= 3;
  );
  DBUG_EXECUTE_IF("buffer_too_short_9",
                  *pos= 254; *max_bytes_available= 8;
  );

  if (*pos <= 251)
    required_length= 1;
  if (*pos == 252)
    required_length= 3;
  if (*pos == 253)
    required_length= 4;

  if (*max_bytes_available < required_length)
    return NULL;

  *string_length= (size_t)net_field_length_ll((uchar **)buffer);

  DBUG_EXECUTE_IF("sha256_password_scramble_too_long",
                  *string_length= SIZE_T_MAX;
  );

  size_t len_len= (size_t)(*buffer - begin);

  DBUG_ASSERT((*max_bytes_available >= len_len) &&
              (len_len == required_length));
  
  if (*string_length > *max_bytes_available - len_len)
    return NULL;

  *max_bytes_available -= *string_length;
  *max_bytes_available -= len_len;
  *buffer += *string_length;
  return (char *)(begin + len_len);
}


/**
  Get a length encoded string from a user-supplied buffer.

  @param buffer[in, out] The buffer to scan; updates position after scan.
  @param max_bytes_available[in, out] Limit the number of bytes to scan
  @param string_length[out] Number of characters scanned

  @remark In case the length is zero, then the total size of the string is
    considered to be 1 byte; the size byte.

  @note the maximum size of the string is 255 because the header is always 
    1 byte.
  @return pointer to first byte after the header in buffer.
    @retval NULL The buffer content is malformed
*/

static
char *get_41_lenc_string(char **buffer,
                         size_t *max_bytes_available,
                         size_t *string_length)
{
 if (*max_bytes_available == 0)
    return NULL;

  /* Do double cast to prevent overflow from signed / unsigned conversion */
  size_t str_len= (size_t)(unsigned char)**buffer;

  /*
    If the length encoded string has the length 0
    the total size of the string is only one byte long (the size byte)
  */
  if (str_len == 0)
  {
    ++*buffer;
    *string_length= 0;
    /*
      Return a pointer to the 0 character so the return value will be
      an empty string.
    */
    return *buffer-1;
  }

  if (str_len >= *max_bytes_available)
    return NULL;

  char *str= *buffer+1;
  *string_length= str_len;
  *max_bytes_available-= *string_length + 1;
  *buffer+= *string_length + 1;
  return str;
}
#endif /* EMBEDDED LIBRARY */


/* the packet format is described in send_client_reply_packet() */
static size_t parse_client_handshake_packet(MPVIO_EXT *mpvio,
                                            uchar **buff, size_t pkt_len)
{
#ifndef EMBEDDED_LIBRARY
  Protocol_classic *protocol = mpvio->protocol;
  char *end;
  bool packet_has_required_size= false;
  DBUG_ASSERT(mpvio->status == MPVIO_EXT::FAILURE);

  uint charset_code= 0;
  end= (char *)protocol->get_net()->read_pos;
  /*
    In order to safely scan a head for '\0' string terminators
    we must keep track of how many bytes remain in the allocated
    buffer or we might read past the end of the buffer.
  */
  size_t bytes_remaining_in_packet= pkt_len;
  
  /*
    Peek ahead on the client capability packet and determine which version of
    the protocol should be used.
  */
  if (bytes_remaining_in_packet < 2)
    return packet_error;
    
  protocol->set_client_capabilities(uint2korr(end));

  /*
    JConnector only sends server capabilities before starting SSL
    negotiation.  The below code is patch for this.
  */
  if (bytes_remaining_in_packet == 4 &&
      protocol->has_client_capability(CLIENT_SSL))
  {
    protocol->set_client_capabilities(uint4korr(end));
    mpvio->max_client_packet_length= 0xfffff;
    charset_code= global_system_variables.character_set_client->number;
    goto skip_to_ssl;
  }

  if (protocol->has_client_capability(CLIENT_PROTOCOL_41))
    packet_has_required_size= bytes_remaining_in_packet >= 
      AUTH_PACKET_HEADER_SIZE_PROTO_41;
  else
    packet_has_required_size= bytes_remaining_in_packet >=
      AUTH_PACKET_HEADER_SIZE_PROTO_40;
  
  if (!packet_has_required_size)
    return packet_error;
  
  if (protocol->has_client_capability(CLIENT_PROTOCOL_41))
  {
    protocol->set_client_capabilities(uint4korr(end));
    mpvio->max_client_packet_length= uint4korr(end + 4);
    charset_code= (uint)(uchar)*(end + 8);
    /*
      Skip 23 remaining filler bytes which have no particular meaning.
    */
    end+= AUTH_PACKET_HEADER_SIZE_PROTO_41;
    bytes_remaining_in_packet-= AUTH_PACKET_HEADER_SIZE_PROTO_41;
  }
  else
  {
    protocol->set_client_capabilities(uint2korr(end));
    mpvio->max_client_packet_length= uint3korr(end + 2);
    end+= AUTH_PACKET_HEADER_SIZE_PROTO_40;
    bytes_remaining_in_packet-= AUTH_PACKET_HEADER_SIZE_PROTO_40;
    /**
      Old clients didn't have their own charset. Instead the assumption
      was that they used what ever the server used.
    */
    charset_code= global_system_variables.character_set_client->number;
  }

skip_to_ssl:
#if defined(HAVE_OPENSSL)
  DBUG_PRINT("info", ("client capabilities: %lu",
                      protocol->get_client_capabilities()));
  
  /*
    If client requested SSL then we must stop parsing, try to switch to SSL,
    and wait for the client to send a new handshake packet.
    The client isn't expected to send any more bytes until SSL is initialized.
  */
  if (protocol->has_client_capability(CLIENT_SSL))
  {
    unsigned long errptr;
#if !defined(DBUG_OFF)
    uint ssl_charset_code= 0;
#endif

    /* Do the SSL layering. */
    if (!ssl_acceptor_fd)
      return packet_error;

    DBUG_PRINT("info", ("IO layer change in progress..."));
    if (sslaccept(ssl_acceptor_fd, protocol->get_vio(),
                  protocol->get_net()->read_timeout, &errptr))
    {
      DBUG_PRINT("error", ("Failed to accept new SSL connection"));
      return packet_error;
    }

    DBUG_PRINT("info", ("Reading user information over SSL layer"));
    int rc= protocol->read_packet();
    pkt_len= protocol->get_packet_length();
    if (rc)
    {
      DBUG_PRINT("error", ("Failed to read user information (pkt_len= %lu)",
                           static_cast<ulong>(pkt_len)));
      return packet_error;
    }
    /* mark vio as encrypted */
    mpvio->vio_is_encrypted= 1;
  
    /*
      A new packet was read and the statistics reflecting the remaining bytes
      in the packet must be updated.
    */
    bytes_remaining_in_packet= pkt_len;

    /*
      After the SSL handshake is performed the client resends the handshake
      packet but because of legacy reasons we chose not to parse the packet
      fields a second time and instead only assert the length of the packet.
    */
    if (protocol->has_client_capability(CLIENT_PROTOCOL_41))
    {
      packet_has_required_size= bytes_remaining_in_packet >= 
        AUTH_PACKET_HEADER_SIZE_PROTO_41;
#if !defined(DBUG_OFF)
      ssl_charset_code=
        (uint)(uchar)*((char *)protocol->get_net()->read_pos + 8);
      DBUG_PRINT("info", ("client_character_set: %u", ssl_charset_code));
#endif
      end= (char *)protocol->get_net()->read_pos
        + AUTH_PACKET_HEADER_SIZE_PROTO_41;
      bytes_remaining_in_packet -= AUTH_PACKET_HEADER_SIZE_PROTO_41;
    }
    else
    {
      packet_has_required_size= bytes_remaining_in_packet >= 
        AUTH_PACKET_HEADER_SIZE_PROTO_40;
      end= (char *)protocol->get_net()->read_pos
        + AUTH_PACKET_HEADER_SIZE_PROTO_40;
      bytes_remaining_in_packet -= AUTH_PACKET_HEADER_SIZE_PROTO_40;
#if !defined(DBUG_OFF)
      /**
        Old clients didn't have their own charset. Instead the assumption
        was that they used what ever the server used.
      */
      ssl_charset_code= global_system_variables.character_set_client->number;
#endif
    }
    DBUG_ASSERT(charset_code == ssl_charset_code);
    if (!packet_has_required_size)
      return packet_error;
  }
#endif /* HAVE_OPENSSL */

  DBUG_PRINT("info", ("client_character_set: %u", charset_code));
  if (mpvio->charset_adapter->init_client_charset(charset_code))
    return packet_error;

  if ((protocol->has_client_capability(CLIENT_TRANSACTIONS)) &&
      opt_using_transactions)
    protocol->get_net()->return_status= mpvio->server_status;

  /*
    The 4.0 and 4.1 versions of the protocol differ on how strings
    are terminated. In the 4.0 version, if a string is at the end
    of the packet, the string is not null terminated. Do not assume
    that the returned string is always null terminated.
  */
  get_proto_string_func_t get_string;

  if (protocol->has_client_capability(CLIENT_PROTOCOL_41))
    get_string= get_41_protocol_string;
  else
    get_string= get_40_protocol_string;

  /*
    When the ability to change default plugin require that the initial password
   field can be of arbitrary size. However, the 41 client-server protocol limits
   the length of the auth-data-field sent from client to server to 255 bytes
   (CLIENT_SECURE_CONNECTION). The solution is to change the type of the field
   to a true length encoded string and indicate the protocol change with a new
   client capability flag: CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA.
  */
  get_proto_string_func_t get_length_encoded_string;

  if (protocol->has_client_capability(CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA))
    get_length_encoded_string= get_56_lenc_string;
  else
    get_length_encoded_string= get_41_lenc_string;

  /*
    In order to safely scan a head for '\0' string terminators
    we must keep track of how many bytes remain in the allocated
    buffer or we might read past the end of the buffer.
  */
  bytes_remaining_in_packet=
    pkt_len - (end - (char *)protocol->get_net()->read_pos);

  size_t user_len;
  char *user= get_string(&end, &bytes_remaining_in_packet, &user_len);
  if (user == NULL)
    return packet_error;

  /*
    Old clients send a null-terminated string as password; new clients send
    the size (1 byte) + string (not null-terminated). Hence in case of empty
    password both send '\0'.
  */
  size_t passwd_len= 0;
  char *passwd= NULL;

  passwd= get_length_encoded_string(&end, &bytes_remaining_in_packet,
                                    &passwd_len);
  if (passwd == NULL)
    return packet_error;

  size_t db_len= 0;
  char *db= NULL;

  if (protocol->has_client_capability(CLIENT_CONNECT_WITH_DB))
  {
    db= get_string(&end, &bytes_remaining_in_packet, &db_len);
    if (db == NULL)
      return packet_error;
  }

  /*
    Set the default for the password supplied flag for non-existing users
    as the default plugin (native passsword authentication) would do it
    for compatibility reasons.
  */
  if (passwd_len)
    mpvio->auth_info.password_used= PASSWORD_USED_YES;

  size_t client_plugin_len= 0;
  const char *client_plugin= get_string(&end, &bytes_remaining_in_packet,
                                  &client_plugin_len);
  if (client_plugin == NULL)
    client_plugin= &empty_c_string[0];

  if ((protocol->has_client_capability(CLIENT_CONNECT_ATTRS)) &&
      read_client_connect_attrs(&end, &bytes_remaining_in_packet,
                                mpvio->charset_adapter->charset()))
    return packet_error;

  char db_buff[NAME_LEN + 1];           // buffer to store db in utf8
  char user_buff[USERNAME_LENGTH + 1];  // buffer to store user in utf8
  uint dummy_errors;


  /*
    Copy and convert the user and database names to the character set used
    by the server. Since 4.1 all database names are stored in UTF-8. Also,
    ensure that the names are properly null-terminated as this is relied
    upon later.
  */
  if (db)
  {
    db_len= copy_and_convert(db_buff, sizeof(db_buff) - 1, system_charset_info,
                             db, db_len, mpvio->charset_adapter->charset(),
                             &dummy_errors);
    db_buff[db_len]= '\0';
    db= db_buff;
  }

  user_len= copy_and_convert(user_buff, sizeof(user_buff) - 1,
                             system_charset_info, user, user_len,
                             mpvio->charset_adapter->charset(),
                             &dummy_errors);
  user_buff[user_len]= '\0';
  user= user_buff;

  /* If username starts and ends in "'", chop them off */
  if (user_len > 1 && user[0] == '\'' && user[user_len - 1] == '\'')
  {
    user[user_len - 1]= 0;
    user++;
    user_len-= 2;
  }

  if (make_lex_string_root(mpvio->mem_root, 
                           &mpvio->db, db, db_len, 0) == 0)
    return packet_error; /* The error is set by make_lex_string(). */
  if (mpvio->auth_info.user_name)
    my_free(mpvio->auth_info.user_name);
  if (!(mpvio->auth_info.user_name= my_strndup(key_memory_MPVIO_EXT_auth_info,
                                               user, user_len, MYF(MY_WME))))
    return packet_error; /* The error is set by my_strdup(). */
  mpvio->auth_info.user_name_length= user_len;

  if (!initialized)
  {
    // if mysqld's been started with --skip-grant-tables option
    mpvio->status= MPVIO_EXT::SUCCESS;
    return packet_error;
  }

  if (find_mpvio_user(mpvio))
    return packet_error;

  if (!(protocol->has_client_capability(CLIENT_PLUGIN_AUTH)))
  {
    /* An old client is connecting */
    client_plugin= native_password_plugin_name.str;
  }
  
  /*
    if the acl_user needs a different plugin to authenticate
    (specified in GRANT ... AUTHENTICATED VIA plugin_name ..)
    we need to restart the authentication in the server.
    But perhaps the client has already used the correct plugin -
    in that case the authentication on the client may not need to be
    restarted and a server auth plugin will read the data that the client
    has just send. Cache them to return in the next server_mpvio_read_packet().
  */
  if (my_strcasecmp(system_charset_info, mpvio->acl_user_plugin.str,
                    plugin_name(mpvio->plugin)->str) != 0)
  {
    mpvio->cached_client_reply.pkt= passwd;
    mpvio->cached_client_reply.pkt_len= passwd_len;
    mpvio->cached_client_reply.plugin= client_plugin;
    mpvio->status= MPVIO_EXT::RESTART;
    return packet_error;
  }

  /*
    ok, we don't need to restart the authentication on the server.
    but if the client used the wrong plugin, we need to restart
    the authentication on the client. Do it here, the server plugin
    doesn't need to know.
  */
  const char *client_auth_plugin=
    ((st_mysql_auth *) (plugin_decl(mpvio->plugin)->info))->client_auth_plugin;

  if (client_auth_plugin &&
      my_strcasecmp(system_charset_info, client_plugin, client_auth_plugin))
  {
    mpvio->cached_client_reply.plugin= client_plugin;
    if (send_plugin_request_packet(mpvio,
                                   (uchar*) mpvio->cached_server_packet.pkt,
                                   mpvio->cached_server_packet.pkt_len))
      return packet_error;

    mpvio->protocol->read_packet();
    passwd_len= protocol->get_packet_length();
    passwd= (char *)protocol->get_net()->read_pos;
  }

  *buff= (uchar *) passwd;
  return passwd_len;
#else
  return 0;
#endif /* EMBEDDED_LIBRARY */
}


/**
  Make sure that when sending plugin supplied data to the client they
  are not considered a special out-of-band command, like e.g. 
  \255 (error) or \254 (change user request packet) or \0 (OK).
  To avoid this the server will send all plugin data packets "wrapped" 
  in a command \1.
  Note that the client will continue sending its replies unrwapped.
*/

static inline int 
wrap_plguin_data_into_proper_command(NET *net, 
                                     const uchar *packet, int packet_len)
{
  return net_write_command(net, 1, (uchar *) "", 0, packet, packet_len);
}

/*
  Note: The following functions are declared inside extern "C" because
  they are used to initialize C structure MPVIO (see
  server_mpvio_initialize()).
*/

extern "C" {

/**
  vio->write_packet() callback method for server authentication plugins

  This function is called by a server authentication plugin, when it wants
  to send data to the client.

  It transparently wraps the data into a handshake packet,
  and handles plugin negotiation with the client. If necessary,
  it escapes the plugin data, if it starts with a mysql protocol packet byte.
*/
static int server_mpvio_write_packet(MYSQL_PLUGIN_VIO *param,
                                   const uchar *packet, int packet_len)
{
  MPVIO_EXT *mpvio= (MPVIO_EXT *) param;
  int res;
  Protocol_classic *protocol = mpvio->protocol;

  DBUG_ENTER("server_mpvio_write_packet");
  /* 
    Reset cached_client_reply if not an old client doing mysql_change_user, 
    as this is where the password from COM_CHANGE_USER is stored.
  */
  if (!((!(protocol->has_client_capability(CLIENT_PLUGIN_AUTH))) &&
        mpvio->status == MPVIO_EXT::RESTART &&
        mpvio->cached_client_reply.plugin == 
        ((st_mysql_auth *) (plugin_decl(mpvio->plugin)->info))->client_auth_plugin
        ))
    mpvio->cached_client_reply.pkt= 0;
  /* for the 1st packet we wrap plugin data into the handshake packet */
  if (mpvio->packets_written == 0)
    res= send_server_handshake_packet(mpvio, (char*) packet, packet_len);
  else if (mpvio->status == MPVIO_EXT::RESTART)
    res= send_plugin_request_packet(mpvio, packet, packet_len);
  else
    res= wrap_plguin_data_into_proper_command(protocol->get_net(),
                                              packet, packet_len);
  mpvio->packets_written++;
  DBUG_RETURN(res);
}

/**
  vio->read_packet() callback method for server authentication plugins

  This function is called by a server authentication plugin, when it wants
  to read data from the client.

  It transparently extracts the client plugin data, if embedded into
  a client authentication handshake packet, and handles plugin negotiation
  with the client, if necessary.

  RETURN
    -1          Protocol failure
    >= 0        Success and also the packet length
*/
static int server_mpvio_read_packet(MYSQL_PLUGIN_VIO *param, uchar **buf)
{
  MPVIO_EXT *mpvio= (MPVIO_EXT *) param;
  Protocol_classic *protocol = mpvio->protocol;
  size_t pkt_len;

  DBUG_ENTER("server_mpvio_read_packet");
  if (mpvio->packets_written == 0)
  {
    /*
      plugin wants to read the data without sending anything first.
      send an empty packet to force a server handshake packet to be sent
    */
    if (mpvio->write_packet(mpvio, 0, 0))
      pkt_len= packet_error;
    else
    {
      protocol->read_packet();
      pkt_len= protocol->get_packet_length();
    }
  }
  else if (mpvio->cached_client_reply.pkt)
  {
    DBUG_ASSERT(mpvio->status == MPVIO_EXT::RESTART);
    DBUG_ASSERT(mpvio->packets_read > 0);
    /*
      if the have the data cached from the last server_mpvio_read_packet
      (which can be the case if it's a restarted authentication)
      and a client has used the correct plugin, then we can return the
      cached data straight away and avoid one round trip.
    */
    const char *client_auth_plugin=
      ((st_mysql_auth *) (plugin_decl(mpvio->plugin)->info))->client_auth_plugin;
    if (client_auth_plugin == 0 ||
        my_strcasecmp(system_charset_info, mpvio->cached_client_reply.plugin,
                      client_auth_plugin) == 0)
    {
      mpvio->status= MPVIO_EXT::FAILURE;
      *buf= (uchar*) mpvio->cached_client_reply.pkt;
      mpvio->cached_client_reply.pkt= 0;
      mpvio->packets_read++;
      DBUG_RETURN ((int) mpvio->cached_client_reply.pkt_len);
    }

    /* older clients don't support change of client plugin request */
    if (!(protocol->has_client_capability(CLIENT_PLUGIN_AUTH)))
    {
      mpvio->status= MPVIO_EXT::FAILURE;
      pkt_len= packet_error;
      goto err;
    }

    /*
      But if the client has used the wrong plugin, the cached data are
      useless. Furthermore, we have to send a "change plugin" request
      to the client.
    */
    if (mpvio->write_packet(mpvio, 0, 0))
      pkt_len= packet_error;
    else
    {
      protocol->read_packet();
      pkt_len= protocol->get_packet_length();
    }
  }
  else
  {
    protocol->read_packet();
    pkt_len= protocol->get_packet_length();
  }

  if (pkt_len == packet_error)
    goto err;

  mpvio->packets_read++;

  /*
    the 1st packet has the plugin data wrapped into the client authentication
    handshake packet
  */
  if (mpvio->packets_read == 1)
  {
    pkt_len= parse_client_handshake_packet(mpvio, buf, pkt_len);
    if (pkt_len == packet_error)
      goto err;
  }
  else
    *buf= protocol->get_net()->read_pos;

  DBUG_RETURN((int)pkt_len);

err:
  if (mpvio->status == MPVIO_EXT::FAILURE)
  {
    my_error(ER_HANDSHAKE_ERROR, MYF(0));
  }
  DBUG_RETURN(-1);
}

/**
  fills MYSQL_PLUGIN_VIO_INFO structure with the information about the
  connection
*/
static void server_mpvio_info(MYSQL_PLUGIN_VIO *vio,
                              MYSQL_PLUGIN_VIO_INFO *info)
{
  MPVIO_EXT *mpvio= (MPVIO_EXT *) vio;
  mpvio_info(mpvio->protocol->get_net()->vio, info);
}

} // extern "C"

static int do_auth_once(THD *thd, const LEX_CSTRING &auth_plugin_name,
                        MPVIO_EXT *mpvio)
{
  DBUG_ENTER("do_auth_once");
  int res= CR_OK, old_status= MPVIO_EXT::FAILURE;
  bool unlock_plugin= false;
  plugin_ref plugin= NULL;

  if (auth_plugin_name.str == native_password_plugin_name.str)
    plugin= native_password_plugin;
#ifndef EMBEDDED_LIBRARY
  else
  {
    if ((plugin= my_plugin_lock_by_name(thd, auth_plugin_name,
                                        MYSQL_AUTHENTICATION_PLUGIN)))
      unlock_plugin= true;
  }
#endif /* EMBEDDED_LIBRARY */

    
  mpvio->plugin= plugin;
  old_status= mpvio->status;
  
  if (plugin)
  {
    st_mysql_auth *auth= (st_mysql_auth *) plugin_decl(plugin)->info;
    res= auth->authenticate_user(mpvio, &mpvio->auth_info);

    if (unlock_plugin)
      plugin_unlock(thd, plugin);
  }
  else
  {
    /* Server cannot load the required plugin. */
    Host_errors errors;
    errors.m_no_auth_plugin= 1;
    inc_host_errors(mpvio->ip, &errors);
    my_error(ER_PLUGIN_IS_NOT_LOADED, MYF(0), auth_plugin_name.str);
    res= CR_ERROR;
  }

  /*
    If the status was MPVIO_EXT::RESTART before the authenticate_user() call
    it can never be MPVIO_EXT::RESTART after the call, because any call
    to write_packet() or read_packet() will reset the status.

    But (!) if a plugin never called a read_packet() or write_packet(), the
    status will stay unchanged. We'll fix it, by resetting the status here.
  */
  if (old_status == MPVIO_EXT::RESTART && mpvio->status == MPVIO_EXT::RESTART)
    mpvio->status= MPVIO_EXT::FAILURE; // reset to the default

  DBUG_RETURN(res);
}


static void
server_mpvio_initialize(THD *thd, MPVIO_EXT *mpvio,
                        Thd_charset_adapter *charset_adapter)
{
  LEX_CSTRING sctx_host_or_ip= thd->security_context()->host_or_ip();

  memset(mpvio, 0, sizeof(MPVIO_EXT));
  mpvio->read_packet= server_mpvio_read_packet;
  mpvio->write_packet= server_mpvio_write_packet;
  mpvio->info= server_mpvio_info;
  mpvio->auth_info.user_name= NULL;
  mpvio->auth_info.user_name_length= 0;
  mpvio->auth_info.host_or_ip= sctx_host_or_ip.str;
  mpvio->auth_info.host_or_ip_length= sctx_host_or_ip.length;

#if defined(HAVE_OPENSSL) && !defined(EMBEDDED_LIBRARY)
  Vio *vio= thd->get_protocol_classic()->get_vio();
  if (vio->ssl_arg)
    mpvio->vio_is_encrypted= 1;
  else
#endif /* HAVE_OPENSSL && !EMBEDDED_LIBRARY */
    mpvio->vio_is_encrypted= 0;
  mpvio->status= MPVIO_EXT::FAILURE;
  mpvio->mem_root= thd->mem_root;
  mpvio->scramble= thd->scramble;
  mpvio->rand= &thd->rand;
  mpvio->thread_id= thd->thread_id();
  mpvio->server_status= &thd->server_status;
  mpvio->protocol= thd->get_protocol_classic();
  mpvio->ip= (char *) thd->security_context()->ip().str;
  mpvio->host= (char *) thd->security_context()->host().str;
  mpvio->charset_adapter= charset_adapter;
}



static void
server_mpvio_update_thd(THD *thd, MPVIO_EXT *mpvio)
{
  thd->max_client_packet_length= mpvio->max_client_packet_length;
  if (mpvio->protocol->has_client_capability(CLIENT_INTERACTIVE))
    thd->variables.net_wait_timeout= thd->variables.net_interactive_timeout;
  thd->security_context()->assign_user(
    mpvio->auth_info.user_name,
    (mpvio->auth_info.user_name ? strlen(mpvio->auth_info.user_name) : 0));
  if (mpvio->auth_info.user_name)
    my_free(mpvio->auth_info.user_name);
  LEX_CSTRING sctx_user= thd->security_context()->user();
  mpvio->auth_info.user_name= (char *) sctx_user.str;
  mpvio->auth_info.user_name_length= sctx_user.length;
  if (thd->get_protocol()->has_client_capability(CLIENT_IGNORE_SPACE))
    thd->variables.sql_mode|= MODE_IGNORE_SPACE;
}

/**
  Calculate the timestamp difference for password expiry

  @param thd			 thread handle
  @param acl_user		 ACL_USER handle

  @retval 0  password is valid
  @retval 1  password has expired
*/
bool
check_password_lifetime(THD *thd, const ACL_USER *acl_user)
{

  bool password_time_expired= false;

  if (likely(acl_user != NULL) && !acl_user->password_expired &&
      acl_user->password_last_changed.time_type != MYSQL_TIMESTAMP_ERROR
      && auth_plugin_is_built_in(acl_user->plugin.str)
      && (acl_user->use_default_password_lifetime ||
      acl_user->password_lifetime))
  {
    MYSQL_TIME cur_time, password_change_by;
    Interval interval;

    thd->set_time();
    thd->variables.time_zone->gmt_sec_to_TIME(&cur_time,
      static_cast<my_time_t>(thd->query_start()));
    password_change_by= acl_user->password_last_changed;
    memset(&interval, 0, sizeof(interval));

    if (!acl_user->use_default_password_lifetime)
      interval.day= acl_user->password_lifetime;
    else
    {
      Mutex_lock lock(&LOCK_default_password_lifetime);
      interval.day= default_password_lifetime;
    }
    if (interval.day)
    {
      if (!date_add_interval(&password_change_by, INTERVAL_DAY, interval))
        password_time_expired= my_time_compare(&password_change_by,
                                               &cur_time) >=0 ? false: true;
      else
      {
        DBUG_ASSERT(FALSE);
        /* Make the compiler happy. */
      }
    }
  }
  DBUG_EXECUTE_IF("force_password_interval_expire",
                  {
                    if (!acl_user->use_default_password_lifetime &&
                        acl_user->password_lifetime)
                      password_time_expired= true;
                  });
  DBUG_EXECUTE_IF("force_password_interval_expire_for_time_type",
                  {
                    if (acl_user->password_last_changed.time_type !=
                        MYSQL_TIMESTAMP_ERROR)
                      password_time_expired= true;
                  });
  return password_time_expired;
}

/**
Logging connection for the general query log, extracted from
acl_authenticate() as it's reused at different times based on
whether proxy users are checked.

@param user                    authentication user name
@param host                    authentication user host or IP address
@param auth_as                 privilege user name
@param db                      default database
@param thd                     thread handle
@param command                 type of command(connect or change user)
*/
void
acl_log_connect(const char *user,
                const char *host,
                const char *auth_as,
                const char *db,
                THD *thd,
                enum enum_server_command command)
{
  const char *vio_name_str= NULL;
  int len= 0;
  get_vio_type_name(thd->get_vio_type(), & vio_name_str, & len);

  if (strcmp(auth_as, user) && (PROXY_FLAG != *auth_as))
  {
    query_logger.general_log_print(thd, command, "%s@%s as %s on %s using %s",
      user,
      host,
      auth_as,
      db ? db : (char*) "",
      vio_name_str);
  }
  else
  {
    query_logger.general_log_print(thd, command, "%s@%s on %s using %s",
      user,
      host,
      db ? db : (char*) "",
      vio_name_str);
  }
}

/*
  Assign priv_user and priv_host fields of the Security_context.

  @param sctx Security context, which priv_user and priv_host fields are
              updated.
  @param user Authenticated user data.
*/
inline void
assign_priv_user_host(Security_context *sctx, ACL_USER *user)
{
  sctx->assign_priv_user(user->user, user->user ? strlen(user->user) : 0);
  sctx->assign_priv_host(user->host.get_host(), user->host.get_host_len());
}

/**
  Perform the handshake, authorize the client and update thd sctx variables.

  @param thd                     thread handle
  @param command                 the command to be executed, it can be either a
                                 COM_CHANGE_USER or COM_CONNECT (if
                                 it's a new connection)

  @retval 0  success, thd is updated.
  @retval 1  error
*/
int
acl_authenticate(THD *thd, enum_server_command command)
{
  int res= CR_OK;
  MPVIO_EXT mpvio;
  LEX_CSTRING auth_plugin_name= default_auth_plugin_name;
  Thd_charset_adapter charset_adapter(thd);

  DBUG_ENTER("acl_authenticate");
  compile_time_assert(MYSQL_USERNAME_LENGTH == USERNAME_LENGTH);
  DBUG_ASSERT(command == COM_CONNECT || command == COM_CHANGE_USER);

  server_mpvio_initialize(thd, &mpvio, &charset_adapter);
  /*
    Clear thd->db as it points to something, that will be freed when
    connection is closed. We don't want to accidentally free a wrong
    pointer if connect failed.
  */
  thd->reset_db(NULL_CSTR);

  auth_plugin_name= default_auth_plugin_name;
  /* acl_authenticate() takes the data from net->read_pos */
  thd->get_protocol_classic()->get_net()->read_pos=
    thd->get_protocol_classic()->get_raw_packet();
  DBUG_PRINT("info", ("com_change_user_pkt_len=%u",
    mpvio.protocol->get_packet_length()));

  if (command == COM_CHANGE_USER)
  {
    mpvio.packets_written++; // pretend that a server handshake packet was sent
    mpvio.packets_read++;    // take COM_CHANGE_USER packet into account

    /* Clear variables that are allocated */
    thd->set_user_connect(NULL);

    if (parse_com_change_user_packet(&mpvio,
                                     mpvio.protocol->get_packet_length()))
    {
      if (!thd->is_error())
        login_failed_error(&mpvio, mpvio.auth_info.password_used);
      server_mpvio_update_thd(thd, &mpvio);
      DBUG_RETURN(1);
    }

    DBUG_ASSERT(mpvio.status == MPVIO_EXT::RESTART ||
                mpvio.status == MPVIO_EXT::SUCCESS);
  }
  else
  {
    /* mark the thd as having no scramble yet */
    mpvio.scramble[SCRAMBLE_LENGTH]= 1;
    
    /*
     perform the first authentication attempt, with the default plugin.
     This sends the server handshake packet, reads the client reply
     with a user name, and performs the authentication if everyone has used
     the correct plugin.
    */

    res= do_auth_once(thd, auth_plugin_name, &mpvio);
  }

  /*
   retry the authentication, if - after receiving the user name -
   we found that we need to switch to a non-default plugin
  */
  if (mpvio.status == MPVIO_EXT::RESTART)
  {
    DBUG_ASSERT(mpvio.acl_user);
    DBUG_ASSERT(command == COM_CHANGE_USER ||
                my_strcasecmp(system_charset_info, auth_plugin_name.str,
                              mpvio.acl_user->plugin.str));
    auth_plugin_name= mpvio.acl_user->plugin;
    res= do_auth_once(thd, auth_plugin_name, &mpvio);
    if (res <= CR_OK)
    {
      if (auth_plugin_name.str == native_password_plugin_name.str)
        thd->variables.old_passwords= 0;
      if (auth_plugin_name.str == sha256_password_plugin_name.str)
        thd->variables.old_passwords= 2;
    }
  }

  server_mpvio_update_thd(thd, &mpvio);
#ifdef HAVE_PSI_THREAD_INTERFACE
  PSI_THREAD_CALL(set_connection_type)(thd->get_vio_type());
#endif /* HAVE_PSI_THREAD_INTERFACE */

  Security_context *sctx= thd->security_context();
  const ACL_USER *acl_user= mpvio.acl_user;
  bool proxy_check= check_proxy_users && !*mpvio.auth_info.authenticated_as;

  DBUG_PRINT("info", ("proxy_check=%s", proxy_check ? "true" : "false"));

  thd->password= mpvio.auth_info.password_used;  // remember for error messages

  // reset authenticated_as because flag value received, but server
  // proxy mapping is disabled:
  if ((!check_proxy_users) && acl_user && !*mpvio.auth_info.authenticated_as)
  {
    DBUG_PRINT("info", ("setting authenticated_as to %s as check_proxy_user is OFF.",
      mpvio.auth_info.user_name));
    strcpy(mpvio.auth_info.authenticated_as, acl_user->user ? acl_user->user : "");
  }
  /*
    Log the command here so that the user can check the log
    for the tried logins and also to detect break-in attempts.

    if sctx->user is unset it's protocol failure, bad packet.
  */
  if (mpvio.auth_info.user_name && !proxy_check)
  {
    acl_log_connect(mpvio.auth_info.user_name, mpvio.auth_info.host_or_ip,
      mpvio.auth_info.authenticated_as, mpvio.db.str, thd, command);
  }
  if (res == CR_OK &&
      (!mpvio.can_authenticate() || thd->is_error()))
  {
    res= CR_ERROR;
  }

  /*
    Assign account user/host data to the current THD. This information is used
    when the authentication fails after this point and we call audit api
    notification event. Client user/host connects to the existing account is
    easily distinguished from other connects.
  */
  if (mpvio.can_authenticate())
    assign_priv_user_host(sctx, const_cast<ACL_USER *>(acl_user));

  if (res > CR_OK && mpvio.status != MPVIO_EXT::SUCCESS)
  {
    Host_errors errors;
    DBUG_ASSERT(mpvio.status == MPVIO_EXT::FAILURE);
    switch (res)
    {
    case CR_AUTH_PLUGIN_ERROR:
      errors.m_auth_plugin= 1;
      break;
    case CR_AUTH_HANDSHAKE:
      errors.m_handshake= 1;
      break;
    case CR_AUTH_USER_CREDENTIALS:
      errors.m_authentication= 1;
      break;
    case CR_ERROR:
    default:
      /* Unknown of unspecified auth plugin error. */
      errors.m_auth_plugin= 1;
      break;
    }
    inc_host_errors(mpvio.ip, &errors);
    if (mpvio.auth_info.user_name && proxy_check)
    {
      acl_log_connect(mpvio.auth_info.user_name, mpvio.auth_info.host_or_ip,
        mpvio.auth_info.authenticated_as, mpvio.db.str, thd, command);
    }
    if (!thd->is_error())
      login_failed_error(&mpvio, mpvio.auth_info.password_used);
    DBUG_RETURN (1);
  }

  sctx->assign_proxy_user("", 0);

  if (initialized) // if not --skip-grant-tables
  {
#ifndef NO_EMBEDDED_ACCESS_CHECKS
    bool is_proxy_user= FALSE;
    bool password_time_expired= false;
    const char *auth_user = acl_user->user ? acl_user->user : "";
    ACL_PROXY_USER *proxy_user;
    /* check if the user is allowed to proxy as another user */
    mysql_mutex_lock(&acl_cache->lock);
    proxy_user= acl_find_proxy_user(auth_user, sctx->host().str, sctx->ip().str,
                                    mpvio.auth_info.authenticated_as,
                                    &is_proxy_user);
    mysql_mutex_unlock(&acl_cache->lock);
    if (mpvio.auth_info.user_name && proxy_check)
    {
      acl_log_connect(mpvio.auth_info.user_name, mpvio.auth_info.host_or_ip,
        mpvio.auth_info.authenticated_as, mpvio.db.str, thd, command);
    }

    if (thd->is_error())
      DBUG_RETURN(1);

    if (is_proxy_user)
    {
      ACL_USER *acl_proxy_user;
      char proxy_user_buf[USERNAME_LENGTH + MAX_HOSTNAME + 5];

      /* we need to find the proxy user, but there was none */
      if (!proxy_user)
      {
        Host_errors errors;
        errors.m_proxy_user= 1;
        inc_host_errors(mpvio.ip, &errors);
        if (!thd->is_error())
          login_failed_error(&mpvio, mpvio.auth_info.password_used);
        DBUG_RETURN(1);
      }

      my_snprintf(proxy_user_buf, sizeof(proxy_user_buf) - 1,
                  "'%s'@'%s'", auth_user,
                  acl_user->host.get_host() ? acl_user->host.get_host() : "");
      sctx->assign_proxy_user(proxy_user_buf, strlen(proxy_user_buf));

      /* we're proxying : find the proxy user definition */
      mysql_mutex_lock(&acl_cache->lock);
      acl_proxy_user= find_acl_user(proxy_user->get_proxied_host() ? 
                                    proxy_user->get_proxied_host() : "",
                                    mpvio.auth_info.authenticated_as, TRUE);
      if (!acl_proxy_user)
      {
        Host_errors errors;
        errors.m_proxy_user_acl= 1;
        inc_host_errors(mpvio.ip, &errors);
        if (!thd->is_error())
          login_failed_error(&mpvio, mpvio.auth_info.password_used);
        mysql_mutex_unlock(&acl_cache->lock);
        DBUG_RETURN(1);
      }
      acl_user= acl_proxy_user->copy(thd->mem_root);
      DBUG_PRINT("info", ("User %s is a PROXY and will assume a PROXIED"
                          " identity %s", auth_user, acl_user->user));
      mysql_mutex_unlock(&acl_cache->lock);
    }
#endif /* NO_EMBEDDED_ACCESS_CHECKS */

    sctx->set_master_access(acl_user->access);
    assign_priv_user_host(sctx, const_cast<ACL_USER *>(acl_user));

    if (!(sctx->check_access(SUPER_ACL)) && !thd->is_error())
    {
      mysql_mutex_lock(&LOCK_offline_mode);
      bool tmp_offline_mode= MY_TEST(offline_mode);
      mysql_mutex_unlock(&LOCK_offline_mode);

      if (tmp_offline_mode)
      {
	my_error(ER_SERVER_OFFLINE_MODE, MYF(0));
        DBUG_RETURN(1);
      }
    }

#ifndef NO_EMBEDDED_ACCESS_CHECKS
    /*
      OK. Let's check the SSL. Historically it was checked after the password,
      as an additional layer, not instead of the password
      (in which case it would've been a plugin too).
    */
    if (acl_check_ssl(thd, acl_user))
    {
      Host_errors errors;
      errors.m_ssl= 1;
      inc_host_errors(mpvio.ip, &errors);
      if (!thd->is_error())
        login_failed_error(&mpvio, thd->password);
      DBUG_RETURN(1);
    }

    /*
      Check whether the account has been locked.
    */
    if (unlikely(mpvio.acl_user->account_locked))
    {
      locked_account_connection_count++;

      my_error(ER_ACCOUNT_HAS_BEEN_LOCKED, MYF(0),
               mpvio.acl_user->user, mpvio.auth_info.host_or_ip);
      sql_print_information(ER(ER_ACCOUNT_HAS_BEEN_LOCKED),
                            mpvio.acl_user->user, mpvio.auth_info.host_or_ip);
      DBUG_RETURN(1);
    }

    if (opt_require_secure_transport &&
        !is_secure_transport(thd->active_vio->type))
    {
      my_error(ER_SECURE_TRANSPORT_REQUIRED, MYF(0));
      DBUG_RETURN(1);
    }


    /* checking password_time_expire for connecting user */
    password_time_expired= check_password_lifetime(thd, mpvio.acl_user);

    if (unlikely(mpvio.acl_user && (mpvio.acl_user->password_expired ||
        password_time_expired) &&
        !(mpvio.protocol->has_client_capability(
            CLIENT_CAN_HANDLE_EXPIRED_PASSWORDS))
        && disconnect_on_expired_password))
    {
      /*
        Clients that don't signal password expiration support
        get a connect error.
      */
      Host_errors errors;

      my_error(ER_MUST_CHANGE_PASSWORD_LOGIN, MYF(0));
      query_logger.general_log_print(thd, COM_CONNECT,
                                     ER(ER_MUST_CHANGE_PASSWORD_LOGIN));
      sql_print_information("%s", ER(ER_MUST_CHANGE_PASSWORD_LOGIN));

      errors.m_authentication= 1;
      inc_host_errors(mpvio.ip, &errors);
      DBUG_RETURN(1);
    }

    /* Don't allow the user to connect if he has done too many queries */
    if ((acl_user->user_resource.questions || acl_user->user_resource.updates ||
         acl_user->user_resource.conn_per_hour ||
         acl_user->user_resource.user_conn ||
         global_system_variables.max_user_connections) &&
        get_or_create_user_conn(thd,
          (opt_old_style_user_limits ? sctx->user().str :
                                       sctx->priv_user().str),
          (opt_old_style_user_limits ? sctx->host_or_ip().str :
                                       sctx->priv_host().str),
          &acl_user->user_resource))
      DBUG_RETURN(1); // The error is set by get_or_create_user_conn()

    /*
      We are copying the connected user's password expired flag to the security
      context.
      This allows proxy user to execute queries even if proxied user password
      expires.
    */
    sctx->set_password_expired(mpvio.acl_user->password_expired ||
                               password_time_expired);
#endif /* NO_EMBEDDED_ACCESS_CHECKS */
  }
  else
    sctx->skip_grants();

  const USER_CONN *uc;
  if ((uc= thd->get_user_connect()) &&
      (uc->user_resources.conn_per_hour || uc->user_resources.user_conn ||
       global_system_variables.max_user_connections) &&
       check_for_max_user_connections(thd, uc))
  {
    DBUG_RETURN(1); // The error is set in check_for_max_user_connections()
  }

  DBUG_PRINT("info",
             ("Capabilities: %lu  packet_length: %ld  Host: '%s'  "
              "Login user: '%s' Priv_user: '%s'  Using password: %s "
              "Access: %lu  db: '%s'",
              thd->get_protocol()->get_client_capabilities(),
              thd->max_client_packet_length,
              sctx->host_or_ip().str, sctx->user().str, sctx->priv_user().str,
              thd->password ? "yes": "no",
              sctx->master_access(), mpvio.db.str));

  if (command == COM_CONNECT &&
      !(thd->m_main_security_ctx.check_access(SUPER_ACL)))
  {
#ifndef EMBEDDED_LIBRARY
    if (!Connection_handler_manager::get_instance()->valid_connection_count())
    {                                         // too many connections
      release_user_connection(thd);
      my_error(ER_CON_COUNT_ERROR, MYF(0));
      DBUG_RETURN(1);
    }
#endif // !EMBEDDED_LIBRARY
  }

  /*
    This is the default access rights for the current database.  It's
    set to 0 here because we don't have an active database yet (and we
    may not have an active database to set.
  */
  sctx->set_db_access(0);

  /* Change a database if necessary */
  if (mpvio.db.length)
  {
    if (mysql_change_db(thd, to_lex_cstring(mpvio.db), false))
    {
      /* mysql_change_db() has pushed the error message. */
      release_user_connection(thd);
      Host_errors errors;
      errors.m_default_database= 1;
      inc_host_errors(mpvio.ip, &errors);
      DBUG_RETURN(1);
    }
  }

  if (mpvio.auth_info.external_user[0])
    sctx->assign_external_user(mpvio.auth_info.external_user,
                               strlen(mpvio.auth_info.external_user));


  if (res == CR_OK_HANDSHAKE_COMPLETE)
    thd->get_stmt_da()->disable_status();
  else
    my_ok(thd);

#ifdef HAVE_PSI_THREAD_INTERFACE
  LEX_CSTRING main_sctx_user= thd->m_main_security_ctx.user();
  LEX_CSTRING main_sctx_host_or_ip= thd->m_main_security_ctx.host_or_ip();
  PSI_THREAD_CALL(set_thread_account)
    (main_sctx_user.str, main_sctx_user.length,
     main_sctx_host_or_ip.str, main_sctx_host_or_ip.length);
#endif /* HAVE_PSI_THREAD_INTERFACE */

  /* Ready to handle queries */
  DBUG_RETURN(0);
}

bool is_secure_transport(int vio_type)
{
  switch (vio_type)
  {
    case VIO_TYPE_SSL:
    case VIO_TYPE_SHARED_MEMORY:
    case VIO_TYPE_SOCKET:
      return TRUE;
  }
  return FALSE;
}

int generate_native_password(char *outbuf, unsigned int *buflen,
                             const char *inbuf, unsigned int inbuflen)
{
  if (my_validate_password_policy(inbuf, inbuflen))
    return 1;
  /* for empty passwords */
  if (inbuflen == 0)
  {
    *buflen= 0;
    return 0;
  }
  char *buffer= (char*)my_malloc(PSI_NOT_INSTRUMENTED,
                                 SCRAMBLED_PASSWORD_CHAR_LENGTH+1,
                                 MYF(0));
  if (buffer == NULL)
    return 1;
  my_make_scrambled_password_sha1(buffer, inbuf, inbuflen);
  /*
    if buffer specified by server is smaller than the buffer given
    by plugin then return error
  */
  if (*buflen < strlen(buffer))
  {
    my_free(buffer);
    return 1;
  }
  *buflen= SCRAMBLED_PASSWORD_CHAR_LENGTH;
  memcpy(outbuf, buffer, *buflen);
  my_free(buffer);
  return 0;
}

int validate_native_password_hash(char* const inbuf, unsigned int buflen)
{
  /* empty password is also valid */
  if ((buflen &&
      buflen == SCRAMBLED_PASSWORD_CHAR_LENGTH && inbuf[0] == '*') ||
      buflen == 0)
    return 0;
  return 1;
}

int set_native_salt(const char* password, unsigned int password_len,
                    unsigned char* salt, unsigned char *salt_len)
{
  /* for empty passwords salt_len is 0 */
  if (password_len == 0)
    *salt_len= 0;
  else
  {
    if (password_len == SCRAMBLED_PASSWORD_CHAR_LENGTH)
    {
      get_salt_from_password(salt, password);
      *salt_len= SCRAMBLE_LENGTH;
    }
  }
  return 0;
}

#if defined(HAVE_OPENSSL)
int generate_sha256_password(char *outbuf, unsigned int *buflen,
                             const char *inbuf, unsigned int inbuflen)
{
  if (inbuflen > SHA256_PASSWORD_MAX_PASSWORD_LENGTH ||
      my_validate_password_policy(inbuf, inbuflen))
    return 1;
  if (inbuflen == 0)
  {
    *buflen= 0;
    return 0;
  }
  char *buffer= (char*)my_malloc(PSI_NOT_INSTRUMENTED,
                                 CRYPT_MAX_PASSWORD_SIZE+1,
                                 MYF(0));
  if (buffer == NULL)
    return 1;
  my_make_scrambled_password(buffer, inbuf, inbuflen);
  memcpy(outbuf, buffer, CRYPT_MAX_PASSWORD_SIZE);
  /*
    if buffer specified by server is smaller than the buffer given
    by plugin then return error
  */
  if (*buflen < strlen(buffer))
  {
    my_free(buffer);
    return 1;
  }
  *buflen= strlen(buffer);
  my_free(buffer);
  return 0;
}

int validate_sha256_password_hash(char* const inbuf, unsigned int buflen)
{
  if ((inbuf && inbuf[0] == '$' &&
      inbuf[1] == '5' && inbuf[2] == '$' &&
      buflen < CRYPT_MAX_PASSWORD_SIZE+1) ||
      buflen == 0)
    return 0;
  return 1;
}

int set_sha256_salt(const char* password MY_ATTRIBUTE((unused)),
                    unsigned int password_len MY_ATTRIBUTE((unused)),
                    unsigned char* salt MY_ATTRIBUTE((unused)),
                    unsigned char *salt_len)
{
  *salt_len= 0;
  return 0;
}

#endif


/**
  MySQL Server Password Authentication Plugin

  In the MySQL authentication protocol:
  1. the server sends the random scramble to the client
  2. client sends the encrypted password back to the server
  3. the server checks the password.
*/
static int native_password_authenticate(MYSQL_PLUGIN_VIO *vio,
                                        MYSQL_SERVER_AUTH_INFO *info)
{
  uchar *pkt;
  int pkt_len;
  MPVIO_EXT *mpvio= (MPVIO_EXT *) vio;

  DBUG_ENTER("native_password_authenticate");

  /* generate the scramble, or reuse the old one */
  if (mpvio->scramble[SCRAMBLE_LENGTH])
    generate_user_salt(mpvio->scramble, SCRAMBLE_LENGTH + 1);

  /* send it to the client */
  if (mpvio->write_packet(mpvio, (uchar*) mpvio->scramble, SCRAMBLE_LENGTH + 1))
    DBUG_RETURN(CR_AUTH_HANDSHAKE);

  /* reply and authenticate */

  /*
    <digression>
      This is more complex than it looks.

      The plugin (we) may be called right after the client was connected -
      and will need to send a scramble, read reply, authenticate.

      Or the plugin may be called after another plugin has sent a scramble,
      and read the reply. If the client has used the correct client-plugin,
      we won't need to read anything here from the client, the client
      has already sent a reply with everything we need for authentication.

      Or the plugin may be called after another plugin has sent a scramble,
      and read the reply, but the client has used the wrong client-plugin.
      We'll need to sent a "switch to another plugin" packet to the
      client and read the reply. "Use the short scramble" packet is a special
      case of "switch to another plugin" packet.

      Or, perhaps, the plugin may be called after another plugin has
      done the handshake but did not send a useful scramble. We'll need
      to send a scramble (and perhaps a "switch to another plugin" packet)
      and read the reply.

      Besides, a client may be an old one, that doesn't understand plugins.
      Or doesn't even understand 4.0 scramble.

      And we want to keep the same protocol on the wire  unless non-native
      plugins are involved.

      Anyway, it still looks simple from a plugin point of view:
      "send the scramble, read the reply and authenticate"
      All the magic is transparently handled by the server.
    </digression>
  */

  /* read the reply with the encrypted password */
  if ((pkt_len= mpvio->read_packet(mpvio, &pkt)) < 0)
    DBUG_RETURN(CR_AUTH_HANDSHAKE);
  DBUG_PRINT("info", ("reply read : pkt_len=%d", pkt_len));

#ifdef NO_EMBEDDED_ACCESS_CHECKS
  DBUG_RETURN(CR_OK);
#endif /* NO_EMBEDDED_ACCESS_CHECKS */

  DBUG_EXECUTE_IF("native_password_bad_reply",
                  {
                    /* This should cause a HANDSHAKE ERROR */
                    pkt_len= 12;
                  }
                  );
  if (mysql_native_password_proxy_users)
  {
    *info->authenticated_as= PROXY_FLAG;
	DBUG_PRINT("info", ("mysql_native_authentication_proxy_users is enabled, setting authenticated_as to NULL"));
  }
  if (pkt_len == 0) /* no password */
    DBUG_RETURN(mpvio->acl_user->salt_len != 0 ?
                CR_AUTH_USER_CREDENTIALS : CR_OK);

  info->password_used= PASSWORD_USED_YES;
  if (pkt_len == SCRAMBLE_LENGTH)
  {
    if (!mpvio->acl_user->salt_len)
      DBUG_RETURN(CR_AUTH_USER_CREDENTIALS);

    DBUG_RETURN(check_scramble(pkt, mpvio->scramble, mpvio->acl_user->salt) ?
                CR_AUTH_USER_CREDENTIALS : CR_OK);
  }

  my_error(ER_HANDSHAKE_ERROR, MYF(0));
  DBUG_RETURN(CR_AUTH_HANDSHAKE);
}

/**
  Interface for querying the MYSQL_PUBLIC_VIO about encryption state.
 
*/

int my_vio_is_encrypted(MYSQL_PLUGIN_VIO *vio)
{
  MPVIO_EXT *mpvio= (MPVIO_EXT *) vio;
  return (mpvio->vio_is_encrypted);
}

#if defined(HAVE_OPENSSL)
#ifndef HAVE_YASSL

int show_rsa_public_key(THD *thd, SHOW_VAR *var, char *buff)
{ 
  var->type= SHOW_CHAR;
  var->value= const_cast<char *>(g_rsa_keys.get_public_key_as_pem());
    
  return 0;
}

void deinit_rsa_keys(void)
{
  g_rsa_keys.free_memory();  
}

// Wraps a FILE handle, to ensure we always close it when returning.
class FileCloser
{
  FILE *m_file;
public:
  FileCloser(FILE *to_be_closed) : m_file(to_be_closed) {}
  ~FileCloser()
  {
    if (m_file != NULL)
      fclose(m_file);
  }
};

/**
  Loads the RSA key pair from disk and store them in a global variable. 
 
 @see init_ssl()
 
 @return Error code
   @retval false Success
   @retval true Error
*/

bool init_rsa_keys(void)
{
  return ((do_auto_rsa_keys_generation() == false) ||
          g_rsa_keys.read_rsa_keys());
}
#endif /* HAVE_YASSL */

static MYSQL_PLUGIN plugin_info_ptr;

int init_sha256_password_handler(MYSQL_PLUGIN plugin_ref)
{
  plugin_info_ptr= plugin_ref;
  return 0;
}

/**

 @param vio Virtual input-, output interface
 @param scramble - Scramble to be saved

 Save the scramble in mpvio for future re-use.
 It is useful when we need to pass the scramble to another plugin.
 Especially in case when old 5.1 client with no CLIENT_PLUGIN_AUTH capability
 tries to connect to server with default-authentication-plugin set to
 sha256_password

*/

void static inline auth_save_scramble(MYSQL_PLUGIN_VIO *vio, const char *scramble)
{
  MPVIO_EXT *mpvio= (MPVIO_EXT *) vio;
  strncpy(mpvio->scramble, scramble, SCRAMBLE_LENGTH+1);
}


/** 
 
 @param vio Virtual input-, output interface
 @param info[out] Connection information
 
 Authenticate the user by recieving a RSA or TLS encrypted password and
 calculate a hash digest which should correspond to the user record digest
 
 RSA keys are assumed to be pre-generated and supplied when server starts. If
 the client hasn't got a public key it can request one.
 
 TLS certificates and keys are assumed to be pre-generated and supplied when
 server starts.
 
*/

static int sha256_password_authenticate(MYSQL_PLUGIN_VIO *vio,
                                        MYSQL_SERVER_AUTH_INFO *info)
{
  uchar *pkt;
  int pkt_len;
  char  *user_salt_begin;
  char  *user_salt_end;
  char scramble[SCRAMBLE_LENGTH + 1];
  char stage2[CRYPT_MAX_PASSWORD_SIZE + 1];
  String scramble_response_packet;
#if !defined(HAVE_YASSL)
  int cipher_length= 0;
  unsigned char plain_text[MAX_CIPHER_LENGTH + 1];
  RSA *private_key= NULL;
  RSA *public_key= NULL;
#endif /* HAVE_YASSL */

  DBUG_ENTER("sha256_password_authenticate");

  generate_user_salt(scramble, SCRAMBLE_LENGTH + 1);

  /*
    Note: The nonce is split into 8 + 12 bytes according to
http://dev.mysql.com/doc/internals/en/connection-phase-packets.html#packet-Protocol::HandshakeV10
    Native authentication sent 20 bytes + '\0' character = 21 bytes.
    This plugin must do the same to stay consistent with historical behavior
    if it is set to operate as a default plugin.
  */
  if (vio->write_packet(vio, (unsigned char *) scramble, SCRAMBLE_LENGTH + 1))
    DBUG_RETURN(CR_ERROR);

  /*
    Save the scramble so it could be used by native plugin in case
    the authentication on the server side needs to be restarted
  */
  auth_save_scramble(vio, scramble);

  /*
    After the call to read_packet() the user name will appear in
    mpvio->acl_user and info will contain current data.
  */
  if ((pkt_len= vio->read_packet(vio, &pkt)) == -1)
    DBUG_RETURN(CR_ERROR);

  /*
    If first packet is a 0 byte then the client isn't sending any password
    else the client will send a password.

    The original intention was that the password is a string[NUL] but this
    never got enforced properly so now we have to accept that an empty packet
    is a blank password, thus the check for pkt_len == 0 has to be made too.
  */
  if ((pkt_len == 0 || pkt_len == 1) && *pkt == 0)
  {
    info->password_used= PASSWORD_USED_NO;
    /*
      Send OK signal; the authentication might still be rejected based on
      host mask.
    */
    if (info->auth_string_length == 0)
    {
      if (sha256_password_proxy_users)
      {
        *info->authenticated_as = PROXY_FLAG;
        DBUG_PRINT("info", ("sha256_password_proxy_users is enabled \
                             , setting authenticated_as to NULL"));
      }
      DBUG_RETURN(CR_OK);
    }
    else
      DBUG_RETURN(CR_ERROR);
  }
  else
    info->password_used= PASSWORD_USED_YES;

  if (!my_vio_is_encrypted(vio))
  {
 #if !defined(HAVE_YASSL)
    /*
      Since a password is being used it must be encrypted by RSA since no
      other encryption is being active.
    */
    private_key= g_rsa_keys.get_private_key();
    public_key=  g_rsa_keys.get_public_key();

    /*
      Without the keys encryption isn't possible.
    */
    if (private_key == NULL || public_key == NULL)
    {
      my_plugin_log_message(&plugin_info_ptr, MY_ERROR_LEVEL, 
        "Authentication requires either RSA keys or SSL encryption");
      DBUG_RETURN(CR_ERROR);
    }
      

    if ((cipher_length= g_rsa_keys.get_cipher_length()) > MAX_CIPHER_LENGTH)
    {
      my_plugin_log_message(&plugin_info_ptr, MY_ERROR_LEVEL, 
        "RSA key cipher length of %u is too long. Max value is %u.",
        g_rsa_keys.get_cipher_length(), MAX_CIPHER_LENGTH);
      DBUG_RETURN(CR_ERROR);
    }

    /*
      Client sent a "public key request"-packet ?
      If the first packet is 1 then the client will require a public key before
      encrypting the password.
    */
    if (pkt_len == 1 && *pkt == 1)
    {
      uint pem_length= static_cast<uint>(strlen(g_rsa_keys.get_public_key_as_pem()));
      if (vio->write_packet(vio,
                            (unsigned char *)g_rsa_keys.get_public_key_as_pem(),
                            pem_length))
        DBUG_RETURN(CR_ERROR);
      /* Get the encrypted response from the client */
      if ((pkt_len= vio->read_packet(vio, &pkt)) == -1)
        DBUG_RETURN(CR_ERROR);
    }

    /*
      The packet will contain the cipher used. The length of the packet
      must correspond to the expected cipher length.
    */
    if (pkt_len != cipher_length)
      DBUG_RETURN(CR_ERROR);
    
    /* Decrypt password */
    RSA_private_decrypt(cipher_length, pkt, plain_text, private_key,
                        RSA_PKCS1_OAEP_PADDING);

    plain_text[cipher_length]= '\0'; // safety
    xor_string((char *) plain_text, cipher_length,
               (char *) scramble, SCRAMBLE_LENGTH);

    /*
      Set packet pointers and length for the hash digest function below 
    */
    pkt= plain_text;
    pkt_len= strlen((char *) plain_text) + 1; // include \0 intentionally.

    if (pkt_len == 1)
      DBUG_RETURN(CR_ERROR);
#else
    DBUG_RETURN(CR_ERROR);
#endif /* HAVE_YASSL */
  } // if(!my_vio_is_encrypter())

  /* Don't process the password if it is longer than maximum limit */
  if (pkt_len > SHA256_PASSWORD_MAX_PASSWORD_LENGTH + 1)
    DBUG_RETURN(CR_ERROR);

  /* A password was sent to an account without a password */
  if (info->auth_string_length == 0)
    DBUG_RETURN(CR_ERROR);
  
  /*
    Fetch user authentication_string and extract the password salt
  */
  user_salt_begin= (char *) info->auth_string;
  user_salt_end= (char *) (info->auth_string + info->auth_string_length);
  if (extract_user_salt(&user_salt_begin, &user_salt_end) != CRYPT_SALT_LENGTH)
  {
    /* User salt is not correct */
    my_plugin_log_message(&plugin_info_ptr, MY_ERROR_LEVEL, 
      "Password salt for user '%s' is corrupt.",
      info->user_name);
    DBUG_RETURN(CR_ERROR);
  }

  /* Create hash digest */
  my_crypt_genhash(stage2,
                     CRYPT_MAX_PASSWORD_SIZE,
                     (char *) pkt,
                     pkt_len-1, 
                     user_salt_begin,
                     (const char **) 0);

  /* Compare the newly created hash digest with the password record */
  int result= memcmp(info->auth_string,
                     stage2,
                     info->auth_string_length);

  if (result == 0)
  {
    if (sha256_password_proxy_users)
    {
      *info->authenticated_as= PROXY_FLAG;
       DBUG_PRINT("info", ("mysql_native_authentication_proxy_users is enabled \
						   , setting authenticated_as to NULL"));
    }
    DBUG_RETURN(CR_OK);
  }

  DBUG_RETURN(CR_ERROR);
}

#if !defined(HAVE_YASSL)
static MYSQL_SYSVAR_STR(private_key_path, auth_rsa_private_key_path,
        PLUGIN_VAR_READONLY,
        "A fully qualified path to the private RSA key used for authentication",
        NULL, NULL, AUTH_DEFAULT_RSA_PRIVATE_KEY);
static MYSQL_SYSVAR_STR(public_key_path, auth_rsa_public_key_path,
        PLUGIN_VAR_READONLY,
        "A fully qualified path to the public RSA key used for authentication",
        NULL, NULL, AUTH_DEFAULT_RSA_PUBLIC_KEY);
static MYSQL_SYSVAR_BOOL(auto_generate_rsa_keys, auth_rsa_auto_generate_rsa_keys,
        PLUGIN_VAR_READONLY | PLUGIN_VAR_OPCMDARG,
        "Auto generate RSA keys at server startup if correpsonding "
        "system variables are not specified and key files are not present "
        "at the default location.",
        NULL, NULL, TRUE);

static struct st_mysql_sys_var* sha256_password_sysvars[]= {
  MYSQL_SYSVAR(private_key_path),
  MYSQL_SYSVAR(public_key_path),
  MYSQL_SYSVAR(auto_generate_rsa_keys),
  0
};


typedef std::string Sql_string_t;

/*
  Exception free resize

  @param content [in/out] : string handle
  @param size [in] : New size


  @returns
    @retval false : Error
    @retval true : Successfully resized
*/
static
bool resize_no_exception(Sql_string_t &content, size_t size)
{
  try
  {
    content.resize(size);
  }
  catch (const std::length_error& le)
  {
    return false;
  }
  catch (std::bad_alloc& ba)
  {
    return false;
  }
  return true;
}


/**

  FILE_IO : Wrapper around std::fstream
  1> Provides READ/WRITE handle to a file
  2> Records error on READ/WRITE operations
  3> Closes file before destruction

*/

class File_IO
{
public:
  File_IO(const File_IO& src)
    : m_file_name(src.file_name()),
      m_read(src.read_mode()),
      m_error_state(src.get_error())
  {
    m_file.open(m_file_name.c_str(),
                m_read ? std::ios::in :
                         std::ios::out|std::ios::trunc);
  }

  File_IO & operator=(const File_IO& src)
  {
    m_file_name= src.file_name();
    m_read= src.read_mode();
    m_file.open(m_file_name.c_str(),
                m_read ? std::ios::in :
                         std::ios::out|std::ios::trunc);

    return *this;
  }

  ~File_IO()
  {
    close();
  }

  /*
    Close an already open file.
  */
  void close()
  {
    if (m_file.is_open())
      m_file.close();
  }

  /*
    Get name of the file. Used by copy constructor
  */
  const Sql_string_t & file_name() const
  { return m_file_name; }

  /*
    Get file IO mode. Used by copy constructor.
  */
  bool read_mode() const
  { return m_read; }

  /*
    Get READ/WRITE error status.
  */
  bool get_error() const
  { return m_error_state; }

  /*
    Set error. Used by >> and << functions.
  */
  void set_error()
  { m_error_state= true; }

  void reset_error()
  { m_error_state= false; }

  File_IO & operator>>(Sql_string_t &s);
  File_IO & operator<<(const Sql_string_t &output_string);

protected:
  File_IO() {};
  File_IO(const Sql_string_t filename, bool read)
    : m_file_name(filename),
      m_read(read),
      m_error_state(false)
  {
    m_file.open(m_file_name.c_str(),
                m_read ? std::ios::in :
                         std::ios::out|std::ios::trunc);
  }
private:
  Sql_string_t m_file_name;
  bool m_read;
  bool m_error_state;
  std::fstream m_file;
  /* Only File_creator can create File_IO */
  friend class File_creator;
};


/*
  Read an open file.

  @param op [in/out] : Handle to FILE_IO
  @param s [out] : String buffer

  Assumption : Caller will free string buffer

  returns File_IO reference. Optionally sets error.
*/
File_IO &
File_IO::operator>>(Sql_string_t &s)
{
  DBUG_ASSERT(read_mode() && m_file.is_open());

  m_file.seekg(0, std::ios::end);
  if (resize_no_exception(s, m_file.tellg()) == false)
    set_error();
  else
  {
    m_file.seekg(0, std::ios::beg);
    m_file.read(&s[0], s.size());
    close();
  }
  return *this;
}


/*
  Write into an open file

  @param op [in/out] : Handle to File_IO
  @parma output_string[in] : content to be written

  Assumption : string must be non-empty.

  @returns File_IO reference. Optionally sets error.
*/
File_IO &
File_IO::operator<<(const Sql_string_t &output_string)
{
  DBUG_ASSERT(!read_mode() && m_file.is_open());

  if (!output_string.size())
    set_error();
  else
    m_file << output_string;

  close();
  return *this;
}


/*
  Helper class to create a File_IO handle.
  Can be extended in future to set more file specific properties.
  Frees allocated memory in destructor.
*/
class File_creator
{
public:
  File_creator() {};

  ~File_creator()
  {
    for(std::vector<File_IO *>::iterator it= m_file_vector.begin();
        it != m_file_vector.end();
        ++it)
      delete(*it);
  }

  /*
    Note : Do not free memory.
  */
  File_IO * operator()(const Sql_string_t filename, bool read=false)
  {
    File_IO * f= new File_IO(filename, read);
    m_file_vector.push_back(f);
    return f;
  }

private:
  std::vector<File_IO *> m_file_vector;
};


/*
  This class encapsulates OpenSSL specific details of RSA key generation.
  It provides interfaces to:

  1> Get RSA structure
  2> Get EVP_PKEY structure
  3> Write Private/Public key into a string
  4> Free RSA/EVP_PKEY structures
*/
class RSA_gen
{
public:
  RSA_gen(uint32_t key_size= 2048, uint32_t exponent= RSA_F4)
    : m_key_size(key_size),
      m_exponent(exponent) {};

  ~RSA_gen() {};

  /**
    Passing key type is a violation against the principle of generic
    programming when this operator is used in an algorithm
    but it at the same time increases usefulness of this class when used
    stand alone.
   */
  RSA *operator()(void)
  {
    /* generate RSA keys */
    RSA *rsa= RSA_generate_key(m_key_size, m_exponent, NULL, NULL);
    return rsa; // pass ownership
  }

private:

  uint32_t m_key_size;
  uint32_t m_exponent;
};


EVP_PKEY *evp_pkey_generate(RSA *rsa)
{
  if (rsa)
  {
    EVP_PKEY *pkey= EVP_PKEY_new();
    EVP_PKEY_assign_RSA(pkey, rsa);
    return pkey;
  }
  return NULL;
}


/*
  Write private key in a string buffer

  @param rsa [in] : Handle to RSA structure where private key is stored

  @returns Sql_string_t object with private key stored in it.
*/
static
Sql_string_t rsa_priv_key_write(RSA *rsa)
{
  DBUG_ASSERT(rsa);
  BIO *buf= BIO_new(BIO_s_mem());
  Sql_string_t read_buffer;
  if (PEM_write_bio_RSAPrivateKey(buf, rsa, NULL, NULL,
                                  0, NULL, NULL))
  {
    size_t len= BIO_pending(buf);
    if (resize_no_exception(read_buffer, len+1) == true)
    {
      BIO_read(buf, (void *)read_buffer.c_str(), len);
      read_buffer[len]='\0';
    }
  }
  BIO_free(buf);
  return read_buffer;
}


/*
  Write public key in a string buffer

  @param rsa [in] : Handle to RSA structure where public key is stored

  @returns Sql_string_t object with public key stored in it.
*/
static
Sql_string_t rsa_pub_key_write(RSA *rsa)
{
  DBUG_ASSERT(rsa);
  BIO *buf= BIO_new(BIO_s_mem());
  Sql_string_t read_buffer;
  if (PEM_write_bio_RSA_PUBKEY(buf, rsa))
  {
    size_t len= BIO_pending(buf);
    if (resize_no_exception(read_buffer, len+1) == true)
    {
      BIO_read(buf, (void *)read_buffer.c_str(), len);
      read_buffer[len]='\0';
    }
  }
  BIO_free(buf);
  return read_buffer;
}


/*
  This class encapsulates OpenSSL specific details of X509 certificate
  generation. It provides interfaces to:

  1> Generate X509 certificate
  2> Read/Write X509 certificate from/to a string
  3> Read/Write Private key from/to a string
  4> Free X509/EVP_PKEY structures
*/
class X509_gen
{
public:
  X509 * operator()(EVP_PKEY *pkey,
                    const Sql_string_t cn,
                    uint32_t serial,
                    uint32_t notbefore,
                    uint32_t notafter,
                    bool self_sign= true,
                    X509 *ca_x509= NULL,
                    EVP_PKEY *ca_pkey= NULL)
  {
    X509 *x509= X509_new();
    X509_EXTENSION *ext= 0;
    X509V3_CTX v3ctx;
    X509_NAME *name= 0;

    DBUG_ASSERT(cn.length() <= MAX_CN_NAME_LENGTH);
    DBUG_ASSERT(serial != 0);
    DBUG_ASSERT(self_sign || (ca_x509 != NULL && ca_pkey != NULL));
    if (!x509)
      goto err;

    /** Set certificate version */
    if (!X509_set_version(x509, 2))
      goto err;

    /** Set serial number */
    if (!ASN1_INTEGER_set(X509_get_serialNumber(x509), serial))
      goto err;

    /** Set certificate validity */
    if (!X509_gmtime_adj(X509_get_notBefore(x509), notbefore) ||
        !X509_gmtime_adj(X509_get_notAfter(x509), notafter))
      goto err;

    /** Set public key */
    if (!X509_set_pubkey(x509, pkey))
      goto err;

    /** Set CN value in subject */
    name= X509_get_subject_name(x509);
    if (!name)
      goto err;

    if (!X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                    (const unsigned char *)cn.c_str(),
                                    -1, -1, 0))
      goto err;

    /** Set Issuer */
    if (!X509_set_issuer_name(x509, self_sign ? name :
                                      X509_get_subject_name(ca_x509)))
      goto err;

    /** Add X509v3 extensions */
    X509V3_set_ctx(&v3ctx, self_sign ? x509 : ca_x509, x509, NULL, NULL, 0);

    /** Add CA:TRUE / CA:FALSE inforamation */
    if (!(ext= X509V3_EXT_conf_nid(NULL, &v3ctx, NID_basic_constraints,
                                   self_sign ?(char *)"critical,CA:TRUE" :
                                              (char *)"critical,CA:FALSE")))
      goto err;
    X509_add_ext(x509, ext, -1);
    X509_EXTENSION_free(ext);

    /** Sign using SHA256 */
    if (!X509_sign(x509, self_sign ? pkey : ca_pkey, EVP_sha256()))
      goto err;

    return x509;
err:
    if (x509)
      X509_free(x509);
    return 0;
  }
};


/*
  Read a X509 certificate into X509 format

  @param input_string [in] : Content of X509 certificate file.

  @returns Handle to X509 structure.

  Assumption : Caller will free X509 object
*/
static
X509 * x509_cert_read(const Sql_string_t &input_string)
{
  X509 * x509= NULL;

  if (!input_string.size())
    return x509;

  BIO *buf= BIO_new(BIO_s_mem());
  BIO_write(buf, input_string.c_str(), input_string.size());
  x509= PEM_read_bio_X509(buf, NULL, NULL, NULL);
  BIO_free(buf);
  return x509;
}


/*
  Write X509 certificate into a string

  @param cert [in] : Certificate information in X509 format.

  @returns certificate information in string format.
*/
static
Sql_string_t x509_cert_write(X509 *cert)
{
  DBUG_ASSERT(cert);
  BIO *buf= BIO_new(BIO_s_mem());
  Sql_string_t read_buffer;
  if (PEM_write_bio_X509(buf, cert))
  {
    size_t len= BIO_pending(buf);
    if (resize_no_exception(read_buffer, len+1) == true)
    {
      BIO_read(buf, (void *)read_buffer.c_str(), len);
      read_buffer[len]='\0';
    }
  }
  BIO_free(buf);
  return read_buffer;
}


/*
  Read Private key into EVP_PKEY structure

  @param input_string [in] : Content of private key file.

  @returns Handle to EVP_PKEY structure.

  Assumption : Caller will free EVP_PKEY object
*/
static
EVP_PKEY * x509_key_read(const Sql_string_t &input_string)
{
  EVP_PKEY *pkey= NULL;
  RSA *rsa= NULL;

  if (!input_string.size())
    return pkey;

  BIO *buf= BIO_new(BIO_s_mem());
  BIO_write(buf, input_string.c_str(), input_string.size());
  rsa= PEM_read_bio_RSAPrivateKey(buf, NULL, NULL, NULL);
  pkey= evp_pkey_generate(rsa);
  BIO_free(buf);
  return pkey;
}


/*
  Write X509 certificate into a string

  @param pkey [in] : Private key information.

  @returns private key information in string format.
*/
static
Sql_string_t x509_key_write(EVP_PKEY *pkey)
{
  DBUG_ASSERT(pkey);
  BIO *buf= BIO_new(BIO_s_mem());
  RSA *rsa= EVP_PKEY_get1_RSA(pkey);
  Sql_string_t read_buffer;
  if (PEM_write_bio_RSAPrivateKey(buf, rsa, NULL, NULL,
                                  10, NULL, NULL))
  {
    size_t len= BIO_pending(buf);
    if (resize_no_exception(read_buffer, len+1) == true)
    {
      BIO_read(buf, (void *)read_buffer.c_str(), len);
      read_buffer[len]='\0';
    }
  }
  BIO_free(buf);
  RSA_free(rsa);
  return read_buffer;
}


/*
  Algorithm to create X509 certificate.
  Relies on:
  1> RSA key generator
  2> X509 certificate generator
  3> FILE reader/writer

  Overwrites key/certificate files if already present.

  @param rsa_gen [in] : RSA generator
  @param cn [in] : Common name field of X509 certificate.
  @param serial [in] : Certificate serial number
  @param cert_filename [in] : File name for X509 certificate
  @param key_filename [in] : File name for private key
  @param filecr [in] : File creator
  @param ca_key_file [in] : CA private key file
  @param ca_cert_file [in] : CA certificate file

  @returns generation status
    @retval false : Error in key/certificate generation.
    @retval true : key/certificate files are generated successfully.
*/

template <typename RSA_generator_func, typename File_creation_func>
bool create_x509_certificate(RSA_generator_func &rsa_gen,
                             const Sql_string_t cn,
                             uint32_t serial,
                             const Sql_string_t cert_filename,
                             const Sql_string_t key_filename,
                             File_creation_func &filecr,
                             const Sql_string_t ca_key_file= "",
                             const Sql_string_t ca_cert_file= "")
{
  bool ret_val= true;
  bool self_sign= true;
  Sql_string_t ca_key_str;
  Sql_string_t ca_cert_str;
  RSA *rsa= NULL;
  EVP_PKEY *pkey= NULL;
  EVP_PKEY *ca_key= NULL;
  X509 *x509= NULL;
  X509 *ca_x509= NULL;
  File_IO *x509_key_file_ostream= NULL;
  File_IO *x509_cert_file_ostream= NULL;
  File_IO *x509_ca_key_file_istream= NULL;
  File_IO *x509_ca_cert_file_istream= NULL;
  X509_gen x509_gen;
  MY_MODE file_creation_mode= get_file_perm(USER_READ | USER_WRITE);
  MY_MODE saved_umask= umask(~(file_creation_mode));

  x509_key_file_ostream= filecr(key_filename);

  /* Generate private key for X509 certificate */
  rsa= rsa_gen();
  DBUG_EXECUTE_IF("null_rsa_error",
                  {
                    RSA_free(rsa);
                    rsa= NULL;
                  });

  if (!rsa)
  {
    sql_print_error("Could not generate RSA private key "
                    "required for X509 certificate.");
    ret_val= false;
    goto end;
  }

  /* Obtain EVP_PKEY */
  pkey= evp_pkey_generate(rsa);

  /* Write private key information to file and set file permission */
  (*x509_key_file_ostream) << x509_key_write(pkey);
  DBUG_EXECUTE_IF("key_file_write_error",
                  {
                    x509_key_file_ostream->set_error();
                  });
  if (x509_key_file_ostream->get_error())
  {
    sql_print_error("Could not write key file: %s", key_filename.c_str());
    ret_val= false;
    goto end;
  }

  if (MY_TEST(my_chmod(key_filename.c_str(),
      USER_READ|USER_WRITE, MYF(MY_FAE+MY_WME))))
  {
    sql_print_error("Could not set file permission for %s",
                    key_filename.c_str());
    ret_val= false;
    goto end;
  }

  /*
    Read CA key/certificate files in PEM format.
  */
  if (ca_key_file.size() && ca_cert_file.size())
  {
    x509_ca_key_file_istream= filecr(ca_key_file, true);
    x509_ca_cert_file_istream= filecr(ca_cert_file, true);
    (*x509_ca_key_file_istream) >> ca_key_str;
    ca_key= x509_key_read(ca_key_str);
    DBUG_EXECUTE_IF("ca_key_read_error",
                    {
                      EVP_PKEY_free(ca_key);
                      ca_key= NULL;
                    });
    if (!ca_key)
    {
      sql_print_error("Could not read CA key file: %s", ca_key_file.c_str());
      ret_val= false;
      goto end;
    }

    (*x509_ca_cert_file_istream) >> ca_cert_str;
    ca_x509= x509_cert_read(ca_cert_str);
    DBUG_EXECUTE_IF("ca_cert_read_error",
                    {
                      X509_free(ca_x509);
                      ca_x509= NULL;
                    });
    if (!ca_x509)
    {
      sql_print_error("Could not read CA certificate file: %s", ca_cert_file.c_str());
      ret_val= false;
      goto end;
    }

    self_sign= false;
  }

  /* Create X509 certificate with validity of 10 year */
  x509= x509_gen(pkey, cn, serial, 0, 365L*24*60*60*10,
                 self_sign, ca_x509, ca_key);
  DBUG_EXECUTE_IF("x509_cert_generation_error",
                  {
                    X509_free(x509);
                    x509= NULL;
                  });
  if (!x509)
  {
    sql_print_error("Could not generate X509 certificate.");
    ret_val= false;
    goto end;
  }

  /* Write X509 certificate to file and set permission */
  x509_cert_file_ostream= filecr(cert_filename);
  (*x509_cert_file_ostream)<< x509_cert_write(x509);
  DBUG_EXECUTE_IF("cert_pub_key_write_error",
                  {
                    x509_cert_file_ostream->set_error();
                  });
  if (x509_cert_file_ostream->get_error())
  {
    sql_print_error("Could not write certificate file: %s", cert_filename.c_str());
    ret_val= false;
    goto end;
  }

  if (MY_TEST(my_chmod(cert_filename.c_str(),
               USER_READ|USER_WRITE|GROUP_READ|OTHERS_READ,
               MYF(MY_FAE+MY_WME))))
  {
    sql_print_error("Could not set file permission for %s",
                    cert_filename.c_str());
    ret_val= false;
    goto end;
  }

end:

  if (pkey)
    EVP_PKEY_free(pkey);                /* Frees rsa too */
  if (ca_key)
    EVP_PKEY_free(ca_key);
  if (x509)
    X509_free(x509);
  if (ca_x509)
    X509_free(ca_x509);

  umask(saved_umask);
  return ret_val;
}


/*
  Algorithm to generate RSA key pair.
  Relies on:
  1> RSA generator
  2> File reader/writer

  Overwrites existing Private/Public key file if any.

  @param rsa_gen [in] : RSA key pair generator
  @param priv_key_filename [in] : File name of private key
  @param pub_key_filename [in] : File name of public key
  @param filecr [in] : File creator

  @returns status of RSA key pair generation.
    @retval false Error in RSA key pair generation.
    @retval true Private/Public keys are successfully generated.
*/
template <typename RSA_generator_func, typename File_creation_func>
bool create_RSA_key_pair(RSA_generator_func &rsa_gen,
                         const Sql_string_t priv_key_filename,
                         const Sql_string_t pub_key_filename,
                         File_creation_func &filecr)
{
  bool ret_val= true;
  File_IO * priv_key_file_ostream= NULL;
  File_IO * pub_key_file_ostream= NULL;
  MY_MODE file_creation_mode= get_file_perm(USER_READ | USER_WRITE);
  MY_MODE saved_umask= umask(~(file_creation_mode));

  DBUG_ASSERT(priv_key_filename.size() && pub_key_filename.size());

  RSA *rsa= rsa_gen();
  DBUG_EXECUTE_IF("null_rsa_error",
                  {
                    RSA_free(rsa);
                    rsa= NULL;
                  });

  if (!rsa)
  {
    sql_print_error("Could not generate RSA Private/Public key pair");
    ret_val= false;
    goto end;
  }

  priv_key_file_ostream= filecr(priv_key_filename);
  (*priv_key_file_ostream)<< rsa_priv_key_write(rsa);

  DBUG_EXECUTE_IF("key_file_write_error",
                  {
                    priv_key_file_ostream->set_error();
                  });
  if (priv_key_file_ostream->get_error())
  {
    sql_print_error("Could not write private key file: %s", priv_key_filename.c_str());
    ret_val= false;
    goto end;
  }
  if (MY_TEST(my_chmod(priv_key_filename.c_str(),
               USER_READ|USER_WRITE, MYF(MY_FAE+MY_WME))))
  {
    sql_print_error("Could not set file permission for %s",
                    priv_key_filename.c_str());
    ret_val= false;
    goto end;
  }

  pub_key_file_ostream= filecr(pub_key_filename);
  (*pub_key_file_ostream)<< rsa_pub_key_write(rsa);
  DBUG_EXECUTE_IF("cert_pub_key_write_error",
                  {
                    pub_key_file_ostream->set_error();
                  });

  if (pub_key_file_ostream->get_error())
  {
    sql_print_error("Could not write public key file: %s", pub_key_filename.c_str());
    ret_val= false;
    goto end;
  }
  if (MY_TEST(my_chmod(pub_key_filename.c_str(),
               USER_READ|USER_WRITE|GROUP_READ|OTHERS_READ,
               MYF(MY_FAE+MY_WME))))
  {
    sql_print_error("Could not set file permission for %s",
                    pub_key_filename.c_str());
    ret_val= false;
    goto end;
  }

end:
  if (rsa)
    RSA_free(rsa);

  umask(saved_umask);
  return ret_val;
}


/*
  Check auto_generate_certs option and generate
  SSL certificates if required.

  SSL Certificates are generated iff following conditions are met.
  1> auto_generate_certs is set to ON.
  2> None of the SSL system variables are specified.
  3> Following files are not present in data directory.
     a> ca.pem
     b> server_cert.pem
     c> server_key.pem

  If above mentioned conditions are satisfied, following action will be taken:

  1> 6 File are generated and placed data directory:
     a> ca.pem
     b> ca_key.pem
     c> server_cert.pem
     d> server_key.pem
     e> client_cert.pem
     f> client_key.pem

     ca.pem is self signed auto generated CA certificate. server_cert.pem
     and client_cert.pem are signed using auto genreated CA.

     ca_key.pem, client_cert.pem and client_key.pem are overwritten if
     they are present in data directory.

  Path of following system variables are set if certificates are either
  generated or already present in data directory.
  a> ssl-ca
  b> ssl-cert
  c> ssl-key

  Assumption : auto_detect_ssl() is called before control reaches to
  do_auto_cert_generation().

  @param auto_detection_status [IN] Status of SSL artifacts detection process

  @returns
    @retval true i Generation is successful or skipped
    @retval false Generation failed.
*/
bool do_auto_cert_generation(ssl_artifacts_status auto_detection_status)
{
  if (opt_auto_generate_certs == true)
  {
    /*
      Do not generate SSL certificates/RSA keys,
      If any of the SSL option was specified.
    */

    if (auto_detection_status == SSL_ARTIFACTS_VIA_OPTIONS)
    {
      sql_print_information("Skipping generation of SSL certificates "
                            "as options related to SSL are specified.");
      return true;
    }
    else if(auto_detection_status == SSL_ARTIFACTS_AUTO_DETECTED ||
            auto_detection_status == SSL_ARTIFACT_TRACES_FOUND)
    {
      sql_print_information("Skipping generation of SSL certificates as "
                            "certificate files are present in data "
                            "directory.");
      return true;
    }
    else
    {
      DBUG_ASSERT(auto_detection_status == SSL_ARTIFACTS_NOT_FOUND);
      /* Initialize the key pair generator. It can also be used stand alone */
      RSA_gen rsa_gen;
      /*
         Initialize the file creator.
       */
      File_creator fcr;
      Sql_string_t ca_name= "MySQL_Server_";
      Sql_string_t server_name= "MySQL_Server_";
      Sql_string_t client_name= "MySQL_Server_";

      ca_name.append(MYSQL_SERVER_VERSION);
      ca_name.append("_Auto_Generated_CA_Certificate");
      server_name.append(MYSQL_SERVER_VERSION);
      server_name.append("_Auto_Generated_Server_Certificate");
      client_name.append(MYSQL_SERVER_VERSION);
      client_name.append("_Auto_Generated_Client_Certificate");

      /*
        Maximum length of X509 certificate subject is 64.
        Make sure that constructed strings are within valid
        bounds or change them to minimal default strings
      */
      if (ca_name.length() > MAX_CN_NAME_LENGTH ||
          server_name.length() > MAX_CN_NAME_LENGTH ||
          client_name.length() > MAX_CN_NAME_LENGTH)
      {
        ca_name.clear();
        ca_name.append("MySQL_Server_Auto_Generated_CA_Certificate");
        server_name.clear();
        server_name.append("MySQL_Server_Auto_Generated_Server_Certificate");
        client_name.clear();
        client_name.append("MySQL_Server_Auto_Generated_Client_Certificate");
      }

      /* Create and write the certa and keys on disk */
      if ((create_x509_certificate(rsa_gen, ca_name, 1, DEFAULT_SSL_CA_CERT,
                                   DEFAULT_SSL_CA_KEY, fcr) == false) ||
          (create_x509_certificate(rsa_gen, server_name, 2,
                                   DEFAULT_SSL_SERVER_CERT,
                                   DEFAULT_SSL_SERVER_KEY, fcr,
                                   DEFAULT_SSL_CA_KEY,
                                   DEFAULT_SSL_CA_CERT) == false) ||
          (create_x509_certificate(rsa_gen, client_name, 3,
                                   DEFAULT_SSL_CLIENT_CERT,
                                   DEFAULT_SSL_CLIENT_KEY, fcr,
                                   DEFAULT_SSL_CA_KEY,
                                   DEFAULT_SSL_CA_CERT) == false))
      {
        return false;
      }
      opt_ssl_ca= (char *)DEFAULT_SSL_CA_CERT;
      opt_ssl_cert= (char *)DEFAULT_SSL_SERVER_CERT;
      opt_ssl_key= (char *)DEFAULT_SSL_SERVER_KEY;
      sql_print_information("Auto generated SSL certificates are placed "
                            "in data directory.");
    }
    return true;
  }
  else
  {
    sql_print_information("Skipping generation of SSL certificates as "
                          "--auto_generate_certs is set to OFF.");
    return true;
  }
}


/*
  Check sha256_password_auto_generate_rsa_keys option and generate
  RSA key pair if required.

  RSA key pair is generated iff following conditions are met.
  1> sha256_password_auto_generate_rsa_keys is set to ON.
  2> sha256_password_private_key_path or sha256_password_public_key_path
     are pointing to non-default locations.
  3> Following files are not present in data directory.
     a> private_key.pem
     b> public_key.pem

  If above mentioned conditions are satified private_key.pem and
  public_key.pem files are generated and placed in data directory.
*/
static bool do_auto_rsa_keys_generation()
{
  if (auth_rsa_auto_generate_rsa_keys == true)
  {
    MY_STAT priv_stat, pub_stat;
    if (strcmp(auth_rsa_private_key_path, AUTH_DEFAULT_RSA_PRIVATE_KEY) ||
        strcmp(auth_rsa_public_key_path, AUTH_DEFAULT_RSA_PUBLIC_KEY))
    {
      sql_print_information("Skipping generation of RSA key pair as "
                            "options related to RSA keys are specified.");
      return true;
    }
    else if (my_stat(AUTH_DEFAULT_RSA_PRIVATE_KEY, &priv_stat, MYF(0)) ||
             my_stat(AUTH_DEFAULT_RSA_PUBLIC_KEY, &pub_stat, MYF(0)))
    {
      sql_print_information("Skipping generation of RSA key pair as "
                            "key files are present in data directory.");
      return true;
    }
    else
    {
      /* Initialize the key pair generator. */
      RSA_gen rsa_gen;
      /* Initialize the file creator. */
      File_creator fcr;

      if (create_RSA_key_pair(rsa_gen, "private_key.pem", "public_key.pem",
                              fcr) == false)
        return false;

      sql_print_information("Auto generated RSA key files are "
                            "placed in data directory.");
      return true;
    }
  }
  else
  {
    sql_print_information("Skipping generation of RSA key pair as "
                          "--sha256_password_auto_generate_rsa_keys "
                          "is set to OFF.");
    return true;
  }
}
#endif /* HAVE_YASSL */
#endif /* HAVE_OPENSSL */

bool MPVIO_EXT::can_authenticate()
{
  return (acl_user && acl_user->can_authenticate);
}

static struct st_mysql_auth native_password_handler=
{
  MYSQL_AUTHENTICATION_INTERFACE_VERSION,
  native_password_plugin_name.str,
  native_password_authenticate,
  generate_native_password,
  validate_native_password_hash,
  set_native_salt,
  AUTH_FLAG_USES_INTERNAL_STORAGE
};

#if defined(HAVE_OPENSSL)
static struct st_mysql_auth sha256_password_handler=
{
  MYSQL_AUTHENTICATION_INTERFACE_VERSION,
  sha256_password_plugin_name.str,
  sha256_password_authenticate,
  generate_sha256_password,
  validate_sha256_password_hash,
  set_sha256_salt,
  AUTH_FLAG_USES_INTERNAL_STORAGE
};

#endif /* HAVE_OPENSSL */

mysql_declare_plugin(mysql_password)
{
  MYSQL_AUTHENTICATION_PLUGIN,                  /* type constant    */
  &native_password_handler,                     /* type descriptor  */
  native_password_plugin_name.str,              /* Name             */
  "R.J.Silk, Sergei Golubchik",                 /* Author           */
  "Native MySQL authentication",                /* Description      */
  PLUGIN_LICENSE_GPL,                           /* License          */
  NULL,                                         /* Init function    */
  NULL,                                         /* Deinit function  */
  0x0101,                                       /* Version (1.0)    */
  NULL,                                         /* status variables */
  NULL,                                         /* system variables */
  NULL,                                         /* config options   */
  0,                                            /* flags            */
}
#if defined(HAVE_OPENSSL)
,
{
  MYSQL_AUTHENTICATION_PLUGIN,                  /* type constant    */
  &sha256_password_handler,                     /* type descriptor  */
  sha256_password_plugin_name.str,              /* Name             */
  "Oracle",                                     /* Author           */
  "SHA256 password authentication",             /* Description      */
  PLUGIN_LICENSE_GPL,                           /* License          */
  &init_sha256_password_handler,                /* Init function    */
  NULL,                                         /* Deinit function  */
  0x0101,                                       /* Version (1.0)    */
  NULL,                                         /* status variables */
#if !defined(HAVE_YASSL)
  sha256_password_sysvars,                      /* system variables */
#else
  NULL,
#endif /* HAVE_YASSL */
  NULL,                                         /* config options   */
  0                                             /* flags            */
}
#endif /* HAVE_OPENSSL */
mysql_declare_plugin_end;

