/*
Copyright (c) 2003, 2011, 2013, Oracle and/or its affiliates. All rights
reserved.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of
the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
02110-1301  USA
*/

#ifndef TCP_DRIVER_INCLUDED
#define	TCP_DRIVER_INCLUDED

#include "binlog_driver.h"
#include "protocol.h"
#include <my_global.h>
#include <mysql.h>
#ifdef min //definition of min() and max() in std and libmysqlclient
           //can be/are different
#undef min
#endif
#ifdef max
#undef max
#endif
#include <cstring>
#include <map>
#define MAX_PACKAGE_SIZE 0xffffff


namespace mysql { namespace system {

class Binlog_tcp_driver : public Binary_log_driver
{
public:

    Binlog_tcp_driver(const std::string& user, const std::string& passwd,
                      const std::string& host, uint port)
      : Binary_log_driver("", 4), m_user(user), m_passwd(passwd), m_host(host),
        m_port(port), m_waiting_event(new Log_event_header()),
        m_shutdown(false), m_total_bytes_transferred(0)
    {
    }

    ~Binlog_tcp_driver()
    {
      if (!m_shutdown)
        mysql_close(m_mysql);
      delete m_waiting_event;
    }


    /**
     * Connect using previously declared connection parameters.
     */
    int connect();
    /**
     * Connect using previously declared conncetion parameters, and
     * start reading from the binlog_file where starting_pos= offset
     */
    int connect(const std::string &binlog_filename, ulong offset);
    /**
     * Blocking wait for the next binary log event to reach the client
     */
    unsigned int wait_for_next_event(mysql::Binary_log_event **event);

    /**
     * Reconnects to the master with a new binlog dump request.
     */
    int set_position(const std::string &str, unsigned long position);
    /**
     * Disconnect from the server. The io service must have been stopped before
     * this function is called.
     * The event queue is emptied.
     */
    int disconnect(void);

    int get_position(std::string *str, unsigned long *position);
    const std::string& user() const { return m_user; }
    const std::string& password() const { return m_passwd; }
    const std::string& host() const { return m_host; }
    uint port() const { return m_port; }

protected:
    /**
     * Connects to a mysql server, authenticates and initiates the event
     * request loop.
     *
     * @param user The user account on the server side
     * @param passwd The password used to authenticate the user
     * @param host The DNS host name or IP of the server
     * @param port The service port number to connect to
     *
     *
     * @return Success or failure code
     *   @retval 0 Successfully established a connection
     *   @retval >1 An error occurred.
     */
    int connect(const std::string& user, const std::string& passwd,
                const std::string& host, uint port,
                const std::string& binlog_filename= "", size_t offset= 4);

private:
    void start_binlog_dump(const char *binlog, size_t offset);
    void start_event_loop(void);

    /**
     * Reconnect to the server by first calling disconnect and then connect.
     */
    void reconnect(void);

    /**
     * Terminates the io service and sets the shudown flag.
     * this causes the event loop to terminate.
     */
    void shutdown(void);

    /**
     * each bin log event starts with a 19 byte long header
     * We use this sturcture every time we initiate an async
     * read.
     */
    uint8_t m_event_header[19];

    /**
     *
     */
    uint8_t m_net_header[4];

    /**
     *
     */
    uint8_t m_net_packet[MAX_PACKAGE_SIZE];
    char * m_event_packet;

    std::string m_user;
    std::string m_passwd;
    std::string m_host;
    uint m_port;

    /**
     * This pointer points to an object constructed from event
     * stream during async communication with
     * server. If it is 0 it means that no event has been
     * constructed yet.
     */
    Log_event_header *m_waiting_event;
    Log_event_header m_log_event_header;

    bool m_shutdown;

    MYSQL *m_mysql;
    uint64_t m_total_bytes_transferred;
};

/**
 * Sends a SHOW MASTER STATUS command to the server and retrieve the
 * current binlog position.
 *
 * @return False if the operation succeeded, true if it failed.
 */
bool fetch_master_status(MYSQL *mysql, std::string *filename,
                         unsigned long *position);

bool fetch_binlog_name_and_size(MYSQL *mysql, std::map<std::string, unsigned long> *binlog_map);

int sync_connect_and_authenticate(MYSQL *mysql, const std::string &user,
                                  const std::string &passwd,
                                  const std::string &host, uint port,
                                  long offset= 4);
} }



#endif	/* TCP_DRIVER_INCLUDED */
