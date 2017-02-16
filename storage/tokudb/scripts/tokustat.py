#!/usr/bin/env python

import sys
import time
import re
import MySQLdb

def usage():
    print "diff the tokudb engine status"
    print "--host=HOSTNAME (default: localhost)"
    print "--port=PORT"
    print "--iterations=MAX_ITERATIONS (default: forever)"
    print "--interval=TIME_BETWEEN_SAMPLES (default: 10 seconds)"
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

def printit(stats, rs, interval):
    for t in rs:
        l = len(t) # grab the last 2 fields in t
        k = t[l-2]
        v = t[l-1]
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
                    if interval != 1:
                        if d >= interval:
                            e = d / interval
                        else:
                            e = float(d) / interval
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
    interval = 10
    iterations = 0

    q = 'show engine tokudb status'

    for a in sys.argv[1:]:
        if a == "-h" or a == "-?" or a == "--help":
            return usage()
        match = re.match("--(.*)=(.*)", a)
        if match:
            exec "%s='%s'" % (match.group(1),match.group(2))
            continue
        return usage()

    iterations = int(iterations)
    interval = int(interval)

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
    i = 0
    while iterations == 0 or i <= iterations:
        i += 1
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
            printit(stats, rs, interval)
            time.sleep(interval)
        except:
            print "printit", sys.exc_info()
            return 3


    return 0

sys.exit(main())
