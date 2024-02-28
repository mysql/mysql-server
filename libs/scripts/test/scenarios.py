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

import test_framework

__doc__ = '''
Definitions of test scenarios for the create and move scripts in libs/scripts.
'''

# ==== REQUIREMENTS ====
#
# 1. Creating a library shall produce a new directory containing CMakeLists.txt
#    and readme.md, and update CMakeLists.txt in the parent directory.
# 2. Creating a source or header shall produce the new file(s) and update
#    CMakeLists.txt in the library root directory.
# 3. Moving a header, source, library, or directory shall update identifiers
#    referring to the old name in moved files, and in CMakeLists.txt on old and
#    new.
# 4. Error shall be emitted if the old and new file extensions do not match
#    or if any of them does not match the requested file type.
# 5. Error shall be emitted if the filenames don't follow the required format.
# 6. Error shall be emitted if new is in a subdirectory of old.
# 7. Moving a source/header/library/directory back and forth shall result in
#    exactly the original state.

# The following is not currently tested:
# - We don't test that libraries compile and build.
# - We don't test with real libraries, only with small ones created by the
#   scripts.
# - We don't test that namespaces and #include directives are updated throughout
#   the codebase.
# These things would be hard to test automatically, so we rely on manual testing
# instead.

def run_scenarios(tester: test_framework.Tester) -> None:
    '''Execute all the scenarios.

    This creates Scenario objects and passes them to the given Tester.'''

    def reverse_test(test: str) -> str:
        tokens = test.split()
        return ' '.join([tokens[0], tokens[1], tokens[3], tokens[2]])

    def s(*, d, f, t, e=None, r=False, ea=None):
        '''Shorthand for executing a scenario.

        As an exception, since it is limited to this function, and there are
        many calls to this function, we use one-letter abbreviations for the
        parameter names.

        Parameters:
        ===========
        :param d: Description.
        :param f: Fixture specification (see GitFixtureManager).
        :param t: Test (a single command).
        :param e: Expected result (see Scenario.__init__)
        '''
        tester.scenario(test_framework.Scenario(
            description=d, fixture=f, test=t, expected=e, expect_absent=ea
        ))
        if r:
            # Since it is easy, and proves we are not entirely off: run the
            # test first normally and then in reverse and expect that the result
            # is a no-op.
            tester.scenario(test_framework.Scenario(
                description=f'{d}, then reverse',
                fixture=f,
                test=[t, f'{reverse_test(t)} --force'],
                expected='',
                expect_absent=''
            ))

    def h(text):
        '''Shorthand for printing a header.'''
        tester.header(text)

    h('#### CREATING LIBRARIES ####')

    s(
        d='Create library in root',
        f='',
        t='create.sh library x',
        e='''
        M CMakeLists.txt
        M readme.md
        A x/CMakeLists.txt
        A x/readme.md
        '''
    ),
    s(
        d='Create library in new subdirectory',
        f='',
        t='create.sh library x/y',
        e='''
        M CMakeLists.txt
        M readme.md
        A x/CMakeLists.txt
        A x/y/CMakeLists.txt
        A x/y/readme.md
        ''',
    ),
    s(
        d='Create library in existing subdirectory',
        f='x/y',
        t='create.sh library x/z',
        e='''
        M readme.md
        M x/CMakeLists.txt
        A x/z/CMakeLists.txt
        A x/z/readme.md
        '''
    ),

    h('#### CREATING SOURCES/HEADERS ####')

    for location, desc in [
            ('', 'root'),
            ('d1/', 'existing subdirectory'),
            ('d2/d3/', 'new subdirectory')
    ]:
        for extension, file_type in [
                ('.cc', 'source'),
                ('.h', 'header'),
        ]:
            file = f'x/{location}a{extension}'
            expected = f'A x/{location}a.h\n'
            if file_type == 'source':
                expected += f'A x/{location}a.cc\n'
            expected += f'M x/CMakeLists.txt\n'
            s(
                d=f'Create {file_type} in {desc}',
                f='x',
                t=f'create.sh {file_type} x/{location}a{extension}',
                e=expected
            )

    h('#### MOVING LIBRARIES ####')

    for old_dir, old_desc in [
        ('', 'root'),
        ('x/', 'directory containing no other libraries'),
        ('y/', 'directory containing other libraries')
    ]:
        for new_dir, new_desc in [
            ('', 'root'),
            ('x/', 'new directory'),
            ('y/', 'existing directory')
        ]:
            if new_dir == old_dir:
                new_desc = 'same directory'
            expected = (
                f'M readme.md\n'
                f'R {old_dir}oldname/readme.md -> {new_dir}newname/readme.md\n'
                f'R {old_dir}oldname/CMakeLists.txt -> {new_dir}newname/CMakeLists.txt\n'
                f'R {old_dir}oldname/d/dummy.cc -> {new_dir}newname/d/dummy.cc\n'
                f'R {old_dir}oldname/d/dummy.h -> {new_dir}newname/d/dummy.h\n'
            )
            if new_dir == old_dir:
                expected += f'M {old_dir}CMakeLists.txt\n'
            else:
                old_mode = 'D' if old_dir == 'x/' else 'M'
                new_mode = 'A' if new_dir == 'x/' else 'M'
                expected += (
                    f'{old_mode} {old_dir}CMakeLists.txt\n'
                    f'{new_mode} {new_dir}CMakeLists.txt\n'
                )
                if 'x/' in (old_dir, new_dir) and '' not in (old_dir, new_dir):
                    expected += 'M CMakeLists.txt'
            s(
                d=f'Move library from {old_desc} to {new_desc}',
                f=f'{old_dir}oldname {old_dir}oldname/d/dummy.cc y/b',
                t=f'move.sh library {old_dir}oldname {new_dir}newname',
                e=expected,
                ea='oldname',
                r=True
            )

    h('#### MOVING FILES ####')

    for old_dir, old_dir_desc in [
            ('', 'root'),
            ('d1/', 'directory "d1"'),
            ('d1/d2/', 'directory "d1/d2"'),
    ]:
        for new_dir, new_dir_desc in [
                ('', 'root'),
                ('d1/', 'existing directory "d1"'),
                ('d1/d2/', 'existing directory "d1/d2"'),
                ('new/', 'new directory'),
        ]:
            for new_lib, new_lib_desc in [
                    ('x/', 'in the same library'),
                    ('y/z/', 'in another library'),
            ]:
                for ext, type in [
                        ('.h', 'header'),
                        ('.cc', 'source'),
                ]:
                    old = f'x/{old_dir}oldname{ext}'
                    new = f'{new_lib}{new_dir}newname{ext}'
                    expect = f'R {old} -> {new}'
                    if type == 'source':
                        expect += '\n' + expect.replace('.cc', '.h')
                    expect += '\nM x/CMakeLists.txt'
                    if new_lib_desc == 'in another library':
                        expect += '\nM y/z/CMakeLists.txt'
                    s(
                        d=(
                            f'Move {type} from {old_dir_desc} '
                            f'to {new_dir_desc} {new_lib_desc}'
                        ),
                        f=(
                            f'x x/d1/d2/dummy.h {old} '
                            f'y/z y/z/d1/d2/dummy.h'
                        ),
                        t=f'move.sh {type} {old} {new}',
                        e=expect,
                        ea='oldname',
                        r=True,
                    )

    h('#### MOVING DIRECTORIES ####')

    for old_dir, old_dir_desc in [
            ('', 'root'),
            ('/d1', 'directory "d1"'),
            ('/d1/d2', 'directory "d1/d2"'),
    ]:
        for subdir, old_desc  in [
                ('', 'flat directory'),
                ('/subdir', 'directory with subdirectories'),
        ]:
            for new_lib, new_lib_desc in [
                    ('x', 'in the same library'),
                    ('y/z', 'in another library'),
            ]:
                for new_dir, new_dir_desc in [
                        ('', 'root'),
                        ('/d1', 'existing directory "d1"'),
                        ('/d1/d2', 'existing directory "d1/d2"'),
                        ('/new', 'new directory'),
                ]:
                    old = f'x{old_dir}/oldname'
                    new = f'{new_lib}{new_dir}/newname'
                    expect = (
                        f'R {old}{subdir}/file.h '
                        f'-> {new}{subdir}/file.h\n'
                        f'R {old}{subdir}/file.cc '
                        f'-> {new}{subdir}/file.cc\n'
                    )
                    expect += f'M x/CMakeLists.txt\n'
                    if new_lib_desc == 'in another library':
                        expect += f'M y/z/CMakeLists.txt\n'
                    s(
                        d=(
                            f'Move {old_desc} from {old_dir_desc} '
                            f'to {new_dir_desc} {new_lib_desc}'
                        ),
                        f=(
                            f'x {old}{subdir}/file.cc '
                            f'y/z y/z/d1/d2/dummy.h'
                        ),
                        t=f'move.sh directory {old} {new}',
                        e=expect,
                        ea='oldname',
                        r=True,
                    )

    h('#### MOVING DIRECTORY CONTENTS ####')

    for old_dir, old_dir_desc in [
            ('', 'root'),
            ('/d1', 'directory "d1"'),
            ('/d1/d2', 'directory "d1/d2/"'),
    ]:
        for new_dir, new_dir_desc in [
                ('', 'root'),
                ('/d1', 'existing directory "d1"'),
                ('/d1/d2', 'existing directory "d1/d2"'),
        ]:
            for new_lib, new_lib_desc in [
                    ('x', 'in the same library'),
                    ('y/z', 'in another library'),
            ]:
                if (
                        # Don't move to subdirectory of itself
                        new_lib != 'x'
                        or not new_dir.startswith(old_dir)
                ):
                    old_prefix = f'x{old_dir}'
                    new_prefix = f'{new_lib}{new_dir}'
                    expect = ''
                    for file in ['/a.h', '/d1/b.h', '/d1/d2/c.h']:
                        if file.startswith(old_dir):
                            rel_file = file.removeprefix(old_dir)
                            expect += (
                                f'R {old_prefix}{rel_file} -> '
                                f'{new_prefix}{rel_file}\n'
                            )
                    expect += f'M x/CMakeLists.txt\n'
                    if new_lib == 'y/z':
                        expect += f'M y/z/CMakeLists.txt\n'
                    s(
                        d=(
                            f'Move directory contents from {old_dir_desc} '
                            f'to {new_dir_desc} {new_lib_desc}'
                        ),
                        f=(
                            'x '
                            'x/a.h '
                            'x/d1/b.h '
                            'x/d1/d2/c.h '
                            'y/z '
                            'y/z/d1/d2/dummy.h'
                        ),
                        t=(
                            f'move.sh directory-contents '
                            f'{old_prefix} {new_prefix}'
                        ),
                        e=expect
                    )

    h('#### ERROR CASES: INVALID FILENAME FORMAT ####')

    for file, desc in [
        ('', 'is empty'),
        (' ', 'is space'),
        ('A', 'is uppercase'),
        ('a b', 'contains space'),
        ('_', 'is underscore'),
        ('_a', 'begins with underscore'),
        ('a_', 'ends with underscore'),
        ('a__b', 'contains double underscore'),
        ('a//b', 'contains double slash'),
        ('0a', 'begins with number'),
        ('abcX', 'contains uppercase'),
        ('abc.def', 'contains dot')
    ]:
        for fixture, test in [
            ('', f'create.sh library "{file}"'),
            ('x', f'create.sh header "x/{file}"'),
            ('x', f'create.sh source "x/{file}"'),
            ('x x/a.h', f'move.sh header x/a.h "x/{file}.h"'),
            ('x x/a.cc', f'move.sh source x/a.cc "x/{file}.cc"'),
            ('x x/d1/a.h', f'move.sh directory x/d1 "x/{file}"'),
        ]:
            s(
                d=f'Filename {desc}',
                f=fixture,
                t=test,
                )

    h('#### ERROR CASES: MOVE TO SUBDIRECTORY OF ITSELF ####')

    s(
        d='Move library to subdirectory of itself',
        f='x',
        t='move.sh library x x/y',
        )
    s(
        d='Move directory to subdirectory of itself',
        f='x x/a/x.h',
        t='move.sh directory x/a x/a/b',
    )
    s(
        d='Move directory contents to subdirectory of itself',
        f='x x/a/x.h',
        t='move.sh directory-contents x/a x/a/b',
    )

    h('#### ERROR CASES: CREATING LIBRARIES IN OR FILES OUTSIDE LIBRARIES ####')

    s(
        d='Create library in library',
        f='x',
        t='create.sh library x/y',
    )
    s(
        d='Create library in subidrectory in library',
        f='x',
        t='create.sh library x/y/z',
    )
    for name, type in [
        ('a.h', 'header'),
        ('b.cc', 'source'),
        ('readme.md', 'readme'),
    ]:
        s(
            d=f'Create {type} outside library',
            f='',
            t=f'create.sh {type} {name}'
        )
    s(
        d='Create library in subidrectory in library',
        f='x',
        t='create.sh library x/y/z',
    )

    h('#### ERROR CASES: MISMATCHING FILE TYPES ####')

    old_dict = {
        'library': 'x',
        'header': 'x/a.h',
        'source': 'x/b.cc',
        'readme': 'x/readme.md',
        'directory': 'x/d1',
    }
    new_dict = {
        'library': 'y',
        'header': 'aa.h',
        'source': 'bb.cc',
        'readme': 'rr.md',
        'directory': 'x/dd',
        # Not directory-contents: it is allowed to move anything to an
        # existing directory.
    }
    for declared_type in old_dict.keys():
        for old_type, old_file in old_dict.items():
            for new_type, new_file in new_dict.items():
                if (
                        old_dict[declared_type] != old_dict[old_type]
                        or old_type != new_type
                ):
                    s(
                        d=f'Move {declared_type} from {old_type} to {new_type}',
                        f='x x/a.h x/b.cc x/d1/c.h x/d2/d.h',
                        t=f'move.sh {declared_type} {old_file} {new_file}',
                    )
