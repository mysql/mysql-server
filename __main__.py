#!/usr/bin/env python
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
This script is used for manage MySQL Group Replication (GR), providing the
commands to check a server configuration compliance with GR requirements,
start a replica set, add and remove servers from a group, cloning and
displaying the health status of the members of the GR group. It also allows
the creation of MySQL sandbox instances.
"""

# pylint: disable=wrong-import-position,wrong-import-order
import io
import logging
import os
import signal
import sys
import json

from mysql_gadgets.common.logger import setup_logging, CustomLevelLogger
from mysql_gadgets.common.tools import fs_decode
from mysql_gadgets.command.sandbox import create_sandbox, stop_sandbox, \
    kill_sandbox, delete_sandbox, start_sandbox, SANDBOX, SANDBOX_CREATE, \
    SANDBOX_DELETE, SANDBOX_KILL, SANDBOX_START, SANDBOX_STOP
from mysql_gadgets.exceptions import GadgetError

PY2 = int(sys.version[0]) == 2

if not PY2:
    unicode = str

# get script name
try:
    _SCRIPT_NAME = os.path.splitext(os.path.split(__file__)[1])[0]
except NameError:
    # Use the default script name if an error occur get __file__ value.
    _SCRIPT_NAME = 'mysqlprovision'
# Force script name if file is renamed to __main__.py for zip packaging.
if _SCRIPT_NAME == '__main__':
    _SCRIPT_NAME = 'mysqlprovision'

# Additional supported command names:
JOIN = "join-replicaset"
LEAVE = "leave-replicaset"
START = "start-replicaset"

def main():
    # Get the provided command
    command = sys.argv[1]

    # Read options from stdin
    data = ""

    if PY2:
        stdin = sys.stdin
    else:
        # Python3 expects the stdin to be ASCII encoded, while Shell writes the
        # UTF-8 encoded input. Wrapping the buffer allows to automatially decode
        # the input.
        stdin = io.TextIOWrapper(sys.stdin.buffer, encoding='utf-8')

    while True:
        line = stdin.readline()
        if line == ".\n":
            break
        data += line
    shell_options = json.loads(data.decode('utf-8') if PY2 else data)
    cmd_options = shell_options[0]
    del shell_options[0]

    # Setup logging with the appropriate verbosity level.
    setup_logging(verbosity=int(cmd_options['verbose']))
    logging.setLoggerClass(CustomLevelLogger)
    _LOGGER = logging.getLogger(_SCRIPT_NAME)

    # Handle keyboard interrupt on password retrieve and command execution.
    try:
        # Perform command
        command_error_msg = "executing operation"
        try:
            if command == SANDBOX:
                sandbox_cmd = cmd_options["sandbox_cmd"]
                command = '{0} {1}'.format(command, sandbox_cmd)
                command_error_msg = "executing sandbox operation"
                if sandbox_cmd == SANDBOX_START:
                    command_error_msg = "starting sandbox"
                    start_sandbox(**cmd_options)
                elif sandbox_cmd == SANDBOX_CREATE:
                    command_error_msg = "creating sandbox"
                    create_sandbox(**cmd_options)
                elif sandbox_cmd == SANDBOX_STOP:
                    command_error_msg = "stopping sandbox"
                    stop_sandbox(**cmd_options)
                elif sandbox_cmd == SANDBOX_KILL:
                    command_error_msg = "killing sandbox"
                    kill_sandbox(**cmd_options)
                elif sandbox_cmd == SANDBOX_DELETE:
                    command_error_msg = "deleting sandbox"
                    delete_sandbox(**cmd_options)

        except GadgetError:
            _, err, _ = sys.exc_info()
            # the check for mysql.get_classic_session is a hack to prevent
            # double printing of the DB error
            if err.cause and u"mysql.get_classic_session" not in unicode(err):
                _LOGGER.error(u"Error %s: %s: %s", command_error_msg, unicode(err), unicode(err.cause))
            else:
                _LOGGER.error(u"Error %s: %s", command_error_msg, unicode(err))
            if _LOGGER.level <= logging.DEBUG:
                import traceback
                _LOGGER.debug("%s", traceback.format_exc())
            sys.exit(1)
        except UnicodeEncodeError:
            _, err, _ = sys.exc_info()
            _LOGGER.error(u"Unicode error %s: %s. Make sure your locale "
                          u"system settings support UTF-8 to use non-ASCII "
                          u"values.", command_error_msg, unicode(err))
            if _LOGGER.level <= logging.DEBUG:
                import traceback
                _LOGGER.debug("%s", traceback.format_exc())
            sys.exit(1)
        except Exception:  # pylint: disable=broad-except
            _, err, _ = sys.exc_info()
            _LOGGER.error(u"Unexpected error %s: %s", command_error_msg,
                          unicode(err))
            if _LOGGER.level <= logging.DEBUG:
                import traceback
                _LOGGER.debug("%s", traceback.format_exc())
            sys.exit(1)

        # Operation completed with success.
        sys.exit(0)

    except KeyboardInterrupt:
        _LOGGER.error("keyboard interruption (^C) received, stopping...")
        if os.name == "nt":
            # Using signal.CTRL_C_EVENT on windows will not set any error code.
            # Simulate the Unix signal exit code 130 - Script terminated by
            # Control-C for ngshell to interpreted it as the same way as on
            # linux.
            sys.exit(130)
        else:
            signal.signal(signal.SIGINT, signal.SIG_DFL)
            os.kill(os.getpid(), signal.SIGINT)


if __name__ == "__main__":
    # set to True if you want to enable tracing of the Python code
    # traces are sent to stdout. For debugging only.
    enable_trace = False
    if enable_trace:
        import trace

        tracer = trace.Trace(
            ignoredirs=[sys.prefix, sys.exec_prefix], count=0)

        # run the new command using the given tracer
        tracer.run('main()')
    else:
        main()
