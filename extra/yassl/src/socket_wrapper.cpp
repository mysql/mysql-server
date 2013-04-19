/*
   Copyright (c) 2005, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING. If not, write to the
   Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
   MA  02110-1301  USA.
*/


/* The socket wrapper source implements a Socket class that hides the 
 * differences between Berkely style sockets and Windows sockets, allowing 
 * transparent TCP access.
 */


#include "runtime.hpp"
#include "socket_wrapper.hpp"

#ifndef _WIN32
    #include <errno.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/ioctl.h>
    #include <string.h>
    #include <fcntl.h>
#endif // _WIN32

#if defined(__sun) || defined(__SCO_VERSION__)
    #include <sys/filio.h>
#endif

#ifdef _WIN32
    const int SOCKET_EINVAL = WSAEINVAL;
    const int SOCKET_EWOULDBLOCK = WSAEWOULDBLOCK;
    const int SOCKET_EAGAIN = WSAEWOULDBLOCK;
#else
    const int SOCKET_EINVAL = EINVAL;
    const int SOCKET_EWOULDBLOCK = EWOULDBLOCK;
    const int SOCKET_EAGAIN = EAGAIN;
#endif // _WIN32


namespace {


extern "C" long system_recv(void *ptr, void *buf, size_t count)
{
  yaSSL::socket_t *socket = (yaSSL::socket_t *) ptr;
  return ::recv(*socket, reinterpret_cast<char *>(buf), count, 0);
}


extern "C" long system_send(void *ptr, const void *buf, size_t count)
{
  yaSSL::socket_t *socket = (yaSSL::socket_t *) ptr;
  return ::send(*socket, reinterpret_cast<const char *>(buf), count, 0);
}


}


namespace yaSSL {


Socket::Socket(socket_t s) 
    : socket_(s), wouldBlock_(false), nonBlocking_(false),
      ptr_(&socket_), send_func_(system_send), recv_func_(system_recv)
{}


void Socket::set_fd(socket_t s)
{
    socket_ = s;
}


socket_t Socket::get_fd() const
{
    return socket_;
}


Socket::~Socket()
{
    // don't close automatically now
}


void Socket::closeSocket()
{
    if (socket_ != INVALID_SOCKET) {
#ifdef _WIN32
        closesocket(socket_);
#else
        close(socket_);
#endif
        socket_ = INVALID_SOCKET;
    }
}


uint Socket::get_ready() const
{
#ifdef _WIN32
    unsigned long ready = 0;
    ioctlsocket(socket_, FIONREAD, &ready);
#else
    /*
       64-bit Solaris requires the variable passed to
       FIONREAD be a 32-bit value.
    */
    unsigned int ready = 0;
    ioctl(socket_, FIONREAD, &ready);
#endif

    return ready;
}


void Socket::set_transport_ptr(void *ptr)
{
  ptr_ = ptr;
}


void Socket::set_transport_recv_function(yaSSL_recv_func_t recv_func)
{
  recv_func_ = recv_func;
}


void Socket::set_transport_send_function(yaSSL_send_func_t send_func)
{
  send_func_ = send_func;
}


uint Socket::send(const byte* buf, unsigned int sz, unsigned int& written)
{
    const byte* pos = buf;
    const byte* end = pos + sz;

    wouldBlock_ = false;

    /* Remove send()/recv() hooks once non-blocking send is implemented. */
    while (pos != end) {
        int sent = send_func_(ptr_, pos, static_cast<int>(end - pos));
        if (sent == -1) {
            if (get_lastError() == SOCKET_EWOULDBLOCK || 
                get_lastError() == SOCKET_EAGAIN) {
                wouldBlock_  = true; // would have blocked this time only
                nonBlocking_ = true; // nonblocking, win32 only way to tell 
                return 0;
            }
            return static_cast<uint>(-1);
        }
        pos += sent;
        written += sent;
    }

    return sz;
}


uint Socket::receive(byte* buf, unsigned int sz)
{
    wouldBlock_ = false;

    int recvd = recv_func_(ptr_, buf, sz);

    // idea to seperate error from would block by arnetheduck@gmail.com
    if (recvd == -1) {
        if (get_lastError() == SOCKET_EWOULDBLOCK || 
            get_lastError() == SOCKET_EAGAIN) {
            wouldBlock_  = true; // would have blocked this time only
            nonBlocking_ = true; // socket nonblocking, win32 only way to tell
            return 0;
        }
    }
    else if (recvd == 0)
        return static_cast<uint>(-1);

    return recvd;
}


void Socket::shutDown(int how)
{
    shutdown(socket_, how);
}


int Socket::get_lastError()
{
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}


bool Socket::WouldBlock() const
{
    return wouldBlock_;
}


bool Socket::IsNonBlocking() const
{
    return nonBlocking_;
}


void Socket::set_lastError(int errorCode)
{
#ifdef _WIN32
    WSASetLastError(errorCode);
#else
    errno = errorCode;
#endif
}


} // namespace
