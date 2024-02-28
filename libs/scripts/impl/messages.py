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
Functionality to print messages to the user.
'''

#### Messages ####

_severity_bound = 0
severity_trace = -2
severity_debug = -1
severity_info = 0
severity_warning = 1
severity_error = 2

def set_severity_bound(severity_bound):
    global _severity_bound
    _severity_bound = severity_bound

def _log(*, text: str, severity: int):
    global _severity_bound
    if severity >= _severity_bound:
        print(text)

def log_debug(text: str, severity: int=severity_debug) -> None:
    '''Print "DEBUG: " + *text* if *severity* >= severity_bound.'''

    _log(text=f'DEBUG: {text}', severity=severity)

def log_info(text: str, severity: int=severity_info) -> None:
    '''Print *text* if *severity* >= severity_bound.'''

    _log(text=text, severity=severity)

def log_warning(text: str, severity: int=severity_warning) -> None:
    '''Print "WARNING " + *text* if *severity* >= severity_bound.'''

    log_info(f'WARNING: {text}', severity=severity)

def log_error(text: str, severity: int=severity_error) -> None:
    '''Print "ERROR: " + *text* if *severity* >= severity_bound, and exit.'''

    log_info(f'ERROR: {text}', severity=severity)
    global _severity_bound
    if _severity_bound >= 2:
        raise SystemError()
    exit(1)

