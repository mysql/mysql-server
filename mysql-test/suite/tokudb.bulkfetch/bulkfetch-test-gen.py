def sqlgen_drop_table(name):
    print "drop table if exists %s;" % name

def sqlgen_create_table(table):
    keyname, keytype = table[0]
    print "create table t ("
    for name, type in table:
        print "    %s %s," % (name, type) 
    print "    primary key (%s)" % keyname
    print ") engine = TokuDB;"

def sqlgen_fill_table(table, n):
    print "insert into t values "
    row_num = 0
    for i in range(n):
        print "  (",
        k = 0
        while (k < len(table)):
            name, type = table[k]
            if k == len(table) - 1:
                comma = ""
            else:
                comma = ","
            if type == "int" or type == "bigint":
                print "%d%s" % ((i*2), comma),
            elif type == "blob":
                print "\"%s%d\"%s" % ("blobbyblobblob", i, comma),
            else:
                print "\"%s%d\"%s" % ("stringystringstring", i, comma),
            k += 1
        if row_num == n - 1:
            print ");"
        else:
            print "),"
        row_num += 1


def sqlgen_range_query(table):
    keyname, keytype = table[0]
    if keytype == "bigint":
        print "select * from t where %s > 2000 and %s < 4000;" % (keyname, keyname)
        print "select * from t where %s > 3000;" % keyname
        print "select * from t where %s < 2000;" % keyname
    elif keytype == "blob":
        print "select * from t where %s < \"blobbyblobblob2000\";" % keyname
        print "select * from t where %s > \"blobbyblobblob3000\";" % keyname
        print "select * from t where %s < \"blobbyblobblob4000\" and %s > \"blobbyblobblob2000\";" % (keyname, keyname)
    else:
        print "select * from t where %s < \"stringystringstring2000\";" % keyname
        print "select * from t where %s > \"stringystringstring3000\";" % keyname
        print "select * from t where %s < \"stringystringstring4000\" and %s > \"stringystringstring2000\";" % (keyname, keyname)


def sqlgen_range_query_full(table):
    keyname, keytype = table[0]
    if keytype == "bigint":
        print "select sum(%s) from t;" % keyname
    print "select count(*) from t;"
    print "select * from t;"

nrows = 6000
tables = [
    [("a", "bigint")],
    [("a", "bigint"), ("b", "blob")],
    [("a", "bigint"), ("b", "blob"), ("c", "varchar(50)")],
    [("a", "bigint"), ("b", "varchar(50)")],
    [("a", "varchar(50)")],
    [("a", "varchar(50)"), ("b", "blob")],
    # blobs cant be keys so this wouldn't make sense
    #[("a", "blob")]
]

# Code generation stats here
print "# Tokutek"
print "# Test that bulk fetch works with various table types"
print ""
print "--disable_warnings"
print "drop table if exists t;"
print "--enable_warnings"
print "set local tokudb_read_buf_size=4096;"
print ""
print "# Test scans over each kind of table"
print ""
for table in tables:
    print "# begin bulk fetch test"
    sqlgen_drop_table("t")
    sqlgen_create_table(table)
    sqlgen_fill_table(table, nrows)
    sqlgen_range_query(table)
    sqlgen_range_query_full(table)
    print ""
print ""
print "drop table t;"
print "set local tokudb_read_buf_size=default;"
