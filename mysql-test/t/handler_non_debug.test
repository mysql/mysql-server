--source include/have_nodebug.inc

# Contents of this test file may be moved into main handler test file
# when bug#25987758 has been fixed

CREATE SCHEMA s1;

CREATE TABLE s1.t1(c1 INTEGER, c2 INTEGER, KEY k1(c1), KEY k2(c2));
INSERT INTO s1.t1 VALUES (1,10), (2,20), (3,30);

CREATE USER u1@localhost;

connect (con1,localhost,u1,,test);
connection con1;

--error ER_TABLEACCESS_DENIED_ERROR
HANDLER s1.t1 OPEN;

connection default;

GRANT SELECT(c1) ON s1.t1 TO u1@localhost;

connection con1;

HANDLER s1.t1 OPEN AS t1;

--error ER_TABLEACCESS_DENIED_ERROR
HANDLER t1 READ k1 FIRST;

--error ER_TABLEACCESS_DENIED_ERROR
HANDLER t1 READ k1=(1,10);

connection default;

# Enable the lines below when bug#25987758 has been fixed
#
#REVOKE SELECT(c1) ON s1.t1 FROM u1@localhost;
#GRANT SELECT(c2) ON s1.t1 TO u1@localhost;
#
#connection con1;
#
#--error ER_TABLEACCESS_DENIED_ERROR
#HANDLER t1 READ k1 FIRST;
#
#--error ER_TABLEACCESS_DENIED_ERROR
#HANDLER t1 READ k1=(1,10);
#
#connection default;
#
#GRANT SELECT(c1) ON s1.t1 TO u1@localhost;
#
#connection con1;
#
#HANDLER t1 READ k1 FIRST;
#
#HANDLER t1 READ k1=(1,10);
#
#connection default;

disconnect con1;

DROP USER u1@localhost;

DROP TABLE s1.t1;
DROP SCHEMA s1;
