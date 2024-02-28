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
Framework to execute tests for the create and move scripts.
'''

import argparse
import contextlib
import hashlib
import os
import shlex
import subprocess
import tempfile
from typing import List, Set, Callable

#### HELPERS ####

class TestFailure(Exception):
    '''Exception class thrown when the test result differs from expectation.'''
    pass

def trim_whitespace(text_lines: List[str]) -> List[str]:
    '''Trim leading/trailing space in each string and remove empty elements.'''
    return list(filter(None, [
        s.strip() for s in text_lines
    ]))

def append_to_file(*, text, file):
    '''Append *text* to the end of *file*.'''
    with open(file, 'a') as fd:
        print(text, file=fd)

def cat_file(file):
    '''Print the contents of *file* to stdout.'''
    with open(file, 'r') as fd:
        for line in fd:
            print(line, end='')

'''Name of the log file.'''
LOG_FILENAME = 'log'

def log(text):
    '''Append to the log file.'''
    append_to_file(file=LOG_FILENAME, text=text)

def cmd(command, *args):
    '''Print and then execute the given command, write the output to the log.

    *command* will be shell-interpreted and may expand to a command followed by
    one or more arguments, whereas *args* will be passed literally.

    If the command fails, this throws subprocess.CalledProcessError.
    '''
    command_line = ' '.join([command] + [shlex.quote(arg) for arg in args])
    log(f'\n[{os.getcwd()}]\n$ {command_line}')
    subprocess.check_call(
        f'{command_line} >> {LOG_FILENAME} 2>&1',
        shell=True,
    )

MAX_SCENARIOS = 10000

#### FIXTURE MANAGER CLASS ####

class GitFixtureManager:
    '''Manages a pool of Fixtures, consisting of git repositories.

    A fixture is the environment in which a test executes. This class provides
    fixtures that have the form of a git repository created as follows: (1) copy
    libs/scripts and libs/mysql/CMakeLists.txt; (2) create a number of
    files/directories from a *fixture specification*. A fixture specification is
    a space-separated list of files. When this class creates a fixture, it
    generates those files using libs/scripts/create.

    Since many scenarios share the same fixture, this class caches the fixtures
    it generates, indexed by the sha1 of the fixture specification. This is much
    faster than generating a new fixture from scratch for each test scenario.

    This class creates a temporary directory under
    "libs/scripts/script_test_var". Under the temporary directory, it creates
    the subdirectory "fixtures" in which the cached fixtures are stored. Also,
    under the temporary directory, it creates directories whose names are
    numbers, each number corresponding to a scenario.

    The temporary directory is never removed. It is up to the user to remove it
    when needed. (The user may need to inspect it after this program
    terminates.)

    The format of a fixture spec is a space-separated list of filenames. This
    class generates a fixture from the spec by invoking `libs/scripts/create
    TYPE FILENAME` for each FILENAME, where TYPE is one of "library", "source",
    or "header", computed from the file extension.
    '''

    def __init__(self):
        '''Create the work directory and the base fixture.
        '''
        # Don't use libs/scripts/test/var, in order to avoid copying into itself
        # when copying `libs/scripts` recursively into the workdir.
        workdir_shared = 'libs/scripts_test_var'
        if not os.path.exists(workdir_shared):
            os.makedirs(workdir_shared)
        self.workdir_root = tempfile.mkdtemp(dir=workdir_shared)
        print(f'Created work directory "{self.workdir_root}".')
        self.workdir_fixtures = f'{self.workdir_root}/fixtures'
        self.workdir_fixtures_base = f'{self.workdir_fixtures}/base'
        os.makedirs(self.workdir_fixtures_base)
        with contextlib.chdir(self.workdir_fixtures_base):
            # Now in libs/script_test_var/tmpxxxx/fixtures/base
            parents = '../' * self.workdir_fixtures_base.count('/')
            cmd('git init')
            append_to_file(text=LOG_FILENAME, file='.gitignore')
            cmd('mkdir libs')
            cmd('mkdir libs/mysql')
            cmd(f'cp -r {parents}scripts libs')
            cmd(
                f'cp {parents}mysql/CMakeLists.txt',
                f'{parents}mysql/readme.md',
                'libs/mysql'
            )
            cmd('git add . .gitignore')
            cmd('git commit -m "Initial commit"')

    @staticmethod
    def split_fixture_spec(fixture_spec: str) -> List[str]:
        '''Split *fixture_spec* into a list of strings, trimming whitespace.'''
        return trim_whitespace(fixture_spec.split())

    @staticmethod
    def compute_fixture_commands(fixture_spec: str) -> List[str]:
        '''Compute a list of executable commands from *fixture_spec*'''
        type_by_extension = {'': 'library', '.h': 'header', '.cc': 'source'}
        return [
            f'create.sh {type_by_extension[os.path.splitext(name)[1]]} {name}'
            for name in GitFixtureManager.split_fixture_spec(fixture_spec)
        ]

    @staticmethod
    def compute_canonical_fixture(fixture_spec: str) -> str:
        '''Remove unnecessary whitespace from *fixture_spec*.'''
        return ' '.join(GitFixtureManager.split_fixture_spec(fixture_spec))

    @staticmethod
    def compute_fixture_hash(fixture_spec: str) -> str:
        '''Return the sha1 hash of *fixture_spec* in hex'''
        canonical_fixture = (
            GitFixtureManager.compute_canonical_fixture(fixture_spec)
        )
        return hashlib.sha1(canonical_fixture.encode()).hexdigest()

    def compute_fixture_dirname(self, fixture_spec: str) -> str:
        '''Return the dirname for the fixture generated by *fixture_spec*.'''
        if GitFixtureManager.compute_canonical_fixture(fixture_spec) == '':
            return self.workdir_fixtures_base
        return (
            f'{self.workdir_fixtures}/{self.compute_fixture_hash(fixture_spec)}'
        )

    def get_fixture_dir(self, fixture_spec: str) -> str:
        '''Generate the fixture for *fixture_spec* if it does not exist.

        Return the directory name containing the fixture.'''
        ret = self.compute_fixture_dirname(fixture_spec)
        if not os.path.exists(ret):
            cmd('cp', '-r', self.workdir_fixtures_base, ret)
            with contextlib.chdir(ret):
                for command in self.compute_fixture_commands(fixture_spec):
                    cmd(f'./libs/scripts/{command} --force')
                cmd('git add .')
                cmd(
                    'git', 'commit', '-m', 'Fixture '
                    + GitFixtureManager.compute_canonical_fixture(fixture_spec)
                )
        return ret

#### SCENARIO CLASS ####

class Scenario:
    '''Run a command and checks if files are changed as expected.
    '''

    def __init__(
            self, *,
            description: str,
            fixture: str,
            test: str | List[str],
            expected: str,
            expect_absent: str,
    ):
        '''Create a new scenario

        :param description: Human-readable description of the scenario.
        :param fixture: 'create' commands to run before executing the test
            command.
        :param test: The test command to execute. It is the effect of this
            command that we are checking. This may also be a list of commands,
            in which case each is executed.
        :param expected: The expected output when running `git status --short`
            after executing *command* followed by `git add .`. Any extra
            whitespace will be removed, and the lines sorted, before performing
            the comparison.
        '''
        self.description = description
        self.test = test
        self.fixture_spec = fixture
        if expected is None:
            self.expected = None
        else:
            self.expected = sorted(trim_whitespace(expected.splitlines()))
        self.expect_absent = expect_absent

    def setup(
            self, *,
            fixture_manager: GitFixtureManager,
            number: int,
    ) -> None:
        '''Set scenario number, setup fixture for the scenario, print header .

        Parameters
        ==========
        :param fixture_manager: GitFixtureManager which this function will use
            to get the fixture directory
        :param number The number of the scenario, used in messages to identify
            this scenario among other scenarios.
        '''

        self.number = number
        self._print_header()
        self.workdir = self._make_dir(fixture_manager=fixture_manager)

    def run(self, *, extra_arg: str):
        '''Run the scenario and check that the result meets expectations.

        You must call *setup* before calling this function.

        Parameters
        ==========
        :param extra_arg: Additional command-line arguments passed to the
            command (for example --debug).
        '''

        with contextlib.chdir(self.workdir):
            self._execute(extra_arg=extra_arg)
            self._check()

    def __str__(self):
        '''Return a string representation of this Scenario.'''
        fixture_text = GitFixtureManager.compute_canonical_fixture(
            self.fixture_spec
        )
        def opt_attr_str(member: str):
            return getattr(self, member) if hasattr(self, member) else 'unset'
        return (
            f"    Workdir: {opt_attr_str('workdir')}\n"
            f"     Number: {opt_attr_str('number')}\n"
            f"Description: {self.description}\n"
            f"    Fixture: {fixture_text}\n"
            f"       Test: {self.test}\n"
        )

    def _print_header(self) -> None:
        '''Print the header for the scenario.'''
        print(f'{self.number}. {self.description}:')
        if isinstance(self.test, list):
            test = '; '.join(self.test)
        else:
            test = self.test
        print(f'$ {test}')

    def _make_dir(self, fixture_manager: GitFixtureManager):
        '''Create the workdir for the scenario copy the fixture into it.'''
        scenario_workdir = f'{fixture_manager.workdir_root}/{self.number}'
        cmd(
            'cp',
            '-r',
            fixture_manager.get_fixture_dir(self.fixture_spec),
            scenario_workdir
        )
        return scenario_workdir

    def _execute(self, *, extra_arg: str) -> None:
        '''Execute the scenario.

        Parameters
        ==========
        :param extra_arg: Additional command-line arguments passed to the
            command (for example --debug).
        '''

        if isinstance(self.test, list):
            for test in self.test:
                self._execute_one(test=test, extra_arg=extra_arg)
        else:
            self._execute_one(test=self.test, extra_arg=extra_arg)

    def _execute_one(self, *, test: str, extra_arg: str) -> None:
        command = f'./libs/scripts/{test}'
        if extra_arg:
            command += f' {extra_arg}'
        if self.expected is None:
            self._execute_and_expect_error(command=command)
        else:
            cmd(command)

    def _execute_and_expect_error(self, *, command: str) -> None:
        '''Execute *command*, expecting that it will fail.'''
        have_exception = False
        try:
            cmd(command)
        except subprocess.CalledProcessError:
            have_exception = True
        if not have_exception:
            raise TestFailure(
                '\n'
                f'Expected failure from: "{command}", '
                f'but it succeeded.'
            )

    def _check(self) -> None:
        '''Check that `git add . && git status -s` prints what we expect.
        '''
        # Expect error. By the time we reach this point, we have already
        # verified that the error was generated.
        if self.expected is None:
            return

        if self.expect_absent:
            expect_absent_command = f'rg -i {self.expect_absent} libs/mysql'
            result = subprocess.run(
                expect_absent_command,
                shell=True,
                encoding='utf-8',
                capture_output=True
            )
            if result.stdout:
                raise TestFailure(
                    '\n'
                    f'The text "{self.expect_absent}" was found in the '
                    f'following places:\n'
                    f'{result.strout}\n'
                )
            elif result.stderr or result.returncode not in (0, 1):
                raise TestFailure(
                    '\n'
                    f'Error running "{expect_absent_command}":'
                    f'return code={result.returncode}\n'
                    f'stderr:\n{result.stderr}\n'
                )

        cmd('git add .')
        output = subprocess.check_output(
            'git status --short',
            shell=True,
            encoding='utf-8'
        ).splitlines()
        output = sorted([
            s[0] + s[2:].replace('libs/mysql/', '')# Drop tree status and prefix
            for s in output
        ])
        if output != self.expected:
            raise TestFailure(
                '\n'
                f"   Expected: {self.expected!r}\n"
                f"        Got: {output!r}\n"
            )

#### TESTER CLASS ####

class Tester:
    '''Manager for context kept while executing a sequence of scenarios.'''
    def __init__(
            self,
            *,
            run_steps: Set[int],
            verbose_steps: Set[int],
            debug_steps: Set[int],
    ):
        '''Create a new Tester with the given options

        Parameters
        ==========
        :param run_steps: The set of scenarios to execute.
        :param verbose_steps: The set of steps invoked with --verbose
        :param debug_steps: The set of steps invoked with --debug
        '''
        self.number = 1
        self.verbose_steps = verbose_steps
        self.debug_steps = debug_steps
        self.run_steps = run_steps
        self.fixture_manager = GitFixtureManager()

    def header(self, text: str) -> None:
        '''Print a header'''
        print(f'\n{text}')

    def scenario(self, scenario: Scenario) -> None:
        '''Execute *scenario* with configured options.

        This also prints debug info on failure.
        '''
        if self.number > MAX_SCENARIOS:
            raise ValueError('Too many scenarios')
        if self.number in self.run_steps:
            extra_arg_list = []
            if self.number in self.verbose_steps:
                extra_arg_list.append('--verbose')
            if self.number in self.debug_steps:
                extra_arg_list.append('--debug')

            try:
                scenario.setup(
                    fixture_manager=self.fixture_manager,
                    number=self.number
                )
            except:
                print(f'Error setting up scenario "{str(scenario)}"')
                raise

            try:
                scenario.run(extra_arg=' '.join(extra_arg_list))
            except:
                print('==== BEGIN LOG ====')
                cat_file(f'{scenario.workdir}/{LOG_FILENAME}')
                print('==== END LOG ====')
                print(f'Error running scenario "{str(scenario)}"')
                raise

        self.number += 1

#### COMMAND LINE ARGUMENTS ####

def parse_interval_set(text: str, min: int=1, max: int=1000) -> Set[int]:
    '''Parses a comma-separated list of intervals into a set of integers.

    The *text* should consist of a comma-separated list of intervals. Each
    interval should have one of the following formats, where N and M are
    integers:
    - N: a single integer
    - N-M: the interval from N to M, inclusive
    - N-: the interval from N to *max*, inclusive
    - -M: the interval from *min* to M, inclusive
    - -: the interval from *min* to *max*, inclusive
    '''
    ret = set()
    for interval in text.split(','):
        if '-' in interval:
            a, b = interval.split('-')
            a = int(a) if a else min
            b = int(b) if b else max
            if not (min <= a <= b <= max):
                raise ValueError(
                    f'Interval [{interval}] does not satisfy '
                    f'{min} <= x <= y <= {max}.'
                )
            for n in range(a, b + 1):
                ret.add(n)
        else:
            ret.add(int(interval))
    return ret

def setup_argument_parser(
        parser: argparse.ArgumentParser=argparse.ArgumentParser(__doc__)
) -> argparse.ArgumentParser:
    '''Configure the ArgumentParser to parse command line arguments.
    '''

    arg = parser.add_argument
    arg('--step', '--steps', action='store', metavar='N[-M][,N[-M]...]',
        default=None,
        help='Run only the steps with the given numbers.')
    arg('--verbose', action='store', nargs='?', metavar='N[-M][,N[-M]...]',
        default='',
        help='Pass --verbose to create/move. If an argument is given, applies '
        'only to the steps with the given numbers.')
    arg('--debug', action='store', nargs='?', metavar='N[-M][,N[-M]...]',
        default='',
        help='Pass --debug to create/move. If an argument is given, applies '
        'only to the steps with the given numbers.')
    return parser

def fix_arguments(
        parser: argparse.ArgumentParser,
        opt: argparse.Namespace
) -> None:
    '''Validate and post-process the parsed command-line arguments.

    :param parser: parser.ArgumentParser that was used to parse the options.
    :param opt: The options that were parsed.
    '''
    def step_set(option_value: str | None) -> Set[str]:
        '''Given an option value that takes an interval set, return a set().

        The option value, declared with
        argparse.add_arguments(..., nargs='?', default=''), will produce option
        value '' when the option is not given; None when the option is given
        without argument (--option); and the user string when the option is
        given with argument (--option=argument). This function will return a
        set of integers that is empty in the first case; equal to the set of
        scenario numbers in the second case; and equal to the set of integers
        in the third case.
        '''

        if option_value == '':
            return set()
        elif option_value is None:
            return set(range(1, MAX_SCENARIOS))
        else:
            return parse_interval_set(option_value, min=1, max=MAX_SCENARIOS)
    opt.run_steps = step_set(opt.step)
    opt.verbose_steps = step_set(opt.verbose)
    opt.debug_steps = step_set(opt.debug)

def parse_arguments() -> argparse.Namespace:
    '''Parse, validate, and post-process command-line arguments.'''

    parser = setup_argument_parser()
    opt = parser.parse_args()
    fix_arguments(parser, opt)
    return opt

def main(run_scenarios: Callable[[Tester], None]) -> None:
    '''Run the program.'''

    opt = parse_arguments()
    tester = Tester(
        run_steps=opt.run_steps,
        verbose_steps=opt.verbose_steps,
        debug_steps=opt.debug_steps
    )
    run_scenarios(tester)
