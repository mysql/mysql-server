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

__doc__ = '''
Check for style issues and code organization problems in MySQL C++ code.
'''

import subprocess
import re
import argparse
import functools
import itertools
import os
import glob
import copy
import tempfile
from typing import Callable, List, Tuple, Set, Dict, Iterator, TypeVar

'''
Things to check:

(Boxes that are ticked have been implemented. Others would be nice to have, but
not yet implemented.)

- #include directives:
  - [X] included files should have the mysql/libname prefix
  - [X] no . and .. in paths
  - [X] #include <angles> vs #include "quotes" used in the correct places
  - [X] local headers before system headers
  - [X] includes in alphabetic order
  - [X] no circular library dependencies
  - [X] no circular header dependencies
  - [ ] libraries with dependencies outside libs
  - [X] source file includes its own header
  - [ ] non-standard headers

- Header guards:
  - [X] exist and have the right name
  - [X] impl.hpp no header guard

- Order of file subsections:
  - [ ] copyright, then header guard, then includes, then namespace

- Cmakelists:
  - [ ] every library should have a cmakelists
  - [ ] source files are linked to library
  - [ ] header files are either under impl or listed in cmakelists
  - [ ] when library A includes library B headers having a corresponding
        source file, library A links with library B
  - [ ] when library A does not include a header from library B, it also
        should not link with library B.

- Readme:
  - [ ] every library should have a readme
  - [ ] the main library readme should have one reference to each library
'''

#### HELPER FUNCTIONS ####

def warn(msg: str) -> None:
    '''Print the given warning message.'''

    print(f'WARNING: {msg}')

def apply_nested(
        *,
        obj,
        func: Callable
):
    '''Apply a function to each element in a possibly nested list.

    The list may be nested arbitrarily deeply. The returned object is a copy of
    the input list, in which all non-list elements have been replaced by the
    value of the function applied to the element.
    '''

    if isinstance(obj, list):
        return [apply_nested(obj=elem, func=func) for elem in obj]
    return func(obj)

def str_dict_of_list(
        *,
        dict_: Dict[str, List[str]],
        key_filter: Callable[[str], str]=lambda k: k,
        value_filter: Callable[[str], str]=lambda v: v,
) -> str:
    '''Return a string representation of the given dict of lists.

    Parameters
    ==========
    :param dict_: The input dictionary, where keys are strings and values are
        lists of strings.
    :param key_filter: Apply this function to each dictionary key. Exclude dict
        items for which the result is empty. Use the returned value rather than
        the original key.
    :param value_filter: Apply this function to each item in the value lists.
        Exclude items for which the result is empty. Use the returned value
        rather than the original item.
    '''

    return '\n'.join(
        f'{key_filter(k)}:'
        + ''.join([
            f'\n  {value_filter(v)}'
            for v in sorted(v_list)
            if value_filter(v) is not None
        ])
        for k, v_list in dict_.items()
        if key_filter(k) is not None
    )

#### SYSTEM HEADERS ####

# From https://en.cppreference.com/w/cpp/header
system_headers = {
    'algorithm', 'any', 'array', 'assert.h', 'atomic', 'barrier', 'bit',
    'bitset', 'cassert', 'cctype', 'cerrno', 'cfenv', 'cfloat', 'charconv',
    'chrono', 'cinttypes', 'climits', 'clocale', 'cmath', 'codecvt', 'compare',
    'complex', 'concepts', 'condition_variable', 'coroutine', 'csetjmp',
    'csignal', 'cstdarg', 'cstddef', 'cstdint', 'cstdio', 'cstdlib', 'cstring',
    'ctime', 'ctype.h', 'cuchar', 'cwchar', 'cwctype', 'debugging', 'deque',
    'errno.h', 'exception', 'execution', 'expected', 'fenv.h', 'filesystem',
    'flat_map', 'flat_set', 'float.h', 'format', 'forward_list', 'fstream',
    'functional', 'future', 'generator', 'hazard_pointer', 'initializer_list',
    'inttypes.h', 'iomanip', 'ios', 'iosfwd', 'iostream', 'istream',
    'iterator', 'latch', 'limits', 'limits.h', 'linalg', 'list', 'locale',
    'locale.h', 'map', 'math.h', 'mdspan', 'memory', 'memory_resource',
    'mutex', 'new', 'numbers', 'numeric', 'optional', 'ostream', 'print',
    'queue', 'random', 'ranges', 'ratio', 'rcu', 'regex', 'scoped_allocator',
    'semaphore', 'set', 'setjmp.h', 'shared_mutex', 'signal.h',
    'source_location', 'span', 'spanstream', 'sstream', 'stack', 'stacktrace',
    'stdarg.h', 'stddef.h', 'stdexcept', 'stdfloat', 'stdint.h', 'stdio.h',
    'stdlib.h', 'stop_token', 'streambuf', 'string', 'string.h', 'string_view',
    'strstream', 'syncstream', 'system_error', 'text_encoding', 'thread',
    'time.h', 'tuple', 'typeindex', 'typeinfo', 'type_traits', 'uchar.h',
    'unordered_map', 'unordered_set', 'utility', 'valarray', 'variant',
    'vector', 'version', 'wchar.h', 'wctype.h',
}

#### FILE-RELATED HELPER FUNCTIONS ####

def path_prefixes(path: str) -> List[str]:
    '''Return a list of all path prefixes of *path*.

    This includes both the full path and the empty string.

    Example: 'a/b/c/' -> ['a/b/c', 'a/b', 'a', '']
    '''

    ret = []
    while path:
        ret.append(path)
        path = os.path.dirname(path)
    ret.append('')
    return ret

def path_components(path: str) -> List[str]:
    '''Return the list of all path components in *path*.'''

    return '/'.split(os.path.normpath(path))

def normjoin(*files: str) -> str:
    '''Join the given paths, and normalize the result.'''

    return os.path.normpath(os.path.join(*files))

def is_header(file: str) -> bool:
    '''Return true if the given *file* has a C++ header suffix.'''

    ext = os.path.splitext(file)
    ret = ext[1] in ('.h', '.hh', '.hpp', '.hxx')
    return ret

def read_file(file: str) -> str:
    '''Read all contents of *file* into a string and return it.'''

    with open(file) as fd:
        return fd.read()

def file_contains(
        *,
        file: str,
        text: str
) -> bool:
    '''Return True if the given *file* contains the given *text*.'''

    return text in read_file(file)

#### INCLUDE GRAPH ####

class FileGraph:
    '''Represents the set of files and libraries that we are to check, as well
    as dependencies between them.

    Terminology used in this class:

    - inc refers to a C++ file that is included by another C++ file through a
      #include directive.
    - relinc is the path and filename for an inc file used after #include; the
      relinc is relative to the include path.
    - absinc is the full path and filename for an inc file relative to the
      project root, after prepending the correct include path under which it
      exists. Since we are in the business of checking  issues, we have to
      assume that the file may exist under multiple include paths, so absinc is
      really a list of paths.

    Generally, this class computes the graph lazily, making use of
    @functools.cached_property. I.e., the first time we need to know if a file
    includes another file, we compute the full include graph and cache it.
    '''

    def __init__(
            self, *,
            graph_dirs: List[str],
            root_incpaths: List[str],
            prefix_incpaths: List[str],
            leaf_incpaths: List[str],
            replace_incpath: Dict[str, str],
    ):
        '''Construct a new FileGraph based on the given root + include paths.

        Parameters:
        ===========
        :param graph_dirs: List of directories to check.
        :param root_incpaths: List of include paths relative to the root.
        :param prefix_incpaths: List of include paths relative to each
            non-empty, non-complete prefix of the path of the file. Example: if
            prefix_incpaths contains'foo', it gives the file 'a/b/c/file.cc' the
            include paths 'a/foo' and 'b/foo'.
        :param leaf_incpaths: List of include paths relative to the directory of
            the file. Example: if leaf_incpaths contains 'bar', it gives the
            file 'a/b/c/file.cc' the include path 'a/b/c/bar'.
        :param replace_incpath: List of patterns for computing include paths
            based on substituting one or more path components. Example: if
            replace_incpath['src']=='include', it gives the file
            'a/src/b/c/file.cc' the include path 'a/include/b/c'.
        '''

        def norm_path_list(path_list):
            return [os.path.normpath(path) for path in path_list]
        self.graph_dirs = norm_path_list(graph_dirs)
        self.root_incpaths = norm_path_list(root_incpaths)
        self.prefix_incpaths = norm_path_list(prefix_incpaths)
        self.leaf_incpaths = norm_path_list(leaf_incpaths)
        self.replace_incpath = {
            os.path.normpath(k): os.path.normpath(v)
            for k, v in replace_incpath.items()
        }

    @functools.cached_property
    def libs(self) -> List[str]:
        '''Return a list of all libraries.

        This is defined as subdirectories of libs/mysql which contain a
        CMakeLists.txt which contains at least one of the words TARGET_SRCS or
        TARGET_HEADERS.
        '''

        return sorted(
            subprocess.check_output(
                "rg --files-with-matches --type=cmake "
                r"""'(TARGET_SRCS|TARGET_HEADERS)' """
                "libs/mysql "
                " | rg 'libs/(.*)/CMakeLists.txt' --replace '$1'",
                shell=True,
                encoding='utf-8'
            ).splitlines()
        )

    @functools.cached_property
    def _lib_regex(self) -> str:
        '''Return a regex that captures the library directory from a path.'''

        return 'libs/(' + '|'.join(self.libs) + ')(?![^/]   )'

    @functools.cached_property
    def _lib_pattern(self) -> re.Pattern:
        '''Return the compiled re.Pattern for _lib_regex.'''

        return re.compile(self._lib_regex)

    @functools.cache
    def file_to_lib(self, file: str) -> str:
        '''Returns the library containing *file*, or '?' if not in a library.'''

        m = self._lib_pattern.match(file)
        if not m: return '?'
        return m.group(1)

    @functools.cache
    def dir_to_incpaths(self, dirname: str) -> Set[str]:
        '''Return the set of incpaths for the given *dirname*.

        In other words, when a file in *dirname* contains an #include directory,
        the places to look for the included file is the set of paths returned
        from this function.
        '''

        ret = copy.copy(self.root_incpaths)
        if self.leaf_incpaths:
            # All leaf_incpaths, concatenated to the dirname
            ret += [
                normjoin(dirname, path)
                for path in self.leaf_incpaths
                if normjoin(dirname, path) in self.dirs
            ]
        if self.prefix_incpaths:
            # All prefixes except the empty one and the full one,
            # times all prefix_incpaths.
            ret += [
                normjoin(prefix, path)
                for prefix, path in itertools.product(
                    path_prefixes(dirname)[1:-1], self.prefix_incpaths
                )
                if normjoin(prefix, path) in self.dirs
            ]
        if self.replace_incpath:
            for old, new in self.replace_incpath.items():
                if f'/{old}/' in f'/{dirname}/':
                    ret.append(
                        f'/{dirname}/'.replace(
                            f'/{old}/', f'/{new}/'
                        )[1:-1]
                    )
        return set(ret)

    @functools.cache
    def relinc_to_incpath(
            self,
            *,
            relinc: str,
            dirname: str
    ) -> List[str]:
        '''Return the incpaths where relinc exists, if included from dirname.

        So if dirname contains an include directive #include "{relinc}", this
        returns those include paths where it is found.

        Normally there should be exactly one, but since we are a checker, one of
        our tasks is to check that, so we return a list.
        '''

        ret = [
            incpath
            for incpath in self.dir_to_incpaths(dirname)
            if normjoin(incpath, relinc) in self.files
        ]
        return ret

    @functools.cache
    def relinc_to_absinc(
            self,
            *,
            relinc: str,
            dirname: str
    ) -> List[str]:
        '''Return the abspaths where relinc is found, if included from dirname.

        So if dirnae contains an include directive #include "{relinc}", this
        returns the paths where it is found.

        If none found, returns a list containing relinc with a '!' character
        prepended.
        '''

        found = self.relinc_to_incpath(relinc=relinc, dirname=dirname)
        if found:
            return [normjoin(incpath, relinc) for incpath in found]
        else:
            return [f'!{relinc}']

    @functools.cache
    def relinc_is_local(
            self,
            *,
            relinc: str,
            dirname: str
    ) -> bool:
        '''Return True if the given relinc is a local file.

        Here local means that it is found in any of the incpaths.
        '''

        return len(self.relinc_to_incpath(relinc=relinc, dirname=dirname)) > 0

    @functools.cache
    def split_lib(self, file: str) -> Tuple[str, str]:
        '''Given a file, return the 2-tuple (lib, remainder).

        Note that this removes the 'libs/' prefix.

        Example: if file=='libs/mysql/dir/lib/subdir/file.cc', this function
        returns ('mysql/dir/lib', 'subdir/file.cc').
        '''

        lib = self.file_to_lib(file)
        if lib == '?':
            return None, file
        return lib, file[5 + len(lib):]

    @functools.cached_property
    def _relincs_str(self) -> List[str]:
        '''Worker function for _file_relinc_angle.

        This returns a list where each item is on the form <FILE>:<CHAR><INC>,
        where FILE is the source file that includes another file, CHAR is the
        " or < character used in the include directive, and INC is the file that
        FILE includes.
        '''

        graph_dirs = ' '.join(self.graph_dirs)

        try:
            nproc = int(subprocess.check_output('nproc').splitlines()[0])
        except:
            nproc = 8

        with tempfile.TemporaryDirectory() as tempdir:
            # Execute in nproc parallel processes.
            # Make each of them write a temporary file that is private to the
            # process, so their outputs don't get mixed up.
            # Place all temporary files in a single temporary directory, which
            # gets auto-deleted when leaving the "with" scope.
            cmd = (
                f'rg --type cpp --files {graph_dirs} '
                f'| xargs -P{nproc} -L256 ./libs/scripts/find_includes.sh '
                f' --output-tempfile-dir {tempdir}'
            )
            subprocess.check_call(
                cmd,
                shell=True,
                encoding='utf-8'
            )
            ret = subprocess.check_output(
                f'cat {tempdir}/*',
                shell=True,
                encoding='utf-8'
            ).splitlines()

        return ret

    @functools.cached_property
    def _file_relinc_angle(self) -> Tuple[
            Dict[str, List[str]],
            Dict[str, Dict[str, bool]]
    ]:
        '''Worker function for file_relinc and file_relinc_angle.

        Returns the pair (self.file_relinc, self.file_relinc_angle). (Both
        implemented in one function for performance.)
        '''

        relincs = {f: [] for f in sorted(self.cppfiles)}
        types = {f: {} for f in sorted(self.cppfiles)}
        for item in self._relincs_str:
            k, v = item.split(':')
            relincs.setdefault(k, []).append(v[1:])
            types.setdefault(k, {})[v[1:]] = (v[0] == '<')
        return relincs, types

    @functools.cached_property
    def file_relinc(self) -> Dict[str, List[str]]:
        '''Return a dict from filename to included files (relative paths).'''

        return self._file_relinc_angle[0]

    @functools.cached_property
    def file_relinc_angle(self) -> Dict[str, Dict[str, bool]]:
        '''Return a two-level dict from file to header to angle type.

        In the outer dict, the keys are files, and values are a dicts. In this
        value dict, each key is a header file included by the file, and each
        value is True if the file is included with angle syntax, rather than
        quote syntax, i.e., #include <...> rather than #include "...".
        '''

        return self._file_relinc_angle[1]

    @functools.cached_property
    def file_relinc_absinc(self) -> Dict[str, Dict[str, List[str]]]:
        '''Return a dict from file to dict from relinc to list of absinc.

        So in the outer dict, the keys are files and the values are dicts. In
        the value dict, the key is a relinc (i.e., the filename specified in the
        include directive, which is relative to an incpath), and the value is
        the list of absincs (i.e., full paths to the included file).
        '''

        return {
            file: {
                relinc: self.relinc_to_absinc(
                    relinc=relinc,
                    dirname=os.path.dirname(file)
                )
                for relinc in self.file_relinc[file]
            }
            for file in sorted(self.cppfiles)
        }

    @functools.cached_property
    def file_relinc_incpath(self) -> Dict[str, Dict[str, List[str]]]:
        '''Return a dict from file to (dict from relinc to (list of incpaths)).

        So in the outer dict, the keys are files and the values are dicts. In
        the value dict, the key is an relinc (i.e., the filename specified,
        which is relative to an incpath), and the value is the list of incpaths
        under which the relinc is found.
        '''

        return {
            file: {
                relinc: self.relinc_to_incpath(
                    relinc=relinc, dirname=os.path.dirname(file)
                )
                for relinc in self.file_relinc[file]
            }
            for file in self.cppfiles
        }

    @functools.cached_property
    def file_absinc(self) -> Dict[str, Set[str]]:
        '''Return a dict from files to sets of absincs of included headers.

        So the keys are filenames and the values are sets, where each element is
        a path to a header included by that file. The path is relative to the
        root.
        '''

        ret = {file: set() for file in self.cppfiles}
        for file in self.cppfiles:
            for relinc in self.file_relinc[file]:
                ret[file].update(self.file_relinc_absinc[file][relinc])
        return ret

    @functools.cached_property
    def lib_absinc(self) -> Dict[str, List[str]]:
        '''Return a dict from libraries to sets of absincs of included headers.

        So the keys are library names, and the values are sets. Each element in
        the set is a path to a header included by some file in the library. The
        path is relative to the root.
        '''

        ret = {lib: set() for lib in self.libs + ['?']}
        for file, absinc in self.file_absinc.items():
            ret[self.file_to_lib(file)].update(absinc)
        return ret

    @functools.cached_property
    def lib_libs(self) -> Dict[str, List[str]]:
        '''Return a dict from libraries to libraries based on included headers.

        So the keys are library names, and the values are sets. Each element in
        the set is the name of a library containing a header included by some
        file in the key library.
        '''

        return {
            lib: sorted({
                self.file_to_lib(absinc)
                for absinc in absincs
                if absinc.removeprefix('!') not in system_headers
            })
            for lib, absincs in self.lib_absinc.items()
        }

    @functools.cached_property
    def absinc_ref(self) -> Dict[str, List[str]]:
        '''Return a dict from headers to files including it.

        So each key is the path of a header file, relative to the root, and
        the values are paths, relative to the root, of files that include the
        header file.
        '''

        ret = {}
        for file, absinc_list in self.file_absinc.items():
            for absinc in absinc_list:
                ret.setdefault(absinc, set()).add(file)
        return ret

    def _rg_files(self, args: str) -> List[str]:
        '''Worker for *files* and *cppfiles*.

        Returns the output of rg --files {args}, split to a list with one
        element per file.
        '''

        ret = subprocess.check_output(
            f'rg --files {args} ',
            shell=True,
            encoding='utf-8'
        ).splitlines()
        return set(ret)

    @functools.cached_property
    def files(self) -> List[str]:
        '''Return the list of all files to be checked.'''

        return self._rg_files('')

    @functools.cached_property
    def cppfiles(self) -> List[str]:
        '''Return the list of all cpp files to be checked.'''

        return self._rg_files('--type cpp')

    @functools.cached_property
    def dirs(self) -> Set[str]:
        '''Return the set of all directories containing some checked file.

        This includes also prefixes of such paths.
        '''

        ret = set()
        for file in self.files:
            dirname = os.path.dirname(file)
            if dirname not in ret:
                for prefix in path_prefixes(dirname):
                    ret.add(prefix)
        return ret

    @functools.cached_property
    def headerfiles(self) -> Set[str]:
        '''Return the set of all headers.'''

        return {file for file in self.files if is_header(file)}

#### CHECKS ####

def in_dirs(
        *,
        file: str,
        inc_dirs: List[str],
        exc_dirs: List[str]
) -> bool:
    '''Return True if *file* is "in" according to *inc_dirs* and *exc_dirs*.

    *file* is considered to be "in" if one of the following holds:
    - inc_dirs is non-empty and file is in one of the inc_dirs.
    - inc_dirs is empty and file is not in exc_dirs.

    At least one of *inc_dirs* and *exc_dirs* must be empty.

    Parameters:
    ===========
    :param file: file to check.
    :param inc_dirs: If nonempty, the file is "in" only if it is in this list.
    :param exc_dirs: If nonempty, the file is "in" only if it is not in this
        list.
    '''

    if inc_dirs and exc_dirs:
        raise ValueError('Pass either inc_dirs or exc_dirs, not both')
    def prefix_in_list(file, alist):
        return any(filter(
            lambda p: os.path.normpath(
                os.path.commonprefix([file, p])
             ) == p,
            alist
        ))
    if inc_dirs:
        return prefix_in_list(file, inc_dirs)
    else:
        return not prefix_in_list(file, exc_dirs)

def any_in_dirs(
        *,
        file_list: List[str],
        inc_dirs: List[str],
        exc_dirs: List[str]
) -> bool:
    '''Return True if any file is "in" according to *inc_dirs* and *exc_dirs*.

    In other words, for at least one element `file` of *file_list*,
    `in_dirs(file, inc_dirs, exc_dirs)` should hold.

    Parameters:
    ===========
    :param file_list: list of files to check.
    :param inc_dirs: If nonempty, a file is "in" only if it is in this list.
    :param exc_dirs: If nonempty, a file is "in" only if it is not in this
        list.
    '''

    return any(filter(
        lambda file: in_dirs(file=file, inc_dirs=inc_dirs, exc_dirs=exc_dirs),
        file_list
    ))

def filter_in_dirs(
        *,
        file_list: List[str],
        inc_dirs: List[str],
        exc_dirs: List[str],
        key: Callable[[str], str]=lambda x: x
) -> List[str]:
    '''Return the list of files for which key(file) is "in".

    In other words, the list of files for which
    `in_dirs(key(file), inc_dirs, exc_dirs)` returns True.

    Parameters:
    ===========
    :param file_list: file to check.
    :param inc_dirs: If nonempty, a file is "in" only if it is in this list.
    :param exc_dirs: If nonempty, a file is "in" only if it is not in this
        list.
    '''

    return [
        file
        for file in file_list
        if in_dirs(file=key(file), inc_dirs=inc_dirs, exc_dirs=exc_dirs)
    ]

def filter_any_in_dirs(
        *,
        file_list_list: List[List[str]],
        inc_dirs: List[str],
        exc_dirs: List[str],
        key: Callable[[str], str]=lambda x:x
) -> List[List[str]]:
    '''Return list of file lists that are "in" if *key* is applied to each file.

    In other words, for each file list in *file_list_list*, we apply *key* to
    each file. Then we check if the resulting list is "in" according to
    `any_in_dirs(modified_list, inc_dirs, exc_dirs)`. If it is "in", we add the
    original list (not modified by *key*) to the returned list.

    Parameters:
    ===========
    :param file_list_list: list of file lists to check.
    :param inc_dirs: If nonempty, a file is "in" only if it is in this list.
    :param exc_dirs: If nonempty, a file is "in" only if it is not in this
        list.
    '''

    return [
        file_list
        for file_list in file_list_list
        if any_in_dirs(
            file_list=[key(file) for file in file_list],
            inc_dirs=inc_dirs,
            exc_dirs=exc_dirs
        )
    ]

def check_inc_path(
        *,
        file_graph: FileGraph,
        report_dirs: List[str] | None,
        no_report_dirs: List[str] | None
) -> None:
    '''Check syntax of include paths.

    This checks the following conditions:
    - Paths in includes don't contain . or ..
    - Included files are found in exactly one include path.
    - Included files under libs are relative to libs, i.e., begin with mysql/
    - Included files under sql are relative to ., i.e., begin with sql/

    Parameters:
    ===========
    :param file_graph: FileGraph to check.
    :param report_dirs: If given, report problems only for directories in this
        list.
    :param no_report_dirs: If given, report problems only for directories that
        are not in this list.
    '''

    for file, relinc_list in file_graph.file_relinc.items():
        if not in_dirs(
                file=file, inc_dirs=report_dirs, exc_dirs=no_report_dirs
        ):
            continue
        for relinc in relinc_list:
            components = relinc.split('/')
            for disallowed in ['.', '..']:
                if disallowed in components:
                    warn(
                        f'File {file} includes "{relinc}" '
                        f'which contains "{disallowed}". '
                        f'Do not use that in include directives.'
                    )
            absinc_list = file_graph.file_relinc_absinc[file][relinc]
            if len(absinc_list) == 0:
                warn(
                    f'File {file} includes {relinc}, which could not be found '
                    f'in any of the include paths '
                    + ', '.join(file_graph.dir_to_incpaths(
                        os.path.dirname(file)
                    ))
                )
            elif len(absinc_list) == 1:
                absinc = absinc_list[0]
                if (
                        absinc.startswith('libs/')
                        and (
                            file_graph.file_relinc_incpath[file][relinc][0]
                            != 'libs'
                        )
                ):
                    warn(
                        f'File {file} includes {relinc}, '
                        f'which exists under libs as {absinc}. '
                        f'Use a path relative to "libs" instead '
                        f'(starting with "mysql/").'
                    )
                elif (
                        absinc.startswith('sql/')
                        and (
                            file_graph.file_relinc_incpath[file][relinc][0]
                            != '.'
                        )
                ):
                    warn(
                        f'File {file} includes {relinc}, '
                        f'which exists under sql as {absinc}. '
                        f'Use a path starting with "sql" instead.'
                    )
            elif len(absinc_list) > 1:
                warn(
                    f'File {file} includes {relinc}, which was found in '
                    'multiple places: '
                    + ', '.join(absinc_list)
                )

def check_inc_type(
        *,
        file_graph: FileGraph,
        report_dirs: List[str] | None,
        no_report_dirs: List[str] | None
) -> None:
    '''Checks that included files have the correct <file> or "file" format.

    Parameters:
    ===========
    :param file_graph: FileGraph to check.
    :param report_dirs: If given, report problems only for directories in this
        list.
    :param no_report_dirs: If given, report problems only for directories that
        are not in this list.
    '''

    for file, relinc_list in file_graph.file_relinc.items():
        if not in_dirs(
                file=file, inc_dirs=report_dirs, exc_dirs=no_report_dirs
        ):
            continue
        for relinc in relinc_list:
            have_angle = file_graph.file_relinc_angle[file][relinc]
            if file_graph.relinc_is_local(
                    relinc=relinc, dirname=os.path.dirname(file)
            ):
                if have_angle:
                    warn(
                        f'File {file} includes <{relinc}> with angles, but '
                        f'quotes are required. Use "{relinc}" instead.'
                    )
            else:
                if not have_angle:
                    warn(
                        f'File {file} includes "{relinc}" with quotes, but '
                        f'angles are required. Use <{relinc}> instead.'
                    )

def check_header_usage(
        *,
        file_graph: FileGraph,
        report_dirs: List[str] | None,
        no_report_dirs: List[str] | None
):
    '''Checks that headers are included by the files that should include them.

    This checks the following conditions:
    - Each header should be used at least once.
    - Headers with name ending with _impl should only be used once: either by
      the header without _impl, or by another _impl.h file.
    - Source files should include their own headers.

    Parameters:
    ===========
    :param file_graph: FileGraph to check.
    :param report_dirs: If given, report problems only for directories in this
        list.
    :param no_report_dirs: If given, report problems only for directories that
        are not in this list.
    '''

    for file in sorted(file_graph.headerfiles):
        if not in_dirs(
                file=file, inc_dirs=report_dirs, exc_dirs=no_report_dirs
        ):
            continue
        refs = file_graph.absinc_ref.get(file, [])
        if not refs:
            warn(
                f'File {file} is not included by any other file. '
                f'Can it be removed?'
            )
        else:
            base = os.path.splitext(file)[0]
            if base.endswith('_impl'):
                good_ref = base.removesuffix('_impl') + '.h'
                if len(refs) == 1:
                    ref = list(refs)[0]
                    if (
                        not os.path.splitext(ref)[0].endswith('_impl')
                        and ref != good_ref
                    ):
                        warn(
                            f'Since {file} ends with _impl, it is expected to '
                            f'only be included once, either from another _impl '
                            f'file or from {good_ref}. But it is included from '
                            f'{ref}.'
                        )
                else:
                    assert len(refs) > 1
                    warn(
                        f'Since {file} ends with _impl, it is expected to only '
                        f'be included once, either from another _impl file or '
                        f'from {good_ref}. But it is included from the '
                        f'following files: ' + ', '.join(refs) + '.'
                    )
            # If there is a source file for this header, then that source
            # file should include this header.
            for ext in 'cc', 'cpp':
                source_file = f'{base}.{ext}'
                if (
                    source_file in file_graph.files
                    and source_file not in refs
                ):
                    warn(
                        f'{source_file} does not include {file}'
                    )

def check_header_guards(
        *,
        file_graph: FileGraph,
        report_dirs: List[str] | None,
        no_report_dirs: List[str] | None
) -> None:
    '''Checks that header guards exist and have the required format.

    More precisely, checks the following conditions:
    - Each header except those ending with _impl.hpp must have a header guard.
    - Header guards should have the required format: the path, with
      non-alphanumeric characters replaced by underscores and alphabetic
      characters converted to uppercase.

    Parameters:
    ===========
    :param file_graph: FileGraph to check.
    :param report_dirs: If given, report problems only for directories in this
        list.
    :param no_report_dirs: If given, report problems only for directories that
        are not in this list.
    '''

    for file in file_graph.headerfiles:
        if not in_dirs(
                file=file, inc_dirs=report_dirs, exc_dirs=no_report_dirs
        ):
            continue
        if file.endswith('_impl.hpp'):
            continue
        if not file.startswith('libs/'):
            continue
        guard = (
            file
            .replace('libs/', '')
            .replace('/', '_')
            .replace('.', '_')
            .upper()
        )
        if not file_contains(file=file, text=f'#define {guard}'):
            try:
                candidate = subprocess.check_output(
                    "rg --multiline --pcre2 "
                    r"'#ifndef (\S*)\n#define \1' "
                    r"--replace '$1' "
                    f'libs/{file}',
                    shell=True,
                    encoding='utf-8'
                ).splitlines()[0]
            except subprocess.CalledProcessError:
                candidate = None
            if candidate:
                warn(
                    f'File {file} has wrong header guard {candidate}. '
                    f'Change to {guard}.'
                )
            else:
                warn(
                    f'File {file} does not contain header guard {guard}. '
                    f'Add it.'
                )

Node = TypeVar('Node')
def topological_sort(
        *,
        nodes: List[Node] | None=None,
        edges: Callable[[Node], Iterator[Node]] | Dict[Node, Iterator[Node]]
) -> Tuple[List[Node], List[List[Node]]]:
    '''Combined topological sort and cycle detector in a directed graph.

    Parameters
    ==========
    :param nodes: A set of nodes.
    :param edges: One of the following:
    - A function that, given a node, returns an iterable over its successors.
    - A dict where keys are nodes and values are iterables over edges.

    If *edges* is a dict, then *nodes* may be omitted and is then taken as the
    key set of the dict.

    :return: A pair. If the graph is acyclic, the first item is one
    topological sort and the second item is an empty list. If the graph contains
    any cycle, the second item contains a list of cycles, each cycle represented
    as a list of nodes; and the first item is the topological sort of a modified
    graph, which is obtained from the original graph by removing the last edge
    of each cycle.
    '''

    if edges is None:
        raise TypeError(f'edges must not be None')
    if isinstance(edges, dict):
        if nodes is None:
            nodes = edges.keys()
        edge_func = lambda node: edges.get(node, [])
    else:
        if nodes is None:
            raise TypeError(f'nodes can only be None if edges is a dict')
        edge_func = edges
    # Algorithm: perform a depth first traversal.

    # When the traversal leaves a node after processing it, we know that all its
    # descendants have been visited. Therefore, we write the node to the output
    # array. In case there are no cycles, the resulting array is topologically
    # sorted.

    # The algorithm makes use of an auxiliary "mark". The mark is used to prune
    # the search and to detect cycles. Initially, every node is unmarked. Nodes
    # are marked False from the point the traversal enters the node until the
    # point the traversal leaves the node, and marked True after the traversal
    # has left the node.

    # When the traversal enters a node that already has the mark True, it means
    # we have just found another path to the same node, but not a cycle.
    # Therefore, we can prune the search, i.e., return immediately before
    # processing the node.  This ensures that every edge is traversed at most
    # once.

    # When the traversal enters a node that already has the mark False, it means
    # that there is a cycle. The cycle is from the point on the call stack where
    # that node is currently being visited, through the call stack to the top of
    # the stack where the same node appears again. Therefore, removing the edge
    # that we just traversed, that leads to the node, would break the cycle. If
    # all such edges (traversed by the algorithm when the target has the mark
    # set to False) are removed from the graph, the graph becomes acyclic.
    # Therefore, we record this cycle.

    # To record cycles, we reconstruct them as we return from function calls on
    # the stack. The return value is therefore a pair. The first component is a
    # set of partially constructed cycles (we call them tail), which we extend
    # by one node each time we return.  The second component is a set of
    # complete cycles. After the recursive call, we inspect the tails that were
    # returned: if any of them end with the current node, we have completed the
    # cycles and move those cycles to the list of complete cycles to return from
    # the current function invocation. For the other tails, we just extend them
    # by the current node and add them to the set of tails to return from the
    # current function invocation. To make it fast to lookup those tails that
    # end in the current node, we store the tails in a dict, indexed by the last
    # node on the tail.
    mark = {}
    topsort = []
    def traverse(node: Node) -> Tuple[
            Dict[Node, List[List[Node]]],
            List[List[Node]]
    ]:
        '''Visit a node in the graph.

        :return: A 2-tuple with the following components:
        - Dict[Node, List[List[Node]]]: set of incomplete circles. The key is
          the last item in a circle, each value is a tail of that circle
          starting in the current node.
        - List[List[Node]]: list of complete circles.
        '''

        nonlocal mark
        if node in mark:
            if mark[node]:
                return {}, []
            else:
                return {node: [[]]}, []
        mark[node] = False
        my_tails = {}
        my_circles = []
        for successor in edge_func(node):
            tails, circles = traverse(successor)
            my_circles.extend(circles)
            for c in tails.pop(node, []):
                c.append(node)
                c.append(c[0])
                c.reverse()
                my_circles.append(c[:-1])
            for n, tailset in tails.items():
                for t in tailset:
                    t.append(node)
                    my_tails.setdefault(n, []).append(t)
        mark[node] = True
        topsort.append(node)
        return my_tails, my_circles
    my_circles = []
    # Sorting is not necessary, but makes it list libraries without dependencies
    # to the right
    for node in sorted(nodes, key=lambda n: len(list(edge_func(n)))):
        tails, circles = traverse(node)
        assert not tails
        my_circles.extend(circles)
    return list(reversed(topsort)), sorted(my_circles)

'''
# Test cases for topological_sort.

print(topological_sort(None,dict(
    a='bcd', b='cda', c='dab', d='abc'
)))

print(topological_sort(None,dict(
    a='b', b='c', c='da', d='a'
)))
'''

def check_lib_inc_cycles(
        *,
        file_graph: FileGraph,
        report_dirs: List[str] | None,
        no_report_dirs: List[str] | None
) -> None:
    '''Check for cycles in library dependencies, determined by included files.

    Does the following:
    - Check for cases where a file in library A includes a header from library
      B, and a file in library B includes a header from library A. And more
      generally, checks for such cycles of any length.
    - Print any violations, and print one topologic sort of the library
      dependency graph.

    It is possible that this reports cycles that check_inc_cycles does not
    report, for example, if lib1/a.cc includes lib2/b.h, and lib2/c.cc includes
    liba/d.h.

    Parameters:
    ===========
    :param file_graph: FileGraph to check.
    :param report_dirs: If given, report problems only for directories in this
        list.
    :param no_report_dirs: If given, report problems only for directories that
        are not in this list.
    '''

    topsort, cycles = topological_sort(
        nodes=file_graph.libs,
        edges=lambda node: filter(
            lambda n: n != node and n != '?',
            file_graph.lib_libs[node]
        )
    )
    # Only include things that are not excluded by --[no-]report-dirs
    topsort = filter_in_dirs(
        file_list=topsort,
        inc_dirs=report_dirs,
        exc_dirs=no_report_dirs,
        key=lambda lib: f'libs/{lib}'
    )
    cycles = filter_any_in_dirs(
        file_list_list=cycles,
        inc_dirs=report_dirs,
        exc_dirs=no_report_dirs,
        key=lambda lib: f'libs/{lib}'
    )
    # For brevity, remove the mysql prefix
    topsort = apply_nested(
        obj=topsort, func=lambda n: n.removeprefix('mysql/')
    )
    cycles = apply_nested(obj=cycles, func=lambda n: n.removeprefix('mysql/'))
    # Print warnings.
    if cycles:
        warn(
            'Found the following dependency cycles among libraries, '
            'based on header inclusion:'
        )
        for cycle in cycles:
            print(' -> '.join(cycle + [cycle[0]]))
        warn(
            'Removing the first dependency in each of those cycles would '
            'make the dependency graph acyclic. Then, the libraries can be '
            'sorted as follows, with no dependency from right to left:'
        )
        print(' -> '.join(topsort))
    else:
        print(
            'The libraries do not have cyclic dependencies. They can be sorted '
            'as follows, with no dependency from right to left:'
        )
        print(' -> '.join(topsort))

def check_inc_cycles(
        *,
        file_graph: FileGraph,
        report_dirs: List[str] | None,
        no_report_dirs: List[str] | None
) -> None:
    '''Checks for cycles among included headers.

    Does the following:
    - Check for cases where header A includes header B and header B includes
      header A. And more generally, checks for such cycles of any length.
    - Print any violations, and print one topologic sort of the include graph.

    It is possible that this reports cycles that check_lib_inc_cycles does not
    report, in case the cycle is contained within one library.

    Parameters:
    ===========
    :param file_graph: FileGraph to check.
    :param report_dirs: If given, report problems only for directories in this
        list.
    :param no_report_dirs: If given, report problems only for directories that
        are not in this list.
    '''

    topsort, cycles = topological_sort(
        nodes=None,#file_graph.cppfiles,
        edges=file_graph.file_absinc
    )
    topsort = filter_in_dirs(
        file_list=topsort,
        inc_dirs=report_dirs,
        exc_dirs=no_report_dirs,
    )
    cycles = filter_any_in_dirs(
        file_list_list=cycles,
        inc_dirs=report_dirs,
        exc_dirs=no_report_dirs,
    )

    if cycles:
        warn('Found the following #include cycles:')
        for cycle in cycles:
            print(' -> '.join(cycle + [cycle[0]]))
    # The topologically sorted list of headers is too big to be useful.
    '''
        warn(
            'Removing the first dependency in each of those cycles would '
            'make the dependency graph acyclic. Then, the headers can be '
            'sorted as follows, with no dependency from right to left:'
        )
        print(' -> '.join(topsort))
    else:
        print(
            'The headers do not have cyclic dependencies. They can be sorted '
            'as follows, with no dependency from right to left:'
        )
        print(' -> '.join(topsort))
    '''

all_checks = dict(
    inc_path='Paths in #include directives should be relative to the libs '
    'directory and not contain ".." or "." or "libs".',
    header_usage='Header files should be included at least once.',
    inc_type='#include "..." should include local headers; '
    '#include <...> should include third party headers.',
    header_guards='Every header should have a header guard and it should '
    'follow the naming convention.',
    lib_inc_cycles='Libraries should not include headers from each other '
    'circularly.',
    inc_cycles='Headers should not include each other circularly.',
)

# Checks that require computing the full file graph
global_checks = {
    'header_usage', 'lib_inc_cycles', 'inc_cycles'
}

def check(
        *,
        file_graph: FileGraph,
        checks: List[str] | None=None,
        no_checks: List[str] | None=None,
        report_dirs: List[str] | None=None,
        no_report_dirs: List[str] | None=None
) -> None:
    '''Run the given checks for files in the given directories.

    Parameters:
    ===========
    :param file_graph: The FileGraph to check.
    :param checks: List of names of checks. Each element should be a key in the
        `all_checks` dict.
    :param no_checks: List of names of checks that should not execute.
    :param report_dirs: If given, report problems only for directories in this
        list.
    :param no_report_dirs: If given, report problems only for directories that
        are not in this list.
    '''

    if not checks:
        checks=all_checks.keys()
    for name in checks:
        if no_checks and name in no_checks:
            continue
        print(f'**** Checking {name} ****')
        func = globals()[f'check_{name}']
        func(
            file_graph=file_graph,
            report_dirs=report_dirs,
            no_report_dirs=no_report_dirs
        )

#### LIST FUNCTIONS ####

def list_lib(
        *,
        file_graph: FileGraph,
        filter: Callable[[str], bool]
) -> None:
    '''Print all library names.'''

    print('\n'.join([
        lib
        for lib in file_graph.libs
        if  not filter or filter(lib)
    ]))

def list_file_inc(
        *,
        file_graph: FileGraph,
        key_filter: Callable[[str], bool],
        include_system_headers: bool
) -> None:
    '''Print the name of each source file and the headers it includes.'''

    print(str_dict_of_list(
        dict_=file_graph.file_relinc,
        key_filter=key_filter,
        value_filter=(
            (lambda inc: inc)
            if include_system_headers else
            (
                lambda inc:
                None if inc in system_headers else inc
            )
        )
    ))

def list_lib_inc(
        *,
        file_graph: FileGraph,
        key_filter: Callable[[str], bool],
        include_system_headers: bool
):
    '''Print the name of each library, and the headers included from it.'''

    print(str_dict_of_list(
        dict_=file_graph.lib_absinc,
        key_filter=key_filter,
        value_filter=(
            (lambda inc: inc.removeprefix('!'))
            if include_system_headers else
            (
                lambda inc:
                None
                if inc.removeprefix('!') in system_headers else
                inc.removeprefix('!')
            )
        )
    ))

def list_lib_inc_dep(
        *,
        file_graph: FileGraph,
        key_filter: Callable[[str], bool]
) -> None:
    '''Print each library, and the libraries containing headers it includes.'''

    print(str_dict_of_list(
        dict_=file_graph.lib_libs,
        key_filter=key_filter,
    ))

#### MAIN ####

'''Default for *-include-path.

This is incomplete - it does not find all include paths for all files. Either
list all explicitly, or figure out how to parse it from the CMakeLists files.

The problem is visible when using --report-dir=''
'''
default_root_incpath = (
    '.:libs:include:sql:packaging/rpm-common:'
    'storage/ndb/src/kernel/vm:'
    + ':'.join(glob.glob('storage/ndb/include/*/')) + ':'
    'storage/ndb/src/kernel/error/:'
    #'storage/ndb/nodejs/jones-ndb/impl/include/common/:'
    + glob.glob('extra/boost/boost*')[0]
)
default_prefix_incpath = (
    '.:include:if'
)
default_leaf_incpath = (
    '.:include:if'
)
default_replace_incpath = (
    'src->include'
)
default_report_dir = (
    'libs'
)

'''Default for --no-report-dir. This is currently unused, but may be useful
if we change the default for --report-dir to include every directory.'''
default_no_report_dir = (
    # External libraries have their own conventions and we don't touch them.
    'extra:internal/extra:'
    # This directory contains files that are included by a generated file.
    'storage/ndb/include/kernel/signaldata'
)

def setup_argument_parser(
        parser: argparse.ArgumentParser=argparse.ArgumentParser(
            description=__doc__
        )
) -> argparse.ArgumentParser:
    '''Setup the ArgumentParser object to parse command line options.

    Parameters
    ----------
    :param parser: The argparse.ArgumentParser to which argument definitions
        should be added. Omit this to create a new parser.
    :return The argparse.ArgumentParser.
    '''

    group = parser.add_argument_group('Actions')
    mutex_group = group.add_mutually_exclusive_group()
    arg = mutex_group.add_argument
    arg('--list-lib', action='store_true',
        help='List libraries.')
    arg('--list-file-inc', action='store_true',
        help='For each file, list its includes.')
    arg('--list-lib-inc', action='store_true',
        help='For each library, list its includes.')
    arg('--list-lib-inc-dep', action='store_true',
        help='''For each library, list the libraries containg its includes. A
        "?" indicates tha the library includes something that is neither a
        standard library header nor in another library.''')
    arg('--run', action='store', nargs='*',
        metavar='NAME',
        help='''Check libraries. Without argument, performs all checks;
        with arguments, performs only the named checks. The following checks
        exist: '''
        + ' '.join([f'{name}: {desc}' for name, desc in all_checks.items()]))
    arg = group.add_argument
    arg('--no-run', nargs='*',
        help='Skip the given checks. This implies --run.')

    group = parser.add_argument_group('Paths')
    mutex_group = group.add_mutually_exclusive_group()
    arg = mutex_group.add_argument
    arg('--report-dir', metavar='DIR[:DIR...]',
        help='''Only warn for problems in these directories. If neither
        --no-report-dir nor --report-dir is given, the default is
        --report-dir='''
        + default_report_dir
        + '''. To warn in all directories, use --report-dir=. .''')
    arg('--no-report-dir', metavar='DIR[:DIR...]',
        help='Do not warn for problems in these directories.')
    arg = group.add_argument
    arg('--include-path', '--include-dir', metavar='DIR[:DIR...]',
        default=default_root_incpath,
        help='''Paths to look for included files in,
        relative to the project root. This is similar to gcc's -I flag.
        Default: '''
        + default_root_incpath)
    arg('--prefix-include-path', '--prefix-include-dir', metavar='DIR[:DIR...]',
        default=default_prefix_incpath,
        help='''For each prefix of a file's path, include the prefix plus each
        DIR to the file's include path (not counting the empty prefix and the
        full prefix). For example, with prefix-include-path=.:a:b, the file
        1/2/3/f.cc will have the include paths 1, 1/a, 1/b, 1/2, 1/2/a, 1/2/b.
        Default: '''
        + default_prefix_incpath)
    arg('--leaf-include-path', '--leaf-include-dir', metavar='DIR[:DIR...]',
        default=default_leaf_incpath,
        help='''Add any file's full directory plus each DIR to the file's
        include paths. For example, with leaf-include-path=.:a:b, the file
        1/2/3/f.cc will have the include paths 1/2/3, 1/2/3/a, 1/2/3/b.
        Default: '''
        + default_leaf_incpath)
    arg('--replace-include-path', metavar='D1->D2[:D1->D2...]',
        default=default_replace_incpath,
        help='''For any file whose path contains D1, add the path obtained by
        replacing D1 by D2 to the file's include path. Default: '''
        + default_replace_incpath)
    arg('--include-system-headers', action='store_true',
        help='Make --list-file-inc and --list-lib-inc print system headers.')
    return parser

def fix_arguments(
        *,
        arg_parser: argparse.ArgumentParser,
        opt: argparse.Namespace
) -> None:
    '''Validate command-line arguments after they have been parsed.

    Parameters
    ----------
    :param arg_parser: The argparse.ArgumentParser.
    :param opt: The argparse.Namespace object containing the parsed arguments.
    '''

    # Check
    if (
        opt.run is None
        and not opt.no_run
        and not opt.list_lib
        and not opt.list_file_inc
        and not opt.list_lib_inc
        and not opt.list_lib_inc_dep
    ):
        arg_parser.error(
            'One of the following arguments is required: '
            '--list-lib --list-file-inc --list-lib-inc --list-lib-lib --run '
            '--no-run'
        )

    # run == None means that we don't run at all.
    # run == [] means that we run in all directories.
    # When user does not pass an option, run defaults to None.
    # When user passes --run, it is set to [].
    # When user passes --no-run, we need to set run=[] to indicate that we run
    # all checks, and from that we subtract no_run.
    if opt.no_run is not None:
        if opt.run is None:
            opt.run = []
    if opt.run is not None:
        invalid_checks = [c for c in opt.run if c not in all_checks]
        if invalid_checks:
            arg_parser.error(
                'Invalid options to "check": ' + ', '.join(invalid_checks)
            )

    # Fix include paths
    opt.root_incpath = list(filter(None, opt.include_path.split(':')))
    opt.prefix_incpath = list(filter(None, opt.prefix_include_path.split(':')))
    opt.leaf_incpath = list(filter(None, opt.leaf_include_path.split(':')))
    opt.replace_incpath = {}
    for p in filter(None, opt.replace_include_path.split(':')):
        old, new = p.split('->', 1)
        opt.replace_incpath[old] = new

    # Fix [no_]warn dir
    if opt.report_dir is not None:
        opt.report_dir = opt.report_dir.split(':')
    elif opt.no_report_dir is not None:
        opt.no_report_dir = opt.no_report_dir.split(':')
    else:
        opt.report_dir = default_report_dir.split(':')

    # Compute graph_dir, i.e., the set of directories for which we compute the
    # include graph. This is a slow operation so we try to limit to only the
    # directories that are strictly needed.
    if opt.run is None or (
        opt.run and global_checks.isdisjoint(opt.run)
    ):
        # When we don't run (only use --list-* options), or when we run but only
        # perform checks that operate within the directories we check, compute
        # only the partial graph, based only on includes in the directories we
        # check.
        opt.graph_dir = opt.report_dir
    else:
        # When we run and perform checks that need to look outside the directory
        # we check (e.g., to transitively follow #include directives), set
        # graph_dir to empty, which translates to passing no directory to rg,
        # which then will search all directories.
        opt.graph_dir = []

    # Check that all directories exist, and normalize the paths
    for item in (
            'report_dir', 'no_report_dir',
            'root_incpath',
    ):
        path_list = getattr(opt, item)
        if path_list:
            nonexisting_paths = [
                p
                for p in path_list
                if not os.path.isdir(p)
            ]
            if nonexisting_paths:
                arg_parser.error(
                    f'Non-existing paths for "{item}": '
                    + ', '.join(nonexisting_paths)
                )
            path_list = [os.path.normpath(path) for path in path_list]
            setattr(opt, item, path_list)

def parse_arguments() -> argparse.Namespace:
    '''Parse and validate the command-line arguments; return the result.

    :returns: The argparse.Namespace object containing the parsed arguments.
    '''

    ap = setup_argument_parser()
    opt = ap.parse_args()
    fix_arguments(arg_parser=ap, opt=opt)
    return opt

def main() -> None:
    '''Parse command line arguments and do what we need to do.'''

    opt = parse_arguments()
    file_graph = FileGraph(
        graph_dirs=opt.graph_dir,
        root_incpaths=opt.root_incpath,
        prefix_incpaths=opt.prefix_incpath,
        leaf_incpaths=opt.leaf_incpath,
        replace_incpath=opt.replace_incpath,
    )
    lib_filter=lambda file: (
        file if in_dirs(
            file=('?' if file == '?' else os.path.join('libs', file)),
            inc_dirs=opt.report_dir, exc_dirs=opt.no_report_dir
        ) else
        None
    )
    if opt.list_lib:
        list_lib(file_graph=file_graph, filter=lib_filter)
    elif opt.list_file_inc:
        list_file_inc(
            file_graph=file_graph,
            key_filter=lambda file: (
                file
                if in_dirs(
                    file=file,
                    inc_dirs=opt.report_dir,
                    exc_dirs=opt.no_report_dir
                ) else
                None
            ),
            include_system_headers=opt.include_system_headers
        )
    elif opt.list_lib_inc:
        list_lib_inc(
            file_graph=file_graph,
            key_filter=lib_filter,
            include_system_headers=opt.include_system_headers
        )
    elif opt.list_lib_inc_dep:
        list_lib_inc_dep(
            file_graph=file_graph,
            key_filter=lib_filter
        )
    elif opt.run is not None:
        check(
            file_graph=file_graph,
            checks=opt.run,
            no_checks=opt.no_run,
            report_dirs=opt.report_dir,
            no_report_dirs=opt.no_report_dir
        )

if __name__ == '__main__':
    main()
