/*
 * Concrete Vio around OpenSSL's SSL structure.
 */

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

VIO_NS_BEGIN

class VioSocket;

class VioSSL : public Vio
{
public:
  enum {
    state_connect	= 1,
    state_accept	= 2
  };
public:
  VioSSL(int	fd, vio_ptr ssl_context, int	state);
  VioSSL(VioSocket* sd, vio_ptr	ssl_context, int state);
  virtual		~VioSSL();
  virtual bool		open() const;
  virtual int		read(	vio_ptr	buf,	int	size);
  virtual int		write(	const vio_ptr	buf,	int	size);
  virtual bool		blocking() const;
  virtual int		blocking(bool onoff);
  virtual int		fastsend(bool onoff=true);
  virtual int		keepalive(bool onoff);
  virtual bool		fcntl() const;
  virtual bool		should_retry() const;
  virtual int		close();
  virtual const char*	description() const;
  virtual const char*	peer_addr() const;
  virtual const char*	peer_name() const;
  virtual const char*	cipher_description() const;

private:
  int init_bio_(int fd,
		vio_ptr	ssl_context,
		int state,
		int bio_flags);
  vio_ptr bio_;
  vio_ptr ssl_con_;
  vio_ptr ssl_cip_;
  char	desc_[100];
  bool	open_;
  VioSocket* sd_;
};

VIO_NS_END

#endif /* VIO_HAVE_OPENSSL */
