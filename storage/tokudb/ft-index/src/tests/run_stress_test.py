#!/usr/local/bin/python2.6
#
# Copyright (C) 2009 Tokutek, Inc.
#

import sys
import os
import optparse

# options
parser = optparse.OptionParser()
parser.add_option('--test', dest='test', type='string', default=None, help="name of stress test to run")
parser.add_option('--iterations', dest='iterations', type='int', default=1, help="Number of test iterations (default = 1)")
parser.add_option('--verbose', dest='verbose', action="store_true", default=False, help="Verbose printing (default = FALSE)")
options, remainder = parser.parse_args()

def run_test():
    cmd = options.test
    if ( options.verbose ): cmd += ' -v'
    for i in range(options.iterations):
        os.system(cmd + ' -i %d' % (i))


def main(argv):
    run_test()
    return 0

if __name__ == '__main__':
    usage = sys.modules["__main__"].__doc__
    parser.set_usage(usage)
    unused_flags, new_argv = parser.parse_args(args=sys.argv[1:], values=options)
    sys.exit(main([sys.argv[0]] + new_argv))
    
