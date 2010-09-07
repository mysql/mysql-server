import sys

def main():
    ts = None
    while 1:
        b = sys.stdin.readline()
        if b == "": break
        f = b.split()
        if len(f) != 2: continue
        newts = int(f[0])
        event = f[1]
        if ts is None:
            ts = int(f[0])
        else:
            print "%8d %s" % (newts - ts, event)
            ts = newts
    return 0
sys.exit(main())
