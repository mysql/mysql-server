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
Helper functions used in move/create scripts, for working with filenames.
'''

from   messages import log_error
import string

import os
from   typing import List, Tuple

def split_existing_nonexisting_path(path: str) -> Tuple[str, str]:
    '''Split path into prefix of existing and suffix of nonexisting directories.

    Return a 2-tuple where the first component is the prefix of existing
    directories and the suffix is the suffix of non-existing directories.
    '''
    suffix = ''
    prefix = path
    while not os.path.exists(prefix):
        prefix, part = os.path.split(prefix)
        if suffix:
            suffix = os.path.join(part, suffix)
        else:
            suffix = part
    return prefix, suffix

def validate_path(path: str) -> None:
    '''Check that the path has a valid format, and report an error otherwise.'''

    if not path.startswith('libs/mysql'):
        log_error(
            f'Path "{path}" does not begin with libs/mysql.'
        )
    if '//' in path:
        log_error(
            f'Path {path} contains double slashes.'
        )
    if path.startswith('//'):
        log_error(
            f'Path {path} starts with slash.'
        )
    if path.endswith('//'):
        log_error(
            f'Path {path} ends with slash.'
        )
    _, nonexisting = split_existing_nonexisting_path(path)
    if not nonexisting: return
    if not string.re_fullmatch(
        pattern=r'(/[a-z][a-z0-9]*(_[a-z0-9]+)*)*(\.cc|\.h)?',
        string=f'/{nonexisting}',
    ):
        log_error(
            f'Path "{path}" is not in the correct format. '
            f'The part of it that does not already exist is "{nonexisting}", '
            'which has to follow the following naming rules. '
            'Each path component may consist of one or more words, separated '
            'by underscores. '
            'Each word must contain one or more letters, all from the set '
            '[a-z0-9]. '
            'The first letter of the first word in each path component must '
            'be in the set [a-z].'
        )

def normalize_path(path: str) -> str:
    '''Add the prefix libs/mysql if needed.

    This assumes that paths that don't start with ./ refer to libs/mysql, and
    add that to the path unless it is there already.
    '''

    path = path.removeprefix('./libs/mysql/')
    path = path.removeprefix('libs/mysql/')
    path = path.removeprefix('mysql/')
    if path[0] in '~./':
        log_error(f'Can only create files in libs/mysql, not in {path}')
    path = 'libs/mysql/' + path
    return path

# File type constants
class Source: pass
class Header: pass
class Markdown: pass
class Directory: pass
class Cmakelists: pass
FileType = (
    type[Source] | type[Header] | type[Markdown] | type[Directory] |
    type[Cmakelists]
)

cmakelists_name = 'CMakeLists.txt'
libs_dox_path = 'libs/mysql/libs.dox'

def file_type_by_extension(path: str) -> FileType:
    '''Determine the file type by the extension.

    Parameters
    ----------
    :param path: The path to check.
    :returns: One of the objects Header, Source, Markdown, or Directory.
    '''

    ext = os.path.splitext(path)[1]
    if ext in ('.h', '.hh', '.hpp', '.hxx'):
        return Header
    if ext in ('.c', '.cc', '.cpp', '.cxx'):
        return Source
    if ext == '.md':
        return Markdown
    if ext == '':
        return Directory
    if os.path.basename(path) == cmakelists_name:
        return Cmakelists
    return ext

def source_to_headers(path: str) -> List[str]:
    '''Returns the list of header(s) for a given file.

    This returns the 8 filenames with extensions [-impl]{.h,.hh,.hpp,.hxx}, and
    the first one is guaranteed to be the good old .h file.
    '''

    file_type = file_type_by_extension(path)
    if file_type == Source:
        noext = os.path.splitext(path)[0]
        return [
            f'{noext}{infix}{extension}'
            for infix in ('', '-impl')
            for extension in ('.h', '.hh', '.hpp', '.hxx')
        ]
    raise ValueError(f"Not a source file: {path}")
