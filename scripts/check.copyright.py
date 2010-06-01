#!/usr/bin/python
#indent "Copyright (c) Tokutek Inc.  All rights reserved."

# check files for tokutek copyright.  exit 0 if all files have the copyright
# otherwise, exit 1

import sys
import os
import re

def checkcopyright(f, verbose):
    cmd = "egrep -q \"Copyright.*Tokutek\" " + f
    exitcode = os.system(cmd)
    if exitcode != 0 and verbose:
        print f
    return exitcode

def checkdir(d, verbose):
    nocopyright = 0
    for dirpath, dirnames, filenames in os.walk(d):
        for f in filenames:
            fullname = dirpath + "/" + f
            # skip svn metadata
            matches = re.match(".*\.svn.*", fullname)
            if matches is None:
                nocopyright += checkcopyright(fullname, verbose)
    return nocopyright

def usage():
    print "copyright.check [--help] [--verbose] DIRS"
    print " default DIRS is \".\""
    return 1

def main():
    verbose = 0
    nocopyright = 0
    dirname = "."
    try:
        if len(sys.argv) <= 1:
            nocopyright += checkdir(dirname, verbose)
        else:
            ndirs = 0
            for arg in sys.argv[1:]:
                if arg == "-h" or arg == "--help":
                    return usage()
                elif arg == "-v" or arg == "--verbose":
                    verbose += 1
                else:
                    ndirs += 1
                    dirname = arg
                    nocopyright += checkdir(dirname, verbose)
            if ndirs == 0:
                nocopyright += checkdir(dirname, verbose)
    except:
        return 1
    
    if nocopyright > 0: exitcode = 1
    else: exitcode = 0
    return exitcode

sys.exit(main())
