## 1. Setup the test
## 2. Trigger warnings
##    a. trigger small amount of warnings (2)
##    b. trigger large amount of warnings (14998)

# Ad 1.
#
RUN USE test
RUN CREATE TABLE table_with_ints(i INTEGER)
RUN SET max_error_count=20000

# Ad 2.a.
#
RUN INSERT INTO table_with_ints(i) VALUES(1),(0),(1),(0)
RUN SELECT 1/i FROM table_with_ints
Got expected warning: 1365 
[ASSERT Mysqlx.Notice.Warning == 2]

# Ad 2.b.
#
RUN DELETE FROM table_with_ints WHERE TRUE
RUN INSERT INTO table_with_ints(i) VALUES(1)
RUN SELECT %FIELDS% FROM table_with_ints
Got expected warnings: 1292 1365 
[ASSERT Mysqlx.Notice.Warning == 15000]
Mysqlx.Ok {
  msg: "bye!"
}
ok
DROP TABLE table_with_ints;
