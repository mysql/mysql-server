def sqlgen_setup():
    print "--disable_warnings"
    print "drop table if exists t;"
    print "--enable_warnings"

def sqlgen_create_table(fields, pk, keys):
    print "create table t ("
    print "%s, " % fields
    print "primary key (%s), " % pk
    print "key (%s)) " % keys
    print "engine = tokudb;"

def sqlgen_drop_table()
    print "drop table t"

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
print "# table whose keys are a subset of the primary key"
fields = "a int, b int, c int"
pk = "a, b, c"
keys = "a, b"
sqlgen_create_table(fields, pk, keys)
print ""
sqlgen_drop_table()


