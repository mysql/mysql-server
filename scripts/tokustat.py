#!/usr/bin/env python

import sys
import time
import re
import MySQLdb

def usage():
    print "diff the tokudb engine status"
    print "--host=HOSTNAME (default: localhost)"
    print "--port=PORT"
    print "--sleeptime=SLEEPTIME (default: 10 seconds)"
    print "--q='show engine tokudb status'"
    print "--q='select * from information_schema.global_status'"

    return 1

def convert(v):
    if type(v) == type('str'):
        try:
            v = int(v)
        except:
            v = float(v)
    return v

def printit(stats, rs, sleeptime):
    # print rs
    for t in rs:
        l = len(t) # grab the last 2 fields in t
        k = t[l-2]
        v = t[l-1]
        # print k, v # debug
        # try to convert v
        try:
            v = convert(v)
        except:
            pass
        if stats.has_key(k):
            oldv = stats[k]
            if v != oldv:
                print k, "|", oldv, "|", v,
                try:
                    d = v - oldv
                    if sleeptime != 1:
                        if d >= sleeptime:
                            e = d / sleeptime
                        else:
                            e = float(d) / sleeptime
                        print "|", d, "|", e
                    else:
                        print "|", d
                except:
                    print
        stats[k] = v
    print

def main():
    host = None
    port = None
    user = None
    passwd = None
    sleeptime = 10
    q = 'show engine tokudb status'

    for a in sys.argv[1:]:
        if a == "-h" or a == "-?" or a == "--help":
            return usage()
        match = re.match("--(.*)=(.*)", a)
        if match:
            exec "%s='%s'" % (match.group(1),match.group(2))
            continue
        return usage()

    connect_parameters = {}
    if host is not None:
        if host[0] == '/':
            connect_parameters['unix_socket'] = host
        else:
            connect_parameters['host'] = host
            if port is not None:
                connect_parameters['port'] = int(port)
    if user is not None:
        connect_parameters['user'] = user
    if passwd is not None:
        connect_parameters['passwd'] = passwd

    try:
        db = MySQLdb.connect(**connect_parameters)
    except:
        print sys.exc_info()
        return 1

    print "connected"

    stats = {}
    while 1:
        try:
            c = db.cursor()
            n = c.execute(q)
            rs = c.fetchall()
            db.commit()
            c.close()
        except:
            print "db", sys.exc_info()
            return 2

        try:
            printit(stats, rs, int(sleeptime))
            time.sleep(int(sleeptime))
        except:
            print "printit", sys.exc_info()
            return 3


    return 0

sys.exit(main())
