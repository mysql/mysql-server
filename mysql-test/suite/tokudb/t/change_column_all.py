import sys
import random
import string
import re

class Field:
    def __init__(self, name, is_nullible):
        self.name = name
        self.is_nullible = is_nullible

class Field_int(Field):
    sizes = [ 1, 2, 3, 4, 8 ]
    types = [ "TINYINT", "SMALLINT", "MEDIUMINT", "INT", "BIGINT" ]
    uint_ranges = [ (0,(1<<8)-1), (0,(1<<16)-1), (0,(1<<24)-1), (0,(1<<32)-1), (0,(1<<64)-1) ]
    int_ranges = [ (-(1<<7), (1<<7)-1), (-(1<<15),(1<<15)-1), (-(1<<23),(1<<23)-1), (-(1<<31),(1<<31)-1), (-(1<<63),(1<<63)-1) ]
    def __init__(self, name, size, is_unsigned, is_nullible):
        Field.__init__(self, name, is_nullible)
        self.idx = Field_int.sizes.index(size)
        self.size = size
        self.is_unsigned = is_unsigned
    def get_type(self):
        t = Field_int.types[self.idx]
        if self.is_unsigned:
            t += " UNSIGNED"
        if not self.is_nullible:
            t += " NOT NULL"
        return t
    def get_value(self):
        if self.is_unsigned:
            r = Field_int.uint_ranges[self.idx]
        else:
            r = Field_int.int_ranges[self.idx]
        return random.randint(r[0],r[1])
    def next_field(self):
        return Field_int(self.name, Field_int.sizes[random.randint(self.idx,len(Field_int.sizes)-1)], self.is_unsigned, self.is_nullible)

class Field_int_auto_inc(Field_int):
    def __init__(self, name, size, is_unsigned, is_nullible):
        Field_int.__init__(self, name, size, is_unsigned, is_nullible)
        self.next_value = 0
    def get_type(self):
        return Field_int.get_type(self)
    def get_value(self):
        v = self.next_value
        self.next_value += 1
        return v

class Field_char(Field):
    types = [ "CHAR", "BINARY" ]
    def __init__(self, name, size, is_binary, is_nullible):
        Field.__init__(self, name, is_nullible)
        assert 0 <= size and size < 256
        self.size = size
        self.is_binary = is_binary
    def get_type(self):
        t = "%s(%d)" % (Field_char.types[self.is_binary], self.size)
        if not self.is_nullible:
            t += " NOT NULL"
        return t
    def next_size(self):
        if self.size < 255:
            return self.size + 1
        return self.size
    def get_value(self):
        l = random.randint(1, self.size)
        s = ''.join(random.choice(string.ascii_lowercase + string.ascii_uppercase + string.digits) for x in range(l))    
        return "'%s'" % (s)
    def next_field(self):
        return Field_char(self.name, self.next_size(), self.is_binary, self.is_nullible)

class Field_varchar(Field):
    types = [ "VARCHAR", "VARBINARY" ]
    def __init__(self, name, size, is_binary, is_nullible):
        Field.__init__(self, name, is_nullible)
        assert 0 <= size and size < 64*1024
        self.size = size
        self.is_binary = is_binary
    def get_type(self):
        t = "%s(%d)" % (Field_varchar.types[self.is_binary], self.size)
        if not self.is_nullible:
            t += " NOT NULL"
        return t;
    def get_value(self):
        l = random.randint(1, self.size)
        s = ''.join(random.choice(string.ascii_lowercase + string.ascii_uppercase + string.digits) for x in range(l))    
        return "'%s'" % (s)
    def next_size(self):
        if self.size < 64*1024:
            return self.size + 1
        return self.size
    def next_field(self):
        if self.size < 256:
            new_size = 256
        else:
            new_size = self.size + 1
        return Field_varchar(self.name, new_size, self.is_binary, self.is_nullible)

class Field_blob(Field):
    types = [ "TINYBLOB", "BLOB", "MEDIUMBLOB", "LONGBLOB" ]
    def __init__(self, name, size, is_nullible, idx):
        Field.__init__(self, name, is_nullible)
        self.size = size
        self.idx = idx
    def get_type(self):
        t = "%s" % (Field_blob.types[self.idx])
        if not self.is_nullible:
            t += " NOT NULL"
        return t
    def get_value(self):
        l = random.randint(1, self.size)
        s = ''.join(random.choice(string.ascii_lowercase + string.ascii_uppercase + string.digits) for x in range(l))    
        return "'%s'" % (s)
    def next_field(self):
        self.size += 1
        if self.idx < 3:
            self.idx += 1
        return Field_blob(self.name, self.size, self.is_nullible, self.idx)

def create_fields():
    fields = []
    fields.append(create_int('a'))
    fields.append(create_int('b'))
    fields.append(create_char('c'))
    fields.append(create_varchar('d'))
    fields.append(create_varchar('e'))
    fields.append(create_varchar('f'))
    fields.append(create_blob('g'))
    fields.append(create_blob('h'))
    fields.append(Field_int_auto_inc('id', 8, 0, 0))
    return fields

def create_int(name):
    int_sizes = [ 1, 2, 3, 4, 8]
    return Field_int(name, int_sizes[random.randint(0,len(int_sizes)-1)], random.randint(0,1), random.randint(0,1))

def create_char(name):
    return Field_char(name, random.randint(1, 100), random.randint(0,1), random.randint(0,1))

def create_varchar(name):
    return Field_varchar(name, random.randint(1, 100), random.randint(0,1), random.randint(0,1))

def create_blob(name):
    return Field_blob(name, random.randint(1,2), random.randint(0,1), random.randint(0,3))

def create_table(fields, tablename, engine):
    if engine == "tokudb":
        key_type = "CLUSTERING KEY"
    else:
        key_type = "KEY"
    t = "CREATE TABLE %s (" % (tablename)
    for f in fields:
        t += "%s %s, " % (f.name, f.get_type())
    t += "KEY(b), %s(e), PRIMARY KEY(id)" % (key_type)
    t += ") ENGINE=%s;" % (engine)
    return t

def insert_row(fields):
    t = "INSERT INTO %s VALUES ("
    for i in range(len(fields)):
        f = fields[i]
        t += "%s" % (f.get_value())
        if i < len(fields)-1:
            t += ","
    t += ");"
    return t

def header():
    print "# generated from change_column_all.py"
    print "# test random column change on wide tables"
    print "source include/have_tokudb.inc;"
    print "--disable_warnings"
    print "DROP TABLE IF EXISTS t, ti;"
    print "--enable_warnings"
    print "SET SESSION TOKUDB_DISABLE_SLOW_ALTER=1;"
    print "SET SESSION DEFAULT_STORAGE_ENGINE='TokuDB';"
    print

def main():
    experiments = 1000
    nrows = 10
    seed = 0
    for arg in sys.argv[1:]:
        match = re.match("--(.*)=(.*)", arg)
        if match:
            exec("%s = %s" % (match.group(1),match.group(2)))
    random.seed(seed)
    header()
    for experiment in range(experiments):
        # generate a schema
        fields = create_fields()

        # create a table with the schema
        print create_table(fields, "t", "tokudb")

        # insert some rows
        for r in range(nrows):
            print insert_row(fields) % ('t')

        print create_table(fields, "ti", "myisam");
        print "INSERT INTO ti SELECT * FROM t;"

        # transform table schema and contents
        for f in [ 0, 2, 3, 5, 6, 7 ]:
            fields[f] = fields[f].next_field()
            print "ALTER TABLE t CHANGE COLUMN %s %s %s;" % (fields[f].name, fields[f].name, fields[f].get_type())
            print "ALTER TABLE ti CHANGE COLUMN %s %s %s;" % (fields[f].name, fields[f].name, fields[f].get_type())
        
            new_row = insert_row(fields)
            print new_row % ('t')
            print new_row % ('ti')

        # compare tables
        print "let $diff_tables = test.t, test.ti;"
        print "source include/diff_tables.inc;"

        # cleanup
        print "DROP TABLE t, ti;"
        print
    return 0    

sys.exit(main())
