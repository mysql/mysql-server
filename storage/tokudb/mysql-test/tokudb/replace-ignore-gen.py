def sqlgen_setup():
    print "--disable_warnings"
    print "drop table if exists t;"
    print "--enable_warnings"

def sqlgen_fill_table(n):
    print "insert into t values"
    for i in range(n):
        print "    (%s, %s, %s)," % (i, i, 10*i)
    print "    (%s, %s, %s);" % (n, n, 10*n)

def sqlgen_create_table(fields, pk, keys):
    print "create table t ("
    print "    %s, " % fields
    print "    primary key (%s), " % pk
    print "    %s" % keys
    print ") engine = tokudb;"

def sqlgen_explain_and_do(query):
    print "explain %s" % query
    print query

def sqlgen_drop_table():
    print "drop table t;"

print "# Tokutek"
print "# Test that replace into and insert ignore insertions "
print "# work under various index schemas. "
print "#"
print "# this test is interesting because tokudb can do blind "
print "# (searchless) insertions into dictionaries when keys are"
print "# a subset of the primary key, but not otherwise."
print ""
sqlgen_setup()
print ""

num_rows = 50;
pk = "a, b"
fields = "a int, b int, c int"
for query in ["insert ignore", "replace into"]:
    print "# testing query type \"%s\"" % query
    for keys in ["key (b)", "key (b), key(c)"]:
        print ""
        print "# testing primary key %s" % pk
        sqlgen_create_table(fields, pk, keys)
        sqlgen_fill_table(num_rows);
        print ""
        for k in ["8", "15"]:
            print "%s t values (%s, %s, -1);" % (query, k, k)
            s = "select * from t where a = %s;" % k
            sqlgen_explain_and_do(s)
            s = "select * from t force index (b) where b = %s;" % k
            sqlgen_explain_and_do(s)
            n = int(k) * 10
            s = "select * from t where c = %s;" % n
            sqlgen_explain_and_do(s)
            s = "select * from t where c = -1;"
            sqlgen_explain_and_do(s)
        sqlgen_drop_table()
