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
Helpers for working with git.
'''

from   messages import log_warning, log_error

import subprocess
from   typing import List


def get_git_unclean_workdir_files() -> List[str]:
    '''Return the list of unclean files in a git repository.'''

    dirty_status = subprocess.check_output(
        ['git', 'status', '--short'],
        encoding='utf-8'
    ).splitlines()
    return dirty_status

def require_clean_git_workdir(*, force: bool, error_message: str) -> None:
    '''Report error or warning if the workdir is not clean.

    Parameters
    ----------
    :param force: If False, and the worktree is not clean, generate an error and
        stop the program. If True, just generate a warning.
    :param message: Text that will be appended to the warning/error.

    :returns: True if the worktree is clean; False otherwise.
    '''

    status = get_git_unclean_workdir_files()
    if status:
        message_suffix = ''
        if force:
            func = log_warning
        else:
            func = log_error
            if error_message:
                message_suffix = f'\n{error_message}'
        func(
            'The git working tree is not clean:'
            + ''.join([f'\n  {file}' for file in status])
            + message_suffix
        )
        return False
    return True
