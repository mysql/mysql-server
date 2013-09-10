#!/usr/local/bin/python2.6
#
# Copyright (C) 2009 Tokutek, Inc.
#

import sys
import os
import optparse
import getpass
import pexpect
import time
from multiprocessing import Process

# options
parser = optparse.OptionParser()
parser.add_option('--test', dest='test', type='string', default=None, help="name of stress test to run")
parser.add_option('--iterations', dest='iterations', type='int', default=1, help="Number of test iterations (default = 1)")
parser.add_option('--verbose', dest='verbose', action="store_true", default=False, help="Verbose printing (default = FALSE)")
parser.add_option('--client', dest='client', type='string', default='192.168.1.107', help='client machine being power failed (default=tick)')
parser.add_option('--sandbox_dir', dest='sbdir', type='string', default='/tpch/mysb/msb_3.0.2-beta_16948/', help='sandbox directory (default = None)')
options,remainder = parser.parse_args()

nameaddr='mysql@'+options.client
password='mytokudb'

ipm_nameaddr='admn@192.168.1.254'
ipm_passwd='admn'

def IPM_cmd(cmds):
    # password handling
    ssh_newkey = 'Are you sure you want to continue connecting'
    p=pexpect.spawn('ssh %s' % ipm_nameaddr, timeout=60)
    i=p.expect([ssh_newkey,'Password:',pexpect.EOF])
    if i==0:
        p.sendline('yes')
        i=p.expect([ssh_newkey,'Password:',pexpect.EOF])
    if i==1:
        p.sendline(ipm_passwd)
    elif i==2:
        print "I either got key or connection timeout"
        pass

    # run command(s)
    i = p.expect('Sentry:')
    for cmd in cmds:
        if i==0:
            p.sendline(cmd)
        else:
            print 'p.expect saw', p.before
        i = p.expect('Sentry:')
        print p.before

    # close session
    p.sendline('quit')
    p.expect(pexpect.EOF)
    return 0

def IPM_power_on():
    IPM_cmd(['on all'])

def IPM_power_off():
    IPM_cmd(['off all'])

def ssh_cmd(cmd, verbose=True, timeout=30):

  ssh_newkey = 'Are you sure you want to continue connecting'
  p=pexpect.spawn('ssh %s %s' % (nameaddr, cmd), timeout=timeout)

  i=p.expect([ssh_newkey,'password:',pexpect.EOF])
  if i==0:
    p.sendline('yes')
    i=p.expect([ssh_newkey,'password:',pexpect.EOF])
  if i==1:
    if verbose:
      print 'ssh %s %s' % (nameaddr, cmd)
    p.sendline(password)
    p.expect(pexpect.EOF)
  elif i==2:
    print "I either got key or connection timeout"
    pass
  if verbose: 
    print p.before 
  return p.before

def client_cmd(cmd, verbose=True, timeout=3600):
    ssh_cmd(cmd, verbose, timeout)

def ping_server(name):
    p=pexpect.spawn("ping -c 1 "+name)
    i=p.expect(['1 packets transmitted, 0 received, +1 errors, 100% packet loss, time 0ms',
                '1 packets transmitted, 1 received, 0% packet loss, time 0ms',
                pexpect.EOF])
    return i


def test_it():
    cmd = "/home/wells/svn/iibench/py/iibench.py --db_config_file=%smy.sandbox.cnf --max_rows=1000000000 --engine=tokudb --outfile=/tmp/pf_%d" % (options.sbdir, options.iterations)
    print "CMD = ", cmd
    client_cmd(cmd, timeout=3600)

def run_test():
#    cmd = options.test
#    if ( options.verbose ): cmd += ' -v'
#    for i in range(options.iterations):

    t0 = Process(target=test_it, args=())
    for iter in range(options.iterations + 1):
        print "Turn On Power to Server"
        IPM_power_on()
        i = ping_server(options.client)
        while ( i != 1 ):
            i = ping_server(options.client)
        print "Server rebooted, wait 30 seconds to restart MySQL"
        time.sleep(30)
        print "Start MySQL"
        client_cmd(options.sbdir+'stop')  # clears out flags from previous start
        client_cmd(options.sbdir+'start')
        if iter < options.iterations:
            print "Run Test"
            t0.start()
            print "Sleep(%d)" % (300 + iter)
            time.sleep(300 + iter)
            print "Turn Off Power to Server"
            IPM_power_off()
            t0.terminate()
        else:
            # last loop through, just cleanup
            client_cmd(options.sbdir+'stop')
        
def main(argv):
    run_test()
    return 0

if __name__ == '__main__':
    usage = sys.modules["__main__"].__doc__
    parser.set_usage(usage)
    unused_flags, new_argv = parser.parse_args(args=sys.argv[1:], values=options)
    sys.exit(main([sys.argv[0]] + new_argv))
    
