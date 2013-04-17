#! /usr/bin/env python
# summarize the code coverage of a collection of named files

import re
import os
import sys
import math
import glob

verbose = 0
dobranches = 0
dofunctions = 0
functions = {}
lines = {}
branches = {}
taken = {}
calls = {}

def add(coverage, fname, m, n):
    global verbose
    addit = 0
    if coverage.has_key(fname):
        (oldm, oldn) = coverage[fname]
        if verbose: print "old", oldm, oldn, m, n
        if m > oldm:
            addit = 1
    else:
        addit = 1
    if addit:
        if verbose: print "add", fname, m, n
        coverage[fname] = (m, n)
    return addit

def add_match(coverage, fname, match):
    percent = float(match.group(1))
    n = int(match.group(2))
    m = int(math.ceil((float(percent)/100)*n))
    return add(coverage, fname, m, n)
    
def gcov(gcovargs, fnames):
    global verbose
    if verbose: print "gcov ", gcovargs, fnames
    f = os.popen("gcov " + gcovargs + " " + fnames)
    fname = ""
    while 1:
        b = f.readline()
        if b == "": break
        if verbose: print ">", b
        match = re.match("File \'(.*)\'", b)
        if match:
            fname = match.group(1)
            coverage = lines
        else:
            match = re.match("Function \'(.*)\'", b)
            if match:
                fname = match.group(1)
                coverage = functions
        if fname[0] == '/' or not os.path.exists(fname):
            continue
        match = re.match("Lines executed:(.*)% of (.*)", b)
        if match:
            if add_match(coverage, fname, match) and coverage == lines:
                cpcmd = "cp " + fname + ".gcov" + " " + fname + ".gcov.best"
                if verbose: print "system", cpcmd
                os.system(cpcmd)
            continue
        match = re.match("Branches executed:(.*)% of (.*)", b)
        if match:
            add_match(branches, fname, match)
            continue
        match = re.match("Taken.*:(.*)% of (.*)", b)
        if match:
            add_match(taken, fname, match)
            continue
        match = re.match("Calls executed:(.*)% of (.*)", b)
        if match:
            add_match(calls, fname, match)
            continue

def usage():
    print "gcovsummary.py [-h] [-v] [-b] [-f] FILENAMES"
    return 1

def percent(m, n):
    return (float(m)/float(n))*100

def main():
    global verbose, dobranches, dofunctions
    # coverage hashes filenames -> (lines_executes, total_lines) tuples
    gcovargs = ""
    threshold = 1

    i = 1
    while i < len(sys.argv):
        arg = sys.argv[i]
        if arg == "-h" or arg == "--help":
            return usage()
        elif arg == "-v" or arg == "--verbose":
            verbose = 1
        elif arg == "-b":
            dobranches = 1
            gcovargs = gcovargs + " " + arg
        elif arg == "-f":
            dofunctions = 1
            gcovargs = gcovargs + " " + arg
        elif arg == "--threshold":
            if i+1 < len(sys.argv):
                i += 1
                threshold = float(sys.argv[i])
        else:
            gcov(gcovargs, arg)
        i += 1

    # print a coverage summary
    if len(functions) > 0:
        fnames = functions.keys()
        fnames.sort()
        for fname in fnames:
            (m,n) = functions[fname]
            if float(m)/float(n) <= threshold:
                print "%s %d/%d %.2f%%" % (fname, m, n, percent(m, n))
    else:
        fnames = lines.keys()
        fnames.sort()
        for fname in fnames:
            (m,n) = lines[fname]
            if float(m)/float(n) > threshold:
                continue
            print "%s" % fname
            print "\t%s %d/%d %.2f%%" % ("Lines", m, n, percent(m, n))
            if branches.has_key(fname):
                (m,n) = branches[fname]
                print "\t%s %d/%d %.2f%%" % ("Branches", m, n, percent(m, n))
            if taken.has_key(fname):
                (m,n) = taken[fname]
                print "\t%s %d/%d %.2f%%" % ("Taken", m, n, percent(m, n))
            if calls.has_key(fname):
                (m,n) = calls[fname]
                print "\t%s %d/%d %.2f%%" % ("Calls", m, n, percent(m, n))

    # rename the best gcov files
    fnames = glob.glob("*.best")
    for fname in fnames:
        if verbose: print "test", fname
        match = re.match("(.*)\.gcov\.best", fname)
        if match != None:
            mvcmd = "mv " + fname + " " + match.group(1) + ".gcov"
            if verbose: print "system", mvcmd
            os.system(mvcmd)
    return 0

sys.exit(main())
