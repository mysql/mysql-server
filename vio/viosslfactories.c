/* Copyright (c) 2000, 2015, Oracle and/or its affiliates.
   Copyright (c) 2011, 2015, MariaDB

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

#include "vio_priv.h"

#ifdef HAVE_OPENSSL
#ifndef HAVE_YASSL
#include <openssl/dh.h>
#include <openssl/bn.h>
#endif

static my_bool     ssl_algorithms_added    = FALSE;
static my_bool     ssl_error_strings_loaded= FALSE;

/* the function below was generated with "openssl dhparam -2 -C 2048" */
static
DH *get_dh2048()
{
  static unsigned char dh2048_p[]={
    0xA1,0xBB,0x7C,0x20,0xC5,0x5B,0xC0,0x7B,0x21,0x8B,0xD6,0xA8,
    0x15,0xFC,0x3B,0xBA,0xAB,0x9F,0xDF,0x68,0xC4,0x79,0x78,0x0D,
    0xC1,0x12,0x64,0xE4,0x15,0xC9,0x66,0xDB,0xF6,0xCB,0xB3,0x39,
    0x02,0x5B,0x78,0x62,0xFB,0x09,0xAE,0x09,0x6B,0xDD,0xD4,0x5D,
    0x97,0xBC,0xDC,0x7F,0xE6,0xD6,0xF1,0xCB,0xF5,0xEB,0xDA,0xA7,
    0x2E,0x5A,0x43,0x2B,0xE9,0x40,0xE2,0x85,0x00,0x1C,0xC0,0x0A,
    0x98,0x77,0xA9,0x31,0xDE,0x0B,0x75,0x4D,0x1E,0x1F,0x16,0x83,
    0xCA,0xDE,0xBD,0x21,0xFC,0xC1,0x82,0x37,0x36,0x33,0x0B,0x66,
    0x06,0x3C,0xF3,0xAF,0x21,0x57,0x57,0x80,0xF6,0x94,0x1B,0xA9,
    0xD4,0xF6,0x8F,0x18,0x62,0x0E,0xC4,0x22,0xF9,0x5B,0x62,0xCC,
    0x3F,0x19,0x95,0xCF,0x4B,0x00,0xA6,0x6C,0x0B,0xAF,0x9F,0xD5,
    0xFA,0x3D,0x6D,0xDA,0x30,0x83,0x07,0x91,0xAC,0x15,0xFF,0x8F,
    0x59,0x54,0xEA,0x25,0xBC,0x4E,0xEB,0x6A,0x54,0xDF,0x75,0x09,
    0x72,0x0F,0xEF,0x23,0x70,0xE0,0xA8,0x04,0xEA,0xFF,0x90,0x54,
    0xCD,0x84,0x18,0xC0,0x75,0x91,0x99,0x0F,0xA1,0x78,0x0C,0x07,
    0xB7,0xC5,0xDE,0x55,0x06,0x7B,0x95,0x68,0x2C,0x33,0x39,0xBC,
    0x2C,0xD0,0x6D,0xDD,0xFA,0xDC,0xB5,0x8F,0x82,0x39,0xF8,0x67,
    0x44,0xF1,0xD8,0xF7,0x78,0x11,0x9A,0x77,0x9B,0x53,0x47,0xD6,
    0x2B,0x5D,0x67,0xB8,0xB7,0xBC,0xC1,0xD7,0x79,0x62,0x15,0xC2,
    0xC5,0x83,0x97,0xA7,0xF8,0xB4,0x9C,0xF6,0x8F,0x9A,0xC7,0xDA,
    0x1B,0xBB,0x87,0x07,0xA7,0x71,0xAD,0xB2,0x8A,0x50,0xF8,0x26,
    0x12,0xB7,0x3E,0x0B,
  };
  static unsigned char dh2048_g[]={
    0x02,
  };
  DH *dh;

  if ((dh=DH_new()) == NULL) return(NULL);
  dh->p=BN_bin2bn(dh2048_p,sizeof(dh2048_p),NULL);
  dh->g=BN_bin2bn(dh2048_g,sizeof(dh2048_g),NULL);
  if ((dh->p == NULL) || (dh->g == NULL))
  { DH_free(dh); return(NULL); }
  return(dh);
}


static void
report_errors()
{
  unsigned long	l;
  const char*	file;
  const char*	data;
  int		line,flags;

  DBUG_ENTER("report_errors");

  while ((l=ERR_get_error_line_data(&file,&line,&data,&flags)) != 0)
  {
#ifndef DBUG_OFF				/* Avoid warning */
    char buf[200];
    DBUG_PRINT("error", ("OpenSSL: %s:%s:%d:%s\n", ERR_error_string(l,buf),
			 file,line,(flags & ERR_TXT_STRING) ? data : "")) ;
#endif
  }
  DBUG_VOID_RETURN;
}

static const char*
ssl_error_string[] = 
{
  "No error",
  "Unable to get certificate",
  "Unable to get private key",
  "Private key does not match the certificate public key",
  "SSL_CTX_set_default_verify_paths failed",
  "Failed to set ciphers to use",
  "SSL_CTX_new failed"
};

const char*
sslGetErrString(enum enum_ssl_init_error e)
{
  DBUG_ASSERT(SSL_INITERR_NOERROR < e && e < SSL_INITERR_LASTERR);
  return ssl_error_string[e];
}

static int
vio_set_cert_stuff(SSL_CTX *ctx, const char *cert_file, const char *key_file,
                   enum enum_ssl_init_error* error)
{
  DBUG_ENTER("vio_set_cert_stuff");
  DBUG_PRINT("enter", ("ctx: 0x%lx  cert_file: %s  key_file: %s",
		       (long) ctx, cert_file, key_file));

  if (!cert_file &&  key_file)
    cert_file= key_file;
  
  if (!key_file &&  cert_file)
    key_file= cert_file;

  if (cert_file &&
      SSL_CTX_use_certificate_chain_file(ctx, cert_file) <= 0)
  {
    *error= SSL_INITERR_CERT;
    DBUG_PRINT("error",("%s from file '%s'", sslGetErrString(*error), cert_file));
    DBUG_EXECUTE("error", ERR_print_errors_fp(DBUG_FILE););
    fprintf(stderr, "SSL error: %s from '%s'\n", sslGetErrString(*error),
            cert_file);
    fflush(stderr);
    DBUG_RETURN(1);
  }

  if (key_file &&
      SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) <= 0)
  {
    *error= SSL_INITERR_KEY;
    DBUG_PRINT("error", ("%s from file '%s'", sslGetErrString(*error), key_file));
    DBUG_EXECUTE("error", ERR_print_errors_fp(DBUG_FILE););
    fprintf(stderr, "SSL error: %s from '%s'\n", sslGetErrString(*error),
            key_file);
    fflush(stderr);
    DBUG_RETURN(1);
  }

  /*
    If we are using DSA, we can copy the parameters from the private key
    Now we know that a key and cert have been set against the SSL context
  */
  if (cert_file && !SSL_CTX_check_private_key(ctx))
  {
    *error= SSL_INITERR_NOMATCH;
    DBUG_PRINT("error", ("%s",sslGetErrString(*error)));
    DBUG_EXECUTE("error", ERR_print_errors_fp(DBUG_FILE););
    fprintf(stderr, "SSL error: %s\n", sslGetErrString(*error));
    fflush(stderr);
    DBUG_RETURN(1);
  }

  DBUG_RETURN(0);
}


static void check_ssl_init()
{
  if (!ssl_algorithms_added)
  {
    ssl_algorithms_added= TRUE;
    SSL_library_init();
    OpenSSL_add_all_algorithms();

  }

  if (!ssl_error_strings_loaded)
  {
    ssl_error_strings_loaded= TRUE;
    SSL_load_error_strings();
  }
}

/************************ VioSSLFd **********************************/
static struct st_VioSSLFd *
new_VioSSLFd(const char *key_file, const char *cert_file,
             const char *ca_file, const char *ca_path,
             const char *cipher, my_bool is_client_method,
             enum enum_ssl_init_error* error)
{
  DH *dh;
  struct st_VioSSLFd *ssl_fd;
  long ssl_ctx_options= SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3;
  DBUG_ENTER("new_VioSSLFd");
  DBUG_PRINT("enter",
             ("key_file: '%s'  cert_file: '%s'  ca_file: '%s'  ca_path: '%s'  "
              "cipher: '%s'",
              key_file ? key_file : "NULL",
              cert_file ? cert_file : "NULL",
              ca_file ? ca_file : "NULL",
              ca_path ? ca_path : "NULL",
              cipher ? cipher : "NULL"));

  check_ssl_init();

  if (!(ssl_fd= ((struct st_VioSSLFd*)
                 my_malloc(sizeof(struct st_VioSSLFd),MYF(0)))))
    DBUG_RETURN(0);

  if (!(ssl_fd->ssl_context= SSL_CTX_new(is_client_method ? 
                                         SSLv23_client_method() :
                                         SSLv23_server_method())))
  {
    *error= SSL_INITERR_MEMFAIL;
    DBUG_PRINT("error", ("%s", sslGetErrString(*error)));
    report_errors();
    my_free(ssl_fd);
    DBUG_RETURN(0);
  }

  SSL_CTX_set_options(ssl_fd->ssl_context, ssl_ctx_options);

  /*
    Set the ciphers that can be used
    NOTE: SSL_CTX_set_cipher_list will return 0 if
    none of the provided ciphers could be selected
  */
  if (cipher &&
      SSL_CTX_set_cipher_list(ssl_fd->ssl_context, cipher) == 0)
  {
    *error= SSL_INITERR_CIPHERS;
    DBUG_PRINT("error", ("%s", sslGetErrString(*error)));
    report_errors();
    SSL_CTX_free(ssl_fd->ssl_context);
    my_free(ssl_fd);
    DBUG_RETURN(0);
  }

  /* Load certs from the trusted ca */
  if (SSL_CTX_load_verify_locations(ssl_fd->ssl_context, ca_file, ca_path) == 0)
  {
    DBUG_PRINT("warning", ("SSL_CTX_load_verify_locations failed"));
    if (ca_file || ca_path)
    {
      /* fail only if ca file or ca path were supplied and looking into 
         them fails. */
      *error= SSL_INITERR_BAD_PATHS;
      DBUG_PRINT("error", ("SSL_CTX_load_verify_locations failed : %s", 
                 sslGetErrString(*error)));
      report_errors();
      SSL_CTX_free(ssl_fd->ssl_context);
      my_free(ssl_fd);
      DBUG_RETURN(0);
    }

    /* otherwise go use the defaults */
    if (SSL_CTX_set_default_verify_paths(ssl_fd->ssl_context) == 0)
    {
      *error= SSL_INITERR_BAD_PATHS;
      DBUG_PRINT("error", ("%s", sslGetErrString(*error)));
      report_errors();
      SSL_CTX_free(ssl_fd->ssl_context);
      my_free(ssl_fd);
      DBUG_RETURN(0);
    }
  }

  if (vio_set_cert_stuff(ssl_fd->ssl_context, cert_file, key_file, error))
  {
    DBUG_PRINT("error", ("vio_set_cert_stuff failed"));
    report_errors();
    SSL_CTX_free(ssl_fd->ssl_context);
    my_free(ssl_fd);
    DBUG_RETURN(0);
  }

  /* DH stuff */
  dh=get_dh2048();
  SSL_CTX_set_tmp_dh(ssl_fd->ssl_context, dh);
  DH_free(dh);

  DBUG_PRINT("exit", ("OK 1"));

  DBUG_RETURN(ssl_fd);
}


/************************ VioSSLConnectorFd **********************************/
struct st_VioSSLFd *
new_VioSSLConnectorFd(const char *key_file, const char *cert_file,
                      const char *ca_file, const char *ca_path,
                      const char *cipher, enum enum_ssl_init_error* error)
{
  struct st_VioSSLFd *ssl_fd;
  int verify= SSL_VERIFY_PEER;

  /*
    Turn off verification of servers certificate if both
    ca_file and ca_path is set to NULL
  */
  if (ca_file == 0 && ca_path == 0)
    verify= SSL_VERIFY_NONE;

  if (!(ssl_fd= new_VioSSLFd(key_file, cert_file, ca_file,
                             ca_path, cipher, TRUE, error)))
  {
    return 0;
  }

  /* Init the VioSSLFd as a "connector" ie. the client side */

  SSL_CTX_set_verify(ssl_fd->ssl_context, verify, NULL);

  return ssl_fd;
}


/************************ VioSSLAcceptorFd **********************************/
struct st_VioSSLFd *
new_VioSSLAcceptorFd(const char *key_file, const char *cert_file,
		     const char *ca_file, const char *ca_path,
		     const char *cipher, enum enum_ssl_init_error* error)
{
  struct st_VioSSLFd *ssl_fd;
  int verify= SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE;
  if (!(ssl_fd= new_VioSSLFd(key_file, cert_file, ca_file,
                             ca_path, cipher, FALSE, error)))
  {
    return 0;
  }
  /* Init the the VioSSLFd as a "acceptor" ie. the server side */

  /* Set max number of cached sessions, returns the previous size */
  SSL_CTX_sess_set_cache_size(ssl_fd->ssl_context, 128);

  SSL_CTX_set_verify(ssl_fd->ssl_context, verify, NULL);

  /*
    Set session_id - an identifier for this server session
    Use the ssl_fd pointer
   */
  SSL_CTX_set_session_id_context(ssl_fd->ssl_context,
				 (const unsigned char *)ssl_fd,
				 sizeof(ssl_fd));

  return ssl_fd;
}

void free_vio_ssl_acceptor_fd(struct st_VioSSLFd *fd)
{
  SSL_CTX_free(fd->ssl_context);
  my_free(fd);
}
#endif /* HAVE_OPENSSL */
