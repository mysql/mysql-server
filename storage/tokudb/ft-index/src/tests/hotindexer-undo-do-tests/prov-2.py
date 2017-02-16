#!/usr/bin/env python
# generate hotindexing undo provisional tests with 2 nested transactions

import sys

def print_tr(fp, tr, trstack):
    trtype = tr[0]
    xid = tr[1:]
    if trtype == 'i':
        print >>fp, "insert", trstack, xid, "v"+xid
    if trtype == 'd':
        print >>fp, "delete", trstack, xid
    if trtype == 'p':
        print >>fp, "placeholder", trstack, xid

def print_test(fp, live, commit, prov0, prov1):
    if live != "":
        for xid in live.split(","):
            print >>fp, "live", xid
    print >>fp, "key k1"
    print_tr(fp, commit, "committed")
    print_tr(fp, prov0, "provisional")
    print_tr(fp, prov1, "provisional")

def main():
    # live transactions
    for live in ["", "200", "200,201"]:
        # committed transaction records
        for commit in ["i0", "d0"]:
            # provisional level 0 transaction records
            for prov0 in ["i200", "d200", "p200"]:
                # provisional level 1 transaction records
                for prov1 in ["i201", "d201"]:
                    if live == "":
                        fname = "prov.%s.%s.%s.test" % (commit, prov0, prov1)
                    else:
                        fname = "prov.live%s.%s.%s.%s.test" % (live, commit, prov0, prov1)
                    print fname
                    fp = open(fname, "w")
                    if fp:
                        print_test(fp, live, commit, prov0, prov1)
                        fp.close()
    return 0

sys.exit(main())
