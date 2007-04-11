/* Copyright (C) 2000 MySQL AB

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

#include "vio_priv.h"

#ifdef HAVE_OPENSSL

static bool     ssl_algorithms_added    = FALSE;
static bool     ssl_error_strings_loaded= FALSE;
static int      verify_depth = 0;

static unsigned char dh512_p[]=
{
  0xDA,0x58,0x3C,0x16,0xD9,0x85,0x22,0x89,0xD0,0xE4,0xAF,0x75,
  0x6F,0x4C,0xCA,0x92,0xDD,0x4B,0xE5,0x33,0xB8,0x04,0xFB,0x0F,
  0xED,0x94,0xEF,0x9C,0x8A,0x44,0x03,0xED,0x57,0x46,0x50,0xD3,
  0x69,0x99,0xDB,0x29,0xD7,0x76,0x27,0x6B,0xA2,0xD3,0xD4,0x12,
  0xE2,0x18,0xF4,0xDD,0x1E,0x08,0x4C,0xF6,0xD8,0x00,0x3E,0x7C,
  0x47,0x74,0xE8,0x33,
};

static unsigned char dh512_g[]={
  0x02,
};

static DH *get_dh512(void)
{
  DH *dh;
  if ((dh=DH_new()))
  {
    dh->p=BN_bin2bn(dh512_p,sizeof(dh512_p),NULL);
    dh->g=BN_bin2bn(dh512_g,sizeof(dh512_g),NULL);
    if (! dh->p || ! dh->g)
    {
      DH_free(dh);
      dh=0;
    }
  }
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


static int
vio_set_cert_stuff(SSL_CTX *ctx, const char *cert_file, const char *key_file)
{
  DBUG_ENTER("vio_set_cert_stuff");
  DBUG_PRINT("enter", ("ctx: 0x%lx  cert_file: %s  key_file: %s",
		       (long) ctx, cert_file, key_file));
  if (cert_file)
  {
    if (SSL_CTX_use_certificate_file(ctx, cert_file, SSL_FILETYPE_PEM) <= 0)
    {
      DBUG_PRINT("error",("unable to get certificate from '%s'", cert_file));
      DBUG_EXECUTE("error", ERR_print_errors_fp(DBUG_FILE););
      fprintf(stderr, "SSL error: Unable to get certificate from '%s'\n",
              cert_file);
      fflush(stderr);
      DBUG_RETURN(1);
    }

    if (!key_file)
      key_file= cert_file;

    if (SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) <= 0)
    {
      DBUG_PRINT("error", ("unable to get private key from '%s'", key_file));
      DBUG_EXECUTE("error", ERR_print_errors_fp(DBUG_FILE););
      fprintf(stderr, "SSL error: Unable to get private key from '%s'\n",
              key_file);
      fflush(stderr);
      DBUG_RETURN(1);
    }

    /*
      If we are using DSA, we can copy the parameters from the private key
      Now we know that a key and cert have been set against the SSL context
    */
    if (!SSL_CTX_check_private_key(ctx))
    {
      DBUG_PRINT("error",
		 ("Private key does not match the certificate public key"));
      DBUG_EXECUTE("error", ERR_print_errors_fp(DBUG_FILE););
      fprintf(stderr,
              "SSL error: "
              "Private key does not match the certificate public key\n");
      fflush(stderr);
      DBUG_RETURN(1);
    }
  }
  DBUG_RETURN(0);
}


static int
vio_verify_callback(int ok, X509_STORE_CTX *ctx)
{
  char buf[256];
  X509 *err_cert;

  DBUG_ENTER("vio_verify_callback");
  DBUG_PRINT("enter", ("ok: %d  ctx: 0x%lx", ok, (long) ctx));

  err_cert= X509_STORE_CTX_get_current_cert(ctx);
  X509_NAME_oneline(X509_get_subject_name(err_cert), buf, sizeof(buf));
  DBUG_PRINT("info", ("cert: %s", buf));
  if (!ok)
  {
    int err, depth;
    err= X509_STORE_CTX_get_error(ctx);
    depth= X509_STORE_CTX_get_error_depth(ctx);

    DBUG_PRINT("error",("verify error: %d  '%s'",err,
			X509_verify_cert_error_string(err)));
    /*
      Approve cert if depth is greater then "verify_depth", currently
      verify_depth is always 0 and there is no way to increase it.
     */
    if (verify_depth >= depth)
      ok= 1;
  }
  switch (ctx->error)
  {
  case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
    X509_NAME_oneline(X509_get_issuer_name(ctx->current_cert), buf, 256);
    DBUG_PRINT("info",("issuer= %s\n", buf));
    break;
  case X509_V_ERR_CERT_NOT_YET_VALID:
  case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
    DBUG_PRINT("error", ("notBefore"));
    /*ASN1_TIME_print_fp(stderr,X509_get_notBefore(ctx->current_cert));*/
    break;
  case X509_V_ERR_CERT_HAS_EXPIRED:
  case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
    DBUG_PRINT("error", ("notAfter error"));
    /*ASN1_TIME_print_fp(stderr,X509_get_notAfter(ctx->current_cert));*/
    break;
  }
  DBUG_PRINT("exit", ("%d", ok));
  DBUG_RETURN(ok);
}


#ifdef __NETWARE__

/* NetWare SSL cleanup */
void netware_ssl_cleanup()
{
  /* free memory from SSL_library_init() */
  EVP_cleanup();

/* OpenSSL NetWare port specific functions */
#ifndef HAVE_YASSL

  /* free global X509 method */
  X509_STORE_method_cleanup();

  /* free the thread_hash error table */
  ERR_free_state_table();
#endif
}


/* NetWare SSL initialization */
static void netware_ssl_init()
{
  /* cleanup OpenSSL library */
  NXVmRegisterExitHandler(netware_ssl_cleanup, NULL);
}

#endif /* __NETWARE__ */


static void check_ssl_init()
{
  if (!ssl_algorithms_added)
  {
    ssl_algorithms_added= TRUE;
    SSL_library_init();
    OpenSSL_add_all_algorithms();

  }

#ifdef __NETWARE__
  netware_ssl_init();
#endif

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
             const char *cipher, SSL_METHOD *method)
{
  DH *dh;
  struct st_VioSSLFd *ssl_fd;
  DBUG_ENTER("new_VioSSLFd");

  check_ssl_init();

  if (!(ssl_fd= ((struct st_VioSSLFd*)
                 my_malloc(sizeof(struct st_VioSSLFd),MYF(0)))))
    DBUG_RETURN(0);

  if (!(ssl_fd->ssl_context= SSL_CTX_new(method)))
  {
    DBUG_PRINT("error", ("SSL_CTX_new failed"));
    report_errors();
    my_free((void*)ssl_fd,MYF(0));
    DBUG_RETURN(0);
  }

  /*
    Set the ciphers that can be used
    NOTE: SSL_CTX_set_cipher_list will return 0 if
    none of the provided ciphers could be selected
  */
  if (cipher &&
      SSL_CTX_set_cipher_list(ssl_fd->ssl_context, cipher) == 0)
  {
    DBUG_PRINT("error", ("failed to set ciphers to use"));
    report_errors();
    SSL_CTX_free(ssl_fd->ssl_context);
    my_free((void*)ssl_fd,MYF(0));
    DBUG_RETURN(0);
  }

  /* Load certs from the trusted ca */
  if (SSL_CTX_load_verify_locations(ssl_fd->ssl_context, ca_file, ca_path) == 0)
  {
    DBUG_PRINT("warning", ("SSL_CTX_load_verify_locations failed"));
    if (SSL_CTX_set_default_verify_paths(ssl_fd->ssl_context) == 0)
    {
      DBUG_PRINT("error", ("SSL_CTX_set_default_verify_paths failed"));
      report_errors();
      SSL_CTX_free(ssl_fd->ssl_context);
      my_free((void*)ssl_fd,MYF(0));
      DBUG_RETURN(0);
    }
  }

  if (vio_set_cert_stuff(ssl_fd->ssl_context, cert_file, key_file))
  {
    DBUG_PRINT("error", ("vio_set_cert_stuff failed"));
    report_errors();
    SSL_CTX_free(ssl_fd->ssl_context);
    my_free((void*)ssl_fd,MYF(0));
    DBUG_RETURN(0);
  }

  /* DH stuff */
  dh=get_dh512();
  SSL_CTX_set_tmp_dh(ssl_fd->ssl_context, dh);
  DH_free(dh);

  DBUG_PRINT("exit", ("OK 1"));

  DBUG_RETURN(ssl_fd);
}


/************************ VioSSLConnectorFd **********************************/
struct st_VioSSLFd *
new_VioSSLConnectorFd(const char *key_file, const char *cert_file,
                      const char *ca_file, const char *ca_path,
                      const char *cipher)
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
                             ca_path, cipher, TLSv1_client_method())))
  {
    return 0;
  }

  /* Init the VioSSLFd as a "connector" ie. the client side */

  /*
    The verify_callback function is used to control the behaviour
    when the SSL_VERIFY_PEER flag is set.
  */
  SSL_CTX_set_verify(ssl_fd->ssl_context, verify, vio_verify_callback);

  return ssl_fd;
}


/************************ VioSSLAcceptorFd **********************************/
struct st_VioSSLFd*
new_VioSSLAcceptorFd(const char *key_file, const char *cert_file,
		     const char *ca_file, const char *ca_path,
		     const char *cipher)
{
  struct st_VioSSLFd *ssl_fd;
  int verify= SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE;
  if (!(ssl_fd= new_VioSSLFd(key_file, cert_file, ca_file,
                             ca_path, cipher, TLSv1_server_method())))
  {
    return 0;
  }
  /* Init the the VioSSLFd as a "acceptor" ie. the server side */

  /* Set max number of cached sessions, returns the previous size */
  SSL_CTX_sess_set_cache_size(ssl_fd->ssl_context, 128);

  /*
    The verify_callback function is used to control the behaviour
    when the SSL_VERIFY_PEER flag is set.
  */
  SSL_CTX_set_verify(ssl_fd->ssl_context, verify, vio_verify_callback);

  /*
    Set session_id - an identifier for this server session
    Use the ssl_fd pointer
   */
  SSL_CTX_set_session_id_context(ssl_fd->ssl_context,
				 (const unsigned char *)ssl_fd,
				 sizeof(ssl_fd));

  return ssl_fd;
}
#endif /* HAVE_OPENSSL */
