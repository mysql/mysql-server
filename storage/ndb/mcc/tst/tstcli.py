#! /usr/bin/env python

import sys
import socket

def cmd_wrap(c, b):
    return '{ "head": { "seq": 0, "cmd": "'+c+'"}, "body": { "ssh": {}, '+b+'}, "reply": null }'

if __name__ == '__main__':
    jss = sys.stdin.read()
    cmd = cmd_wrap(sys.argv[3], jss)
    c = socket.create_connection((sys.argv[1], sys.argv[2]))
    req = 'POST foo HTTP/1.0\nFrom: foo@bar.com\nUser-Agent: tstcli.py\nContent-Type: application/json\nContent-Length: '+str(len(cmd))+'\n\n'+cmd
    print '<-- '+req
    try:
        c.sendall(req)
        f = c.makefile()
        print '--> '+f.read()
    finally:
        c.close()
