/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#ifndef CLUSTER_CONNECTION_HPP
#define CLUSTER_CONNECTION_HPP

/**
 * @class Ndb_cluster_connection
 * @brief Represents a connection to a cluster of storage nodes.
 *
 * Any NDB application program should begin with the creation of a
 * single Ndb_cluster_connection object, and should make use of one
 * and only one Ndb_cluster_connection. The application connects to
 * a cluster management server when this object's connect() method is called.
 * By using the wait_until_ready() method it is possible to wait
 * for the connection to reach one or more storage nodes.
 */
class Ndb_cluster_connection {
public:
  /**
   * Create a connection to a cluster of storage nodes
   *
   * @param connectstring The connectstring for where to find the
   *                      management server
   */
  Ndb_cluster_connection(const char * connectstring = 0);
  ~Ndb_cluster_connection();

  /**
   * Connect to a cluster management server
   *
   * @param no_retries specifies the number of retries to attempt
   *        in the event of connection failure; a negative value 
   *        will result in the attempt to connect being repeated 
   *        indefinitely
   *
   * @param retry_delay_in_seconds specifies how often retries should
   *        be performed
   *
   * @param verbose specifies if the method should print a report of its progess
   *
   * @return 0 = success, 
   *         1 = recoverable error,
   *        -1 = non-recoverable error
   */
  int connect(int no_retries=0, int retry_delay_in_seconds=1, int verbose=0);

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  int start_connect_thread(int (*connect_callback)(void)= 0);
#endif

  /**
   * Wait until the requested connection with one or more storage nodes is successful
   *
   * @param timeout_for_first_alive   Number of seconds to wait until
   *                                  first live node is detected
   * @param timeout_after_first_alive Number of seconds to wait after
   *                                  first live node is detected
   *
   * @return = 0 all nodes live,
   *         > 0 at least one node live,
   *         < 0 error
   */
  int wait_until_ready(int timeout_for_first_alive,
		       int timeout_after_first_alive);

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  int get_no_ready();
  const char *get_connectstring(char *buf, int buf_sz) const;
  int get_connected_port() const;
  const char *get_connected_host() const;

  void set_optimized_node_selection(int val);

  unsigned no_db_nodes();
  unsigned node_id();
#endif

private:
  friend class Ndb;
  friend class NdbImpl;
  friend class Ndb_cluster_connection_impl;
  class Ndb_cluster_connection_impl & m_impl;
  Ndb_cluster_connection(Ndb_cluster_connection_impl&);
};

#endif
