/* 
**  Virtual I/O library
**  Written by Andrei Errapart <andreie@no.spam.ee>
*/

/*
 * Concrete Vio around socket. Doesn't differ much from VioFd.
 */

#ifdef WIN32
	typedef	SOCKET	vio_socket;
#else
	typedef int	vio_socket;
#endif /* WIN32 */

VIO_NS_BEGIN

class VioSSL;
class VioSocket : public Vio
{
public:
  VioSocket(vio_socket sd, bool	localhost=true);
  virtual ~VioSocket();
  virtual bool		is_open() const;
  virtual int		read(vio_ptr		buf,	int	size);
  virtual int		write(const vio_ptr		buf,	int	size);
  virtual int		blocking(bool    onoff);
  virtual bool		blocking() const;
  virtual int		fastsend(bool	onoff=true);
  virtual int		keepalive(bool	onoff);
  virtual bool		should_retry() const;
  virtual int		close();
  virtual const char*	description() const;
  virtual bool		peer_addr(char *buf) const;
  virtual const char*	cipher_description() const;
  virtual int 		vio_errno();
  int			shutdown(int how);

private:
  vio_socket			sd_;
  const bool			localhost_;
  int				fcntl_;
  bool				fcntl_set_;
  char				desc_[30];
  mutable struct sockaddr_in	local_;
  mutable struct sockaddr_in	remote_;
  mutable char*			cipher_description_;

  friend class VioSSL;	// he wants to tinker with this->sd_;
};

VIO_NS_END

#endif /* vio_VioSocket_h_ */

