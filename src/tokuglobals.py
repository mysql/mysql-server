#!/usr/bin/python

import sys
import os
import stat
import re

def checkglobals(libname, exceptsymbols, verbose):
    badglobals = 0
    nmcmd = "nm -g " + libname
    f = os.popen(nmcmd)
    b = f.readline()
    while b != "":
        match = re.match("^([0-9a-f]+)\s(.?)\s(.*)$", b)
        if match == None:
            match = re.match("^\s+(.*)$", b)
            if match == None:
                print "unknown", b
                badglobals = 1
        else:
            type = match.group(2)
            symbol = match.group(3)
            if verbose: print type, symbol
            match = re.match("^toku_|^__toku|^db_env_set", symbol)
            if match == None and not exceptsymbols.has_key(symbol):
                print "non toku symbol=", symbol
                badglobals = 1
        b = f.readline()
    f.close()
    return badglobals

def main():
    verbose = 0
    libname = "libdb.so"
    for arg in sys.argv[1:]:
        if arg == "-v":
            verbose += 1
        elif arg[0:3] == "lib":
            libname = arg

    try: st = os.stat(libname)
    except: return 1
    mode = st[stat.ST_MODE]
    if not (mode & stat.S_IREAD): return 1

    exceptsymbols = {}
    for n in [ "_init", "_fini", "_end", "_edata", "__bss_start" ]:
        exceptsymbols[n] = 1
    for n in [ "db_env_create", "db_create", "db_strerror", "db_version", "log_compare", 
               "db_env_set_func_fsync", 
               "db_env_set_func_free", 
               "db_env_set_func_malloc", 
               "db_env_set_func_realloc", 
               "db_env_set_func_pwrite", 
               "db_env_set_func_write", 
               "db_env_set_func_loader_fwrite", 
               "db_env_set_checkpoint_callback", 
               "db_env_set_checkpoint_callback2",
               "db_env_set_recover_callback", 
               "db_env_set_recover_callback2",
               "setup_dlmalloc", "dlfree", "dlmalloc", "dlrealloc" 
               ]:
        exceptsymbols[n] = 1
    return checkglobals(libname, exceptsymbols, verbose)
    
sys.exit(main())
