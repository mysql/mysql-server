#!/usr/bin/env python

import sys
try:
    data = open(sys.argv[1])
except:
    print "Could not open '%s'" % (sys.argv[1][0])
    exit(0)

ts_factor = 1.
ts_prev = 0.

threadlist = []

for line in data:
    line = line.rstrip("\n")
    vals = line.split()
    [n, tid, ts, funcline] = vals[0:4]
    # 'note' is all text following funcline
    note = ''
    for v in vals[4:-1]:
        note += v+' '
    note += vals[-1]

    if ( note == 'calibrate done' ):
        ts_factor = float(ts) - ts_prev
        print "Factor = ", ts_factor, "("+str(ts_factor/1000000000)[0:4]+"GHz)"

    time = (float(ts)-ts_prev)/ts_factor

    # create a list of threads
    #  - each thread has a list of <note,time> pairs, where time is the accumulated time for that note
    #  - search threadlist for thread_id (tid)
    #      - if found, search corresponding list of <note,time> pairs for the current note
    #             - if found, update (+=) the time
    #             - if not found, create a new <note,time> pair
    #      - if not found, create a new thread,<note,time> entry
    found_thread = 0
    for thread in threadlist:
        if tid == thread[0]:
            found_thread = 1
            notetimelist = thread[1]
            found_note = 0
            for notetime in notetimelist:
                if note == notetime[0]:
                    found_note = 1
                    notetime[1] += time
                    break
            if found_note == 0:
                thread[1].append([note, time])
            break
    if found_thread == 0:
        notetime = []
        notetime.append([note, time])
        threadlist.append([tid, notetime])

    ts_prev = float(ts)

# trim out unneeded
for thread in threadlist:
    trimlist = []
    for notetime in thread[1]:
        if notetime[0][0:9] == 'calibrate':
            trimlist.append(notetime)
    for notetime in trimlist:
        thread[1].remove(notetime)
print ''

# sum times to calculate percent (of 100)
total_time = 0
for thread in threadlist:
    for [note, time] in thread[1]:
        total_time += time

print '    thread         operation      time(sec)   percent'
for thread in threadlist:
    print 'tid : %5s' % thread[0]
    for [note, time] in thread[1]:
        print '           %20s    %f %5d' % (note, time, 100. * time/total_time)


    
