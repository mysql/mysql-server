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
Move libraries, directory, or C++ source/header files from one place to another.

While doing so, update namespaces, header guards, and Doxygen headers in moved
source/header files; maintain CMakeLists.txt and readme.md when needed; and
update #include directives and namespaces in the entire source tree.
'''

import cmakelists as cm
from   debug import trace
import file_edit
import file_name
import file_system
import git
from   messages import *
import readme_warning
import template_processor as tp

import argparse
import os
from   typing import List

@trace
def fix_cmakelists_after_moving_source_or_header(
        *, old: str, new: str, section: str
) -> None:
    '''Update CMakeLists.txt after moving a source or header file.

    If the file is listed in the old CMakeLists.txt, removes it from there and
    adds it in the new CMakeLists.txt.

    Parameters
    ----------
    :param old: Old filename
    :param new: New filename
    :param section: Either "TARGET_SRCS" or "TARGET_HEADERS"
    '''

    old_cmakelists = cm.require_library_cmakelists(old)
    new_cmakelists = cm.require_library_cmakelists(new)
    filename, found = cm.remove_from_cmakelists(
        path=old,
        section=section,
    )
    if found:
        cm.insert_in_cmakelists(
            path=new,
            section=section
        )
    else:
        log_warning(
            f'Did not find "{filename}" in "{old_cmakelists}". '
            f'Will not insert it in "{new_cmakelists}".'
        )

@trace
def fix_after_moving_header(
        *, old: str, new: str,
        old_lib_path: str | None=None, new_lib_path: str | None=None,
        fix_cmakelists: bool, fix_namespace: bool
) -> None:
    '''Fix header guards, namespaces, etc, after moving a header.

    This replaces header guards, namespace, and Doxygen group name in the
    header; and replaces the filename in all #include directories throughout the
    source tree.

    Parameters
    ----------
    :param old: The old filename
    :param new: The new filename
    :param old_lib_path: The path to the library of *old*. Pass None to compute
        it and require that it exists).
    :param new_lib_path: The path to the library of *new*. Pass None to compute
        it and require that it exists).
    :param fix_cmakelists: If True, will remove the file from CMakeLists.txt in
        the old location and add it on the new location.
    :param fix_namespace: If True, will replace the namespace in the header.
    '''

    if fix_cmakelists:
        fix_cmakelists_after_moving_source_or_header(
            old=old, new=new, section='TARGET_HEADERS'
        )
    old_symbols = tp.file_symbols(path=old, lib_path=old_lib_path)
    new_symbols = tp.file_symbols(path=new, lib_path=new_lib_path)
    keys = ['include_guard', 'doc_groupname']
    if fix_namespace:
        keys += ['namespace']
    file_edit.multi_replace_in_file_from_dict_with_warning(
        path=new, old_dict=old_symbols, new_dict=new_symbols,
        keys=keys,  message=True, words=True
    )
    log_info(
        f'Replacing {old_symbols["rel_filename"]!r} by '
        f'{new_symbols["rel_filename"]!r} in #include directives'
    )
    file_edit.replace_in_git_tree(
        old=f'#include "{old_symbols["rel_filename"]}"',
        new=f'#include "{new_symbols["rel_filename"]}"',
        words=False,
        file_patterns=file_edit.cpp_file_patterns
    )

@trace
def fix_after_moving_source(
        *, old: str, new: str,
        old_lib_path: str | None=None, new_lib_path: str | None=None,
        fix_cmakelists: bool
) -> None:
    '''Fix namespace after moving a source file.

    Parameters
    ----------
    :param old: The old filename
    :param new: The new filename
    :param old_lib_path: The path to the library of *old*. Pass None to compute
        it and require that it exists).
    :param new_lib_path: The path to the library of *new*. Pass None to compute
        it and require that it exists).
    :param fix_cmakelists: If True, will remove the file from CMakeLists.txt in
        the old location and add it on the new location.
    '''

    if fix_cmakelists:
        fix_cmakelists_after_moving_source_or_header(
            old=old, new=new, section='TARGET_SRCS'
        )
    old_symbols = tp.file_symbols(path=old, lib_path=old_lib_path)
    new_symbols = tp.file_symbols(path=new, lib_path=new_lib_path)
    file_edit.multi_replace_in_file_from_dict_with_warning(
        path=new, old_dict=old_symbols, new_dict=new_symbols,
        keys=['namespace'],
        message=True, words=True
    )

@trace
def fix_after_moving_readme(
        *, old: str, new: str,
        old_lib_path: str | None=None, new_lib_path: str | None=None
) -> None:
    '''Fix section headers, group names, page names, after moving a readme file

    Parameters
    ----------
    :param old: The old filename
    :param new: The new filename
    :param old_lib_path: The path to the library of *old*. Pass None to compute
        it and require that it exists).
    :param new_lib_path: The path to the library of *new*. Pass None to compute
        it and require that it exists).
    '''

    old_symbols = tp.file_symbols(path=old, lib_path=old_lib_path)
    new_symbols = tp.file_symbols(path=new, lib_path=new_lib_path)
    file_edit.multi_replace_in_file_from_dict_with_warning(
        path=new, old_dict=old_symbols, new_dict=new_symbols,
        keys=[
            #'libname_text', 'libname_snakecase', 'libname_camelcase',
            'lib_filename',
            'doc_groupname', 'doc_lib_title_underscored',
            'doc_page_directive',
            'namespace',
        ],
        no_warn=['namespace'],
        message=True, words=True
    )

@trace
def fix_after_moving_directory(
        *, old: str, new: str,
        old_lib_path: str | None=None, new_lib_path: str | None=None
) -> None:
    '''Fix namespaces in all files, after moving a directory.

    Parameters
    ----------
    :param old: The old directory name
    :param new: The new directory name
    :param old_lib_path: The path to the library of *old*. Pass None to compute
        it and require that it exists).
    :param new_lib_path: The path to the library of *new*. Pass None to compute
        it and require that it exists).
    '''

    old_symbols = tp.file_symbols(path=old, lib_path=old_lib_path)
    new_symbols = tp.file_symbols(path=new, lib_path=new_lib_path)
    log_info(
        f'Replacing {old_symbols["namespace"]!r} '
        f'by {new_symbols["namespace"]!r} in all C++ files'
    )
    file_edit.replace_in_git_tree(
        old=old_symbols['namespace'],
        new=new_symbols['namespace'],
        words=True,
        file_patterns=file_edit.cpp_file_patterns
    )

@trace
def fix_after_moving_cmakelists(
    *, old: str
) -> None:
    '''Remove *old* library from parent CMakeLists.txt after moving library.'''
    lib_dir = os.path.dirname(old)
    log_info(f'Removing {lib_dir} from parent CMakeLists.txt')
    cm.remove_library_from_parent(lib_dir)

@trace
def fix_after_moving_directory_tree(
        *,
        file_list: List[str] | None,
        old_root: str,
        new_root: str,
        old_lib_path: str,
        new_lib_path: str,
        fix_cmakelists: bool
):
    '''Fix all symbols in all files, after moving an entire directory tree.

    This invokes the appropriate fix_after_moving_* function on each file in
    *file_list*.

    Parameters
    ----------
    :param file_list: List of files to fix. Pass None to get read it from the
        file system at *new_root*.
    :param old_root: The root of the tree in the old location.
    :param new_root: The root of the tree in the new location.
    :param old_lib_path: The root directory of the library containing the old
        location.
    :param new_lib_path: The root directory of the library containing the new
        location.
    :param fix_cmakelists: If True, will remove source and header files from
        CMakeLists.txt at the old location, and add them to CMakeLists.txt at
        the new location. If False, will not do that. Usually, False should be
        used in case the CMakeLists.txt itself was moved.
    '''

    if file_list is None:
        file_list = file_system.walk_tree(root=new_root)
    unknown_type = ''
    for rel_filename in file_list:
        old_path = os.path.join(old_root, rel_filename)
        new_path = os.path.join(new_root, rel_filename)
        file_type = file_name.file_type_by_extension(rel_filename)
        args = dict(
            old=old_path,
            new=new_path,
            old_lib_path=old_lib_path,
            new_lib_path=new_lib_path
        )
        if file_type == file_name.Source:
            fix_after_moving_source(**args, fix_cmakelists=fix_cmakelists)
        elif file_type == file_name.Header:
            # Do not fix the namespace, since fix_after_moving_directory does
            # it for the entire tree.
            fix_after_moving_header(
                **args, fix_cmakelists=fix_cmakelists, fix_namespace=False
            )
        elif file_type == file_name.Markdown:
            fix_after_moving_readme(**args)
        elif file_type == file_name.Cmakelists:
            fix_after_moving_cmakelists(old=old_path)
        elif file_type == file_name.Directory:
            pass
        else:
            unknown_type += f'  {old_path} -> {new_path}\n'
    fix_after_moving_directory(
        old=old_root,
        new=new_root,
        old_lib_path=old_lib_path,
        new_lib_path=new_lib_path
    )
    if unknown_type:
        log_warning(
            f'The following files were moved, but their types could not be '
            f'determined:\n'
            f'{unknown_type}'
            f'You may want to check if they contain any symbols that need to '
            f'be moved.'
        )

@trace
def fix_after_moving_library(
        *,
        file_list: List[str] | None,
        old: str,
        new: str,
):
    fix_after_moving_directory_tree(
        file_list=file_list,
        old_root=old,
        new_root=new,
        old_lib_path=old,
        new_lib_path=new,
        fix_cmakelists=False
    )

    # The library name may occur in CMakeLists.txt files
    old_symbols = tp.file_symbols(path=old, lib_path=old)
    new_symbols = tp.file_symbols(path=new, lib_path=new)
    file_edit.replace_in_git_tree(
        old=old_symbols['lib_filename'],
        new=new_symbols['lib_filename'],
        words=True,
        file_patterns=file_edit.cmakelists_file_patterns
    )
    file_section_parameters = dict(
        path='libs/mysql/readme.md',
        start_regex=r'(?=\\defgroup GroupLibsMysql)',
        item_regex=r'\\defgroup GroupLibsMysql.*\n\\ingroup GroupLibsMysql\n',
        end_regex=r'(?!\\defgroup)'
    )
    before, after = file_edit.remove_from_file_section(
        text=old_symbols['doc_defgroup_directive'],
        **file_section_parameters
    )
    if before != after:
        file_edit.insert_in_file_section(
            text=new_symbols['doc_defgroup_directive'],
            **file_section_parameters
        )

    readme_warning.warn_library_parent_readme(
        old=old,
        new=new
    )

def warn_if_moved_source_from_library(*, old: str, new: str) -> None:
    '''Warn that moving a source may require changes in linking.

    Parameters:
    -----------
    :param old: Old source filename.
    :param new: New source filename.
    '''

    old_lib_path = cm.require_library_path(old)
    new_lib_path = cm.require_library_path(new)
    if old_lib_path != new_lib_path:
        log_warning(
            f'In case any remaining code in {old_lib_path} '
            f'depends on functionality that we moved to {new_lib_path}, '
            f'you now need to insert {new_lib_path} in the '
            f'SET(LINK_LIBRARIES ...) section in '
            f'{old_lib_path}/{file_name.cmakelists_name}'
        )

@trace
def move_file(*, old: str, new: str) -> None:
    '''Move *old* to *new*.

    This does not maintain CMakeLists.txt or anything else, just moves the file.

    Parameters
    ----------
    :param old: Old filename
    :param new: New filename
    '''

    if not os.path.isfile(old):
        log_error(f'Source {old} is not an existing file.')
    if os.path.exists(new):
        log_error(f'Target file {new} exists already.')

    file_system.makedirs(path=os.path.dirname(new))
    file_system.rename(old=old, new=new)

@trace
def move_header(*, old: str, new: str) -> None:
    '''Move a header file from *old* to *new*.

    - Target must be a non-existing header file in an existing directory within
      an existing library
    - Rename file
    - Fix all places that #include it
    - Fix header guard
    - Fix CMakeLists.txt
    - Fix namespace in this file but not in files that may use it.
    '''

    move_file(old=old, new=new)
    fix_after_moving_header(
        old=old, new=new, fix_cmakelists=True, fix_namespace=True
    )
    if os.path.dirname(old) != os.path.dirname(new):
        log_warning(
            f'If {old} contained a namespace, '
            f'the namespace was renamed in *this* file '
            f'when it was renamed to {new}, '
            f'but not updated in other files that use it. '
            f'You may need to edit any such files manually.'
        )

@trace
def move_source(*, old: str, new: str) -> None:
    '''Move a source file from *old* to *new*.

    - Target must be a non-existing source file in an existing directory within
      an existing library
    - Rename file, creating any missing sub-directories
    - Fix CMakeLists.txt

    Parameters
    ----------
    :param old: Old filename
    :param new: New filename
    :param old_lib_path: Path to library containing *old*. Omit it to compute
        it.
    '''

    move_file(old=old, new=new)
    fix_after_moving_source(old=old, new=new, fix_cmakelists=True)

@trace
def move_source_and_header(*, old: str, new: str) -> None:
    '''Move a source and its header from *old* to *new*.'''

    move_source(old=old, new=new)
    for old_header, new_header in zip(
            file_name.source_to_headers(old),
            file_name.source_to_headers(new)
    ):
        if os.path.exists(old_header):
            move_header(old=old_header, new=new_header)

@trace
def move_directory_contents(*, old: str, new: str) -> None:
    '''Move the contents of directory *old* to within directory *new*.

    - Target must be an existing directory within a library, and all files to be
      moved must be non-existent on new.
    - Move all files within the directory and its
      subdirectories, one by one.
    - Fix all namespaces, doc sections, header guards, etc.
    '''

    # Generate the file list by traversing the directory tree rooted at old,
    # before moving files, because there may be pre-existing files at new, which
    # should not be included in the file list.
    #
    # Do not yield directories, since they may or may not exist on *new*. And
    # they are auto-created on *new* and auto-purged on *old*, anyways.
    #
    # Exclude CMakeLists.txt since we need to preserve the one that exists on
    # *new*, and we will update it with the moved files anyways.
    #
    # Exclude readme.md and let user merge readme.md files manually.
    file_list = [
        file
        for file in file_system.walk_tree(root=old, yield_directories=False)
        if (
            file_name.file_type_by_extension(file)
            not in (file_name.Cmakelists, file_name.Markdown)
        )
    ]

    old_lib_path = cm.require_library_path(old)
    for rel_filename in file_list:
        old_path = os.path.join(old, rel_filename)
        new_path = os.path.join(new, rel_filename)
        move_file(old=old_path, new=new_path)
    new_lib_path = cm.require_library_path(new)

    fix_after_moving_directory_tree(
        file_list=file_list,
        old_root=old,
        new_root=new,
        old_lib_path=old_lib_path,
        new_lib_path=new_lib_path,
        fix_cmakelists=True
    )

@trace
def move_library(*, old: str, new: str) -> None:
    '''Move library *old* to *new*, updating all files accordingly.'''

    library_root = cm.require_library_path(old)
    if library_root != old:
        log_error(
            f'The command "move library OLD" requires that OLD is a library, '
            f'but {old} is a subdirect of the library "{library_root}".'
        )
    cm.require_no_library_cmakelists(new)
    cm.create_library_parents(new)
    file_list = list(file_system.walk_tree(root=old, yield_directories=False))
    file_system.rename(old=old, new=new)

    fix_after_moving_library(
        file_list=file_list,
        old=old,
        new=new,
    )

@trace
def move_directory(*, old: str, new: str) -> None:
    '''Move directory *old* to *new*, updating all files accordingly.'''

    cm.require_library_cmakelists(os.path.dirname(old))
    file_system.makedirs(path=new)
    move_directory_contents(old=old, new=new)

@trace
def move(*, type: str, old: str, new: str) -> None:
    '''Move *old*, which is of type *type*, to *new*.

    Parameters
    ----------
    :param old: The old file or directory name.
    :param new: The new file or directory name.
    :param type: One of the strings "directory-contents", "source", "header",
        "directory", or "library".
    '''

    old = file_name.normalize_path(old)
    new = file_name.normalize_path(new)
    file_name.validate_path(new)
    old_type = file_name.file_type_by_extension(old)
    new_type = file_name.file_type_by_extension(new)
    if new_type != old_type:
        log_error(
            f'OLD {old} and NEW {new} have filename extensions '
            f'of different types.'
        )
    if type == 'directory-contents':
        if not os.path.isdir(old):
            log_error(f'OLD "{old}" is not a directory.')
        if not os.path.isdir(new):
            log_error(f'NEW "{new}" is not a directory.')
    else:
        if os.path.isdir(new):
            new = os.path.join(new, os.path.basename(old))
        if os.path.exists(new):
            log_error(f'NEW "{new}" exists')
        if type in ('source', 'header', 'readme'):
            if not os.path.isfile(old):
                log_error(f'OLD "{old}" is not an existing file.')
            if type == 'source':
                if old_type != file_name.Source:
                    log_error(
                        f'OLD "{old}" does not have a '
                        f'C++ source file extension.'
                    )
            elif type == 'header':
                if old_type != file_name.Header:
                    log_error(
                        f'OLD "{old}" does not have a '
                        f'C++ header file extension.'
                    )
        else:
            if not os.path.isdir(old):
                log_error(f'OLD "{old}" is not an existing directory.')
            if not os.path.relpath(new, old).startswith('..'):
                log_error(f'NEW "{new}" is a subdirectory of OLD "{old}"')
    move_functions = {
        'source': move_source_and_header,
        'header': move_header,
        'directory-contents': move_directory_contents,
        'directory': move_directory,
        'library': move_library
    }
    move_functions[type](old=old, new=new)
    if type in ('source', 'directory-contents', 'directory'):
        warn_if_moved_source_from_library(old=old, new=new)

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

    arg = parser.add_argument
    arg('type', metavar='TYPE',
        choices=[
            'source', 'header', 'readme', 'directory-contents', 'directory',
            'library'
        ],
        help=f'What to move: "header" moves or renames a C++ header file, '
        f'updating the namespace, header guards and Doxygen section names, '
        f'updating other files that include the header, '
        f'and updating {file_name.cmakelists_name} both at the old and the new place. '
        f'It updates the namespace only in the moved file, '
        f'so you need to update it manually in other places it is used. '
        f'"source" moves or renames a C++ source file, '
        f'updating the namespace, '
        f'updating {file_name.cmakelists_name} at both the old and the new place, '
        f'and then moving its header as described before. '
        f'It only updates the namespace in the source and header file, '
        f'so you need to update it manually in other places it is used. '
        f'It deduces the header filename by replacing the extension. '
        f'"directory-contents" moves all files '
        f'within a directory into an existing directory, '
        f'making the necessary changes related to C++ files and readme files, '
        f'as described above. '
        f'It also updates all occurrences of the namespace throughout the '
        f'codebase. '
        f'"directory" moves or renames an entire directory, making the '
        f'necessary changes related to C++ files and readme.md files, '
        f'and updates the namespace throughout the codebase, '
        f'as described above. '
        f'"library" moves or renames an entire library, making the '
        f'necessary changes related to C++ files and readme.md files, '
        f'and updates the namespace throughout the codebase, '
        f'as described above.')
    arg('old', metavar='OLD',
        help='Move this file or directory. '
        f'It must exist. '
        f'The "./libs/mysql/" prefix is not required.')
    arg('new', metavar='NEW',
        help=f'The new file or directory. '
        f'When type is source, header, directory, or library, '
        f'this path or file must not exist. '
        f'When type is directory-contents, this path must exist. '
        f'The "./libs/mysql/" prefix is not required.')
    arg('--force', action='store_true',
        help='Run even if the git status is unclean.')

    gra = parser.add_mutually_exclusive_group().add_argument
    gra('--trace', action='store_true',
        help='Print significant function calls, and debug information if any.')
    gra('--debug', action='store_true',
        help='Print debug information if any.')
    gra('--verbose', '-v', action='store_true',
        help='Print main file changes this script does.')
    gra('--quiet', action='store_true',
        help='Suppress warnings.')

    return parser

def fix_arguments(opt: argparse.Namespace) -> None:
    '''Validate command-line arguments after they have been parsed.

    Parameters
    ----------
    :param opt: The argparse.Namespace object containing the parsed arguments.
    '''

    if not opt.old:
        log_error('OLD is an empty string')
    if not opt.new:
        log_error('NEW is an empty string')

    if opt.trace:
        set_severity_bound(severity_trace)
    elif opt.debug:
        set_severity_bound(severity_debug)
    elif opt.verbose:
        set_severity_bound(severity_info)
    elif opt.quiet:
        set_severity_bound(severity_error)
    else:
        set_severity_bound(severity_warning)

def parse_arguments() -> argparse.Namespace:
    '''Parse and validate the command-line arguments; return the result.

    :returns: The argparse.Namespace object containing the parsed arguments.
    '''

    parser = setup_argument_parser()
    opt = parser.parse_args()
    fix_arguments(opt)
    return opt

def pre_check(opt: argparse.Namespace) -> None:
    '''Verify that the git worktree and index are clean.'''

    git.require_clean_git_workdir(
        force=opt.force, error_message='Use --force to run anyway.'
    )

def main() -> None:
    '''Run the whole tool.'''

    opt = parse_arguments()
    pre_check(opt)
    move(type=opt.type, old=opt.old, new=opt.new)

if __name__ == '__main__':
    main()
