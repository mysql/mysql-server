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
Helper functions for editing CMakeLists.txt files
'''

from   debug import trace
from   file_edit import *
from   file_name import *
import file_system
from   messages import log_error
from   template_processor import file_symbols, write_template

from typing import Tuple

def is_library_cmakelists(path: str) -> bool:
    '''Determine if *path* is the CMakeLists.txt for a library.'''

    return (
        file_contains(path=path, text='TARGET_HEADERS')
        or file_contains(path=path, text='TARGET_SRCS')
    )

def find_cmakelists(path: str) -> str | None:
    '''Return the CMakeLists.txt existing in the longest prefix of *path*.

    If *path* is under an existing library, it returns the library
    CMakeLists.txt (because we don't have CMakeLists.txt files in subdirectories
    of libraries). If *path* is above a library, or in a not-yet-created
    library, it returns the CMakeLists.txt of the containing directory (or the
    deepest containing directory that contains a CMakeLists.txt).
    '''

    while path:
        cmakelists = os.path.join(path, cmakelists_name)
        if os.path.exists(cmakelists):
            return cmakelists
        path = os.path.dirname(path)
    return None

def require_library_cmakelists(path: str) -> str:
    '''Return the CMakeLists.txt for the library containing *path*.

    Generate an error if nothing found.
    '''

    cmakelists = find_cmakelists(path)
    if not cmakelists:
        log_error(
            f'No {cmakelists_name} found in {path} or its parent directories.'
        )
    if not is_library_cmakelists(cmakelists):
        log_error(
            f'No library-level {cmakelists_name} found at {path}; '
            f'only {cmakelists} '
            f'which is missing the TARGET_HEADERS and TARGET_SRCS sections.'
        )
    return cmakelists

def require_no_library_cmakelists(path: str) -> None:
    '''Fail if the CMakeLists.txt for *path* is a library CMakeLists.txt'''

    cmakelists = find_cmakelists(path)
    if cmakelists and is_library_cmakelists(cmakelists):
        log_error(f'Found library-level "{cmakelists}" above {path}')

def require_library_path(path: str) -> str:
    '''Find the path to the library containing the given path.'''

    return os.path.dirname(require_library_cmakelists(path))

def cmakelists_file_section_definition(section):
    return dict(
        start_regex=r'SET\(' + f'{section} *\n',
        item_regex=r' *([a-z0-9_/\.]+) *\n',
        end_regex=r' *\)\n',
    )

@trace
def insert_in_cmakelists(
        *, path: str, section: str
) -> None:
    '''Add *path* in *section* of the nearest CMakeLists.txt above *path*

    Parameters
    ----------
    :param path: Update the nearest CMakeLists.txt above this *path*, and
        insert *path*, relative to CMakeLists.txt, in that file.
    :param section: Either "TARGET_SRCS" or "TARGET_HEADERS".
    '''

    cmakelists = require_library_cmakelists(path)
    log_info(f'Inserting in {cmakelists}')
    relpath = os.path.relpath(path, os.path.commonpath([path, cmakelists]))

    before, after = insert_in_file_section(
        path=cmakelists,
        text=f'  {relpath}\n',
        **cmakelists_file_section_definition(section)
    )
    if before == after:
        if before == 0:
            log_error(
                f'Missing section {section} in {cmakelists}, '
                f'where we need to insert {relpath}.'
            )
        else:
            log_error(
                f'{relpath} is already in {section} of {cmakelists}.'
            )
    if section == 'TARGET_SRCS':
        # After adding a source file, insert the directive to build a library.
        append_create_library_to_cmakelists(cmakelists)

@trace
def remove_from_cmakelists(*, path: str, section: str) -> Tuple[str, bool]:
    '''Remove *path* from *section* of the nearest CMakeLists.txt above *path*.

    Parameters
    ----------
    :param path: Update the nearest CMakeLists.txt above this *path*, and
        remove *path*, relative to CMakeLists.txt, in that file.
    :param section: Either "TARGET_SRCS" or "TARGET_HEADERS".
    :returns: The tuple (relpath, found), where: relpath is the relative path of
        *path* to the CMakeLists.txt in the containing library, and found is
        True if the path was found and removed.
    '''

    cmakelists = require_library_cmakelists(path)
    log_info(f'Removing from {cmakelists}')
    relpath = os.path.relpath(path, os.path.commonpath([path, cmakelists]))

    before, after = remove_from_file_section(
        path=cmakelists,
        text=f'  {relpath}\n',
        **cmakelists_file_section_definition(section)
    )
    if section == 'TARGET_SRCS' and after == 0:
        # After removing the last source file, remove the directive.
        remove_create_library_from_cmakelists(cmakelists)
    return relpath, before != after

@trace
def append_create_library_to_cmakelists(cmakelists: str) -> None:
    '''Add LIBS_MYSQL_CREATE_LIBRARY(...) if it is not in *cmakelists*.'''

    if not file_contains(path=cmakelists, text='LIBS_MYSQL_CREATE_LIBRARY'):
        lib_filename = file_symbols(
            path=cmakelists,
            lib_path=os.path.dirname(cmakelists)
        )['lib_filename']
        append_to_file(
            path=cmakelists,
            text='''
LIBS_MYSQL_CREATE_LIBRARY(''' + lib_filename + '''
  TARGET_SRCS ${TARGET_SRCS}
  TARGET_HEADERS ${TARGET_HEADERS}
)
'''
        )

@trace
def remove_create_library_from_cmakelists(cmakelists: str) -> None:
    '''Remove LIBS_MYSQL_CREATE_LIBRARY(...) if it is in *cmakelists*.'''

    replace_in_file(
        path=cmakelists,
        old=string.re_compile(
            # Any string starting with '\nLIBS_MYSQL_CREATE_LIBRARY(',
            # followed by any number of lines that don't begin with ')',
            # followed by ')\n'
            #r'\nLIBS_MYSQL_CREATE_LIBRARY\([^\n]*\n([^\)][^\n]*\n)*\)\n'
            r'\nLIBS_MYSQL_CREATE_LIBRARY\([^\n]*\n([^\)][^\n]*\n)*\)\n'
        ),
        new=''
    )

@trace
def create_library_parents(path: str) -> None:
    '''Create parent directories and their CMakeLists.txts, for a library.

    Does not create the library directory itself, nor the library's
    CMakeLists.txt file.

    Parameters
    ----------
    :param path: Root directory of the library.
    '''
    existing_cmakelists = find_cmakelists(path)
    if is_library_cmakelists(existing_cmakelists):
        log_error(
            f'Cannot create library in {path} '
            f'since there already appears to be a library in a parent '
            f'directory: "{existing_cmakelists}" looks like a library-level '
            f'{cmakelists_name}.'
        )
    # Explanation by example. Suppose we create a library in a directory, like:
    # libs/mysql/dir1/dir2/dir3/libdir, and there is a CMakeLists.txt in dir1
    # but not in any of the deeper directories. Then this logic will, in turn:
    # (1) create dir3/CMakeLists.txt, (2) create dir2/CMakeLists.txt, (3) insert
    # ADD_SUBDIRECTORY(dir2) in dir1/CMakeLists.txt. So it iterates from deeper
    # nested directories to their parents, and creates a new CMakeLists.txt in
    # all levels except the last one. In the last level it edits CMakeLists.txt
    # instead.
    prefix = path
    cmakelists = None
    while cmakelists != existing_cmakelists:
        prefix, basename = os.path.split(prefix)
        cmakelists = os.path.join(prefix, cmakelists_name)
        add_subdirectory = f'ADD_SUBDIRECTORY({basename})\n'
        # For the existing CMakeLists, attempt to edit it.
        # If succssful, we are done.
        if cmakelists == existing_cmakelists:
            if file_contains(path=cmakelists, text=add_subdirectory):
                return
            log_info(f'Updating {cmakelists}')
            before, after = insert_in_file_section(
                path=cmakelists,
                text=add_subdirectory,
                start_regex=r'\n(?= *ADD_SUBDIRECTORY)',
                item_regex=r' *ADD_SUBDIRECTORY\((.*)\)\n',
                end_regex=r' *(?!ADD_SUBDIRECTORY)'
            )
            if before != after: return
            # If insert_in_file_section was not successful, it is because there
            # was no existing ADD_DIRECTORY directive, so insert_in_file_section
            # did not consider that it found the appropriate section. So we fall
            # through and call append_to_file below.
        else:
            # For non-existing CMakeLists.txt, by definition the file does
            # not exist. Create it. This also creates the directory, if it does
            # not already exist.
            write_template(
                path=cmakelists,
                template=f'{cmakelists_name}.template',
                replacements=file_symbols(path=path, lib_path=path)
            )
        # Insert enough newlines that at least one one blank line precedes the
        # inserted line.
        last = read_file(path=cmakelists)
        if last.endswith('\n\n'):
            nl = ''
        elif last.endswith('\n'):
            nl = '\n'
        else:
            nl = '\n\n'
        append_to_file(path=cmakelists, text=f'{nl}{add_subdirectory}')

@trace
def remove_library_from_parent(lib_path):
    '''Remove *lib_path* from CMakeFile.txt, pruning empty parents.

    It removes lib_path from the CMakeFile.txt in the parent directory. If that
    files became empty (ignoring comments and whitespace), remove that too, and
    recursively remove its directory from its parent CMakeLists.txt.
    '''

    # From x/y/CMakeLists.txt, strip y/CMakeLists
    parent_dir = os.path.dirname(lib_path)
    parent_cmakelists = find_cmakelists(parent_dir)
    delta = os.path.relpath(lib_path, os.path.dirname(parent_cmakelists))

    remove_from_file_section(
        path=parent_cmakelists,
        text=f'ADD_SUBDIRECTORY({delta})\n',
        start_regex=r'\n *(?=ADD_SUBDIRECTORY\()',
        item_regex=r' *ADD_SUBDIRECTORY\((.*)\)\n',
        end_regex='(?!ADD_SUBDIRECTORY\()',
    )

    # Does the file contain anything else than comments and whitespace?
    if subprocess.call(
        ['rg', '-q', '-v', r'^\s*(#.*)?$', parent_cmakelists]
    ) == 1:
        # Remove CMakeLists.txt if it became empty
        file_system.remove_file(parent_cmakelists)
        remove_library_from_parent(os.path.dirname(parent_cmakelists))
