#!/usr/local/bin/python

import sys
try:
    data = open(sys.argv[1])
except:
    print "Could not open '%s'" % (sys.argv[1][0])
    exit(0)

ts_factor = 1.
ts_prev = 0.

xxx = []

for line in data:
    line = line.rstrip("\n")
    vals = line.split()
    [n, tid, ts, funcline] = vals[0:4]
    note = ''
    for v in vals[4:-1]:
        note += v+' '
    note += vals[-1]

    if ( note == 'calibrate done' ):
        ts_factor = float(ts) - ts_prev
        print "Factor = ", ts_factor, "("+str(ts_factor/1000000000)[0:4]+"GHz)"

    time = (float(ts)-ts_prev)/ts_factor

    found_it = 0
    for x in xxx:
        if note == x[0]:
            found_it = 1
            x[1] += time
            break
            
    if found_it == 0:
        xxx.append([note,time])

    ts_prev = float(ts)

# trim out unneeded
yyy = []
for x in xxx:
    if x[0][0:9] == 'calibrate': yyy.append(x)
    
for y in yyy:
    xxx.remove(y)

print ''    
        
total_time = 0;
for x in xxx:
    total_time += x[1]
for x in xxx:
    print "%20s %5d" % (x[0], 100.*x[1]/total_time)

    
