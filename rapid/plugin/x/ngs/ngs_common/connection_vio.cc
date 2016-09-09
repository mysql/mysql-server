/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "ngs_common/connection_vio.h"
#include "ngs_common/connection_type.h"
#include "ngs_common/options_ssl.h"
#include "ngs/log.h"

#include <sstream>
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#include <sys/types.h>
#include <signal.h>
#endif

using namespace ngs;

class Error_formatter
{
public:
  Error_formatter(std::string &output)
  : m_output(output)
  {
  }

  ~Error_formatter()
  {
    m_output = m_stream.str();
  }

  std::stringstream &stream()
  {
    return m_stream;
  }

private:
  std::stringstream m_stream;
  std::string &m_output;
};


class Options_session_supports_ssl : public Options_session_default
{
public:
  Options_session_supports_ssl() {}

  bool supports_tls() { return true; }
};

class Socket_operations: public Socket_operations_interface
{
public:

  int bind(const my_socket &socket, const struct sockaddr *addr, socklen_t len)
  {
    return ::bind(socket, addr, len);
  }

  my_socket accept(const my_socket &socket_listen,
      struct sockaddr *addr, socklen_t *addr_len)
  {
    return ::accept(socket_listen, addr, addr_len);
  }

  my_socket socket(int domain, int type, int protocol)
  {
    return ::socket(domain, type, protocol);
  }

  virtual int listen(const my_socket &socket, int backlog)
  {
    return ::listen(socket, backlog);
  }

  virtual int get_socket_errno()
  {
    return socket_errno;
  }
};

#if defined(HAVE_SYS_UN_H)
class Unix_system_operations: public System_operations_interface
{
public:
  virtual int open(const char* name, int access, int permission)
  {
    return ::open(name, access, permission);
  }

  virtual int close(int fd)
  {
    return ::close(fd);
  }

  virtual int read(int  fd, void *buffer, int nbyte)
  {
    return ::read(fd, buffer, nbyte);
  }

  virtual int write(int fd, void *buffer, int nbyte)
  {
    return ::write(fd, buffer, nbyte);
  }

  virtual int fsync(int fd)
  {
    return ::fsync(fd);
  }

  virtual int unlink(const char* name)
  {
    return ::unlink(name);
  }

  virtual int get_errno()
  {
    return errno;
  }

  virtual int getppid()
  {
    return ::getppid();
  }

  virtual int getpid()
  {
    return ::getpid();
  }

  virtual int kill(int pid, int signal)
  {
    return ::kill(pid, signal);
  }
};
#endif

Connection_vio::Connection_vio(Ssl_context &ssl_context, Vio *vio)
: m_vio(vio), m_ssl_context(ssl_context)
{
}


Connection_vio::~Connection_vio()
{
  if (NULL != m_vio)
    vio_delete(m_vio);
}

my_socket Connection_vio::get_socket_id()
{
  return vio_fd(m_vio);
}


ssize_t Connection_vio::write(const Const_buffer_sequence &data)
{
  ssize_t c = 0;
  for (Const_buffer_sequence::const_iterator it = data.begin(); it != data.end(); ++it)
  {
    ssize_t n = write(it->first, it->second);
    if (n <= 0)
      return n;

    c += n;
  }
  return c;
}


ssize_t Connection_vio::write(const char *buffer, const std::size_t buffer_size)
{
  ssize_t bytes_to_send = buffer_size;

  do
  {
    ssize_t result = 0;
    {
      Mutex_lock lock(m_shutdown_mutex);
      result = vio_write(m_vio, (const uchar*)buffer, bytes_to_send);
    }

    if (result <= 0)
      return result;

    bytes_to_send -= result;
    buffer += result;
  } while (bytes_to_send > 0);

  return buffer_size;
}


bool Connection_vio::peer_address(std::string &address, uint16 &port)
{
  address.resize(256);
  char *buffer = &address[0];

  buffer[0] = 0;

  if (vio_peer_addr(m_vio, buffer, &port, address.capacity()))
    return false;

  address.resize(strlen(buffer));

  return true;
}


Connection_type Connection_vio::connection_type()
{
  if (options()->active_tls())
    return Connection_tls;

  const enum_vio_type type = vio_type(m_vio);

  return Connection_type_helper::convert_type(type);
}


ssize_t Connection_vio::read(char *buffer, const std::size_t buffer_size)
{
  ssize_t bytes_to_send = buffer_size;
  do
  {
    const ssize_t result = vio_read(m_vio, (uchar*)buffer, bytes_to_send);

    if (result <= 0)
      return result;

    bytes_to_send -= result;
    buffer += result;
  }while(bytes_to_send > 0);

  return buffer_size;
}

int Connection_vio::shutdown(Shutdown_type how_to_shutdown)
{
#if defined(_WIN32)
  if (Connection_namedpipe == connection_type())
  {
    FlushFileBuffers(m_vio->hPipe);
  }
#endif

  Mutex_lock lock(m_shutdown_mutex);
  return vio_shutdown(m_vio);
}


void Connection_vio::close()
{
  // vio_shutdown cloeses socket, no need to reimplement close
  shutdown(Shutdown_both);
}


IOptions_session_ptr Connection_vio::options()
{
  if (!m_options_session)
  {
    if (m_ssl_context.has_ssl())
      m_options_session.reset(new Options_session_supports_ssl());
    else
      m_options_session.reset(new Options_session_default());
  }

  return m_options_session;
}


Ssl_context::Ssl_context()
: m_ssl_acceptor(NULL),
  m_options(new Options_context_default())
{
}

bool Ssl_context::setup(const char *tls_version,
                        const char *ssl_key,
                        const char *ssl_ca,
                        const char *ssl_capath,
                        const char *ssl_cert,
                        const char *ssl_cipher,
                        const char *ssl_crl,
                        const char *ssl_crlpath)
{
  enum_ssl_init_error error = SSL_INITERR_NOERROR;

  long ssl_ctx_flags= process_tls_version(tls_version);

  m_ssl_acceptor = new_VioSSLAcceptorFd(ssl_key, ssl_cert,
                      ssl_ca, ssl_capath,
                      ssl_cipher,
                      &error,
                      ssl_crl, ssl_crlpath, ssl_ctx_flags);

  if (NULL == m_ssl_acceptor)
  {
    log_warning("Failed at SSL configuration: \"%s\"", sslGetErrString(error));
    return false;
  }

  m_options.reset(new Options_context_ssl(m_ssl_acceptor));

  return true;
}


Ssl_context::~Ssl_context()
{
  if (m_ssl_acceptor)
    free_vio_ssl_acceptor_fd(m_ssl_acceptor);
}


/** Start a TLS session in the connection.
 */
bool Ssl_context::activate_tls(Connection_vio &conn, int handshake_timeout)
{
  unsigned long error;
  if (sslaccept(m_ssl_acceptor, conn.m_vio, handshake_timeout, &error) != 0)
  {
    log_warning("Error during SSL handshake for client connection (%i)", (int)error);
    return false;
  }
  conn.m_options_session = IOptions_session_ptr(new Options_session_ssl(conn.m_vio));
  return true;
}

void Connection_vio::close_socket(my_socket &fd)
{
  if (INVALID_SOCKET != fd)
  {
    ::closesocket(fd);

    fd = INVALID_SOCKET;
  }
}

void Connection_vio::unlink_unix_socket_file(const std::string &unix_socket_file)
{
  if (unix_socket_file.empty())
    return;

  if (!m_system_operations)
    return;

  const std::string unix_socket_lockfile = get_lockfile_name(unix_socket_file);

  (void) m_system_operations->unlink(unix_socket_file.c_str());
  (void) m_system_operations->unlink(unix_socket_lockfile.c_str());
}

void Connection_vio::get_error(int& err, std::string& strerr)
{
  err = socket_errno;
#ifdef _WIN32
  char *s = NULL;
  if (0 == FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&s, 0, NULL))
  {
    char text[256];
    my_snprintf(text, sizeof(text), "Error %i", err);
    strerr = text;
  }
  else
  {
    strerr = s;
    LocalFree(s);
  }
#else
  strerr = strerror(err);
#endif
}


my_socket Connection_vio::accept(my_socket sock, struct sockaddr* addr, socklen_t& len, int& err, std::string& strerr)
{
  bool cont = false;
  my_socket res = 0;
  do
  {
    cont = false;
    res = m_socket_operations->accept(sock, addr, &len);

    if (INVALID_SOCKET == res)
    {
      if (m_socket_operations->get_socket_errno() == SOCKET_EINTR || m_socket_operations->get_socket_errno() == SOCKET_EAGAIN)
        cont = true;
      else
        get_error(err, strerr);
    }
  } while (cont);

  return res;
}

my_socket Connection_vio::create_and_bind_socket(const unsigned short port, std::string &error_message, const uint32 backlog)
{
  int err;
  std::string errstr;

  my_socket result = m_socket_operations->socket(AF_INET, SOCK_STREAM, 0);
  if (result == INVALID_SOCKET)
  {
    get_error(err, errstr);
    Error_formatter(error_message).stream() <<
        "can't create TCP Socket: " << errstr.c_str() <<
        "(" << err << ")";
    return INVALID_SOCKET;
  }

  {
    int one = 1;
    setsockopt(result, SOL_SOCKET, SO_REUSEADDR, (const char*)&one, sizeof(one));
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);
  if (m_socket_operations->bind(result, (const struct sockaddr *)&addr, sizeof(addr)) < 0)
  {
    // lets decide later if its an error or not
    get_error(err, errstr);

    Error_formatter(error_message).stream() <<
        "could not bind to port " << port << ": " << errstr <<
        " (" << err << ")";

    Connection_vio::close_socket(result);

    return INVALID_SOCKET;
  }

  if (m_socket_operations->listen(result, backlog) < 0)
  {
    // lets decide later if its an error or not
    get_error(err, errstr);

    Error_formatter(error_message).stream() <<
        "listen() failed with error: " << errstr <<
        "(" << err << ")";

    Connection_vio::close_socket(result);

    return INVALID_SOCKET;
  }

  return result;
}

my_socket Connection_vio::create_and_bind_socket(const std::string &unix_socket_file, std::string &error_message, const uint32 backlog)
{
#if defined(HAVE_SYS_UN_H)
  struct sockaddr_un addr;
  int err;
  std::string errstr;

  log_debug("UNIX Socket is %s", unix_socket_file.c_str());

  if (unix_socket_file.empty())
  {
    log_info("UNIX socket not configured");
    error_message = "UNIX socket path is empty";
    return INVALID_SOCKET;
  }

  // Check path length, probably move to set unix port?
  if (unix_socket_file.length() > (sizeof(addr.sun_path) - 1))
  {
    Error_formatter(error_message).stream() <<
        "the socket file path is too long (> " <<
        sizeof(addr.sun_path) - 1 << "): " <<
        unix_socket_file.c_str();
    return INVALID_SOCKET;
  }

  if (!create_lockfile(unix_socket_file, error_message))
  {
    return INVALID_SOCKET;
  }


  my_socket listener_socket = m_socket_operations->socket(AF_UNIX, SOCK_STREAM, 0);

  if (INVALID_SOCKET == listener_socket)
  {
    get_error(err, errstr);
    Error_formatter(error_message).stream() <<
        "can't create UNIX Socket: " << errstr << " (" << err << ")";

    return INVALID_SOCKET;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family= AF_UNIX;
  my_stpcpy(addr.sun_path, unix_socket_file.c_str());
  (void) unlink(unix_socket_file.c_str());

  // bind
  int old_mask = umask(0);
  if (m_socket_operations->bind(listener_socket,
           reinterpret_cast<struct sockaddr *> (&addr),
           sizeof(addr)) < 0)
  {
    umask(old_mask);
    get_error(err, errstr);
    Error_formatter(error_message).stream() <<
        "bind() on UNIX socket failed: " << errstr << " (" << err << "). " <<
        " Do you already have another mysqld server running with Mysqlx on socket: " <<
        unix_socket_file.c_str() << " ?";

    Connection_vio::close_socket(listener_socket);

    return INVALID_SOCKET;
  }
  umask(old_mask);

  // listen
  if (m_socket_operations->listen(listener_socket, backlog) < 0)
  {
    get_error(err, errstr);

    Error_formatter(error_message).stream() <<
        "listen() on UNIX socket failed with error " << errstr.c_str() <<
        "(" << err << ")";

    Connection_vio::close_socket(listener_socket);

    return INVALID_SOCKET;
  }

  return listener_socket;
#else
  return INVALID_SOCKET;
#endif // defined(HAVE_SYS_UN_H)
}

bool Connection_vio::create_lockfile(const std::string &unix_socket_file, std::string &error_message)
{
#if !defined(HAVE_SYS_UN_H)
  return false;
#else
  int fd;
  char buffer[8];
  const char x_prefix = 'X';
  const pid_t cur_pid= m_system_operations->getpid();
  const std::string lock_filename= get_lockfile_name(unix_socket_file);

  compile_time_assert(sizeof(pid_t) == 4);
  int retries= 3;
  while (true)
  {
    if (!retries--)
    {
      Error_formatter(error_message).stream() <<
          "unable to create UNIX socket lock file " << lock_filename << " after " << retries << "retries";

      return false;
    }

    fd= m_system_operations->open(lock_filename.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);

    if (fd >= 0)
      break;

    if (m_system_operations->get_errno() != EEXIST)
    {
      error_message = "could not create UNIX socket lock file ";
      error_message += lock_filename;

      return false;
    }

    fd= m_system_operations->open(lock_filename.c_str(), O_RDONLY, 0600);
    if (fd < 0)
    {
      error_message = "could not open UNIX socket lock file ";
      error_message += lock_filename;

      return false;
    }

    ssize_t len = 0;
    ssize_t read_result = 1;

    while (read_result)
    {
      if ((read_result= m_system_operations->read(fd, buffer + len, sizeof(buffer) - 1 - len)) < 0)
      {
        error_message = "could not read UNIX socket lock file ";
        error_message += lock_filename;

        m_system_operations->close(fd);
        return false;
      }

      len += read_result;
    }

    m_system_operations->close(fd);

    if (len == 0)
    {
      error_message = "UNIX socket lock file is empty ";
      error_message += lock_filename;
      return false;
    }
    buffer[len]= '\0';

    if (x_prefix != buffer[0])
    {
      error_message = "UNIX socket lock file wasn't allocated by X Plugin ";
      error_message += lock_filename;
      return false;
    }

    pid_t parent_pid= m_system_operations->getppid();
    pid_t read_pid= atoi(buffer + 1);

    if (read_pid <= 0)
    {
      error_message = "invalid PID in UNIX socket lock file ";
      error_message += lock_filename;

      return false;
    }

    if (read_pid != cur_pid && read_pid != parent_pid)
    {
      if (m_system_operations->kill(read_pid, 0) == 0)
      {
        Error_formatter(error_message).stream() << "another process with PID " << read_pid << " is using "
                  "UNIX socket file";
        return false;
      }
    }

    /*
      Unlink the lock file as it is not associated with any process and
      retry.
    */
    if (m_system_operations->unlink(lock_filename.c_str()) < 0)
    {
      error_message = "could not remove UNIX socket lock file ";
      error_message += lock_filename;

      return false;
    }
  }

  // The "X" should fail legacy UNIX socket lock-file allocation
  snprintf(buffer, sizeof(buffer), "%c%d\n", x_prefix, static_cast<int>(cur_pid));
  if (m_system_operations->write(fd, buffer, strlen(buffer)) !=
      static_cast<signed>(strlen(buffer)))
  {
    m_system_operations->close(fd);

    Error_formatter(error_message).stream() << "could not write UNIX socket lock file "<< lock_filename << ", errno: " << errno;

    return false;
  }

  if (m_system_operations->fsync(fd) != 0)
  {
    m_system_operations->close(fd);

    Error_formatter(error_message).stream() << "could not sync UNIX socket lock file " << lock_filename << ", errno: " << errno;

    return false;
  }

  if (m_system_operations->close(fd) != 0)
  {
    Error_formatter(error_message).stream()
        << "could not close UNIX socket lock file " << lock_filename << ", errno: " << errno;

    return false;
  }
  return true;
#endif // defined(HAVE_SYS_UN_H)
}

std::string Connection_vio::get_lockfile_name(const std::string &unix_socket_file)
{
  return unix_socket_file+ ".lock";
}

Socket_operations_interface::Unique_ptr Connection_vio::m_socket_operations;
System_operations_interface::Unique_ptr Connection_vio::m_system_operations;

void Connection_vio::init()
{
  m_socket_operations.reset(new Socket_operations());
#if defined(HAVE_SYS_UN_H)
  m_system_operations.reset(new Unix_system_operations());
#endif
}

void Connection_vio::set_socket_operations(Socket_operations_interface* socket_operations)
{
  m_socket_operations.reset(socket_operations);
}

void Connection_vio::set_system_operations(System_operations_interface* system_operations)
{
  m_system_operations.reset(system_operations);
}
