/*
 * Abstract Virtual IO interface - class Vio. Heavily
 * influenced by Berkeley sockets and oriented toward MySQL.
 */

/* 
**  Virtual I/O library
**  Written by Andrei Errapart <andreie@no.spam.ee>
**  Modified by Monty
*/

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

VIO_NS_BEGIN

enum enum_vio_type { VIO_CLOSED, VIO_TYPE_TCPIP, VIO_TYPE_SOCKET,
		     VIO_TYPE_NAMEDPIPE, VIO_TYPE_SSL};

class Vio {
public:
  virtual bool		is_open() const = 0;
  virtual int		read(vio_ptr buf, int size) = 0;
  virtual int		write(const vio_ptr buf, int size) = 0;
  virtual int		blocking(bool	onoff) = 0;
  virtual bool		blocking() const = 0;
  virtual bool		fcntl() const = 0;
  virtual int		fastsend(bool	onoff = true) = 0;
  virtual int		keepalive(bool	onoff) = 0;
  virtual bool		should_retry() const = 0;
  virtual int		close() = 0;
  virtual void 		release();
  virtual const char*	description() const = 0;
  virtual bool		peer_addr(char *buf) const = 0;
  virtual const char*	cipher_description() const = 0;
  virtual int 		vio_errno();
  virtual ~Vio();
};

/* Macros to simulate the violite C interface */


Vio *vio_new(my_socket	sd, enum enum_vio_type type,
	     my_bool localhost);
#ifdef __WIN__
Vio*		vio_new_win32pipe(HANDLE hPipe);
#endif

#define vio_delete(vio) delete vio
#define vio_read(vio,buf,size) vio->read(buf,size)
#define vio_write(vio,buf,size) vio->write(buf,size)
#define vio_blocking(vio,mode) vio->blocking(mode)
#define vio_is_blocking(vio) vio->is_blocking()
#define vio_fastsend(vio,mode) vio->fastsend(mode)
#define vio_keepalive(vio,mode) vio->keepalive(mode)
#define vio_shouldretry(vio) vio->shouldretry(mode)
#define vio_close(vio) vio->close()
#define vio_description(vio) vio->description()
#define vio_errno(Vio *vio) vio->errno()
#define vio_peer_addr(vio,buf) vio->peer_addr(buf)
#define vio_in_addr(vio,in) vio->in_addr(in)

VIO_NS_END
