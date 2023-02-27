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
This file contains the exceptions used by Gadgets and their libraries.
"""


class GadgetError(Exception):
    """Base class for Gadgets.

    This exception class is used to report errors gadgets and its libraries
    (modules), being used to communicate expected errors to the user.
    """

    def __init__(self, message, errno=0, cause=None):
        super(GadgetError, self).__init__()
        self.args = (message, errno, cause)
        self.errmsg = message
        self.errno = errno
        self.cause = cause

    def __str__(self):
        return self.errmsg


class GadgetLogError(GadgetError):
    """Errors for the logger module.

    Note: No logging feature might be available yet at the time this kind of
    error is raised.
    """

    def __init__(self, message, errno=0, cause=None):
        super(GadgetLogError, self).__init__(message, errno, cause)


class GadgetCnxInfoError(GadgetError):
    """Error when the server connection information is not valid.
    """

    def __init__(self, message, errno=0, cause=None):
        super(GadgetCnxInfoError, self).__init__(message, errno, cause)


class GadgetCnxFormatError(GadgetError):
    """Error parsing connection information (wrong format).
    """

    def __init__(self, message, errno=0, cause=None):
        super(GadgetCnxFormatError, self).__init__(message, errno, cause)


class GadgetServerError(GadgetError):
    """Error with the server.
    """
    def __init__(self, message, errno=0, cause=None, server=None):
        super(GadgetServerError, self).__init__(message, errno, cause)
        self.server = server
        self.args = (message, errno, cause, server)

    def __str__(self):
        if self.server:
            return '{0} - {1}'.format(self.server, self.errmsg)
        else:
            return self.errmsg


class GadgetCnxError(GadgetServerError):
    """Error with the server connection.

    Note: This is a subclass of the GadgetServerError.
    """

    def __init__(self, message, errno=0, cause=None, server=None):
        super(GadgetCnxError, self).__init__(message, errno, cause, server)


class GadgetQueryError(GadgetServerError):
    """Error when executing a statement (query) on the server.

    Note: This is a subclass of the GadgetServerError.
    """

    def __init__(self, message, query, errno=0, cause=None, server=None):
        super(GadgetQueryError, self).__init__(message, errno, cause, server)
        self.query = query
        self.args = (message, query, errno, cause, server)

    def __str__(self):
        return '{0}. Query: {1}'.format(
            super(GadgetQueryError, self).__str__(), self.query)


class GadgetDBError(GadgetServerError):
    """Error when a database operations fail.

    Note: This is a subclass of the GadgetServerError.
    """

    def __init__(self, message, errno=0, cause=None, server=None, db=None):
        super(GadgetDBError, self).__init__(message, errno, cause, server)
        self.db = db
        self.args = (message, errno, cause, server, db)


class GadgetRPLError(GadgetError):
    """Error during replication operations.
    """

    def __init__(self, message, errno=0, cause=None):
        super(GadgetRPLError, self).__init__(message, errno, cause)


class GadgetConfigParserError(GadgetError):
    """Error during handling of option files (config parser).
    """

    def __init__(self, message, errno=0, cause=None):
        super(GadgetConfigParserError, self).__init__(message, errno, cause)


class GadgetVersionError(GadgetError):
    """Error when checking the version of a component.
    """

    def __init__(self, message, errno=0, cause=None):
        super(GadgetVersionError, self).__init__(message, errno, cause)
