/* Copyright (c) 2011, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"

#if defined(HAVE_OPENSSL)
#include "crypt_genhash_impl.h"
#include "mysql/client_authentication.h"
#include "m_ctype.h"
#include "sql_common.h"
#include "errmsg.h"
#include "sql_string.h"

#include <string.h>
#include <stdarg.h>
#if !defined(HAVE_YASSL)
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#if defined(_WIN32) && !defined(_OPENSSL_Applink) && defined(HAVE_OPENSSL_APPLINK_C)
#include <openssl/applink.c>
#endif
#endif
#include "mysql/service_my_plugin_log.h"

#define MAX_CIPHER_LENGTH 1024

#if !defined(HAVE_YASSL)
mysql_mutex_t g_public_key_mutex;
#endif

int sha256_password_init(char *a, size_t b, int c, va_list d)
{
#if !defined(HAVE_YASSL)
  mysql_mutex_init(0,&g_public_key_mutex, MY_MUTEX_INIT_SLOW);
#endif
  return 0;
}

int sha256_password_deinit(void)
{
#if !defined(HAVE_YASSL)
  mysql_mutex_destroy(&g_public_key_mutex);
#endif
  return 0;
}


#if !defined(HAVE_YASSL)
/**
  Reads and parse RSA public key data from a file.

  @param mysql connection handle with file path data
 
  @return Pointer to the RSA public key storage buffer
*/

RSA *rsa_init(MYSQL *mysql)
{
  static RSA *g_public_key= NULL;
  RSA *key= NULL;

  mysql_mutex_lock(&g_public_key_mutex);
  key= g_public_key;
  mysql_mutex_unlock(&g_public_key_mutex);

  if (key != NULL)
    return key;

  FILE *pub_key_file= NULL;

  if (mysql->options.extension != NULL &&
      mysql->options.extension->server_public_key_path != NULL &&
      mysql->options.extension->server_public_key_path != '\0')
  {
    pub_key_file= fopen(mysql->options.extension->server_public_key_path,
                        "r");
  }
  /* No public key is used; return 0 without errors to indicate this. */
  else
    return 0;

  if (pub_key_file == NULL)
  {
    /*
      If a key path was submitted but no key located then we print an error
      message. Else we just report that there is no public key.
    */
    fprintf(stderr,"Can't locate server public key '%s'\n",
              mysql->options.extension->server_public_key_path);

    return 0;
  }
  
  mysql_mutex_lock(&g_public_key_mutex);
  key= g_public_key= PEM_read_RSA_PUBKEY(pub_key_file, 0, 0, 0);
  mysql_mutex_unlock(&g_public_key_mutex);
  fclose(pub_key_file);
  if (g_public_key == NULL)
  {
    ERR_clear_error();
    fprintf(stderr, "Public key is not in PEM format: '%s'\n",
            mysql->options.extension->server_public_key_path);
    return 0;
  }

  return key;
}
#endif // !defined(HAVE_YASSL)

/**
  Authenticate the client using the RSA or TLS and a SHA256 salted password.
 
  @param vio Provides plugin access to communication channel
  @param mysql Client connection handler

  @return Error status
    @retval CR_ERROR An error occurred.
    @retval CR_OK Authentication succeeded.
*/

extern "C"
int sha256_password_auth_client(MYSQL_PLUGIN_VIO *vio, MYSQL *mysql)
{
  bool uses_password= mysql->passwd[0] != 0;
#if !defined(HAVE_YASSL)
  unsigned char encrypted_password[MAX_CIPHER_LENGTH];
  static char request_public_key= '\1';
  RSA *public_key= NULL;
  bool got_public_key_from_server= false;
#endif
  bool connection_is_secure= false;
  unsigned char scramble_pkt[20];
  unsigned char *pkt;


  DBUG_ENTER("sha256_password_auth_client");

  /*
    Get the scramble from the server because we need it when sending encrypted
    password.
  */
  if (vio->read_packet(vio, &pkt) != SCRAMBLE_LENGTH)
  {
    DBUG_PRINT("info",("Scramble is not of correct length."));
    DBUG_RETURN(CR_ERROR);
  }
  /*
    Copy the scramble to the stack or it will be lost on the next use of the 
    net buffer.
  */
  memcpy(scramble_pkt, pkt, SCRAMBLE_LENGTH);

  if (mysql_get_ssl_cipher(mysql) != NULL)
    connection_is_secure= true;
  
  /* If connection isn't secure attempt to get the RSA public key file */
  if (!connection_is_secure)
  {
 #if !defined(HAVE_YASSL)
    public_key= rsa_init(mysql);
#endif
  }

  if (!uses_password)
  {
    /* We're not using a password */
    static const unsigned char zero_byte= '\0'; 
    if (vio->write_packet(vio, (const unsigned char *) &zero_byte, 1))
      DBUG_RETURN(CR_ERROR);
  }
  else
  {
    /* Password is a 0-terminated byte array ('\0' character included) */
    unsigned int passwd_len= strlen(mysql->passwd) + 1;
    if (!connection_is_secure)
    {
#if !defined(HAVE_YASSL)
      /*
        If no public key; request one from the server.
      */
      if (public_key == NULL)
      {
        if (vio->write_packet(vio, (const unsigned char *) &request_public_key,
                              1))
          DBUG_RETURN(CR_ERROR);
      
        int pkt_len= 0;
        unsigned char *pkt;
        if ((pkt_len= vio->read_packet(vio, &pkt)) == -1)
          DBUG_RETURN(CR_ERROR);
        BIO* bio= BIO_new_mem_buf(pkt, pkt_len);
        public_key= PEM_read_bio_RSA_PUBKEY(bio, NULL, NULL, NULL);
        BIO_free(bio);
        if (public_key == 0)
        {
          ERR_clear_error();
          DBUG_RETURN(CR_ERROR);
        }
        got_public_key_from_server= true;
      }
      
      /* Obfuscate the plain text password with the session scramble */
      xor_string(mysql->passwd, strlen(mysql->passwd), (char *) scramble_pkt,
                 SCRAMBLE_LENGTH);
      /* Encrypt the password and send it to the server */
      int cipher_length= RSA_size(public_key);
      /*
        When using RSA_PKCS1_OAEP_PADDING the password length must be less
        than RSA_size(rsa) - 41.
      */
      if (passwd_len + 41 >= (unsigned) cipher_length)
      {
        /* password message is to long */
        DBUG_RETURN(CR_ERROR);
      }
      RSA_public_encrypt(passwd_len, (unsigned char *) mysql->passwd,
                         encrypted_password,
                         public_key, RSA_PKCS1_OAEP_PADDING);
      if (got_public_key_from_server)
        RSA_free(public_key);

      if (vio->write_packet(vio, (uchar*) encrypted_password, cipher_length))
        DBUG_RETURN(CR_ERROR);
#else
      set_mysql_extended_error(mysql, CR_AUTH_PLUGIN_ERR, unknown_sqlstate,
                                ER(CR_AUTH_PLUGIN_ERR), "sha256_password",
                                "Authentication requires SSL encryption");
      DBUG_RETURN(CR_ERROR); // If no openssl support
#endif
    }
    else
    {
      /* The vio is encrypted already; just send the plain text passwd */
      if (vio->write_packet(vio, (uchar*) mysql->passwd, passwd_len))
        DBUG_RETURN(CR_ERROR);
    }
    
    memset(mysql->passwd, 0, passwd_len);
  }
    
  DBUG_RETURN(CR_OK);
}

#endif
