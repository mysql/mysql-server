/* 
**  Virtual I/O library
**  Written by Andrei Errapart <andreie@no.spam.ee>
*/

#include	"vio-global.h"

#ifdef	VIO_HAVE_OPENSSL

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include	<netinet/in.h>
#include	<openssl/x509.h>
#include	<openssl/ssl.h>
#include	<openssl/err.h>
#include	<openssl/pem.h>
#include	<openssl/asn1.h>

VIO_NS_BEGIN

#define	this_ssl_method		my_static_cast(SSL_METHOD*)(this->ssl_method_)
#define this_ssl_context	my_static_cast(SSL_CTX*)(this->ssl_context_)
typedef unsigned char*		ssl_data_ptr_t;

static bool	ssl_algorithms_added	= FALSE;
static bool	ssl_error_strings_loaded= FALSE;
static int	verify_depth = 0;
static int	verify_error = X509_V_OK;

static int
vio_verify_callback(int ok, X509_STORE_CTX *ctx)
{
  DBUG_ENTER("vio_verify_callback");
  DBUG_PRINT("enter", ("ok=%d, ctx=%p", ok, ctx));
  char	buf[256];
  X509*	err_cert;
  int	err,depth;

  err_cert=X509_STORE_CTX_get_current_cert(ctx);
  err=	   X509_STORE_CTX_get_error(ctx);
  depth=   X509_STORE_CTX_get_error_depth(ctx);

  X509_NAME_oneline(X509_get_subject_name(err_cert),buf,sizeof(buff));
  if (!ok)
  {
    DBUG_PRINT("error",("verify error:num=%d:%s\n",err,
			X509_verify_cert_error_string(err)));
    if (verify_depth >= depth)
    {
      ok=1;
      verify_error=X509_V_OK;
    }
    else
    {
      ok=0;
      verify_error=X509_V_ERR_CERT_CHAIN_TOO_LONG;
    }
  }
  switch (ctx->error) {
  case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
    X509_NAME_oneline(X509_get_issuer_name(ctx->current_cert),buf,256);
    DBUG_PRINT("info",("issuer= %s\n",buf));
    break;
  case X509_V_ERR_CERT_NOT_YET_VALID:
  case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
    DBUG_PRINT("error", ("notBefore"));
    //ASN1_TIME_print_fp(stderr,X509_get_notBefore(ctx->current_cert));
    break;
  case X509_V_ERR_CERT_HAS_EXPIRED:
  case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
    DBUG_PRINT("error", ("notAfter error"));
    //ASN1_TIME_print_fp(stderr,X509_get_notAfter(ctx->current_cert));
    break;
  }
  DBUG_PRINT("exit", ("r=%d", ok));
  DBUG_RETURN(ok);
}


static int
vio_set_cert_stuff(SSL_CTX *ctx, const char *cert_file, const char *key_file)
{
  DBUG_ENTER("vio_set_cert_stuff");
  DBUG_PRINT("enter", ("ctx=%p, cert_file=%p, key_file=%p",
		       ctx, cert_file, key_file));
  if (cert_file != NULL)
  {
    if (SSL_CTX_use_certificate_file(ctx,cert_file,SSL_FILETYPE_PEM) <= 0)
    {
      DBUG_PRINT("error",("unable to get certificate from '%s'\n",cert_file));
      /* FIX stderr */
      ERR_print_errors_fp(stderr);
      DBUG_RETURN(0);
    }
    if (key_file == NULL)
      key_file = cert_file;
    if (SSL_CTX_use_PrivateKey_file(ctx,key_file,
				    SSL_FILETYPE_PEM) <= 0)
    {
      DBUG_PRINT("error", ("unable to get private key from '%s'\n",key_file));
      /* FIX stderr */
      ERR_print_errors_fp(stderr);
      DBUG_RETURN(0);
    }

    /* If we are using DSA, we can copy the parameters from
     * the private key */
    /* Now we know that a key and cert have been set against
     * the SSL context */
    if (!SSL_CTX_check_private_key(ctx))
    {
      DBUG_PRINT("error", ("Private key does not match the certificate public key\n"));
      DBUG_RETURN(0);
    }
  }
  DBUG_RETURN(1);
}

/************************ VioSSLConnectorFd **********************************/
VioSSLConnectorFd::VioSSLConnectorFd(const char* key_file,
				     const char* cert_file,
				     const char* ca_file,
				     const char* ca_path)
:ssl_context_(0),ssl_method_(0)
{
  DBUG_ENTER("VioSSLConnectorFd::VioSSLConnectorFd");
  DBUG_PRINT("enter",
	     ("this=%p, key_file=%s, cert_file=%s, ca_path=%s, ca_file=%s",
	      this, key_file, cert_file, ca_path, ca_file));

  /* FIXME: constants! */
  int	verify = SSL_VERIFY_PEER;

  if (!ssl_algorithms_added)
  {
    DBUG_PRINT("info", ("todo: SSLeay_add_ssl_algorithms()"));
    ssl_algorithms_added = true;
    SSLeay_add_ssl_algorithms();
  }
  if (!ssl_error_strings_loaded)
  {
    DBUG_PRINT("info", ("todo:SSL_load_error_strings()"));
    ssl_error_strings_loaded = true;
    SSL_load_error_strings();
  }
  ssl_method_ = SSLv3_client_method();
  ssl_context_ = SSL_CTX_new(this_ssl_method);
  if (ssl_context_ == 0)
  {
    DBUG_PRINT("error", ("SSL_CTX_new failed"));
    report_errors();
    goto ctor_failure;
  }
  /*
   * SSL_CTX_set_options
   * SSL_CTX_set_info_callback
   * SSL_CTX_set_cipher_list
   */
  SSL_CTX_set_verify(this_ssl_context, verify, vio_verify_callback);
  if (vio_set_cert_stuff(this_ssl_context, cert_file, key_file) == -1)
  {
    DBUG_PRINT("error", ("vio_set_cert_stuff failed"));
    report_errors();
    goto ctor_failure;
  }
  if (SSL_CTX_load_verify_locations( this_ssl_context, ca_file,ca_path)==0)
  {
    DBUG_PRINT("warning", ("SSL_CTX_load_verify_locations failed"));
    if (SSL_CTX_set_default_verify_paths(this_ssl_context)==0)
    {
      DBUG_PRINT("error", ("SSL_CTX_set_default_verify_paths failed"));
      report_errors();
      goto ctor_failure;
    }
  }
  DBUG_VOID_RETURN;
ctor_failure:
  DBUG_PRINT("exit", ("there was an error"));
  DBUG_VOID_RETURN;
}

VioSSLConnectorFd::~VioSSLConnectorFd()
{
  DBUG_ENTER("VioSSLConnectorFd::~VioSSLConnectorFd");
  DBUG_PRINT("enter", ("this=%p", this));
  if (ssl_context_!=0)
    SSL_CTX_free(this_ssl_context);
  DBUG_VOID_RETURN;
}

VioSSL* VioSSLConnectorFd::connect(	int	fd)
{
  DBUG_ENTER("VioSSLConnectorFd::connect");
  DBUG_PRINT("enter", ("this=%p, fd=%d", this, fd));
  DBUG_RETURN(new VioSSL(fd, ssl_context_, VioSSL::state_connect));
}

VioSSL*
VioSSLConnectorFd::connect(	VioSocket*	sd)
{
  DBUG_ENTER("VioSSLConnectorFd::connect");
  DBUG_PRINT("enter", ("this=%p, sd=%s", this, sd->description()));
  DBUG_RETURN(new VioSSL(sd, ssl_context_, VioSSL::state_connect));
}

void
VioSSLConnectorFd::report_errors()
{
  unsigned long	l;
  const char*	file;
  const char*	data;
  int		line,flags;

  DBUG_ENTER("VioSSLConnectorFd::report_errors");
  DBUG_PRINT("enter", ("this=%p", this));

  while ((l=ERR_get_error_line_data(&file,&line,&data,&flags)) != 0)
  {
    char buf[200];
    DBUG_PRINT("error", ("OpenSSL: %s:%s:%d:%s\n", ERR_error_string(l,buf),
			 file,line,(flags&ERR_TXT_STRING)?data:"")) ;
  }
  DBUG_VOID_RETURN;
}

/************************ VioSSLAcceptorFd **********************************/

VioSSLAcceptorFd::VioSSLAcceptorFd(const char*	key_file,
				   const char*	cert_file,
				   const char*	ca_file,
				   const char*	ca_path)
  :ssl_context_(0), ssl_method_(0)
{
  DBUG_ENTER("VioSSLAcceptorFd::VioSSLAcceptorFd");
  DBUG_PRINT("enter",
	     ("this=%p, key_file=%s, cert_file=%s, ca_path=%s, ca_file=%s",
	      this, key_file, cert_file, ca_path, ca_file));

  /* FIXME: constants! */
  int	verify = (SSL_VERIFY_PEER			|
		  SSL_VERIFY_FAIL_IF_NO_PEER_CERT	|
		  SSL_VERIFY_CLIENT_ONCE);
  session_id_context_ = static_cast<vio_ptr>(this);

  if (!ssl_algorithms_added)
  {
    DBUG_PRINT("info", ("todo: SSLeay_add_ssl_algorithms()"));
    ssl_algorithms_added = true;
    SSLeay_add_ssl_algorithms();
  }
  if (!ssl_error_strings_loaded)
  {
    DBUG_PRINT("info", ("todo: SSL_load_error_strings()"));
    ssl_error_strings_loaded = true;
    SSL_load_error_strings();
  }
  ssl_method_ = SSLv3_server_method();
  ssl_context_ = SSL_CTX_new(this_ssl_method);
  if (ssl_context_==0)
  {
    DBUG_PRINT("error", ("SSL_CTX_new failed"));
    report_errors();
    goto ctor_failure;
  }
  /*
   * SSL_CTX_set_quiet_shutdown(ctx,1);
   * 
   */
  SSL_CTX_sess_set_cache_size(this_ssl_context,128);

  /* DH?
   */
  SSL_CTX_set_verify(this_ssl_context, verify, vio_verify_callback);
  /*
   * Double cast needed at least for egcs-1.1.2 to
   * supress warnings:
   * 1) ANSI C++ blaah implicit cast from 'void*' to 'unsigned char*'
   * 2) static_cast from 'void**' to 'unsigned char*'
   * Wish I had a copy of standard handy...
   */
  SSL_CTX_set_session_id_context(this_ssl_context,
				 my_static_cast(ssl_data_ptr_t)
				 (my_static_cast(void*)(&session_id_context_)),
				 sizeof(session_id_context_));

  /*
   * SSL_CTX_set_client_CA_list(ctx,SSL_load_client_CA_file(CAfile));
   */
  if (vio_set_cert_stuff(this_ssl_context, cert_file, key_file) == -1)
  {
    DBUG_PRINT("error", ("vio_set_cert_stuff failed"));
    report_errors();
    goto ctor_failure;
  }
  if (SSL_CTX_load_verify_locations( this_ssl_context, ca_file, ca_path)==0)
  {
    DBUG_PRINT("warning", ("SSL_CTX_load_verify_locations failed"));
    if (SSL_CTX_set_default_verify_paths(this_ssl_context)==0)
    {
      DBUG_PRINT("error", ("SSL_CTX_set_default_verify_paths failed"));
      report_errors();
      goto ctor_failure;
    }
  }
  DBUG_VOID_RETURN;
ctor_failure:
  DBUG_PRINT("exit", ("there was an error"));
  DBUG_VOID_RETURN;
}

VioSSLAcceptorFd::~VioSSLAcceptorFd()
{
  DBUG_ENTER("VioSSLAcceptorFd::~VioSSLAcceptorFd");
  DBUG_PRINT("enter", ("this=%p", this));
  if (ssl_context_!=0)
    SSL_CTX_free(this_ssl_context);
  DBUG_VOID_RETURN;
}

VioSSL*
VioSSLAcceptorFd::accept(int fd)
{
  DBUG_ENTER("VioSSLAcceptorFd::accept");
  DBUG_PRINT("enter", ("this=%p, fd=%d", this, fd));
  DBUG_RETURN(new VioSSL(fd, ssl_context_, VioSSL::state_accept));
}

VioSSL*
VioSSLAcceptorFd::accept(VioSocket* sd)
{
  DBUG_ENTER("VioSSLAcceptorFd::accept");
  DBUG_PRINT("enter", ("this=%p, sd=%s", this, sd->description()));
  DBUG_RETURN(new VioSSL(sd, ssl_context_, VioSSL::state_accept));
}

void
VioSSLAcceptorFd::report_errors()
{
  unsigned long	l;
  const char*	file;
  const char*	data;
  int		line,flags;

  DBUG_ENTER("VioSSLConnectorFd::report_errors");
  DBUG_PRINT("enter", ("this=%p", this));

  while ((l=ERR_get_error_line_data(&file,&line,&data,&flags)) != 0)
  {
    char buf[200];
    DBUG_PRINT("error", ("OpenSSL: %s:%s:%d:%s\n", ERR_error_string(l,buf),
			 file,line,(flags&ERR_TXT_STRING)?data:"")) ;
  }
  DBUG_VOID_RETURN;
}

VIO_NS_END

#endif /* VIO_HAVE_OPENSSL */
