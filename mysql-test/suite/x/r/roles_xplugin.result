RUN DROP ROLE IF EXISTS r1, r2, u1@localhost, u2

0 rows affected
RUN CREATE USER u1@localhost IDENTIFIED BY 'foo', u2

0 rows affected
RUN CREATE ROLE r1,r2

0 rows affected
RUN GRANT r1, r2 TO u1@localhost, u2

0 rows affected
RUN GRANT ALL ON test.* TO r2

0 rows affected
RUN ALTER USER u1@localhost DEFAULT ROLE r1,r2

0 rows affected
RUN SELECT CURRENT_ROLE()
CURRENT_ROLE()
NONE
0 rows affected
RUN SET ROLE ALL

0 rows affected
RUN SET ROLE DEFAULT

0 rows affected
RUN SET ROLE NONE

0 rows affected
RUN DROP ROLE r1,r2

0 rows affected
RUN DROP USER u1@localhost, u2

0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
