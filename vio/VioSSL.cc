/* 
**  Virtual I/O library for SSL wrapper
**  Written by Andrei Errapart <andreie@no.spam.ee>
*/

/*
 * This file has some huge DBUG_ statements. Boy, this is silly...
 */

#include	"vio-global.h"
#ifdef	VIO_HAVE_OPENSSL
#include	<assert.h>
#include	<netinet/in.h>
#include	<openssl/x509.h>
#include	<openssl/ssl.h>
#include	<openssl/err.h>
#include	<openssl/pem.h>

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

VIO_NS_BEGIN

#define this_ssl_con	my_static_cast(SSL*)(this->ssl_con_)
#define this_bio	my_static_cast(BIO*)(this->bio_)
typedef char*		dataptr_t;

static void
report_errors()
{
  unsigned long	l;
  const char*	file;
  const char*	data;
  int		line,flags;
  DBUG_ENTER("VioSSLConnectorFd::report_errors");

  while ((l=ERR_get_error_line_data(&file,&line,&data,&flags)) != 0)
  {
    char buf[200];
    DBUG_PRINT("error", ("OpenSSL: %s:%s:%d:%s\n", ERR_error_string(l,buf),
			 file,line,(flags&ERR_TXT_STRING)?data:"")) ;
  }
  DBUG_VOID_RETURN;
}

//FIXME: duplicate code!
VioSSL::VioSSL(int fd,
	       vio_ptr	ssl_context,
	       int state)
  : bio_(0), ssl_con_(0), open_(FALSE), sd_(new VioSocket(fd))
{
  DBUG_ENTER("VioSSL::VioSSL");
  DBUG_PRINT("enter", ("this=%p, fd=%d, ssl_context=%p, state=%d",
		       this, fd, ssl_context, state));
  assert(fd!=0);
  assert(ssl_context!=0);
  assert(state==state_connect || state==state_accept);

  if (!init_bio_(fd, ssl_context, state, BIO_NOCLOSE))
    open_ = true;
  DBUG_VOID_RETURN;
}


VioSSL::VioSSL(VioSocket* sd,
	       vio_ptr	 ssl_context,
	       int state)
  :bio_(0), ssl_con_(0), open_(FALSE), sd_(sd)
{
  DBUG_ENTER("VioSSL::VioSSL");
  DBUG_PRINT("enter",
	     ("this=%p, sd=%s, ssl_context=%p, state=%d",
	      this, sd ? sd->description() : "0", ssl_context, state));
  assert(sd != 0);
  assert(ssl_context != 0);
  assert(state == state_connect || state==state_accept);

  if (!init_bio_(sd->sd_, ssl_context, state, BIO_NOCLOSE))
    open_ = true;
  DBUG_VOID_RETURN;
}

VioSSL::~VioSSL()
{
  DBUG_ENTER("VioSSL::~VioSSL");
  DBUG_PRINT("enter", ("this=%p", this));
  if (ssl_con_!=0)
  {
    SSL_shutdown(this_ssl_con);
    SSL_free(this_ssl_con);
  }
  if (sd_!=0)
    delete sd_;
  /* FIXME: no need to close bio? */
  /*
    if (bio_!=0)
    BIO_free(this_bio);
  */
  DBUG_VOID_RETURN;
}

bool
VioSSL::is_open() const
{
  return open_;
}

int
VioSSL::read(vio_ptr buf, int size)
{
  int r;
  DBUG_ENTER("VioSSL::read");
  DBUG_PRINT("enter", ("this=%p, buf=%p, size=%d", this, buf, size));
  assert(this_ssl_con != 0);
  r = SSL_read(this_ssl_con, my_static_cast(dataptr_t)(buf), size);
  if ( r< 0)
    report_errors();
  DBUG_PRINT("exit", ("r=%d", r));
  DBUG_RETURN(r);
}

int
VioSSL::write(const vio_ptr buf, int size)
{
  int r;
  DBUG_ENTER("VioSSL::write");
  DBUG_PRINT("enter", ("this=%p, buf=%p, size=%d", this, buf, size));
  assert(this_ssl_con!=0);
  r = SSL_write(this_ssl_con, my_static_cast(dataptr_t)(buf), size);
  if (r<0)
    report_errors();
  DBUG_PRINT("exit", ("r=%d", r));
  DBUG_RETURN(r);
}

int
VioSSL::blocking(bool onoff)
{
  int r;
  DBUG_ENTER("VioSSL::blocking");
  DBUG_PRINT("enter", ("this=%p, onoff=%s", this, onoff?"true":"false"));
  r = sd_->blocking(onoff);
  DBUG_PRINT("exit", ("r=%d", (int)r ));
  DBUG_RETURN(r);
}

bool 
VioSSL::blocking() const
{
  bool r;
  DBUG_ENTER("VioSSL::blocking");
  DBUG_PRINT("enter", ("this=%p", this));
  r = sd_->blocking();
  DBUG_PRINT("exit", ("r=%d", (int)r ));
  DBUG_RETURN(r);
}

int
VioSSL::fastsend(bool onoff)
{
  int r;
  DBUG_ENTER("VioSSL::fastsend");
  DBUG_PRINT("enter", ("this=%p, onoff=%d", this, (int) onoff));
  r = sd_->fastsend(onoff);
  DBUG_PRINT("exit", ("r=%d", (int)r ));
  DBUG_RETURN(r);
}

int VioSSL::keepalive(bool onoff)
{
  int r;
  DBUG_ENTER("VioSSL::keepalive");
  DBUG_PRINT("enter", ("this=%p, onoff=%d", this, (int) onoff));
  r = sd_->keepalive(onoff);
  DBUG_PRINT("exit", ("r=%d", int(r) ));
  DBUG_RETURN(r);
}

bool
VioSSL::fcntl() const
{
  bool	r;
  DBUG_ENTER("VioSSL::fcntl");
  DBUG_PRINT("enter", ("this=%p", this));
  r = sd_->fcntl();
  DBUG_PRINT("exit", ("r=%d", (int)r ));
  DBUG_RETURN(r);
}

bool
VioSSL::should_retry() const
{
  bool r;
  DBUG_ENTER("VioSSL::should_retry");
  DBUG_PRINT("enter", ("this=%p", this));
  r = sd_->should_retry();
  DBUG_PRINT("exit", ("r=%d", (int)r ));
  DBUG_RETURN(r);
}

int
VioSSL::close()
{
  int r= -2;
  DBUG_ENTER("VioSSL::close");
  DBUG_PRINT("enter", ("this=%p", this));
  if (ssl_con)
  {
    r = SSL_shutdown(this_ssl_con);
    SSL_free(this_ssl_con);
    ssl_con_ = 0;
    BIO_free(this_bio);
    bio_ = 0;
  }
  DBUG_PRINT("exit", ("r=%d", r));
  DBUG_RETURN(r);
}

const char*
VioSSL::description() const
{
  return desc_;
}

const char*
VioSSL::peer_addr() const
{
  if (sd_!=0)
    return sd != 0 ? sd_->peer_addr() : "";
}

const char*
VioSSL::peer_name() const
{
  return sd != 0 ? sd_->peer_name() : "";
}

const char*
VioSSL::cipher_description() const
{
  return SSL_get_cipher_name(this_ssl_con);
}


int
VioSSL::init_bio_(int fd,
		  vio_ptr ssl_context,
		  int state,
		  int bio_flags)
{
  DBUG_ENTER("VioSSL::init_bio_");
  DBUG_PRINT("enter",
	     ("this=%p, fd=%p, ssl_context=%p, state=%d, bio_flags=%d",
	      this, fd, ssl_context, state, bio_flags));

  
  if (!(ssl_con_ = SSL_new(my_static_cast(SSL_CTX*)(ssl_context))))
 {
    DBUG_PRINT("error", ("SSL_new failure"));
    report_errors();
    DBUG_RETURN(-1);
  }
  if (!(bio_ = BIO_new_socket(fd, bio_flags)))
  {
    DBUG_PRINT("error", ("BIO_new_socket failure"));
    report_errors();
    SSL_free(ssl_con_);
    ssl_con_ =0;
    DBUG_RETURN(-1);
  }
  SSL_set_bio(this_ssl_con, this_bio, this_bio);
  switch(state) {
  case state_connect:
    SSL_set_connect_state(this_ssl_con);
    break;
  case state_accept:
    SSL_set_accept_state(this_ssl_con);
    break;
  default:
    assert(0);
  }
  sprintf(desc_, "VioSSL(%d)", fd);
  ssl_cip_ = new SSL_CIPHER ;
  DBUG_RETURN(0);
}


VIO_NS_END

#endif /* VIO_HAVE_OPENSSL */

