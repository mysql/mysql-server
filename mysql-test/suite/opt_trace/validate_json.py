#! /usr/bin/python

# Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; version 2 of the License.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

import sys

usage = """
This is from WL#5257 "first API for optimizer trace".

Usage:
  %s <a_file> <another_file> <etc>

It will verify that all optimizer traces of files (usually a_file
is a .result or .reject file which contains
SELECT * FROM OPTIMIZER_TRACE; ) are JSON-compliant, and that
they contain no duplicates keys.
Exit code is 0 if all ok.
""" % sys.argv[0]

if len(sys.argv) < 2:
    print usage
    sys.exit(1)

import json, re

trace_start_re = re.compile("^.*(\t)?{\n")
trace_end_re = re.compile("^}")

def check(trace, first_trace_line):
    global retcode
    s = "".join(trace)
    try:
        parsed = json.loads(s)
    except:
        print "parse error at line", first_trace_line
        error = str(sys.exc_info())
        print error
        # if there is a character position specified, put a mark ('&')
        # in front of this character
        matchobj = re.search(r"ValueError\('Invalid control character at: line \d+ column \d+ \(char (\d+)\)'", error)
        if matchobj:
            first_error_char = int(matchobj.group(1))
            print s[:first_error_char] + "&" + s[first_error_char:]
        else:
            print s
        retcode = 1
        print
        return
    # detect non-unique keys in one object, by counting
    # number of quote symbols ("'): the json module outputs only
    # one of the non-unique keys, making the number of " and '
    # smaller compared to the input string.
    before = s.count('"') + s.count("'")
    str_parsed = str(parsed)
    after = str_parsed.count('"') + str_parsed.count("'")
    if (before != after):
        print "non-unique keys at line %d (%d vs %d)" % (first_trace_line, before, after)
        print s
        retcode = 1
        print
        return
    print "ok at line", first_trace_line

def handle_one_file(name):
    all = open(name).readlines()
    first_trace_line = trace_line = 0
    trace = None
    for l in all:
        trace_line += 1
        if trace_start_re.match(l) and first_trace_line == 0:
            trace = []
            first_trace_line = trace_line
            trace.append("{\n")
            continue
        if trace_end_re.match(l):
            assert first_trace_line != 0
            trace.append("}") # eliminate any following columns of table (MISSING_PRIVILEGES etc)
            check(trace, first_trace_line)
            first_trace_line = 0
        if first_trace_line != 0:
            # eliminate /* */ from end_marker=on (not valid JSON)
            no_comment = re.sub("/\*.*\*/", "", l)
            trace.append(no_comment)

retcode=0
for f in sys.argv[1:]:
    print "FILE %s" % f
    print
    handle_one_file(f)
    print
sys.exit(retcode)
