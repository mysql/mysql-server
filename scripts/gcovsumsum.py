#! /usr/bin/env python

import sys
import re
import os.path

verbose = 0

def percent(a, b):
    return (float(a)/float(b))*100

def sumarize():
    fname = None
    cfiles = {}
    while 1:
        b = sys.stdin.readline()
        if b == "":
            break
        b = b.rstrip('\n')
        if verbose: print ">", b
        match = re.match("^(\S*)$", b)
        if match:
            maybe = match.group(1)
            # skip test files
            match = re.match(".*test|keyrange-|bug|shortcut|manyfiles", maybe)
            if match == None:
                fname = os.path.basename(maybe)
                # print fname
            else:
                fname = None
        else:
            match = re.match("\t(.*) (.*)\/(.*) (.*)%", b)
            if match and fname:
                # print b
                type = match.group(1)
                m = int(match.group(2))
                n = int(match.group(3))
                if cfiles.has_key(fname):
                    h = cfiles[fname]
                else:
                    h = {}
                    cfiles[fname] = h
                if h.has_key(type):
                    (om,on) = h[type]
                    if percent(m,n) > percent(om,on):
                        h[type] = (m,n)
                else:
                    h[type] = (m,n)

    summary = {}
    keys = cfiles.keys(); keys.sort()
    for k in keys:
        h = cfiles[k]
        print k
        ktypes = h.keys(); ktypes.sort()
        for t in ktypes:
            (m,n) = h[t]
            print "\t%s %d/%d %.2f%%" % (t, m, n, percent(m,n))
            if summary.has_key(t):
                (om,on) = summary[t]; summary[t] = (om+m,on+n)
            else:
                summary[t] = (m,n)
    print
    printhash("Summary", summary)

def printhash(s, h):
    print s
    ktypes = h.keys(); ktypes.sort()
    for t in ktypes:
        (m,n) = h[t]
        print "\t%s %d/%d %.2f%%" % (t, m, n, percent(m,n))

def main():
    global verbose
    for arg in sys.argv[1:]:
        if arg == "-v" or arg == "--verbose":
            verbose = 1
    sumarize()

sys.exit(main())
