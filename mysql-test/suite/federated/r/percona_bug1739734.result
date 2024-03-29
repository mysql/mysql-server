#
# Bug lp1739734  "Federated table returns error 1430 from storage engine"
#
# This is a bug introduced in the artful/gcc7 compilation fixes, caused by
# a moved break statement.
CREATE DATABASE federated;
CREATE DATABASE federated;
CREATE DATABASE lp1739734;
use lp1739734;
CREATE SERVER local_server
FOREIGN DATA WRAPPER mysql
OPTIONS (
HOST '127.0.0.1',
PORT MASTER_PORT,
USER 'root',
PASSWORD '',
DATABASE 'lp1739734'
);
CREATE TABLE remote_table (
a INT,
b INT,
KEY ab (a,b),
KEY ba (b,a)
);
CREATE TABLE local_table (
a INT,
b INT,
KEY ab (a,b),
KEY ba (b,a)
) ENGINE=federated CONNECTION='local_server/remote_table';
SELECT * FROM local_table;
a	b
SELECT * FROM local_table USE INDEX (ab)
WHERE a<1 AND b=0;
a	b
SELECT * FROM local_table USE INDEX (ba)
WHERE a<1 AND b=0;
a	b
DROP DATABASE lp1739734;
DROP SERVER local_server;
DROP TABLE IF EXISTS federated.t1;
DROP DATABASE federated;
DROP TABLE IF EXISTS federated.t1;
DROP DATABASE federated;
