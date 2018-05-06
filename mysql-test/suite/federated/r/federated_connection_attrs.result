CREATE DATABASE federated;
CREATE DATABASE federated;
#
# Bug #16555730: FEDERATED SHOULD DEFINE CONNECTION ATTRIBUTES 
#  TO IDENTIFY ITSELF
#
CREATE TABLE federated.t1 (a int, b int) DEFAULT CHARSET=latin1;
INSERT INTO federated.t1 VALUES (1,1),(2,2);
CREATE TABLE federated.t1 (a int, b int)
ENGINE="FEDERATED" DEFAULT CHARSET=latin1
CONNECTION='mysql://root@127.0.0.1:SLAVE_PORT/federated/t1';
SELECT * FROM federated.t1 ORDER BY a;
a	b
1	1
2	2
# must be non-zero
SELECT COUNT(*) FROM performance_schema.session_account_connect_attrs
WHERE ATTR_NAME='program_name' AND ATTR_VALUE='mysqld';
COUNT(*)
1
DROP TABLE federated.t1;
DROP TABLE federated.t1;
DROP TABLE IF EXISTS federated.t1;
DROP DATABASE federated;
DROP TABLE IF EXISTS federated.t1;
DROP DATABASE federated;
