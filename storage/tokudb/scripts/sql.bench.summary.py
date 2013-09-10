#!/usr/bin/env python

# summarize the sql-bench trace file

import sys
import re
import os.path

class testreports:
    def __init__(self):
        self.reports = []

    def append(self, report):
        self.reports.append(report)

    def duration(self, start, stop):
        t0 = os.popen('date -d"' + start + '" +%s').readline()
        t1 = os.popen('date -d"' + stop + '" +%s').readline()
        return int(t1) - int(t0)

    def printit(self, i):
        report = self.reports[i]
        d = self.duration(report["start"], report["stop"])
        print "%s %s %6u %s" % (report["result"].upper(), report["start"], d, report["name"])
        # print self.reports[i]

    def printall(self):
        for i in range(len(self.reports)):
            self.printit(i)

    def stoptime(self, stoptime):
        if len(self.reports) > 0:
            lastreport = self.reports[-1]
            lastreport["stop"] = stoptime

def main():
    reports = testreports()
    testreport = {}
    while 1:
        b = sys.stdin.readline()
        if b == "": break
        
        b = b.rstrip('\n')

        match = re.match("^(\d{8} \d{2}:\d{2}:\d{2})$", b)
        if match:
            if totaltime == "" and testreport["result"] == "pass":
                testreport["result"] = "fail"
            testreport["stop"] = match.group(1)
            reports.append(testreport)
            testreport = {}
            continue
        
        match = re.match("^(\d{8} \d{2}:\d{2}:\d{2}) (test-.*)$", b)
        if match:
            testreport["start"] = match.group(1)
            testreport["name"] = match.group(2)
            testreport["result"] = "pass"
            totaltime = ""
            continue
        
        match = re.match(".*Got error|.*Died at", b)
        if match: testreport["result"] = "fail"

        match = re.match("^Total time|^Estimated total time", b)
        if match: totaltime = b

        match = re.match("skip", b)
        if match: testreport["result"] = "skip"
                
    reports.printall()
        
    return 0

sys.exit(main())
