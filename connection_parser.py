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
This module contains the methods to support common operations
over the ip address or hostnames and parsing of connection strings.
"""

import re
import os
import logging
# Use backported OrderedDict if not available (for Python 2.6)
try:
    from collections import OrderedDict
except ImportError:
    from ordered_dict_backport import OrderedDict

from mysql_gadgets.common.logger import CustomLevelLogger
from mysql_gadgets.exceptions import GadgetCnxFormatError

logging.setLoggerClass(CustomLevelLogger)
_LOGGER = logging.getLogger(__name__)

_BAD_CONN_FORMAT = (u"Connection '{0}' cannot be parsed. Please review the "
                    u"used connection string (accepted formats: "
                    u"<user>@<host>[:<port>][:<socket>]")

_BAD_QUOTED_HOST = u"Connection '{0}' has a malformed quoted host"

_INVALID_PORT = (u"Invalid port '{0}' for connection string '{1}'. "
                 u"Port must be >= 0 and <= 65535.")

_UNPARSED_CONN_FORMAT = ("Connection '{0}' not parsed completely. Parsed "
                         "elements '{1}', unparsed elements '{2}'")

_CONN_QUOTEDHOST = re.compile(
    r"((?:^[\'].*[\'])|(?:^[\"].*[\"]))"  # quoted host name
    r"(?:\:(\d+))?"                       # Optional port number
    r"(?:\:([\/\\w+.\w+.\-]+))?"          # Optional path to socket
)

_CONN_LOGINPATH = re.compile(
    r"((?:\\\"|[^:])+|(?:\\\'|[^:])+)"  # login-path
    r"(?:\:(\d+))?"                     # Optional port number
    r"(?:\:([\/\\w+.\w+.\-]+))?"        # Optional path to socket
)

_CONN_CONFIGPATH = re.compile(
    r"([\w\:]+(?:\\\"|[^[])+|(?:\\\'|[^[])+)"  # config-path
    r"(?:\[([^]]+))?",                         # group
    re.U
)

_CONN_ANY_HOST = re.compile(
    r"""(
         (?![.-])              # Match cannot start by '.' or '-'
          [\w._-]*             # start with 0 or more: alphanum or '%'
          %                    # must have a wildcard '%' at least
          [\w.%_-]*            # then 0 or more: alphanum, '%', '.' or '-'
         (?<![.-])             # but cannot be ended by '.' or '-'.
        )
        (?::(.+))?$            # capture all the rest
    """, re.VERBOSE)


_CONN_HOST_NAME = re.compile(
    r"""(
        (?:
           (?:
              (?:
                 (?!-)         # must not start with hyphen '-'
                 (?:[\w\d-])*  # must not end with the hyphen
                 [A-Za-z]      # starts with a character from the alphabet
                 (?:[\w\d-])*
                 (?:
                    (?<!-)     # end capturing if a '-' is followed by '.'
                 )
               ){1,63}         # limited length for segment
            )
         (?:                   # following segments
            (?:\.)?            # the segment separator  the dot '.'
            (?:
               (?!-)
               [\w\d-]{1,63}   # last segment
               (?<!-)          #shuld not end with hyphen
            )
          )*
         )
        )
       (.*)                    # capture all the rest
     """, re.VERBOSE)

_CONN_IPV4_NUM_ONLY = re.compile(
    r"""(
          (?:         # start of the IPv4 1st group
             25[0-4]  # this match numbers 250 to 254
                    | # or
             2[0-4]\d # this match numbers from 200 to 249
                    | # or
             1\d\d    # this match numbers from 100 to 199
                    | # or
             [1-9]{0,1}\d # this match numbers from 0 to 99
           )
          (?:         # start of the 3 next groups
             \.       # the prefix '.' like in '.255'
             (?:
                25[0-4]|2[0-4]\d|1\d\d|[1-9]?\d
                      # same group as before
              )
           )
             {3}      # but it will match 3 times of it and prefixed by '.'
          )
          (?:\:{0,1}(.*))
          """, re.VERBOSE)

_CONN_PORT_ONLY = re.compile(
    r"""(?:
          \]{0,1}             # the ']' of IPv6 -optional
                 \:{0,1}      # the ':' for port number
                        (
                         -\d+|\d*  # matches any sequence of numbers,
                         )         # including negative/invalid port numbers
         )          # end of port number group
        (?:\:{0,1}(.*))      # all the rest to extract the socket
        """, re.VERBOSE)

_CONN_SOCKET_ONLY = re.compile(
    r"""(?:           # Not capturing group of ':'
           \:{0,1}
             ([      # Capturing '\' or '/' file name.ext
               \/\\w+.\w+.\-
               ]+    # to match a path
              )
        )?
       (.*)          # all the rest to advice the user.
    """, re.VERBOSE)

_CONN_IPV6 = re.compile(
    r"""
    \[{0,1}                   # the optional heading '['
    (
     (?!.*::.*::)              # Only a single whildcard allowed
     (?:(?!:)|:(?=:))          # Colon iff it would be part of a wildcard
     (?:                       # Repeat 6 times:
        [0-9a-f]{0,4}          # A group of at most four hexadecimal digits
        (?:(?<=::)|(?<!::):)   # Colon unless preceded by wildcard
     ){6}                      # expecting 6 groups
     (?:                       # Either
        [0-9a-f]{0,4}          # Another group
        (?:(?<=::)|(?<!::):)   # Colon unless preceded by wildcard
        [0-9a-f]{0,4}          # Last group
        (?:(?<=::)             # Colon iff preceded by exacly one colon
           |(?<!:)
           |(?<=:)(?<!::):
         )
      )
     )
     (?:
        \]{0,1}\:{0,1}(.*)     # optional closing ']' and group for the rest
      )
    """, re.VERBOSE)

# Type of address amd Key names for the dictionary IP_MATCHERS
HN = "hostname"
IPV4 = "IPv4"
IPV6 = "IPv6"
ANY_LIKE = "host like"

# This dictionary is used to identify the matched type..
IP_MATCHERS = OrderedDict([
    (IPV4, _CONN_IPV4_NUM_ONLY),
    (IPV6, _CONN_IPV6),
    (ANY_LIKE, _CONN_ANY_HOST),
    (HN, _CONN_HOST_NAME),
])

_DEFAULT_PORT = 3306


def hostname_is_ip(hostname):
    """Check if specified hostname is an IP address.

    :param hostname: hostname or IP address to check.
    :type  hostname: string

    :return: True if the specified 'hostname' is a valid IP address (IPv4 or
              IPv6), otherwise False.
    :rtype:   boolean
    """
    if len(hostname.split(":")) <= 1:  # if fewer colons, must be IPv4
        grp = _CONN_IPV4_NUM_ONLY.match(hostname)
    else:
        grp = _CONN_IPV6.match(hostname)
    if not grp:
        return False
    return True


def parse_connection(connection_str, options=None):
    """Parse connection values.

    The function parses a connection string with the following format:
    user@host[:port][:socket].

    A dictionary is returned containing the connection parameters. The
    function is designed so that it shall be possible to use it with a
    ``connect`` call in the following manner:

      cnx_options = parse_connection(connection_values, options)
      cnx = mysql.connector.connect(**cnx_options)

    Notes:
    This method validates IPv4 addresses and standard IPv6 addresses.

    This method accepts quoted host portion strings. If the host is marked
    with quotes, the code extracts this without validation and assigns it to
    the host variable in the returned tuple. This allows users to specify host
    names and IP addresses that are outside of the supported validation.

    :param connection_str: Connection string in the form:
                           user@host[:port][:socket].
    :type  connection_str: string
    :param options:        Dictionary of options (e.g. charset, ssl_cert,
                           ssl_ca, ssl_key, ssl).
    :type  options:        dictionary

    :return: dictionary with the parsed connection values (user, host, port,
              etc.).
    :rtype:   dictionary

    :raises GadgetCnxFormatError: if a parsing error occurs.
    """
    if options is None:
        options = {}

    # SSL options, must not be overwritten with those from options.
    ssl_ca = None
    ssl_cert = None
    ssl_key = None
    ssl = None

    # Split on the '@' to determine the connection string format.
    # The user/password may have the '@' character, split by last occurrence.
    conn_format = connection_str.rsplit('@', 1)

    if len(conn_format) == 2:

        # Handle as in the format: user@host[:port][:socket]
        user, hostportsock = conn_format

        # Handle host, port and socket
        if len(hostportsock) <= 0 or len(user) <= 0:
            raise GadgetCnxFormatError(
                _BAD_CONN_FORMAT.format(connection_str))

        if hostportsock[0] in ('"', "'"):
            # Need to strip the quotes
            res = _match(_CONN_QUOTEDHOST, hostportsock)
            if res and len(res) == 3:
                host = res[0]
                port = res[1]
                socket = res[2]
            else:
                raise GadgetCnxFormatError(
                    _BAD_CONN_FORMAT.format(connection_str))
            if host[0] == '"':
                host = host.strip('"')
            if host and host[0] == "'":
                host = host.strip("'")
            # Raise exception if no host is specified, e.g. user@""
            if not host:
                raise GadgetCnxFormatError(
                    _BAD_CONN_FORMAT.format(connection_str))
        else:
            host, port, socket, _ = parse_server_address(hostportsock)

    else:
        # Unrecognized format
        raise GadgetCnxFormatError(_BAD_CONN_FORMAT.format(connection_str))

    # Get character-set from options
    if isinstance(options, dict):
        charset = options.get("charset", None)
        # If one SSL option was found before, not mix with those in options.
        if not ssl_cert and not ssl_ca and not ssl_key and not ssl:
            ssl_cert = options.get("ssl_cert", None)
            ssl_ca = options.get("ssl_ca", None)
            ssl_key = options.get("ssl_key", None)
            ssl = options.get("ssl", None)
    else:
        charset = None
        ssl_cert = None
        ssl_ca = None
        ssl_key = None
        ssl = None
        _LOGGER.warning("options is not instance of dict: %s", options)

    # Set parsed connection values
    connection = {
        "user": user,
        "host": host,
    }

    if charset:
        connection["charset"] = charset
    if ssl_cert:
        connection["ssl_cert"] = ssl_cert
    if ssl_ca:
        connection["ssl_ca"] = ssl_ca
    if ssl_key:
        connection["ssl_key"] = ssl_key
    if ssl:
        connection["ssl"] = ssl
    # If a port was specified we use the port, if a socket was specified
    # we use the socket, else if we specify neither, we use the
    # default port.
    if port:
        port = int(port)
        # Check if port range is valid.
        if port < 0 or port > 65535:
            raise GadgetCnxFormatError(_INVALID_PORT.format(port,
                                                            connection_str))
        connection["port"] = port
    elif socket is not None and os.name == "posix":
        connection['unix_socket'] = socket
        # Port is mandatory to create Server instance.
        connection["port"] = None
    else:
        connection["port"] = _DEFAULT_PORT
    return connection


def parse_server_address(connection_str):
    """Parses host, port and socket from the given connection string.

    :param connection_str: Connection string in the form:
                           user@host[:port][:socket].
    :type  connection_str: string

    :return: a tuple of (host, port, socket, add_type) where add_type is
             the name of the parser that successfully parsed the hostname
             from the connection string.
    :rtype:  tuple

    :raises GadgetCnxFormatError: if a parsing error occurs.
    """
    # Default values to return.
    host = None
    port = None
    socket = None
    address_type = None
    unparsed = None
    # From the matchers look the one that match a host.
    for ip_matcher in IP_MATCHERS:
        try:
            group = _match(IP_MATCHERS[ip_matcher], connection_str)
            if group:
                host = group[0]
                if ip_matcher == IPV6:
                    host = "[%s]" % host

                if group[1]:
                    part2_port_socket = _match(_CONN_PORT_ONLY, group[1],
                                               throw_error=False)
                    if not part2_port_socket:
                        unparsed = group[1]
                    else:
                        port = part2_port_socket[0]
                        if part2_port_socket[1]:
                            part4 = _match(_CONN_SOCKET_ONLY,
                                           part2_port_socket[1],
                                           throw_error=False)
                            if not part4:
                                unparsed = part2_port_socket[1]
                            else:
                                socket = part4[0]
                                unparsed = part4[1]

            # If host is match we stop looking as is the most significant.
            if host:
                address_type = ip_matcher
                break
        # ignore the error trying to match.
        except GadgetCnxFormatError:
            pass
    # we must alert, that the connection could not be parsed.
    if host is None:
        raise GadgetCnxFormatError(_BAD_CONN_FORMAT.format(connection_str))
    _verify_parsing(connection_str, host, port, socket, address_type, unparsed)

    _LOGGER.debug("->parse_server_address \n  host: %s \n  address_type: %s",
                  host, address_type)
    return host, port, socket, address_type


def _verify_parsing(connection_str, host, port, socket, address_type,
                    unparsed):
    """Verify connection string parsing.

    Verify that the connection string was totally parsed and not parts of
    it where not matched, otherwise raise an error.

    :param connection_str: Connection string in the form:
                           user@host[:port][:socket].
    :type  connection_str: string
    :param host:           the parsed  host
    :type  host:           string
    :param port:           the parsed port
    :type  port:           string
    :param socket:         the parsed socket
    :type  socket:         string
    :param address_type:   Type of the parsed host, one of the following value:
                           "IPv4", "IPv6" or "hostname"
    :type  address_type:   string
    :param unparsed:       unparsed string (not identified part)
    :type  unparsed:       string

    :raises GadgetCnxFormatError: if a part of the connection string was not
                                  identified while parsed.
    """
    exp_connection_str = connection_str
    # _LOGGER.debug("exp_connection_str %s", exp_connection_str)
    parsed_connection_list = []
    if host:
        # _LOGGER.debug("host %s", host)
        if address_type == IPV6 and "[" not in connection_str:
            host = host.replace("[", "")
            host = host.replace("]", "")
        parsed_connection_list.append(host)
    if port:
        # _LOGGER.debug("port %s", port)
        parsed_connection_list.append(port)
    if socket:
        # _LOGGER.debug("socket %s", socket)
        parsed_connection_list.append(socket)
    parsed_connection = ":".join(parsed_connection_list)
    # _LOGGER.debug('parsed_connection %s', parsed_connection)
    diff = None
    if not unparsed:
        # _LOGGER.debug('not unparsed found, creating diff')
        diff = connection_str.replace(host, "")
        if port:
            diff = diff.replace(port, "")
        if socket:
            diff = diff.replace(socket, "")
        # _LOGGER.debug("diff %s", diff)
    # _LOGGER.debug("unparsed %s", unparsed)
    if unparsed or (exp_connection_str != parsed_connection and
                    (diff and diff != ":")):
        # _LOGGER.debug("raising exception")
        parsed_args = "host:%s, port:%s, socket:%s" % (host, port, socket)
        err_msg = _UNPARSED_CONN_FORMAT.format(connection_str, parsed_args,
                                               unparsed)
        # _LOGGER.warning(err_msg)
        raise GadgetCnxFormatError(err_msg)


def _match(pattern, connection_str, throw_error=True):
    """Check pattern match with connection string.

    Tries to match a pattern with the connection string and returns the
    groups.

    :param pattern: Regular expression object used to parse the connection
                    string.
    :type  pattern: re.RegexObject
    :param connection_str: Connection string in the form:
                           user@host[:port][:socket].
    :type  connection_str: string
    :param throw_error: Indicate if an exception is raised (True) if the
                        connection string does not match the pattern, or False
                        is returned. By default: True.
    :type throw_error:  boolean

    :return: tuple containing all the subgroups of the matched pattern. If no
             match is found False is returned if 'throw_error' is set to False.
    :rtype:  tuple or boolean

    :raises GadgetCnxFormatError: if connection string does not match the
                                  given pattern and throw_error is set to True.
    """
    grp = pattern.match(connection_str)
    if not grp:
        if throw_error:
            raise GadgetCnxFormatError(_BAD_CONN_FORMAT.format(connection_str))
        return False
    return grp.groups()


def clean_IPv6(host_address):
    """Clean IPv6 host address.

    :param host_address: host address (IPv6)
    :type  host_address: string

    :return: the given host address without '[' and ']' characters (removed).
    :rtype:  string
    """
    if host_address:
        host_address = host_address.replace("[", "")
        host_address = host_address.replace("]", "")
    return host_address


def parse_user_host(user_host):
    """Parse user, passwd, host, port from user:passwd@host

    :param user_host:  MySQL user string (user:passwd@host)
    :type user_host:   string

    :return: Tuple with the user and host.
    :rtype:  tuple
    :raises: GadgetError if the user and host could not be parsed
    """

    no_ticks = user_host.replace("'", "")
    try:
        conn_values = parse_connection(no_ticks)
    except GadgetCnxFormatError as err:
        raise GadgetCnxFormatError("Cannot parse user@host from: {0}."
                                   "".format(no_ticks), cause=err)
    return (conn_values['user'], conn_values['host'])
