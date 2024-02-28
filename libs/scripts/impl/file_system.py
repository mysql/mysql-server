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

from   debug import trace
from   messages import log_error, log_info

import os
from   typing import Iterator

'''
Helper functions for working with the file system.
'''

@trace
def walk_tree(*,
        root: str,
        recurse: bool=True,
        yield_directories: bool=True,
        yield_root: bool=None,
        topdown: bool=True
) -> Iterator[str]:
    '''Iterator over files within the given directory.

    This computes the set of files once and then traverses them. Thus, it is
    reliable even if the user moves the files.

    Within a given directory, yields files before subdirectories.

    Yields filenames relative to *root*.

    Parameters
    ----------
    :param root: The root directory
    :param recurse: Recurse into subdirectories.
    :param yield_directories: If True, yield both files and directories within
        the traversed tree. If False, filter out subdirectories (but still
        recurse into them, if recurse==True.)
    :param yield_root: If True, yield the root of the tree. If False, don't.
        Defaults to the same value as *yield_directories*.
    :param topdown: If True, yield parents before children. If False, yield
        children before parents. Contrary to os.walk, modifying the yielded
        items has no impact on the traversal, even when topdown=True.
    '''

    if yield_root is None:
        yield_root = yield_directories
    # Pre-compute the complete list, so that it is stable
    # even if the directory structure changes while we
    # traverse it
    iterator = os.walk(root, topdown)
    if recurse:
        full_list = list(iterator)
    else:
        first = next(iterator, None)
        full_list = [first] if first else []
    for dirpath, dirnames, filenames in full_list:
        relpath = os.path.relpath(dirpath, root)
        if relpath == '.':
            relpath = ''
            if yield_root and topdown:
                yield ''
        for filename in filenames:
            yield os.path.join(relpath, filename)
        if yield_directories:
            for dirname in dirnames:
                yield os.path.join(relpath, dirname)
        if relpath == '' and yield_root and not topdown:
            yield ''

def is_empty_dir(path: str) -> bool:
    '''Return true if *path* is a directory without contents.'''

    return not next(walk_tree(root=path, yield_root=False), None)

@trace
def makedirs(path: str) -> None:
    '''Create all missing trailing path components of *path*.'''

    prefix = path
    while not os.path.exists(prefix):
        prefix = os.path.dirname(prefix)
    if not os.path.isdir(prefix):
        log_error(
            f'Cannot create "{path}" because "{prefix}" is a non-directory.'
        )
    if prefix == path: return
    path_to_create = os.path.relpath(path, prefix)
    log_info(f'Creating {path_to_create} under {prefix}')
    os.makedirs(path)

@trace
def prune_empty_dirs(path) -> str:
    '''Prune remove path if it is empty, then do the same for the parent.

    Return the deepest of the parent directories that still exists.
    '''

    while path and is_empty_dir(path):
        log_info(f'Pruning {path}')
        os.rmdir(path)
        path = os.path.dirname(path)
    return path

@trace
def rename(*, old: str, new: str) -> None:
    '''Rename *old* file to *new*, pruning empty parent directories for old.

    Return the deepest of the parent directories that still exists.
    '''

    log_info(f'Renaming {old} to {new}')
    os.rename(old, new)
    return prune_empty_dirs(os.path.dirname(old))

@trace
def remove_file(file: str) -> None:
    '''Remove *file*, pruning empty parent directories.

    Returns the deepest of the parent directory that still exists.
    '''

    log_info(f'Removing {file}')
    os.remove(file)
    return prune_empty_dirs(os.path.dirname(file))
