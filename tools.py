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
This module contains methods for working with mysql server tools.
"""

# quote() is available in a different module starting from Python 3.3.
try:
    # Import from shlex for Python 3.
    from shlex import quote
except ImportError:
    # Otherwise, import from pipes module for Python 2.
    from pipes import quote

import shlex
import logging
import os
import signal
import socket
import subprocess
import sys

from mysql_gadgets.common.constants import QUOTE_CHAR
from mysql_gadgets.common.logger import CustomLevelLogger
from mysql_gadgets.exceptions import GadgetError

try:
    import mysqlsh
    testutil = mysqlsh.globals.testutil
except:
    testutil = None

LISTENING_PORT_CHECK_TIMEOUT=5
PY2 = int(sys.version[0]) == 2

logging.setLoggerClass(CustomLevelLogger)
_LOGGER = logging.getLogger(__name__)


def _add_basedir(search_paths, path_str):
    """Add a basedir and all known sub directories

    This method builds a list of possible paths for a basedir for locating
    special MySQL files like mysqld (mysqld.exe), etc.

    Note: The resulting paths are append to the list passed by the
    'search_paths' parameter.

    :param search_paths: List of paths to append. The list passed by this
                         parameter is updated by this method.
    :type search_paths: list
    :param path_str:    The basedir path to append.
    :param path_str:    string
    """
    search_paths.append(path_str)
    search_paths.append(os.path.join(path_str, "sql"))       # for source trees
    search_paths.append(os.path.join(path_str, "client"))    # for source trees
    search_paths.append(os.path.join(path_str, "share"))
    search_paths.append(os.path.join(path_str, "scripts"))
    search_paths.append(os.path.join(path_str, "bin"))
    search_paths.append(os.path.join(path_str, "libexec"))
    search_paths.append(os.path.join(path_str, "mysql_utils"))


def get_tool_path(basedir, tool, fix_ext=True, required=True,
                  defaults_paths=None, search_path=False, quote=False,
                  check_tool_func=None):
    """Search for a MySQL tool and return the full path

    :param basedir:        The initial basedir (of a MySQL server) to search.
    :type basedir:         string or None
    :param tool:           The name of the tool to find
    :type tool:            string
    :param fix_ext:        If True (default is True), add .exe if running on
                           Windows.
    :type fix_ext:         boolean
    :param required:       If True (default is True) then an error will be
                           raised if the tool is not found.
    :type required:        boolean
    :param defaults_paths: Default list of paths to search for the tool.
                           By default (None) an empty list is assumed, i.e. [].
    :type defaults_paths:  list
    :param search_path:    Indicates if the paths specified by the PATH
                           environment variable will be used to search for the
                           tool. By default the PATH will not be searched,
                           i.e. search_path=False.
    :type search_path:     boolean
    :param quote:          if True then the resulting path is surrounded with
                           the OS quotes.
    :type quote:           boolean
    :param check_tool_func Function to verify the validity of the found tool.
                           This function must take the path of the tool as
                           parameter and return True or False depending if the
                           tool is valid or not (e.g., verify the version).
                           If this check tool function is specified, it will
                           continue searching for the tool in the provided
                           default paths until a valid one is found. By
                           default: None, meaning that it returns the first
                           location found with the tool (without any check).
    :type check_tool_func  function

    :return: the full path to tool or a list of paths where the tool was found
            if 'search_all' is set to True, or None if not found and 'required'
             is set to False.
    :rtype:  string

    :raises GadgetError: if the tool cannot be found and 'required' is set to
                         True.
    """
    if not defaults_paths:
        defaults_paths = []
    search_paths = []
    if quote:
        if os.name == "posix":
            quote_char = "'"
        else:
            quote_char = '"'
    else:
        quote_char = ''
    if basedir:
        # Add specified basedir path to search paths
        _add_basedir(search_paths, basedir)

    # Search in path from the PATH environment variable
    if search_path:
        for path in os.environ['PATH'].split(os.pathsep):
            search_paths.append(path)

    if defaults_paths and len(defaults_paths):
        # Add specified default paths to search paths
        for path in defaults_paths:
            search_paths.append(path)
    else:
        # Add default MySQL paths to search for tool
        if os.name == "nt":
            search_paths.append("C:/Program Files/MySQL/MySQL Server 5.7/bin")
            search_paths.append("C:/Program Files/MySQL/MySQL Server 8.0/bin")
        else:
            search_paths.append("/usr/sbin/")
            search_paths.append("/usr/local/mysql/bin/")
            search_paths.append("/usr/bin/")
            search_paths.append("/usr/local/bin/")
            search_paths.append("/usr/local/sbin/")
            search_paths.append("/opt/local/bin/")
            search_paths.append("/opt/local/sbin/")

    if os.name == "nt" and fix_ext:
        tool = "{0}.exe".format(tool)
    # Search for the tool
    none_found = True
    for path in search_paths:
        norm_path = os.path.normpath(path)
        if os.path.isdir(norm_path):
            toolpath = os.path.normpath(os.path.join(norm_path, tool))
            if os.path.isfile(toolpath):
                none_found = False
                if not check_tool_func or check_tool_func(toolpath):
                    return r"{0}{1}{0}".format(quote_char, toolpath)
            else:
                if tool == "mysqld.exe":
                    toolpath = os.path.normpath(
                        os.path.join(norm_path, "mysqld-nt.exe"))
                    if os.path.isfile(toolpath):
                        none_found = False
                        if not check_tool_func or check_tool_func(toolpath):
                            return r"{0}{1}{0}".format(quote_char, toolpath)

    # Valid tool not found, raise exception or return None.
    if required:
        # Distinguish between no tool being found and a valid tool (based on
        # the check_tool_func parameter) not being found. Use a different
        # error message and error number.
        if none_found:
            raise GadgetError("Cannot find {0}.".format(tool), errno=1)
        else:
            raise GadgetError("Cannot find valid {0}.".format(tool), errno=2)

    return None


def get_abs_path(path_string, relative_to=None):
    """ Get the absolute path for a file
    This function is used to get the absolute path for the argument provided
    via the path_string parameter. If the provided path_string is an
    absolute path, we return it as is, otherwise we will assume it is a
    relative path relative to the relative_to parameter and use that to
    calculate the absolute path.
    :param path_string: Absolute or relative path to which we want to
                        obtain an absolute path.
    :param path_string: string.
    :param relative_to: absolute path to a directory or file which will be used
                        as the path to which the path_string argument is
                        relative.
    :type relative_to: string
    :return: Absolute path for the path specified in path_string.
    :rtype:  string
    :raises GadgetError: if path_string is not an absolute path and the
                 provided relative_dir parameter is not an absolute path.
    """
    if path_string[0] == '"' and path_string[-1] == '"':
        path_string=path_string[1:-1]

    if os.path.isabs(os.path.expanduser(path_string)):
        return os.path.expanduser(path_string)
    else:
        if not os.path.isabs(relative_to):
            raise GadgetError("{0} is not a valid absolute path.".format(
                relative_to))
        else:
            if os.path.isfile(relative_to):
                relative_to = os.path.dirname(relative_to)
            return os.path.normpath(os.path.expanduser(
                os.path.join(relative_to, path_string)))


def get_subclass_dict(cls):
    """Get a dictionary with the subclasses of class 'cls'.

    This method returns a dictionary with all the classes that inherit from
    class cls.
    Note that this method only works for new style classes

    :param cls: class to which we want to find all subclasses
    :type cls: class object
    :return: dictionary whose keys are the names of the subclasses and values
             are the subclass objects.
    :rtype: dict
    """
    subclass_dict = {}

    for subclass in cls.__subclasses__():
        subclass_dict[subclass.__name__] = subclass
        subclass_dict.update(get_subclass_dict(subclass))

    return subclass_dict


def is_listening(host, port):
    """Try to check if a given port on a given host is bound and listening.

    :param host: hostname we want to check
    :type host: str
    :param port: port number to check if is bound
    :type port: int
    :return: True if port is bound and listening, False otherwise
    :rtype: bool
    """
    # Socket Initialization
    if testutil:
        # testutil will only exist during tests. Outside of tests, this will
        # throw an exception, which will be ignored and the regular check
        # is executed.
        return testutil.is_tcp_port_listening(host, port)
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(LISTENING_PORT_CHECK_TIMEOUT)
    try:
        s.connect((host, port))
        return True
    except socket.error:
        return False


def is_executable(exec_path):
    """ Checks if a given path belongs to an executable file

    :param exec_path: absolute path to the file we want to test is executable.
    :type exec_path: str

    :return: True if file exists and is executable, False otherwise
    :rtype: bool
    """
    _LOGGER.debug("Checking if file '%s' is executable.", exec_path)
    is_file = os.path.isfile(fs_encode(exec_path))
    if not is_file:
        _LOGGER.debug("File '%s' could not be found.", exec_path)
        return False
    elif os.access(fs_encode(exec_path), os.X_OK):
        _LOGGER.debug("File '%s' exists and is executable.", exec_path)
        return True
    else:
        _LOGGER.debug("File '%s' exists but is not executable.", exec_path)
        return False


def run_subprocess(cmd_str, **kwargs):
    """Runs the provided command in a subprocess.

    This method takes the provided argument cmd_str, splits it into a argument
    list  and then feeds it to a subprocess.Popen constructor along with any
    extra keyword argument provided. It return the subprocess.Popen instance.

    :param cmd_str: string with the command to be executed.
    :type cmd_str: str
    :param kwargs: dictionary of extra parameters to be passed to the
                   subprocess.Popen call
    :type kwargs: dict
    :return: the subprocess.Popen instance
    :rtype: subprocess.Popen
    """
    _LOGGER.debug("Spawning subprocess with command '%s'", cmd_str)
    if os.name == "nt":
        if PY2:
            # on windows we can use the string directly
            cmd_list = cmd_str.encode('mbcs')
        else:
            cmd_list = cmd_str
        # create process without a console window (also detaching from our console)
        CREATE_NO_WINDOW = 0x08000000
        # create process in a new group, otherwise pressing CTRL-C in the
        # current console will kill the created process
        kwargs['creationflags'] = kwargs.get(
            'creationflags', 0) | subprocess.CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW
        kwargs['close_fds'] = kwargs.get('close_fds', False)
        # on Windows 'close_fds' closes all descriptors, including standard
        # input/output/error, redirection in this case cannot be used
        if kwargs['close_fds'] and sys.version_info[:2] < (3, 7):
            kwargs['stdin'] = None
            kwargs['stdout'] = None
            kwargs['stderr'] = None
    else:
        if PY2:
            # shlex.split() does not fully support UTF-8.
            enc_cmd_str = cmd_str.encode('utf-8')
        else:
            enc_cmd_str = cmd_str
        cmd_list = map(lambda s: s, shlex.split(enc_cmd_str))
    proc = subprocess.Popen(cmd_list, **kwargs)
    return proc


def stop_process_with_pid(pid, force=False):
    """Terminates or kills a process with a given pid.

    This method attempts to stop a process with the provided pid.
    If force parameter is False, this it attempts to gracefully terminate
    the process. Otherwise, if force parameter is True it attempts to
    forcefully terminate the process.

    :param pid: pid of the process we want to terminate/kill.
    :type pid: int
    :param force: If True process is forcefully killed, otherwise it is
                  gracefully terminated.
    :type force: bool
    :raises GadgetError: if unable to kill or terminate the process.
    """
    error_msg = "Unable to {0} process '{1}': '{2}'"
    if force:
        if os.name == "nt":
            # windows doesn't have sigkill, we need to use taskkill
            # to terminate a process.
            kill_proc = run_subprocess("taskkill /PID {0} /F".format(pid),
                                       shell=False, stderr=subprocess.PIPE,
                                       universal_newlines=True)
            _, err = kill_proc.communicate()
            if kill_proc.returncode:
                raise GadgetError(error_msg.format("kill", pid, str(err)))
        else:  # posix
            try:
                # send SIGKILL
                # pylint: disable=E1101
                os.kill(pid, signal.SIGKILL)
            except OSError as err:
                raise GadgetError(error_msg.format("kill", pid, str(err)))
    else:
        try:
            # send SIGTERM
            os.kill(pid, signal.SIGTERM)
        except OSError as err:
            raise GadgetError(error_msg.format("terminate", pid, str(err)))


def shell_quote(param):
    """Quote the string parameter from the shell command line.

    Quote the specified string parameter, returning a shell-escaped version
    of the string that can be safely used in a shell command line.

    Note: Quoting is compatible with UNIX shells and with shlex.split(), for
          Windows only the parameter is only surrounded with double quotes (").

    :param param: Parameter to quote.
    :type param: string

    :return: A shell-escaped version of the specified string parameter.
    """
    if os.name == "posix":
        return quote(param)
    else:
        return u"{0}{1}{0}".format(QUOTE_CHAR, param)


def fs_encode(value):
    """ Encode the value according to the file system default codec.

    This function is used to encode non-ascii strings (e.g., paths) correctly,
    according to the default system codec. By default, on non-windows
    platforms 'utf-8' is used (supported), and on Windows 'mbcs' (ANSI) is
    used. Not using the right codec will result in incorrect characters,
    and failure of file system operations for paths with non-ascii characters.

    NOTE: On non-windows platforms, even if locale are not set (LC_CTYPE),
          'utf-8' can be used since it is expected to be supported, otherwise
          an UnicodeEncodeError will be raised. For this reason,
          sys.getfilesystemencoding() is not used to automatically get the
          encoding, "forcing" the use of the expected supported codec
          independently of the current locale settings.

    :param value: value (non-ascii) to encode.
    :return: encoded value according to the default system codec.
    """
    if PY2:
        if os.name == "nt":
            return value.encode('mbcs')
        else:
            return value.encode('utf-8')
    else:
        return value


def fs_decode(value):
    """ Decode the value according to the file system default codec.

    This function is used to decode non-ascii strings correctly,
    according to the default system codec. By default, on non-windows
    platforms 'utf-8' is used (supported), and on Windows 'mbcs' (ANSI) is
    used. Not using the right codec will result in incorrect characters.

    :param value: value (non-ascii) to decode.
    :return: decoded value according to the default system codec.
    """
    if PY2:
        if os.name == "nt":
            return value.decode('mbcs')
        else:
            return value.decode('utf-8')
    else:
        return value
