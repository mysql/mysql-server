#!/usr/local/bin/python2.6

import sys
import os
import pexpect
import getpass

#
#  remote_cmd
#

nameaddr='admn@192.168.1.254'
passwd='admn'

def IPM_cmd(cmds):
    # password handling
    ssh_newkey = 'Are you sure you want to continue connecting'
    p=pexpect.spawn('ssh %s' % nameaddr, timeout=60)
    i=p.expect([ssh_newkey,'Password:',pexpect.EOF])
    if i==0:
        p.sendline('yes')
        i=p.expect([ssh_newkey,'Password:',pexpect.EOF])
    if i==1:
        p.sendline(passwd)
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

def main(argv):
#    passwd = getpass.getpass('password for %s:' % (nameaddr))
    if argv[1] == 'on':
        IPM_power_on()
    elif argv[1] == 'off':
        IPM_power_off()
    else:
        IPM_cmd(argv[1:])
    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))
