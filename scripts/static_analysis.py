#!/usr/bin/env python3
# Copyright (c) 2024, 2026, Oracle and/or its affiliates.
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

"""
Runs static analysis tool(s) on MySQL code base.
See static_analysis.md file for detailed usage instructions.
"""

import argparse
import sys
from static_analysis_inc import helpers
from static_analysis_inc import tool_clang_tidy

repo_root = ''


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="Runs static analysis tools on source tree or given commit.")
    parser.add_argument('-v', '--verbose',
                        dest='verbose', action='store_true',
                        help='more verbose output')
    parser.add_argument('-p', '--path',
                        dest='build_path', metavar="<PATH>",
                        help='build path, where the compile_commands.json is stored')
    parser.add_argument("--clang-tidy", metavar="<PATH>",
                        help="clang-tidy binary path")
    parser.add_argument("--clang-tidy-diff", metavar="<PATH>",
                        help="path to clang-tidy-diff script")
    parser.add_argument("-j", "--jobs", metavar="<THREADS>", type=int,
                        default=0,
                        help="number of parallel analysis jobs")
    parser.add_argument("--scan-root", metavar="<PATH>",
                        help="path to the source tree sub-directory, for partial tree scanning")
    parser.add_argument('--nowarn',
                        dest='nowarn', action='store_true',
                        help='do not warn on detecting unofficial clang-tidy version')

    group = parser.add_argument_group('Scan target')
    target_group = group.add_mutually_exclusive_group()
    target_group.add_argument('-t', '--tree',
                              dest='scan_tree', action='store_true',
                              help='scan an entire source code tree')
    # commit name/hash is optional, default=HEAD
    target_group.add_argument('-c', '--commit', nargs='?', const='HEAD',
                              dest='scan_commit', metavar='<COMMIT>',
                              help='scan a single git repository commit')

    global args
    args = parser.parse_args()

    helpers.VERBOSE = args.verbose

    # validate parameters
    if not args.scan_tree and not args.scan_commit:
        print("ERROR: You must provide scan target (--tree or --commit)!",
              file=sys.stderr)
        sys.exit(1)

    args.clang_tidy, args.clang_tidy_diff = helpers.pick_tidy_binary(
        args.clang_tidy,
        args.clang_tidy_diff,
        args.scan_tree,
        args.nowarn)

    global repo_root
    if args.scan_tree:
        # detect root folder
        repo_root = helpers.detect_source_root_dir()
        if not repo_root:
            print("ERROR: Could not detect source code tree root!",
                  file=sys.stderr)
            sys.exit(1)

    if args.jobs <= 0:
        args.jobs = int(helpers.get_cpu_count() / 3)
    helpers.verbose_log(f"Run analysis using {args.jobs} parallel jobs", True)

    if not args.build_path:
        args.build_path = helpers.find_build_path(repo_root)
        if not args.build_path:
            print("ERROR: Failed to detect build path!", file=sys.stderr)
            sys.exit(1)

    if args.scan_root and not args.scan_tree:
        print("ERROR: --scan-root can only be used with --tree target!",
              file=sys.stderr)
        sys.exit(1)


def run_static_analysis():
    if args.scan_tree:
        if not args.clang_tidy:
            print("ERROR: Could not find clang-tidy binary!", file=sys.stderr)
            sys.exit(1)
        tool_clang_tidy.scan_tree(args.clang_tidy, repo_root, args.jobs,
                                  args.build_path, args.scan_root)
    else:
        if not args.clang_tidy_diff:
            print("ERROR: Could not find clang-tidy-diff.py script!",
                  file=sys.stderr)
            sys.exit(1)
        tool_clang_tidy.scan_commit(args.clang_tidy_diff,
                                    args.scan_commit, args.jobs,
                                    args.build_path)


def main():
    parse_arguments()
    run_static_analysis()


if __name__ == '__main__':
    main()
