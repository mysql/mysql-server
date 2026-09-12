SELECT CURRENT_USER();

#
# Verify the mysql.tables_priv.grantor.
#

CREATE TABLE t1 (f1 INT);
GRANT INSERT ON test.* TO u2@localhost;
GRANT UPDATE ON test.t1 TO u2@localhost;
SELECT host,db,user,grantor,table_name FROM mysql.tables_priv
  WHERE user LIKE 'u2%' ORDER BY host,db,user,table_name;
DROP TABLE t1;

#
# Verify the mysql.proxies_priv.grantor.
#

GRANT PROXY ON u1@host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890 TO u2@localhost;
SELECT * FROM mysql.proxies_priv WHERE Proxied_user ='u1';
REVOKE PROXY ON u1@host_1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890abcdefghij1234567890 FROM u2@localhost;

#
# Verify the mysql.procs_priv.grantor.
#

CREATE PROCEDURE p1() SELECT 1;
GRANT EXECUTE ON PROCEDURE p1 TO u2@localhost;
SELECT User, Host, Grantor FROM mysql.procs_priv WHERE User LIKE 'u2%';
DROP PROCEDURE p1;
