/*
 * Wrapper around SSL_CTX.
 */

#ifdef VIO_HAVE_OPENSSL

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

VIO_NS_BEGIN

class VioSSLAcceptorFd : public VioAcceptorFd
{
public:
  VioSSLAcceptorFd(const char*	key_file,
		   const char*	cert_file,
		   const char*	ca_file,
		   const char*	ca_path);
						
  virtual ~VioSSLAcceptorFd();
  virtual VioSSL*  accept(int	fd);
  virtual VioSSL*  accept(VioSocket*	sd);
private:
  VioSSLAcceptorFd(const VioSSLAcceptorFd& rhs);//undefined
  VioSSLAcceptorFd& operator=(const VioSSLAcceptorFd& rhs);//undefined
private:
  void	 report_errors();
  vio_ptr ssl_;
  vio_ptr ssl_context_;
  vio_ptr ssl_method_;
  vio_ptr session_id_context_;
};

VIO_NS_END

/*
 * The Factory where Vio's are made!
 */

class VioSSLConnectorFd : public VioConnectorFd
{
public:
  VioSSLConnectorFd(const char*	key_file,
		    const char*	cert_file,
		    const char*	ca_file,
		    const char*	ca_path);
						
  virtual ~VioSSLConnectorFd();
  virtual VioSSL* connect(int		fd);
  virtual VioSSL* connect(VioSocket*	sd);
private:
  VioSSLConnectorFd(const VioSSLConnectorFd& rhs);//undefined
  VioSSLConnectorFd& operator=(const VioSSLConnectorFd& rhs);//undefined
private:
  void	  report_errors();
  vio_ptr ssl_context_;
  vio_ptr ssl_method_;
  vio_ptr ssl_;
};

VIO_NS_END

#endif /* VIO_HAVE_OPENSSL */
