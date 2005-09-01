/* socket_wrapper.cpp                           
 *
 * Copyright (C) 2003 Sawtooth Consulting Ltd.
 *
 * This file is part of yaSSL.
 *
 * yaSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * yaSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
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
#endif // _WIN32

#ifdef __sun
    #include <sys/filio.h>
#endif

#ifdef _WIN32
    const int SOCKET_EINVAL = WSAEINVAL;
    const int SOCKET_EWOULDBLOCK = WSAEWOULDBLOCK;
#else
    const int SOCKET_EINVAL = EINVAL;
    const int SOCKET_EWOULDBLOCK = EWOULDBLOCK;
#endif // _WIN32


namespace yaSSL {


Socket::Socket(socket_t s) 
    : socket_(s) 
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
    closeSocket();
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
    int ready = 0;
    ioctl(socket_, FIONREAD, &ready);
#endif

    return ready;
}


uint Socket::send(const byte* buf, unsigned int sz, int flags) const
{
    assert(socket_ != INVALID_SOCKET);
    int sent = ::send(socket_, reinterpret_cast<const char *>(buf), sz, flags);

    if (sent == -1)
        return 0;

    return sent;
}


uint Socket::receive(byte* buf, unsigned int sz, int flags) const
{
    assert(socket_ != INVALID_SOCKET);
    int recvd = ::recv(socket_, reinterpret_cast<char *>(buf), sz, flags);

    if (recvd == -1) 
        return 0;

    return recvd;
}


// wait if blocking for input, or error
void Socket::wait() const
{
    byte b;
    receive(&b, 1, MSG_PEEK);
}


void Socket::shutDown(int how)
{
    assert(socket_ != INVALID_SOCKET);
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


void Socket::set_lastError(int errorCode)
{
#ifdef _WIN32
    WSASetLastError(errorCode);
#else
    errno = errorCode;
#endif
}


} // namespace
