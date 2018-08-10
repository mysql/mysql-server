# Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

import csv
import itertools
import os
import shutil
import subprocess
import sys

from subprocess import check_output

BUILD_DIR = "BUILD"

def do_call(cmd):
    do_dot()
    check_output(cmd, stderr=subprocess.STDOUT, cwd=BUILD_DIR)

def do_dot():
    sys.stdout.write(".")
    sys.stdout.flush()

def cmake(options):
    cmd = ["cmake", ".."]
    cmd.extend("-D{0}={1}".format(k, v) for k,v in options.items())
    do_call(cmd)

def do_build(enable_tests, build_type, build_program, harness_name=None, with_gmock=None):
    args = { 'WITH_UNIT_TESTS': 'yes' if enable_tests else 'no' }
    if build_type:
        args['CMAKE_BUILD_TYPE'] = build_type
    if build_program:
        args['ENABLE_HARNESS_PROGRAM'] = 'yes'
    if harness_name:
        args['HARNESS_NAME'] = harness_name
    if with_gmock:
        args['WITH_GMOCK'] = with_gmock

    shutil.rmtree(BUILD_DIR, ignore_errors=True)
    os.mkdir(BUILD_DIR)
    do_dot()
    cmake(args)
    null = open(os.devnull, 'w')
    do_call(['make', '-j4'])
    if enable_tests:
        do_call(['make', 'test'])

def _pretty(val):
    if val is None:
        return ''
    if isinstance(val, bool):
        return 'Yes' if val else 'No'
    return str(val)

def write_table(columns, titles, rows):
    header = [titles[col] for col in columns]
    rows_with_header = [header]
    rows_with_header.extend(rows)
    width = [max(len(str(r)) for r in col) for col in zip(*rows_with_header)]
    def _line(row):
        return " ".join(
            "{:>{}}".format(x, width[i]) for i, x in enumerate(row)
        )

    lines = [_line(header)]
    lines.append(_line(['-' * w for w in width]))
    for row in rows:
        lines.append(_line(_pretty(c) for c in row))
    print "\n".join(lines)

def main():
    facets = [
        [False, True],              # Enable tests or not
        ['Debug', 'Release'],       # Build type
        [False, True],              # Build harness program
        [None, 'oddity'],           # Harness name to use
        [None, '/opt/gmock-1.7.0'], # GMock source code to use
    ]

    columns = ['test', 'btype', 'program', 'name', 'gmock', 'result']
    titles = {
        'test': 'Build Tests',
        'btype': 'Build Type',
        'program': 'Build Program',
        'name': 'Harness Name',
        'gmock': 'GMock Root',
        'result': 'Result'
    }
    rows = []
    sys.stdout.write("Building...")
    for testing, btype, build_program, name, gmock in itertools.product(*facets):
        result = {
            'test': testing,
            'btype': btype,
            'program': build_program,
            'name': name,
            'gmock': gmock
        }
        try:
            do_build(testing, btype, build_program, name, gmock)
            result['result'] = 'OK'
        except subprocess.CalledProcessError as err:
            result['result'] = 'FAILED'
            result['errors'] = err.output
        rows.append([result[c] for c in columns])
    sys.stdout.write("\n")

    # Print the summary table
    write_table(columns, titles, rows)

    # Print any errors that occured
    for row in rows:
        if 'errors' in row:
            for col in columns:
                print "{0}: {1}".format(title[col], row[col])
            print "Errors:"
            print row['errors']

    sys.exit(0)

if __name__ == '__main__':
    main()
