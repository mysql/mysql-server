/*
 * Concrete Vio around a file descriptor.
 */

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

VIO_NS_BEGIN

class VioFd : public Vio
{
public:
  VioFd(	int	fd);
  virtual			~VioFd();
  virtual bool		open() const;
  virtual int		read(	vio_ptr	buf,	int	size);
  virtual int		write(	const vio_ptr	buf,	int	size);
  virtual bool		blocking() const;
  virtual int		blocking(bool onoff);
  virtual int		fastsend(bool onoff=true);
  virtual int		keepalive(	bool	onoff);
  virtual bool		fcntl() const;
  virtual bool		should_retry() const;
  virtual int		fcntl(	int	cmd);
  virtual int		fcntl(	int	cmd,	long	arg);
  virtual int		fcntl(	int	cmd,	struct flock*	lock);
  virtual int		close();
  virtual const char*	description() const;
  virtual const char*	peer_addr() const;
  virtual bool		peer_name(char *buf) const;
  virtual const char*	cipher_description() const;
private:
  int			fd_;
  char			desc_[100];
};

VIO_NS_END
