/*
 * Concrete Vio around Handle.
 */

#ifdef __WIN__

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

VIO_NS_BEGIN

class VioPipe : public Vio
{
public:
  VioPipe(int fd);
  virtual		~VioPipe();
  virtual bool		is_open() const;
  virtual int		read(vio_ptr buf, int size);
  virtual int		write(const vio_ptr buf, int size);
  virtual int		blocking(bool	onoff);
  virtual bool		blocking() const;
  virtual bool		fcntl() const;
  virtual int		fastsend(bool	onoff = true);
  virtual int		keepalive(bool	onoff);
  virtual bool		should_retry() const;
  virtual int		close();
  virtual void 		release();
  virtual const char*	description() const;
  virtual bool		peer_addr(char *buf) const;
  virtual const char*	cipher_description() const { return "";}
  virtual int 		vio_errno();
private:
};

VIO_NS_END

#endif /* WIN32 */
