#!/usr/bin/python3.11
# Copyright (c) 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

'''
Function decorator to trace calls.
'''

from messages import log_debug

from collections.abc import Callable
from functools import wraps

_trace_indent = 0

def trace(function: Callable) -> Callable:
    '''Function decorator that debug-prints any invocations.

    If a function has this decorator, and the severity_bound is -1, each
    invocation of the function will print the function name and its parameters.

    Parameters
    ----------
    :param f: The function
    '''
    @wraps(function)
    def work(*args, **kwargs):
        global _trace_indent
        log_debug(
            ' ' * (_trace_indent * 2)
            + f'Calling {function.__name__}('
            + ', '.join(
                [
                    f'{arg!r}' for arg in args
                ] + [
                    f'{key}={value!r}' for key, value in kwargs.items()
                ]
            )
            + ')'
        )
        _trace_indent += 1
        ret = function(*args, **kwargs)
        _trace_indent -= 1
        log_debug(' ' * (_trace_indent * 2) + '.')
        return ret
    return work