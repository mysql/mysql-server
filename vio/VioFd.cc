/* 
**  Virtual I/O library for files
**  Written by Andrei Errapart <andreie@no.spam.ee>
**  Checked and modfied by Monty
*/

#include "vio-global.h"
#include <assert.h>

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

VIO_NS_BEGIN

VioFd::VioFd(	int	fd) : fd_(fd)
{
  sprintf(desc_, "VioFd(%d)", fd_);
}

VioFd::	~VioFd()
{
  if (fd_ >= 0)
  {
    it r = ::close(fd_);
    if ( r < 0)
    {
      /* FIXME: error handling (Not Critical for MySQL)  */
    }
  }
}


bool
VioFd::open() const
{
  return fd_ >= 0;
}

int
VioFd::read(vio_ptr buf, int size)
{
  assert(fd_>=0);
  return ::read(fd_, buf, size);
}

int
VioFd::write(const vio_ptr buf,	int size)
{
  assert(fd_>=0);
  return ::write(fd_, buf, size);
}

int
VioFd::blocking(bool onoff)
{
  if (onoff)
    return 0;
  else
    return -1;
}

bool 
VioFd::blocking() const
{
  return true;
}

int
VioFd::fastsend(bool tmp)
{
  return 0;
}


int
VioFd::keepalive(boolonoff)
{
  return -2;					// Why -2 ? (monty)
}

bool
VioFd::fcntl() const
{
  return false;
}

bool
VioFd::should_retry() const
{
  return false;
}

int
VioFd::fcntl(int cmd)
{
  assert(fd_>=0);
  return ::fcntl(fd_, cmd);
}

int
VioFd::fcntl(int cmd, long arg)
{
  assert(fd_>=0);
  return ::fcntl(fd_, cmd, arg);
}

int
VioFd::fcntl(int cmd, struct flock* lock)
{
  assert(fd_>=0);
  return ::fcntl(fd_, cmd, lock);
}

int
VioFd::close()
{
  int	r = -2;
  if (fd_>=0)
  {
    
    if ((r= ::close(fd_)) == 0)
      fd_ = -1;
  }
  else
  {
    /* FIXME: error handling */
  }
  return r;
}

const char*
VioFd::description() const
{
  return desc_;
}

const char*
VioFd::peer_addr() const
{
  return "";
}

const char*
VioFd::peer_name() const
{
  return "localhost";
}

const char*
VioFd::cipher_description() const
{
  return "";
}

VIO_NS_END
