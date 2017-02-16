#!/usr/bin/env python 

import sys
import time
import re
import MySQLdb
import os.path


# run as
# [tcallaghan@mork ~]$ tokufilecheck.py --socket=/tmp/tmc.sock --user=root --datadir=$DB_DIR/data/



def usage():
    print "check for filesystem issues"
    print "[--socket=SOCKET]"
    print "[--port=PORT]"
    print "[--user=USERNAME]"
    print "[--password=PASSWORD]"
    print "[--datadir=<full-path-to-mysql-data-directory>]"
    return 1


# check that each file in our file map exists in the file system
def check_file_map(rs, datadir):
    print "checking that all files in our file map exist in the file system"
    filesExisting = 0
    filesMissing = 0
    
    for t in rs:
        dictName = t[0]
        fileName = t[1]
        if os.path.isfile(os.path.join(datadir, fileName)):
            #print datadir + fileName + " exists"
            filesExisting += 1
        else:
            print "  *** " + datadir + fileName + " is missing"
            filesMissing += 1
    
    print "  %d file(s) found | %d file(s) missing." % (filesExisting, filesMissing)


# check if a .tokudb file exists in our file map
def lookup_tokudb(rs, checkFileName):
    fileExists = False
    
    for t in rs:
        if (t[1] == checkFileName):
            fileExists = True
            break
    
    return fileExists


# check that all .frm files exist
def check_frm(rs, datadir):
    print "checking that all .frm files exist"
    filesExisting = 0
    filesMissing = 0
    
    for t in rs:
        databaseName = t[0]
        tableName = t[1]
        storageEngine = t[2]
        if os.path.isfile(os.path.join(datadir + os.sep + databaseName, tableName + '.frm')):
            #print datadir + fileName + " exists"
            filesExisting += 1
        else:
            print "  *** missing FRM file for table " + tableName + " in database " + databaseName
            filesMissing += 1
    
    print "  %d file(s) found | %d file(s) missing." % (filesExisting, filesMissing)


# check if a .tokudb file exists in our file map
def lookup_frm(rs, tableName, dbName):
    frmExists = False
    
    for t in rs:
        if ((t[0] == dbName) and (t[1] == tableName)):
            frmExists = True
            break
    
    return frmExists


def main():
    host = "localhost"
    socket = None
    port = None
    user = None
    passwd = None
    datadir = "./"

    for a in sys.argv[1:]:
        if a == "-h" or a == "-?" or a == "--help":
            return usage()
        match = re.match("--(.*)=(.*)", a)
        if match:
            exec "%s='%s'" % (match.group(1),match.group(2))
            continue
        return usage()

    connect_parameters = {}
    if socket is not None:
        connect_parameters['unix_socket'] = socket
    if port is not None:
        connect_parameters['host'] = host
        connect_parameters['port'] = int(port)
    if user is not None:
        connect_parameters['user'] = user
    if passwd is not None:
        connect_parameters['passwd'] = passwd
        
    datadir = os.path.normpath(datadir) + os.sep

    try:
        db = MySQLdb.connect(**connect_parameters)
    except:
        print sys.exc_info()
        return 1

    print "connected to MySQL"

    print "retrieving file map"
    try:
        q = 'select * from information_schema.tokudb_file_map'
        c = db.cursor()
        n = c.execute(q)
        rsFileMap = c.fetchall()
        db.commit()
        c.close()
    except MySQLdb.Error, e:
        print "  ** Error %d: %s" % (e.args[0], e.args[1])
        return 2
    except:
        print "db", sys.exc_info()
        return 2

    print "retrieving full table listing"
    try:
        q = "select table_schema, table_name, engine from information_schema.tables where table_schema != 'information_schema'"
        c = db.cursor()
        n = c.execute(q)
        rsTables = c.fetchall()
        db.commit()
        c.close()
    except MySQLdb.Error, e:
        print "  ** Error %d: %s" % (e.args[0], e.args[1])
        return 2
    except:
        print "db", sys.exc_info()
        return 2

    check_file_map(rsFileMap, datadir);
    check_frm(rsTables, datadir);

    print "checking for extra .frm and/or .tokudb files"

    extraFrm = 0
    extraTokudb = 0

    for (path, dirs, files) in os.walk(datadir):
        for file in files:
            if (file.endswith('.frm')):
                tableName = os.path.splitext(file)[0]
                dbName = os.path.basename(os.path.normpath(path))
                if not lookup_frm(rsTables, tableName, dbName):
                    extraFrm += 1
                    print "found .frm file for tableName / dbName = %s / %s" % (tableName, dbName)
            if (file.endswith('.tokudb')):
                # check that it exists in rsFileMap
                checkFileName = "./" + file
                if not lookup_tokudb(rsFileMap, checkFileName):
                    extraTokudb += 1
                    print "  ** found extra .tokudb file %s" % (checkFileName)
                    
    print "  %d unmatched .frm file(s) found" % (extraFrm)
    print "  %d unmatched .tokudb file(s) found" % (extraTokudb)

    return 0

sys.exit(main())
