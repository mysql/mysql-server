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
Functions to generate a new file from a template
'''

import file_edit
#import cmakelists
import file_name
import file_system
import messages
from   string import convert_case, Lower, Upper, Camel

import datetime
import os
import os.path
from typing import Dict

#### Templates ####

def format_template(*, filename: str, replacements: Dict[str, str]) -> str:
    '''Replace {brace_enclosed} tokens in *filename*.

    This reads libs/scripts/impl/*filename*, skips all text up to and including
    '==== begin template ====\n', and uses str.format to replace
    {brace_enclosed} tokens, using substitutions in the dict *replacements*.

    :returns: the resulting string.
    '''

    full_file = file_edit.read_file(
        os.path.join('libs/scripts/impl', filename)
    )
    delimiter = '==== begin template ====\n'
    start_index = full_file.index(delimiter) + len(delimiter)
    return full_file[start_index:].format(**replacements)

def write_template(
        *, path: str, template: str, replacements: Dict[str, str]
) -> None:
    '''Replace {brace_enclosed} tokens in *template* and write to *path*.

    See format_template.
    '''

    file_system.makedirs(path=os.path.dirname(path))
    messages.log_info(f'Creating {path}')
    file_edit.overwrite_file(
        path=path,
        text=format_template(filename=template, replacements=replacements)
    )

#### Get dictionary of symbols for a given filename ####

def file_symbols(*, path: str, lib_path: str | None) -> Dict[str, str]:
    '''Return a dict of generated symbols related to the given file.

    The dict contains the following items:

    - year: The current year.
    - doc_groupdef: The parameters for the Doxygen directive @defgroup
    - doc_groupname: The Doxygen group name for the library.
    - namespace: The namespace for the file.
    - doc_lib_title: The English name of the library.
    - doc_lib_title_underscored: doc_lib_title, underscored
    - lib_filename: The filename of the library
    - header: Header filename. Exists only if path is a header
    - header_guard: The header guard in the header. Exists only if path is a
      header.

    Parameters
    ----------
    :param path: The path of the file.
    :param lib_path: The path of the library. Pass None to compute it and assert
        that it exists.
    '''

    assert path.startswith('libs/mysql/'), path

    rel_filename = path.removeprefix('libs/')

    if not lib_path:
        # cmakelists imports template_processor. Therefore we import cmakelists
        # only here during execution, to avoid cyclic imports.
        import cmakelists
        lib_path = cmakelists.require_library_path(path)
    libname_snakecase = lib_path.removeprefix(
        'libs/mysql/'
    ).replace('/', '_')  # 'a_b/c_d' -> 'a_b_c_d'
    libname_text = convert_case(text=libname_snakecase, case=Camel, glue=' ')
    libname_camelcase = convert_case(text=libname_snakecase, case=Camel)
    lib_filename=f'mysql_{libname_snakecase}'

    doc_lib_title = f'MySQL Library: {libname_text}'
    doc_lib_title_underscored = doc_lib_title + '\n' + '=' * len(doc_lib_title)
    doc_groupname = f'GroupLibsMysql{libname_camelcase}'
    doc_defgroup_directive = (
        f'\\defgroup {doc_groupname} {libname_text}\n'
        f'\\ingroup GroupLibsMysql\n'
    )
    doc_pagename = f'PageLibsMysql{libname_camelcase}'
    doc_page_directive = (
        f'\\page {doc_pagename} Library: {libname_text}'
    )

    dir_parts = rel_filename.split('/') # 'a_b/c_d' -> ['a_b', 'c_d']
    file_type = file_name.file_type_by_extension(dir_parts[-1])
    if file_type != file_name.Directory:
        dir_parts = dir_parts[:-1]
    namespace = convert_case(text=dir_parts, case=Lower, glue='::')

    include_guard = convert_case(text=rel_filename, case=Upper)

    symbols = dict(
        libname_text=libname_text,
        libname_snakecase=libname_snakecase,
        libname_camelcase=libname_camelcase,
        lib_filename=lib_filename,

        doc_groupname=doc_groupname,
        doc_lib_title_underscored=doc_lib_title_underscored,
        doc_defgroup_directive=doc_defgroup_directive,
        doc_page_directive=doc_page_directive,

        namespace=namespace,

        include_guard=include_guard,

        rel_filename=rel_filename,

        year=datetime.date.today().year,

        # The pre-push hook forbids pushing lines that begin with this text.
        # Templates may use it to reduce the risk that developers forget to fix
        # the line.
        do_not_check_in_this_line='DO_NOT_CHECK_IN_THIS_LINE',
    )
    if file_type == file_name.Source:
        symbols['my_header'] = file_name.source_to_headers(rel_filename)[0]
    return symbols
