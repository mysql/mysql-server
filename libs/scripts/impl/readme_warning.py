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
Helper function for generating a warning related to readme files.
'''

from   messages import log_warning

import os

def warn_library_parent_readme(*, old: str | None, new: str | None) -> None:
    '''Warn that a file may need to be added/removed in readme.md files.

    For example, after moving a library from libs/mysql/a/b to libs/mysql/c,
    the message would be:

    "
    You may need to manually add a description of "c" to
      libs/mysql/readme.md
    and remove any description of "a/b" from
      libs/mysql/readme.md
      libs/mysql/a/readme.md
    "

    Parameters
    ----------
    :param old: If not None, warn that the file may need to be removed from
        readme.md files in this path.
    :param new: If not None, warn that the file may need to be added to
        readme.md in files in this path.
    '''

    def parent_readme_file_list(path):
        ret = ''
        p = path
        while p and p != 'libs/mysql':
            p = os.path.dirname(p)
            ret += f'\n  {os.path.join(p, "readme.md")}'
        return ret
    file_sets = []
    if new is not None:
        file_sets.append(
            f'add a description of "{new.removeprefix("libs/mysql/")}" to'
            f'{parent_readme_file_list(new)}'
        )
    if old is not None:
        file_sets.append(
            f'remove any description of '
            f'"{old.removeprefix("libs/mysql/")}" from'
            f'{parent_readme_file_list(old)}'
        )
    if file_sets:
        log_warning(
            'You may need to manually ' +
            '\nand '.join(file_sets)
        )
