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
Create a new C++ source or header file, or a new library.

Follow all the conventions for namespaces, header guards, Doxygen, and copyright
headers; maintain CMakeLists.txt.
'''

import cmakelists as cm
from   debug import trace
import file_edit
import file_name
import git
from   messages import *
import readme_warning
import template_processor as tp

import argparse
import os

#### Creating specific files ####

@trace
def create_source(path: str) -> None:
    '''
    Create a new source file.

    - Add its header file
    - Create the source file
    - Add it to the list of source files in CMakeLists.txt
    - Add code to build a library to CMakeLists.txt, if it's not already there.

    Parameters
    ----------
    :param path: The full path of the source file.
    '''

    if os.path.exists(path):
        log_error(f'File already exists: {path}')
    lib_path = cm.require_library_path(path)
    log_info(f"* New source file {path}")
    symbols = tp.file_symbols(path=path, lib_path=lib_path)
    tp.write_template(
        path=path, template='source.cpp.template', replacements=symbols
    )
    cm.insert_in_cmakelists(
        path=path,
        section='TARGET_SRCS'
    )
    header = f'libs/{symbols["my_header"]}'
    if not os.path.exists(header):
        create_header(header)

@trace
def create_header(path: str) -> None:
    '''
    Create a new header file.

    - Require that a containing library exists. It is one having a
      CMakeLists.txt that contains "SET(TARGET_HEADERS"
    - Create the header
    - Add it to the list of headers in CMakeLists.txt, unless its basename ends
      with _impl or it is located under an impl directory.

    Parameters
    ----------
    :param path: The full path of the header file.
    '''

    if os.path.exists(path):
        log_error(f'File already exists: {path}')
    lib_path = cm.require_library_path(path)
    log_info(f"* New header file {path}")
    symbols = tp.file_symbols(path=path, lib_path=lib_path)
    tp.write_template(
        path=path,
        template='header.h.template',
        replacements=symbols
    )
    if '_impl.h' not in path and '/impl/' not in path:
        cm.insert_in_cmakelists(
            path=path,
            section='TARGET_HEADERS'
        )

@trace
def create_library(path: str) -> None:
    '''Create a library.

    - Require that it does not exist, and is not contained in a library.
    - Create the paths
    - Create the CMakeLists.txt to build the library
    - Create the CMakeLists.txt in containing directories, and/or update if
      there is one.
    - Create readme.md.

    Parameters
    ----------
    :param path: Full path of the library's root directory.
    '''

    # Create all CMakeLists.txt in parent directories, and finally in the
    # library directory.
    if os.path.exists(path):
        log_error(f'Library directory exists already: {path}')
    cm.require_no_library_cmakelists(path)
    symbols = tp.file_symbols(path=path, lib_path=path)
    log_info(f'* New library {symbols["lib_filename"]}')
    cm.create_library_parents(path)
    cmakelists = os.path.join(path, file_name.cmakelists_name)
    tp.write_template(
        path=cmakelists,
        template=f'{file_name.cmakelists_name}.template',
        replacements=symbols
    )
    file_edit.append_to_file(
        path=cmakelists,
        text='''
SET(TARGET_HEADERS
)

SET(TARGET_SRCS
)
'''
    )
    tp.write_template(
        path=os.path.join(path, 'readme.md'),
        template='readme.md.template',
        replacements=symbols
    )
    file_edit.insert_in_file_section(
        path='libs/mysql/readme.md',
        text=symbols['doc_defgroup_directive'],
        start_regex=r'(?=\\defgroup GroupLibsMysql)',
        item_regex=r'\\defgroup GroupLibsMysql.*\n\\ingroup GroupLibsMysql\n',
        end_regex=r'(?!\\defgroup)'
    )
    readme_warning.warn_library_parent_readme(old=None, new=path)

@trace
def create(*, type: str, path: str) -> None:
    '''Create the given type of object at the given path.

    :param type: One of the strings "library", "header", or "source".
    :param path: Full path to the object to create.
    '''

    path = file_name.normalize_path(path)
    file_name.validate_path(path)
    file_type = file_name.file_type_by_extension(path)
    if os.path.exists(path):
        log_error(f'Target "{path}" exists already.')
    if type == "library":
        if file_type != file_name.Directory:
            log_error(f'Libraries should not have file extensions.')
        create_library(path)
    elif type == "header":
        if file_type != file_name.Header:
            log_error(f'File "{path}" does not have a header file extension.')
        create_header(path)
    elif type == "source":
        if file_type != file_name.Source:
            log_error(f'File "{path}" does not have a source file extension.')
        create_source(path)
    else:
        assert False, "notreached"

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
    :return The parser.
    '''

    arg = parser.add_argument
    arg('type', metavar='TYPE',
        choices=['source', 'header', 'library'],
        help=f'What to create: '
        f'"header" creates a C++ header file. '
        f'"source" creates a C++ source file and, unless a header exists, also '
        f'the corresponding header. '
        f'"library" creates a new library.')
    arg('path', metavar='PATH',
        help=f'Path to create. '
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
    '''Validate command-line arguments after they have been parsed

    Parameters
    ----------
    :param opt: The argparse.Namespace object containing the parsed arguments.
    '''

    if not opt.path:
        log_error('Empty path')

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

    :returns: A 2-tuple where the first component is the argparse.Namespace
        object containing the parsed arguments, and the second object is the
        list of unparsed arguments, which constitute the command to run.
    '''

    parser = setup_argument_parser()
    opt = parser.parse_args()
    fix_arguments(opt)
    return opt

def pre_check(opt: argparse.Namespace) -> None:
    '''Check that the git tree is clean.'''

    git.require_clean_git_workdir(
        force=opt.force, error_message='Use --force to run anyway.'
    )

def main() -> None:
    '''Run the whole tool.'''

    opt = parse_arguments()
    pre_check(opt)
    create(type=opt.type, path=opt.path)

if __name__ == '__main__':
    main()
