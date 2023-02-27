#
# Copyright (c) 2016, 2020, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms, as
# designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
# This program is distributed in the hope that it will be useful,  but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
# the GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
#

"""
This module contains an abstraction of a MySQL server object used
by multiple gadgets. It also contains methods and functions for common server
operations.
"""

import logging
import os
import random
import re
import socket
import threading
import time
import sys

import subprocess

import mysqlsh

from mysql_gadgets import MIN_MYSQL_VERSION, MAX_MYSQL_VERSION
from mysql_gadgets.exceptions import (GadgetCnxInfoError, GadgetCnxError,
                                      GadgetQueryError, GadgetServerError,
                                      GadgetError)
from mysql_gadgets.common.connection_parser import (parse_connection,
                                                    hostname_is_ip,
                                                    clean_IPv6,)
from mysql_gadgets.common.logger import CustomLevelLogger
from mysql_gadgets.common.tools import (get_abs_path,
                                        is_executable, run_subprocess,
                                        shell_quote)

CR_SERVER_LOST = 2013
ER_OPTION_PREVENTS_STATEMENT = 1290

_FOREIGN_KEY_SET = "SET foreign_key_checks = {0}"
_AUTOCOMMIT_SET = "SET AUTOCOMMIT = {0}"
_GTID_ERROR = ("The server {host}:{port} does not comply to the latest GTID "
               "feature support. Errors:")

logging.setLoggerClass(CustomLevelLogger)
_LOGGER = logging.getLogger(__name__)


class Secret(object):
    def __init__(self, s):
        super(Secret, self).__init__()
        self.value = s

    def __str__(self):
        return str(self.value)

    def __repr__(self):
        return repr(self.value)


class Query(object):
    def __init__(self, query_string, *params):
        super(Query, self).__init__()
        self._query_string = query_string
        self._params = params

    def extend(self, query_string, *params):
        self._query_string += " " + query_string
        self._params.extend(params)

    def query(self):
        return self._query_string

    def params(self, mask=False):
        if mask:
            return tuple(map(lambda x: "<secret>" if isinstance(x, Secret) else x, self._params))

        return tuple(map(lambda x: x.value if isinstance(x, Secret) else x, self._params))

    def __str__(self):
        if self._params:
            return "{0}, with params: {1}".format(self.query(), self.params(True))

        return self.query()

    def log(self):
        return self.__str__()


def _to_str(value, charset="utf-8"):
    """Cast value to str except when None

    :param value:      Value to be cast to str
    :type  value:      bytearray
    :param charset:    the charset to decode the value. Used only on python 3
    :type  charset:    string

    :return: value as string instance or None.
    :rtype: string or None
    """
    if sys.version_info > (3, 0):
        # for Python 3 use bytes instead of str
        return None if value is None else bytes(value).decode(charset)
    else:
        return None if value is None else str(value)


def get_mysqld_version(mysqld_path):
    """Get the version of the mysql server through the mysqld executable.

    :param mysqld_path: absolute path to the mysqld executable
    :type mysqld_path: str
    :return: tuple with with major, minor and release number and version string
    :rtype tuple((int, int, int), str)
    """
    cmd = u"{0} --version".format(shell_quote(mysqld_path))
    version_proc = run_subprocess(cmd,
                                  stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, shell=False,
                                  universal_newlines=True)
    output, error = version_proc.communicate()
    match = re.match(r'^.*mysqld.*?\s+(Ver\s+(\d+\.\d+(?:\.\d+)*).*)', output)
    if match:
        return tuple(int(n) for n in match.group(2).split('.')), match.group(1)
    else:
        error_msg = ''
        if error:
            error_msg = " Error executing '{0}': {1}".format(cmd, error)
        raise GadgetError(
            "Unable to parse version output '{0}' from mysqld executable '{1}'"
            ".{2}".format(output, mysqld_path, error_msg))


def is_valid_mysqld(mysqld_path):
    """Check if the provided mysqld is valid.

    Check the version of the given mysqld and return True if valid and
    False otherwise.

    :param mysqld_path: Path to mysqld to check.
    :type mysqld_path: string

    :return: True if the provided mysqld is valid, or False otherwise.
    :rtype: bool
    """
    # Check if mysqld is executable and version is valid.
    if is_executable(mysqld_path):
        mysqld_ver, _ = get_mysqld_version(mysqld_path)
        if MIN_MYSQL_VERSION <= mysqld_ver < MAX_MYSQL_VERSION:
            _LOGGER.debug("Valid mysqld found with version %s: '%s'",
                          ".".join(str(num) for num in mysqld_ver),
                          mysqld_path)
            return True

    # mysqld is not valid.
    _LOGGER.debug("Invalid mysqld version (or not executable): '%s'",
                  mysqld_path)
    return False


class MySQLUtilsCursorResult(object):
    def __init__(self, result):
        self._result = result
        self.with_rows = result.has_data()
        self.column_names = result.column_names

    def fetchone(self):
        row = self._result.fetch_one()
        if row:
            return tuple(str(row[i]) if row[i] is not None else 'NULL'
                         for i in range(row.length))
        return None

    def fetchall(self):
        rows = []
        row = self.fetchone()
        while row:
            rows.append(row)
            row = self.fetchone()
        return rows

    def close(self):
        pass


def get_connection_dictionary(conn_info, ssl_dict=None):
    """Get the connection dictionary.

    Convert the given connection information into a dictionary.
    The method accepts one of the following types for the connection
    information:
        - dictionary containing connection information including:
          (user, passwd, host, port, socket)
        - connection string in the form: user:pass@host:port:socket
        - an instance of the Server class

    :param conn_info: Connection information to be converted.
    :type  conn_info: dictionary or Server object or string
    :param ssl_dict:  A dictionary with the ssl options.
    :type  ssl_dict:  dictionary

    :return dictionary with connection information (user, passwd, host, port,
            socket).
    :rtype: dictionary
    """
    if conn_info is None:
        return conn_info

    if isinstance(conn_info, dict):
        # Not update conn_info if already has any ssl certificate.
        if (ssl_dict is not None and
                not (conn_info.get("ssl_ca", None) or
                     conn_info.get("ssl_cert", None) or
                     conn_info.get("ssl_key", None) or
                     conn_info.get("ssl", None))):
            conn_info.update(ssl_dict)
        conn_val = conn_info
    elif isinstance(conn_info, Server):
        # get server's dictionary
        conn_val = conn_info.get_connection_values()
    elif isinstance(conn_info, str):
        # parse the string
        conn_val = parse_connection(conn_info, options=ssl_dict)
    else:
        raise GadgetCnxInfoError("Cannot determine connection information"
                                 " type.")

    return conn_val


def check_hostname_alias(server1_cnx_values, server2_cnx_values):
    """Check to see if the servers are the same machine by host name.

    :param server1_cnx_values:   connection values for server 1
    :type  server1_cnx_values:   dictionary
    :param server2_cnx_values:   connection values for server 2
    :type  server2_cnx_values:   dictionary

    :return: True if both servers are the same otherwise False.
    :rtype: boolean
    """
    server1 = Server({'conn_info': server1_cnx_values})
    server2 = Server({'conn_info': server2_cnx_values})

    return (server1.is_alias(server2.host) and
            int(server1.port) == int(server2.port))


def get_server(server_info, ssl_dict=None, connect=True):
    """Get a server instance from server connection information | string or
    a Server instance.

    The method accepts one of the following types for server_info:

        - dictionary containing connection information including:
          (user, passwd, host, port, socket)
        - connection string in the form: user:pass@host:port:socket or
                                         login-path:port:socket
        - an instance of the Server class

    :param server_info:  Connection information
    :type  server_info:  dict | Server | str
    :param ssl_dict:     A dictionary with the ssl certificates
    :type  ssl_dict:     dict
    :param connect:      Attempt to connect to the server
    :type  connect:      boolean

    :raise GadgetCnxInfoError:  if the connection information on server_info
                                could not be parsed
    :raise GadgetServerError:   if a connection fails.

    @returns a Server instance
    :rtype: Server
    """
    if server_info is None:
        return server_info

    if isinstance(server_info, dict) and 'host' in server_info:
        # Don't update server_info if already has any ssl certificate.
        if (ssl_dict is not None and
                not (server_info.get("ssl_ca", None) or
                     server_info.get("ssl_cert", None) or
                     server_info.get("ssl_key", None) or
                     server_info.get("ssl", None))):
            server_info.update(ssl_dict)

        options = {"conn_info": server_info}
        server = Server(options)

    elif isinstance(server_info, Server):
        server = server_info

    elif isinstance(server_info, str):
        # parse the string
        conn_val = parse_connection(server_info, options=ssl_dict)
        options = {"conn_info": conn_val}
        server = Server(options)

    else:
        raise GadgetCnxInfoError("Cannot determine connection information"
                                 " type.")

    if connect:
        server.connect()

    return server


def generate_server_id(strategy='RANDOM'):
    """Generate a server ID based on the provided strategy.

    Two strategies are supported to generate a server_id: TIME and RANDOM.

    The TIME strategy allow to generate a server_id based on the current
    timestamp, ensuring that a unique server_id can be generated every second
    during approximately 31 years (until 2038, epoch end for the time.time()
    function in Unix). If different server_ids are generated during the same
    second they will all have the same value. Minimum value generated is 1
    and maximum 999999999.

    The RANDOM strategy allow to generate a pseudo-random server_id, with a
    value between 1 and 4294967295. Two generated values have a low
    probability of being the same (independently of the time they are
    generated). Minimum value generated is 1 and maximum 4294967295.

    For the random generation (assuming random generation is uniformly) the
    probability of the same value being generated is given by:
        P(n, t) = 1 - t!/(t^n * (t-n)!)
    where t is the total number of different values that can be generated, and
    n is the number of values that are generated.

    In this case, t = 4294967295 (max number of values that can be generated),
    and for example the probability of generating the same id for 15, 100, and
    1000 servers (n=15, n=100, and n=1000) is approximately:
        P(15, 4294967295)   = 2.44 * 10^-8    (0.00000244 %)
        P(100, 4294967295)  = 1.15 * 10^-6    (0.000115 %)
        P(1000, 4294967295) = 1.16 * 10^-4    (0.0116 %)

    Note: Zero is not a valid sever_id.

    :param strategy:  Strategy used to generate the server id value. Supported
                      values: 'RANDOM' to generate a random ID and 'TIME' to
                      generate the ID based on the current timestamp.
                      By default: 'RANDOM'.
    :type strategy:   String

    :return: The generated server_id.
    :rtype: string

    :raises GadgetError: If an unsupported strategy is specified.
    """
    strategy = strategy.upper()
    if strategy == 'RANDOM':
        # Generate random int between the min valid and max allowed value.
        # Note: 0 is not a valid server_id.
        return str(random.randint(1, 4294967295))
    elif strategy == 'TIME':
        # Only the nine last digits from the time can be used to avoid
        # generating a value higher than the max allowed server_id value.
        server_id = str(int(time.time()))[-9:]
        # 0 is not a valid server_id, in that case return 1 (1 second later).
        if int(server_id) == 0:
            return '1'
        return server_id
    else:
        raise GadgetError("Invalid strategy used to generate the server_id "
                          "(supported values: 'RANDOM' or 'TIME'): "
                          "{0}.".format(strategy))


# pylint: disable=too-many-public-methods
class Server(object):
    """The Server class can be used to connect to a running MySQL server.
    It provides several features, such as:
        - connect/disconnect to the server
        - get the server version
        - Retrieve a server variable
        - Execute a query, commit, and rollback
        - Return list of all databases
        - Read SQL statements from a file and execute
        - check for specific plugin support
        - etc.
    """

    def __init__(self, options):
        """Constructor

        The constructor accepts one of the following types for the
        connection information provided trough the options parameter,
        more specifically for the options['conn_info']:
            - dictionary containing connection information including:
              (user, passwd, host, port, socket)
            - connection string in the form: user:pass@host:port:socket
            - an instance of the Server class

        :param options:    Options and definitions used to create the Server
                           object. Supported key values:
            conn_info      a dictionary containing connection information
                           (user, passwd, host, port, socket)
            role           Name or role of server (e.g., server, master)
            verbose        print extra data during operations (optional)
                           default value = False
            charset        Default character set for the connection.
                           (default None)
        :type  options:    dictionary
        """
        if options is None:
            options = {}

        if options.get("conn_info") is None:
            raise GadgetCnxInfoError(
                "Server connection information missing. The option parameter"
                "must contain a 'conn_info' entry.")

        self.verbose = options.get("verbose", False)
        self.db_conn = None
        self.host = None
        self.role = options.get("role", "Server")
        self.has_ssl = False
        conn_values = get_connection_dictionary(options.get("conn_info"))
        try:
            self.host = conn_values["host"]
            self.user = conn_values["user"]
            self.passwd = conn_values["passwd"] \
                if "passwd" in conn_values else None
            self.socket = conn_values["unix_socket"] \
                if "unix_socket" in conn_values else None
            self.port = 3306
            if conn_values["port"] is not None:
                self.port = int(conn_values["port"])
            self.charset = options.get("charset",
                                       conn_values.get("charset", None))
            # Optional values
            self.ssl_ca = conn_values.get('ssl_ca', None)
            self.ssl_cert = conn_values.get('ssl_cert', None)
            self.ssl_key = conn_values.get('ssl_key', None)
            self.ssl = conn_values.get('ssl', False)
            if self.ssl_cert or self.ssl_ca or self.ssl_key or self.ssl:
                self.has_ssl = True
        except KeyError as err:
            raise GadgetCnxInfoError(
                "Server connection dictionary format not recognized. "
                "Mandatory value missing ({0}): "
                "{1}".format(str(err), conn_values))
        self.connect_error = None
        # Set to TRUE when foreign key checks are ON. Check with
        # foreign_key_checks_enabled.
        self.fkeys = None
        self.autocommit = True  # Set autocommit to True by default.
        self.read_only = False
        self.aliases = set()
        self.grants_enabled = None
        self._version = None
        self._version_full = None

    @classmethod
    def from_server(cls, server, conn_info=None):
        """ Create a new server instance from an existing one.

        Factory method that will allow the creation of a new server instance
        from an existing server.

        :param server:    Source object used to create the new server. It must
                          be an instance of the Server class or a subclass.
        :type  server:    Server object
        :param conn_info: Connection information for the new server. If
                          provided this information will overwrite the one
                          from the source server.
        :type  conn_info: dictionary

        :return: An new instance of the Server class based on the provided
                 source server snf (optional) connection information..
        :rtype: Server object
        """

        if isinstance(server, Server):
            options = {"role": server.role,
                       "verbose": server.verbose,
                       "charset": server.charset}
            if conn_info is not None and isinstance(conn_info, dict):
                options["conn_info"] = conn_info
            else:
                options["conn_info"] = server.get_connection_values()

            return cls(options)
        else:
            raise TypeError("The server argument's type is neither Server nor "
                            "a subclass of Server")

    def is_alive(self):
        """Determine if connection to server is alive.

        :returns: True if server is alive (and responding) or False if an
                  error occurred when trying to connect to the server.
        :rtype: boolean
        """
        res = True
        try:
            if self.db_conn is None:
                res = False
            else:
                # ping and is_connected only work partially, try exec_query
                # to make sure connection is really alive
                retval = self.db_conn.is_open()
                if retval:
                    self.exec_query("SELECT 1")
                else:
                    res = False
        except Exception:  # pylint: disable=W0703
            res = False
        return res

    def _update_alias(self, ip_or_hostname, suffix_list):
        """Update list of aliases for the given IP or hostname.

        Gets the list of aliases for host *ip_or_hostname*. If any
        of them matches one of the server's aliases, then update
        the list of aliases (self.aliases). It also receives a list (tuple)
        of suffixes that can be ignored when checking if two hostnames are
        the same.

        :param ip_or_hostname: IP or hostname to test.
        :type  ip_or_hostname: string
        :param suffix_list:    Tuple with list of suffixes that can be ignored.
        :type  suffix_list:    list

        :returns: True if ip_or_hostname is a server alias, otherwise False.
        :rtype:   boolean
        """
        host_or_ip_aliases = self._get_aliases(ip_or_hostname)
        host_or_ip_aliases.add(ip_or_hostname)

        # Check if any of aliases matches with one the servers's aliases
        common_alias = self.aliases.intersection(host_or_ip_aliases)
        if common_alias:  # There are common aliases, host is the same
            self.aliases.update(host_or_ip_aliases)
            return True
        else:  # Check with and without suffixes
            no_suffix_server_aliases = set()
            no_suffix_host_aliases = set()

            for suffix in suffix_list:
                # Add alias with and without suffix from self.aliases
                for alias in self.aliases:
                    if alias.endswith(suffix):
                        host, _ = alias.rsplit('.', 1)
                        no_suffix_server_aliases.add(host)
                    no_suffix_server_aliases.add(alias)
                # Add alias with and without suffix from host_aliases
                for alias in host_or_ip_aliases:
                    if alias.endswith(suffix):
                        host, _ = alias.rsplit('.', 1)
                        no_suffix_host_aliases.add(host)
                    no_suffix_host_aliases.add(alias)
            # Check if there is any alias in common
            common_alias = no_suffix_host_aliases.intersection(
                no_suffix_server_aliases)
            if common_alias:  # Same host, so update self.aliases
                self.aliases.update(
                    no_suffix_host_aliases.union(no_suffix_server_aliases)
                )
                return True

        return False

    def _get_aliases(self, host):
        """Gets the aliases for the given host.

        :param host:    the host name or IP
        :type  host:    string

        :return: aliases for the given host.
        :rtype:  list
        """
        aliases = set([clean_IPv6(host)])
        if hostname_is_ip(clean_IPv6(host)):  # IP address
            try:
                my_host = socket.gethostbyaddr(clean_IPv6(host))
                aliases.add(my_host[0])
                # socket.gethostbyname_ex() does not work with ipv6
                if (not my_host[0].count(":") < 1 or
                        not my_host[0] == "ip6-localhost"):
                    host_ip = socket.gethostbyname_ex(my_host[0])
                else:
                    addrinfo = socket.getaddrinfo(my_host[0], None)
                    host_ip = ([socket.gethostbyaddr(addrinfo[0][4][0])],
                               [fiveple[4][0] for fiveple in addrinfo],
                               [addrinfo[0][4][0]])
            except (socket.gaierror, socket.herror,
                    socket.error) as err:
                host_ip = ([], [], [])
                if self.verbose:
                    _LOGGER.warning(
                        "IP lookup by address failed for %s, reason: %s",
                        host, err.strerror)
        else:
            try:
                # server may not really exist.
                host_ip = socket.gethostbyname_ex(host)
            except (socket.gaierror, socket.herror,
                    socket.error) as err:
                if self.verbose:
                    _LOGGER.warning(
                        "hostname: %s may not be reachable,reason: %s",
                        host, err.strerror)
                return aliases
            aliases.add(host_ip[0])
            addrinfo = socket.getaddrinfo(host, None)
            local_ip = None
            error = None
            for addr in addrinfo:
                try:
                    local_ip = socket.gethostbyaddr(addr[4][0])
                    break
                except (socket.gaierror, socket.herror,
                        socket.error) as err:
                    error = err

            if local_ip:
                host_ip = ([local_ip[0]],
                           [fiveple[4][0] for fiveple in addrinfo],
                           [addrinfo[0][4][0]])
            else:
                host_ip = ([], [], [])
                if self.verbose:
                    _LOGGER.warning(
                        "IP lookup by name failed for %s, reason: %s",
                        host, error.strerror)
        aliases.update(set(host_ip[1]))
        aliases.update(set(host_ip[2]))

        return aliases

    def is_alias(self, host_or_ip):
        """Determine if the given host is an alias of the server host.

        :param host_or_ip: host or IP address to check.
        :type  host_or_ip: string

        :returns: True is the given host or IP address is an alias of the
                  server host, otherwise False.
        :rtype:   boolean
        """
        # List of possible suffixes
        suffixes = ('.local', '.lan', '.localdomain')

        host_or_ip = clean_IPv6(host_or_ip.lower())

        # for quickness, verify in the existing  aliases, if they exist.
        if self.aliases:
            if host_or_ip.lower() in self.aliases:
                return True
            else:
                # get the alias for the given host_or_ip
                return self._update_alias(host_or_ip, suffixes)

        # no previous aliases information
        # First, get the local information
        hostname_ = socket.gethostname()
        try:
            local_info = socket.gethostbyname_ex(hostname_)
            local_aliases = set([local_info[0].lower()])
            # if dotted host name, take first part and use as an alias
            try:
                local_aliases.add(local_info[0].split('.')[0])
            except Exception:  # pylint: disable=W0703
                pass
            local_aliases.update(['127.0.0.1', 'localhost', '::1', '[::1]'])
            local_aliases.update(local_info[1])
            local_aliases.update(local_info[2])
            local_aliases.update(self._get_aliases(hostname_))
        except (socket.herror, socket.gaierror, socket.error) as err:
            if self.verbose:
                _LOGGER.warning("Unable to find aliases for hostname '%s', "
                                "reason: %s", hostname_, str(err))
            # Try with the basic local aliases.
            local_aliases = set(['127.0.0.1', 'localhost', '::1', '[::1]'])

        # Get the aliases for this server host
        self.aliases = self._get_aliases(self.host)

        # Check if this server is local
        for host in self.aliases.copy():
            if host in local_aliases:
                # Is local then save the local aliases for future.
                self.aliases.update(local_aliases)
                break
            # Handle special suffixes in hostnames.
            for suffix in suffixes:
                if host.endswith(suffix):
                    # Remove special suffix and attempt to match with local
                    # aliases.
                    host, _ = host.rsplit('.', 1)
                    if host in local_aliases:
                        # Is local then save the local aliases for future.
                        self.aliases.update(local_aliases)
                        break

        # Check if the given host_or_ip is alias of the server host.
        if host_or_ip in self.aliases:
            return True

        # Check if any of the aliases of ip_or_host is also an alias of the
        # host server.
        return self._update_alias(host_or_ip, suffixes)

    def user_host_exists(self, user_name, host_name):
        """Check if the 'user_name'@'host_name' account exists.

        This function checks if the specified 'user_name' and 'host_name' match
        an existing account on the server, considering the respective
        parts of the account name 'user_name'@'host_name'.

        Wildcard (%) matches are also taken into consideration for the
        'host_name' part of the account name. For example, if an account
        'myname'@'%' exists then any user with the name 'myname' will match
        the account independently of the host name.

        :param user_name: user name of the account.
        :type  user_name: string
        :param host_name: hostname or IP address of the account.
                          Note: wildcard '%' can be used.
        :type  host_name: string

        :return: True if the given 'user_name' and 'host_name' match an
                 existing account, otherwise False.
        :rtype:  boolean

        :raise GadgetServerError: If an error occurs getting the user accounts
                                  information.
        """
        res = self.exec_query(Query("SELECT host FROM mysql.user WHERE user = ? "
                                    "AND ? LIKE host", user_name, host_name))
        if res:
            return True
        return False

    def get_connection_values(self):
        """Return a dictionary of connection values for the server.

        :return: Return the connection information for the server.
        :rtype:  dictionary
        """
        conn_vals = {
            "user": self.user,
            "host": self.host
        }
        if self.passwd:
            conn_vals["passwd"] = self.passwd
        if self.socket:
            conn_vals["unix_socket"] = self.socket
        if self.port:
            conn_vals["port"] = self.port
        if self.ssl_ca:
            conn_vals["ssl_ca"] = self.ssl_ca
        if self.ssl_cert:
            conn_vals["ssl_cert"] = self.ssl_cert
        if self.ssl_key:
            conn_vals["ssl_key"] = self.ssl_key
        if self.ssl:
            conn_vals["ssl"] = self.ssl

        return conn_vals

    def connect(self):
        """Connect to the server.

        Attempts to connect to the server according to its connection
        parameters.

        Note: This method must be called before executing statements.

        :raise GadgetServerError: if an error occurs during the connection.
        """
        try:
            self.db_conn = self.get_connection()
            if self.ssl:
                res = self.exec_query("SHOW STATUS LIKE 'Ssl_cipher'")
                if res[0][1] == '':
                    raise GadgetCnxError("Can not encrypt server connection.")
        except GadgetServerError:
            # Reset any previous value if the connection cannot be established,
            # before raising an exception. This prevents the use of a broken
            # database connection.
            self.db_conn = None
            raise
        self.connect_error = None
        self.read_only = self.show_server_variable("READ_ONLY")[0][1]

    def get_connection(self):
        """Return a new connection to the server.

        Attempts to connect to the server according to its connection
        parameters and returns a connection object.

        :return: The resulting MySQL connection object.
        :rtype:  MySQLConnection object

        :raise GadgetCnxError: if an error occurred during the server
                               connection process.
        """
        try:
            parameters = {
                'user': self.user,
                'host': self.host,
                'port': self.port,
            }
            if self.socket and os.name == "posix":
                parameters['socket'] = self.socket
            if self.passwd and self.passwd != "":
                parameters['password'] = self.passwd
            parameters['host'] = parameters['host'].replace("[", "")
            parameters['host'] = parameters['host'].replace("]", "")

            # Add SSL parameters ONLY if they are not None
            if self.ssl_ca is not None:
                parameters['ssl-ca'] = self.ssl_ca
            if self.ssl_cert is not None:
                parameters['ssl-cert'] = self.ssl_cert
            if self.ssl_key is not None:
                parameters['ssl-key'] = self.ssl_key

            # The ca certificate is verified only if the ssl option is also
            # specified.
            if self.ssl and parameters['ssl-ca']:
                parameters['ssl-mode'] = "VERIFY_CA"

            if not "ssl-mode" in parameters:
                parameters['ssl-mode'] = "PREFERRED"
            db_conn = mysqlsh.mysql.get_classic_session(parameters)
            # Return MySQL connection object.
            return db_conn
        except mysqlsh.DBError as err:
            _LOGGER.debug("Connector Error: %s", err)
            raise GadgetCnxError(
                err.args[1], err.args[0], cause=err, server=self)
        except AttributeError as err:
            # Might be raised by mysql.connector.connect()
            raise GadgetCnxError(str(err), cause=err, server=self)

    def disconnect(self):
        """Disconnect from the server.
        """
        if self.db_conn is None:
            raise GadgetCnxError("Cannot disconnect from a not connected"
                                 "server. You must use connect() first.")
        try:
            self.db_conn.close()
        except mysqlsh.DBError:
            # No error expected even if already disconnected, anyway ignore it.
            pass

    def get_version(self, full=False):
        """Return version number of the server.

        Get the server version. The respective instance variable is set with
        the result after querying the server the first time. The version is
        immediately returned when already known, avoiding querying the server
        at each time.

        :param full: If True return the full version information including
                     suffixes (e.g., 5.7.14-log), otherwise only the major,
                     minor and release number (e.g., 5.7.14) are returned.
                     By default: False.
        :type  full: boolean

        :return: List of version numbers(ints) if full = False or a string if
                 full = True or None if an error occurs when trying to get
                 the version information from the server.
        :rtype:  string or int list or None
        """
        # Return the local version value if already known.
        if self._version:
            if full:
                return self._version_full
            else:
                return self._version

        # Query the server for its version.
        try:
            res = self.show_server_variable("VERSION")
            if res:
                self._version_full = res[0][1]
                match = re.match(r'^(\d+\.\d+(\.\d+)*).*$',
                                 self._version_full.strip())
                if match:
                    self._version = [int(x) for x in match.group(1).split('.')]
                    # Ensure a 3 elements list
                    self._version = (self._version + [0])[:3]
        except (GadgetCnxError, GadgetQueryError):
            # Ignore errors and return version initialized with None.
            pass

        if full:
            return self._version_full
        else:
            return self._version

    def check_version_compat(self, t_major, t_minor, t_rel):
        """Checks version of the server against requested version.

        This method can be used to check for version compatibility.

        :param t_major: target server version (major)
        :type  t_major: str or int
        :param t_minor: target server version (minor)
        :type  t_minor: str or int
        :param t_rel:   target server version (release)
        :type  t_rel:   str or int

        :return: True if server version is greater or equal (>=) than the
                 specified version. False if server version is lower (<) than
                 the specified version.
        :rtype:  boolean
        """
        version = self.get_version()
        if version is not None:
            return version >= [int(t_major), int(t_minor), int(t_rel)]
        else:
            return False

    def exec_query(self, query, options=None, exec_timeout=0):
        """Execute a query and return result.

        This is the singular method to execute queries. It should be the only
        method used as it contains critical error code to catch the issue
        with mysql.connector throwing an error on an empty result set.

        Notes:
            - It handles exception if query fails.
            - If 'fetch' is False in the options, the method returns the
              cursor instance.
            - By default a commit is performed at the end, unless 'commit'
              is set to False in the options.

        :param query:    Query object or string with the SQL statement to
                         execute.
        :type  query:    Query
        :type  query:    str
        :param options:      Options to control the execution of the statement.
                             The follow values are supported:
            columns        Add column headings as first row. By default, False.
            fetch          Execute the fetch as part of the operation and
                           use a buffered cursor. By default, True.
            raw            If True use a buffered raw cursor, meaning that
                           all returned values are strings (i.e., not converted
                           to the corresponding Python type). By default, True.
            commit         Perform a commit (if needed) automatically at the
                           end. By default, True.
        :type  options:      dictionary
        :param exec_timeout: Timeout value in seconds to kill the query
                             execution if exceeded. Value must be greater than
                             zero for this feature to be enabled. By default 0,
                             meaning that the query will not be killed.
        :type  exec_timeout: integer

        :return: List of rows (tuples) or Cursor with the result of the query.
        :rtype:  list of tuples or Cursor object

        :raise GadgetCnxError: If an error occurs with the server connection
                               or creating the cursor.
        :raise GadgetQueryError: If an error occurs when excuting the
                                 statement (query), fetching results or
                                 committing changes.
        """
        if options is None:
            options = {}
        columns = options.get('columns', False)
        fetch = options.get('fetch', True)
        raw = options.get('raw', True)
        do_commit = options.get('commit', True)

        if isinstance(query, str):
            query = Query(query)

        # Guard for connect() prerequisite
        if not self.db_conn:
            raise GadgetCnxError(
                "You must call connect before executing a query.", server=self)

        # Execute query, handling parameters.
        q_killer = None
        try:
            if exec_timeout > 0:
                if query.params():
                    raise NotImplementedError()
                # Spawn thread to kill query if timeout is reached.
                # Note: set it as daemon to avoid waiting for it on exit.
                q_killer = QueryKillerThread(self, query.query(), exec_timeout)
                q_killer.daemon = True
                q_killer.start()
            # Execute query.
            cur = None

            _LOGGER.debug("MySQL query: " + query.log())
            if query.params():
                cur = MySQLUtilsCursorResult(
                    self.db_conn.run_sql(query.query(), query.params()))
            else:
                cur = MySQLUtilsCursorResult(
                    self.db_conn.run_sql(query.query()))
        except mysqlsh.DBError as err:
            if cur:
                cur.close()
            if err.args[0] == CR_SERVER_LOST and exec_timeout > 0:
                # If the connection is killed (because the execution timeout is
                # reached), then it attempts to re-establish it (to execute
                # further queries) and raise a specific exception to track this
                # event.
                # CR_SERVER_LOST = Errno 2013 Lost connection to MySQL server
                # during query.
                self.connect()
                raise GadgetQueryError("Timeout executing query", query.log(),
                                       errno=err.args[0], cause=err, server=self)
            else:
                raise GadgetQueryError("Query failed. {0}".format(err),
                                       query.log(), errno=err.args[0], cause=err,
                                       server=self)
        except Exception as err:
            if cur:
                cur.close()
            raise GadgetQueryError("Unknown error: {0}".format(err),
                                   query.log(), errno=0, cause=err,
                                   server=self)
        finally:
            # Stop query killer thread if alive.
            if q_killer and q_killer.is_alive():
                q_killer.stop()

        # Fetch rows (only if available or fetch = True).
        if cur.with_rows:
            if fetch or columns:
                try:
                    results = cur.fetchall()
                    if columns:
                        col_headings = cur.column_names
                        col_names = []
                        for col in col_headings:
                            col_names.append(col)
                        results = col_names, results
                except mysqlsh.DBError as err:
                    raise GadgetQueryError(
                        "Error fetching all query data: {0}".format(err),
                        query.log(), errno=err.args[0], cause=err, server=self)
                finally:
                    cur.close()
                return results
            else:
                # Return cursor to fetch rows elsewhere (fetch = false).
                return cur
        else:
            # No results (not a SELECT)
            try:
                if do_commit:
                    self.db_conn.run_sql("commit")
            except mysqlsh.DBError as err:
                raise GadgetQueryError(
                    "Error performing commit: {0}".format(err), query.log(),
                    errno=err.args[0], cause=err, server=self)
            finally:
                cur.close()
            return cur

    def commit(self):
        """Perform a COMMIT.

        :raise GadgetCnxError: If connection was not previously established.
        """
        # Guard for connect() prerequisite
        if not self.db_conn:
            raise GadgetCnxError(
                "You must call connect before commit.", server=self)

        self.db_conn.run_sql("commit")

    def rollback(self):
        """Perform a ROLLBACK.

        :raise GadgetCnxError: If connection was not previously established.
        """
        # Guard for connect() prerequisite
        if not self.db_conn:
            raise GadgetCnxError(
                "You must call connect before rollback.", server=self)

        self.db_conn.run_sql("rollback")

    def show_server_variable(self, variable):
        """Get the variable information using SHOW VARIABLES statement.

        Return one or more rows from the SHOW VARIABLES statement according
        to the specified variable parameter.

        :param variable:   The variable name or wildcard string to match the
                           returned variable information.
        :type  variable:   string

        :returns: Variables names and respective value matching the specified
                  variable parameter.
        :rtype:   list of tuples

        :raise GadgetServerError: If an error occur getting the variable value.
        """
        return self.exec_query(Query("SHOW VARIABLES LIKE ?", variable))

    def set_variable(self, var_name, var_value, var_type="session"):
        """Set server variable using the SET statement.

        This function sets the value of system variables using the SET
        statement.

        :param var_name: Name of the variable to set.
        :type  var_name: str
        :param var_value: value to which we want to set the variable.
        :type  var_value: str
        :param var_type: Type of the variable ('session', 'global', 'persist'
                         'persist_only'). By default session is used.
        :type  var_type: str
        :raises GadgetDBError: if an invalid var_type is provided as argument.
        """

        if var_type.lower() in ('global', 'session', 'persist',
                                'persist_only'):
            var_type = '{0}.'.format(var_type)  # Add dot (.)
        else:
            raise GadgetError("Invalid variable type: {0}. Supported types: "
                              "'global' and 'session'.".format(var_type))

        # Execute SET @@var_type.var_name.
        self.exec_query("SET @@{0}{1}={2}".format(var_type, var_name,
                                                  var_value))

    def select_variable(self, var_name, var_type=None):
        """Get server system variable value using SELECT statement.

        This function displays the value of system variables using the SELECT
        statement. This can be used as a workaround for variables with very
        long values, as SHOW VARIABLES is subject to a version-dependent
        display-width limit.

        Note: Some variables may not be available using SELECT @@var_name, in
        such cases use SHOW VARIABLES LIKE 'var_name'.

        :param var_name: Name of the variable to display.
        :type  var_name: string
        :param var_type: Type of the variable ('session' or 'global'). By
                         default None (no type is used), meaning that the
                         session value is returned if it exists and the global
                         value otherwise.
        :type  var_type: 'session', 'global', '', or None

        :return: value for the given server system variable.
        :rtype:  string

        :raise GadgetServerError: If an unsupported variable type is
                                  specified.
        :raise GadgetQueryError: If the variable does not exist in the server.
        """
        if var_type is None:
            var_type = ''
        elif var_type.lower() in ('global', 'session', ''):
            var_type = '{0}.'.format(var_type)  # Add dot (.)
        else:
            raise GadgetServerError(
                "Invalid variable type: {0}. Supported types: "
                "'global' and 'session'.".format(var_type), server=self)
        # Execute SELECT @@[var_type.]var_name.
        # Note: An error is issued if the given variable is not known.
        res = self.exec_query("SELECT @@{0}{1}".format(var_type, var_name))
        return res[0][0]

    def has_default_value(self, var_name):
        """Check if the variable has the default value or was already change.

        Return a boolean value indicating if the variable is set with the
        compiled default value (true), or if it was already changed explicitly
        by the user somehow (false), i.e. variable changed with the SET
        statement, a command line option, or the configuration file.

        NOTE: This method requires the performance_schema to be enabled.

        :param var_name: Name of the target variable to check.
        :type var_name: string
        :return: True if the variable value is the compiled default one, or
                 False otherwise (meaning that the variable value was already
                 changed by the user).
        :rtype: boolean
        """
        res = self.exec_query(Query("SELECT variable_source "
                                    "FROM performance_schema.variables_info "
                                    "WHERE variable_name=?", var_name))
        if res[0][0] == 'COMPILED':
            return True
        else:
            return False

    def flush_logs(self, log_type=None):
        """Execute the FLUSH [log_type] LOGS statement.

        Reload internal logs cache and closes and reopens all log files, or
        only of the specified log_type.

        Note: The log_type option is available starting from MySQL 5.5.3.

        :param log_type: Type of the log files to be flushed. Supported values:
                         BINARY, ENGINE, ERROR, GENERAL, RELAY, SLOW.
        :type  log_type: string

        :raise GadgetServerError: If an error occurs when executing the FLUSH
                                  LOGS statement.
        """
        if log_type:
            self.exec_query("FLUSH {0} LOGS".format(log_type))
        else:
            self.exec_query("FLUSH LOGS")

    def supports_gtid(self):
        """Check if the server supports GTIDs.

        :return: True if GTID is supported and turned on and False if
                 supported but not enabled.
        :rtype:  boolean
        :raise GadgetServerError: If GTID is not supported or the GTID mode
                                  cannot be obtained.
        """
        # Check server for GTID support
        version_ok = self.check_version_compat(5, 6, 5)
        if not version_ok:
            raise GadgetServerError("GTIDs are only supported starting from"
                                    "MySQL 5.6.5", server=self)
        try:
            res = self.exec_query("SELECT @@GLOBAL.GTID_MODE")
        except (GadgetCnxError, GadgetQueryError) as err:
            raise GadgetServerError(
                "Unable to get GTID_MODE value: {0}".format(err.errmsg),
                cause=err, server=self)
        # Return result
        if res[0][0] == 'ON':
            return True
        elif res[0][0] == 'OFF':
            return False
        else:
            raise GadgetServerError(
                "Unexpected value for @@GLOBAL.GTID_MODE: {0}."
                "Expected: 'ON' or 'OFF'.".format(res[0][0]), server=self)

    def get_gtid_executed(self, skip_gtid_check=True):
        """Get the executed GTID set of the server.

        This function retrieves the (current) GTID_EXECUTED set of the server.

        :param skip_gtid_check:  Flag indicating if the check for GTID support
                                will be skipped or not. By default 'True'
                                (check is skipped).

        :returns a string with the GTID_EXECUTED set for this server.
        :rtype str
        :raises GadgetError: if GTIDs are not supported or not enabled.
        """
        if not skip_gtid_check:
            # Check server for GTID support.
            gtid_support = self.supports_gtid()
            if not gtid_support:
                raise GadgetServerError("Global Transaction IDs are not "
                                        "supported.", server=self)
        # Get GTID_EXECUTED.
        try:
            return self.exec_query("SELECT @@GLOBAL.GTID_EXECUTED")[0][0]
        except GadgetQueryError:
            if skip_gtid_check:
                # Query likely failed because GTIDs are not supported,
                # therefore skip error in this case.
                return ""
            else:
                # If GTID check is not skipped re-raise exception.
                raise
        except IndexError:
            # If no rows are returned by query then return an empty string.
            return ""

    def supports_plugin(self, plugin, state='ACTIVE'):
        """Check if the given plugin is supported.

        Check if the server supports the specified plugin. Return True if
        plugin is installed and active.

        :param plugin: Name of plugin to check
        :type  plugin: string
        :param state: the expected plugin state to check, by default ACTIVE.
        :type  state: string

        :returns: True if the plugin is supported and it has the given state,
                  False otherwise.
        :rtype:   boolean

        :raise GadgetServerError: If an error occurs when checking for the
                                  plugin support.
        """
        _PLUGIN_QUERY = Query(
            "SELECT PLUGIN_NAME, PLUGIN_STATUS "
            "FROM INFORMATION_SCHEMA.PLUGINS WHERE PLUGIN_NAME LIKE ?", "{0}%".format(plugin))
        res = self.exec_query(_PLUGIN_QUERY)
        if not res:
            # plugin not found.
            _LOGGER.debug("Plugin %s is not installed", plugin)
            return False

        elif res[0][1].upper() != state.upper():
            # The state is not the desired.
            _LOGGER.debug("Plugin %s has state: %s and not the expected: %s",
                          plugin, res[0][1], state)
            return False
        return True

    def get_all_databases(self, ignore_internal_dbs=True):
        """Get all databases from the server.

        Retrieve the list of all databases on the server, except for internal
        databases (e.g., INFORMATION_SCHEMA and PERFORMANCE_SCHEMA) if
        the 'ignore_internal_dbs' parameter is set to True.

        Note: New internal database 'sys' added by default for MySQL 5.7.7+.

        :param ignore_internal_dbs: Ignore internal databases.
        :type  ignore_internal_dbs: boolean

        :returns: Result with the name of all the databases in the server.
        :rtype:   list of tuples

        :raise GadgetServerError: If an error occurs when getting the list of
                                  all databases.
        """

        if ignore_internal_dbs:
            _GET_DATABASES = """
            SELECT SCHEMA_NAME
            FROM INFORMATION_SCHEMA.SCHEMATA
            WHERE SCHEMA_NAME != 'INFORMATION_SCHEMA'
            AND SCHEMA_NAME != 'PERFORMANCE_SCHEMA'
            """
            # Starting from MySQL 5.7.7, sys schema is installed by default.
            if self.check_version_compat(5, 7, 7):
                _GET_DATABASES = ("{0} AND SCHEMA_NAME != 'sys'"
                                  "".format(_GET_DATABASES))
        else:
            _GET_DATABASES = """
            SELECT SCHEMA_NAME
            FROM INFORMATION_SCHEMA.SCHEMATA
            """

        return self.exec_query(_GET_DATABASES)

    def read_and_exec_sql(self, input_file, verbose=False):
        """Read an input file containing SQL statements and execute them.

        :param input_file: The full path to the SQL file.
        :type  input_file: string
        :param verbose:    Log the read statements. By default, False.
        :type  verbose:    boolean

        :raise GadgetServerError: If an error occurs when executing the
                                  statements in the SQL file.
        """
        with open(input_file) as f_input:
            while True:
                cmd = f_input.readline()
                if not cmd:
                    break
                if len(cmd) > 1:
                    if cmd[0] != '#':
                        if verbose:
                            _LOGGER.debug("%s", cmd)
                        query_options = {
                            'fetch': False
                        }
                        self.exec_query(cmd, query_options)

    def binlog_enabled(self):
        """Check binary logging status.

        Check if binary logging is enabled on the server based on the value
        of the 'log_bin' variable.

        :return: False if binary logging is disabled and True otherwise.
        :rtype:  boolean

        :raise GadgetServerError: If an error occur when trying to get the
                                  value of the 'log_bin' variable.
        """
        try:
            res = self.show_server_variable("log_bin")
        except GadgetServerError as err:
            raise GadgetServerError("Cannot get value of 'log_bin' variable. "
                                    "{0}".format(err.errmsg), cause=err,
                                    server=self)
        if not res:
            raise GadgetServerError(
                "No value returned for 'log_bin' variable.", server=self)
        if res[0][1] in ("OFF", "0"):
            return False
        return True

    def toggle_global_read_lock(self, enable=True):
        """
        Enable or disable read-only mode on the server.

        Note: user must have SUPER privilege

        :param enable: Indicate if read-only mode will be enabled (True) or
                       disabled (False). If True (default) then flush all
                       tables with read lock and set the [super_]read_only to
                       'ON'. If False then set [super_]read_only  mode to 'OFF'
                       and unlock all tables.
        :type enable: boolean
        """

        # Starting with version 5.7.8 MySQL has a new super_read_only system
        # variable
        has_super_read_only = self.check_version_compat(5, 7, 8)
        var_name = "super_read_only" if has_super_read_only else "read_only"

        if enable:
            self.exec_query("FLUSH TABLES WITH READ LOCK")
            self.set_variable(var_name, "ON", "global")
        else:
            self.set_variable(var_name, "OFF", "global")
            self.exec_query("UNLOCK TABLES")

    def toggle_binlog(self, action='disable'):
        """Enable or disable binary logging.

        Note: User must have SUPER privilege.

        :param action: if 'disable' then turn off the binary logging.
                       if 'enable' then turn on binary logging. If none
                       of the previous action is specified then nothing is
                       done (no action).
        :type  action: 'disable' or 'enable'

        :raise GadgetServerError: If an error occur when setting the
                                  SQL_LOG_BIN value.
        """
        if action.lower() == 'disable':
            self.exec_query("SET SQL_LOG_BIN=0")
        elif action.lower() == 'enable':
            self.exec_query("SET SQL_LOG_BIN=1")

    def get_server_id(self):
        """Retrieve the server ID.

        :return: Value of the 'server_id' variable.
        :rtype:  integer

        :raise GadgetServerError: If an error occurs when trying to retrieve
                                  the server ID.
        """
        try:
            res = self.show_server_variable("server_id")
        except GadgetServerError as err:
            raise GadgetServerError("Cannot retrieve 'server_id'. "
                                    "{0}".format(err.errmsg), cause=err,
                                    server=self)
        return int(res[0][1])

    def get_server_uuid(self):
        """Retrieve the UUID of the server.

        :return: Value of the 'server_uuid' variable. If not available None
                 is returned.
        :rtype:  string

        :raises GadgetServerError: If an errors occurs when trying to retrieve
                                   the server UUID.
        """
        try:
            res = self.show_server_variable("server_uuid")
            if res is None or res == []:
                return None
        except GadgetServerError as err:
            raise GadgetServerError("Cannot retrieve 'server_uuid': "
                                    "{0}".format(err.errmsg), cause=err,
                                    server=self)
        return res[0][1]

    def grant_tables_enabled(self):
        """Check if grant tables is enabled on the server.

        In other words, this functions checks if the privileges system is
        enabled on the server. Grant tables might be disabled if the
        --skip-grant-tables option is used when starting the server.

        :return: True if grant tables (privileges system) is enabled otherwise
                 False (disabled).
        :rtype:  boolean
        """
        if self.grants_enabled is None:
            try:
                self.exec_query("SHOW GRANTS FOR 'snuffles'@'host'")
                self.grants_enabled = True
            except (GadgetCnxError, GadgetQueryError) as err:
                if (err.args[0] == ER_OPTION_PREVENTS_STATEMENT and
                        "--skip-grant-tables" in err.errmsg):
                    self.grants_enabled = False
                # Ignore other errors as they are not pertinent to the check
                else:
                    self.grants_enabled = True
        return self.grants_enabled

    def get_server_binlogs_list(self, include_size=False):
        """Get the binary log file names listed on a server.

        Obtains the binlog file names available on the server by using the
        'SHOW BINARY LOGS' statement, returning these file names as a list.

        :param include_size: Indicate if the returning list shall include the
                             size of the file.
        :type  include_size: boolean

        :return: List with the binary log file names available on the server.
        :rtype:  list

        :raises GadgetServerError: If an errors occurs when executing SHOW
                                   BINARY LOGS.
        """
        res = self.exec_query("SHOW BINARY LOGS")

        server_binlogs = []
        for row in res:
            if include_size:
                server_binlogs.append(row)
            else:
                server_binlogs.append(row[0])
        return server_binlogs

    def is_plugin_installed(self, plugin_name, is_active=False,
                            silence_warnings=False):
        """Test if the given plugin is installed/loaded in this server.

        :param plugin_name: The name of the plugin to load.
        :type plugin_name:  str.
        :param is_active: If True verifies the plugin is also in 'ACTIVE'
                          state.
        :type is_active:  bool
        :param silence_warnings: If True avoids logging warning messages.
        :type silence_warnings:  bool (False by default)

        :return: True if the plugin is installed otherwise False.
        :rtype: bool.
        """
        res = self.exec_query("show plugins")
        for row in res:
            if plugin_name in row[0]:
                if is_active:
                    return True if "ACTIVE" in row[1] else False
                else:
                    return True
        if not silence_warnings:
            _LOGGER.warning("The %s plugin has not been "
                            "installed/loaded in %s", plugin_name, self)
        return False

    def install_plugin(self, plugin_name):
        """Attempts to install/load the given plugin in this server.

        :param plugin_name: The name of the plugin to load.
        :type plugin_name:  str.

        :raise GadgetServerError: If the GR plugin could not be loaded.

        :return:  True if the plugin is successfully loaded or was already
                  loaded, else an exception is raised.
        :rtype:   boolean

        :raise GadgetServerError: If the plugin could not be installed.
        """
        msg = "Initializing {0} plugin on {1}".format(plugin_name, self)
        _LOGGER.info(msg)
        if self.is_plugin_installed(plugin_name, silence_warnings=True):
            return True
        else:
            try:
                if "WIN" in self.select_variable("version_compile_os").upper():
                    ext = ".dll"
                else:
                    ext = ".so"
                self.exec_query("INSTALL PLUGIN {plugin_name} SONAME "
                                "'{plugin_name}{ext}'"
                                "".format(plugin_name=plugin_name, ext=ext))
                _LOGGER.debug("The %s plugin has been successfully install "
                              "in server: %s", plugin_name, self)
            except GadgetQueryError as err:
                if "already exists" in err.errmsg:
                    _LOGGER.debug("The %s plugin is already installed: %s"
                                  "", plugin_name, err.errmsg)
                else:
                    _LOGGER.error("An error was found trying to install the "
                                  "%s plugin: %s", plugin_name, err.errmsg)
                    raise GadgetServerError("The {0} plugin could not be "
                                            "loaded in the server {1}"
                                            "".format(plugin_name, self),
                                            cause=err)
        return True

    def uninstall_plugin(self, plugin_name):
        """Attempts to uninstall the given plugin in this server.

        :param plugin_name: The name of the plugin to unload.
        :type plugin_name:  str.

        :raise GadgetServerError: If the plugin could not be uninstalled.
        """
        try:
            self.exec_query("UNINSTALL PLUGIN {plugin}"
                            "".format(plugin=plugin_name))
        except GadgetQueryError as err:
            if "does not exist" in err.errmsg:
                _LOGGER.debug("The %s plugin is not installed: %s"
                              "", plugin_name, err.errmsg)
            else:
                _LOGGER.error("An error was found trying to uninstall the %s "
                              "plugin: %s", plugin_name, err.errmsg)
                raise GadgetServerError("The {0} plugin could not be "
                                        "uninstalled in the server {1}"
                                        "".format(plugin_name, self),
                                        cause=err)

    def start_plugin(self, plugin_name):
        """Starts the given plugin
        :param plugin_name: The name of the plugin to load.
        :type plugin_name:  str.
        """
        self.exec_query("START {plugin}".format(plugin=plugin_name))

    def stop_plugin(self, plugin_name):
        """Stops the given plugin
        :param plugin_name: The name of the plugin to load.
        :type plugin_name:  str.
        """
        self.exec_query("STOP {plugin}".format(plugin=plugin_name))

    def __str__(self):
        """String representation of the class Server

        :return: representation the server with information of the host
                 and port.
        :rtype:  string
        """
        if self.socket and os.name == "posix":
            return "'{0}:{1}'".format(self.host, self.socket)
        else:
            return "'{0}:{1}'".format(self.host, self.port)


class QueryKillerThread(threading.Thread):
    """Class to run a thread to kill an executing query.

    This class is used to spawn a thread than will kill the execution
    (connection) of a query upon reaching a given timeout.
    """

    def __init__(self, server, query, timeout):
        """Constructor.

        :param server:  Server instance where the target query is executed.
        :type  server:  Server object
        :param query:   Target query to kill.
        :type  query:   string
        :param timeout: Timeout value in seconds used to kill the query when
                        reached.
        :type  timeout: integer
        """
        threading.Thread.__init__(self)
        self._stop_event = threading.Event()
        self._query = query
        self._timeout = timeout
        self._server = server
        self._connection = server.get_connection()
        server.get_version()

    def run(self):
        """Main execution of the query killer thread.
        Stop the thread if instructed as such
        """
        connector_error = None
        # Kill the query connection upon reaching the given execution timeout.
        while not self._stop_event.is_set():
            # Wait during the defined time.
            self._stop_event.wait(self._timeout)
            # If the thread was asked to stop during wait, it does not try to
            # kill the query.
            if not self._stop_event.is_set():
                try:
                    cur = None
                    # Get process information from threads table when available
                    # (for versions > 5.6.1), since it does not require a mutex
                    # and has minimal impact on server performance.
                    cur = MySQLUtilsCursorResult(self._connection.run_sql(
                        "SELECT processlist_id "
                        "FROM performance_schema.threads"
                        " WHERE processlist_command='Query'"
                        " AND processlist_info='{0}'".format(self._query)))
                    result = cur.fetchall()

                    try:
                        process_id = result[0][0]
                    except IndexError:
                        # No rows are returned if the query ended in the
                        # meantime.
                        process_id = None

                    # Kill the connection associated to que process id.
                    # Note: killing the query will not work with
                    # connector-python,since it will hang waiting for the
                    #  query to return.
                    if process_id:
                        self._connection.run_sql("KILL {0}".format(process_id))
                except mysqlsh.DBError as err:
                    # Hold error to raise at the end.
                    connector_error = err
                finally:
                    # Close cursor if available.
                    if cur:
                        cur.close()
                # Stop this thread.
                self.stop()

        # Close connection.
        try:
            self._connection.disconnect()
        except mysqlsh.DBError:
            # Only raise error if no previous error has occurred.
            if not connector_error:
                raise
        finally:
            # Raise any previous error that already occurred.
            if connector_error is not None:
                # pylint: disable=E0702
                raise connector_error

    def stop(self):
        """Stop the thread.

        Set the event flag for the thread to stop as soon as possible.
        """
        self._stop_event.set()


class LocalErrorLog(object):
    """This class can be used to read the local Error log file.
    Note: In the case the log_error variable is set to "stderr", then it will
    not be possible to retrieve the messages.
    """

    def __init__(self, server, raise_error=False):
        """Constructor

        Create the LocalErrorLog object setting the server to retrieve
        messages logged to its error log file.

        :param server: The server to retrieve messages from the error log.
        :type server:  mysql_gadgets.common.server.Server

        :raise GadgetServerError: If the "log_error" var is set to stderr
                                  instead of a file.
        """
        self.log_error = server.select_variable("log_error")
        if self.log_error == "stderr":
            if raise_error:
                raise GadgetServerError("The log error is set to stderr")
            self._log_file = None
        else:
            datadir = server.select_variable("datadir")
            self._log_file = get_abs_path(self.log_error, datadir)

    def get_size(self):
        """Get the current size of the log error file.

        Returns the current size of the server log error file. This method
        can be used as a starting point to read from the file.

        :return: Size of the error log file.
        :rtype: int
        """
        if self._log_file:
            return os.stat(self._log_file).st_size
        return None

    def read(self, offset, errors_only=True):
        """Reads the Error log file.

        :param offset: Starting position.
        :type offset:  int
        :param errors_only: If the messages returned should be "ERROR"
                            messages only, By default True.
        :type errors_only:  boolean

        :return: Messages logged at the logging file.
        :rtype: str
        """
        if self._log_file:
            with open(self._log_file, "r") as e_file:
                if offset:
                    e_file.seek(offset)
                result = []
                for line in e_file:
                    if errors_only and "[ERROR]" not in line:
                        continue
                    result.append(line)
                return "".join(result)
        return ""
