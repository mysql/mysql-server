#ifdef HAVE_OPENSSL
#ifndef HAVE_YASSL
#include "crypt_genhash_impl.h"
#include "mysql/client_authentication.h"
#include "m_ctype.h"
#include "sql_common.h"
#include "errmsg.h"
#include "sql_string.h"

#include <string.h>
#include <stdarg.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include "mysql/service_my_plugin_log.h"

#define MAX_CIPHER_LENGTH 1024

RSA *sha256_password_init(MYSQL *mysql)
{
  static RSA *g_public_key= NULL;
  
  if (g_public_key != NULL)
    return g_public_key;
  
  FILE *pub_key_file = fopen(mysql->options.extension->server_public_key_path,
                             "r");
  if (pub_key_file == NULL)
  {
      //String error;
      //error.append("Can't locate server public key '");
      //error.append(mysql->options.extension->server_public_key_path);
      //error.append("'");
      // TODO Figure out where to send the error and how.
      return 0;
  }
  
  g_public_key= PEM_read_RSA_PUBKEY(pub_key_file, 0, 0, 0);
  fclose(pub_key_file);
  if (g_public_key == NULL)
  {
    //String error;
    //error.append("Public key is not in PEM format: '");
    //error.append(mysql->options.extension->server_public_key_path);
    //error.append("'");
    // TODO Figure out where to send the error and how.
    return 0;
  }
  return g_public_key;
}

extern "C"
int sha256_password_auth_client(MYSQL_PLUGIN_VIO *vio, MYSQL *mysql)
{
  unsigned char encrypted_password[MAX_CIPHER_LENGTH];
  static char request_public_key= '\1';
  bool uses_password= mysql->passwd[0] != 0;
  DBUG_ENTER("sha256_password_auth_client");
  RSA *public_key= NULL;
  bool connection_is_secure= false;
  unsigned char scramble_pkt[20];
  unsigned char *pkt;
  bool got_public_key_from_server= false;

  /*
    Always get scramble from the server. This is done because the plugin
    framework won't work if a server side plugin starts with a read_packet()
  */
  if (vio->read_packet(vio, &pkt) != SCRAMBLE_LENGTH)
    DBUG_RETURN(CR_ERROR);
  /*
    Copy the scramble to the stack or it will be lost on the next use of the 
    net buffer.
  */
  memcpy(scramble_pkt, pkt, SCRAMBLE_LENGTH);

  if (mysql_get_ssl_cipher(mysql) != NULL)
    connection_is_secure= true;
  
  /* If connection isn't secure attempt to get the RSA public key file */
  if (!connection_is_secure)
    public_key= sha256_password_init(mysql);

  if (!uses_password)
  {
    /* We're not using a password */
    if (vio->write_packet(vio, (const unsigned char *)&mysql->passwd[0],1))
      DBUG_RETURN(CR_ERROR);
  }
  else
  {
    /* Password is a 0-terminated byte array ('\0' character included) */
    unsigned int passwd_len= strlen(mysql->passwd)+1;
    if (!connection_is_secure)
    {
      /*
        If no public key; request one from the server.
      */
      if (public_key == NULL)
      {
        if (vio->write_packet(vio, (const unsigned char *)&request_public_key,
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
          DBUG_RETURN(CR_ERROR);
        got_public_key_from_server= true;
      }
      
      /* Obfuscate the plain text password with the session scramble */
      xor_string(mysql->passwd, strlen(mysql->passwd), (char *)scramble_pkt,
                 SCRAMBLE_LENGTH);
      /* Encrypt the password and send it to the server */
      RSA_public_encrypt(passwd_len, (unsigned char *)mysql->passwd,
                         encrypted_password,
                         public_key, RSA_PKCS1_OAEP_PADDING);
      int cipher_length= RSA_size(public_key);
      if (got_public_key_from_server)
        RSA_free(public_key);
      if (cipher_length > MAX_CIPHER_LENGTH)
        DBUG_RETURN(CR_ERROR);
      if (vio->write_packet(vio, (uchar*)encrypted_password, cipher_length))
        DBUG_RETURN(CR_ERROR);
    }
    else
    {
      if (vio->write_packet(vio, (uchar*)mysql->passwd, passwd_len))
        DBUG_RETURN(CR_ERROR);
    }
    
    memset(mysql->passwd, 0, passwd_len);
  }
    
  DBUG_RETURN(CR_OK);
}

#endif // HAVE_YASSL
#endif // HAVE_OPENSSL
