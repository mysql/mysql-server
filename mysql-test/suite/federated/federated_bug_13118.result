CREATE DATABASE federated;
CREATE DATABASE federated;
DROP TABLE IF EXISTS federated.bug_13118_table;
CREATE TABLE federated.bug_13118_table (
`foo` integer,
`bar` integer
);
DROP TABLE IF EXISTS federated.t1;
CREATE TABLE federated.t1 (
`foo` integer,
`bar` integer
) ENGINE="FEDERATED"
    CONNECTION='mysql://root@127.0.0.1:SLAVE_PORT/federated/bug_13118_table';
SELECT * from federated.t1;
foo	bar
INSERT INTO federated.t1 VALUES (1,1);
SELECT * FROM federated.t1;
foo	bar
1	1
INSERT INTO federated.t1 VALUES (1,1);
SELECT * FROM federated.t1;
foo	bar
1	1
1	1
DROP TABLE federated.t1;
DROP TABLE federated.bug_13118_table;
DROP TABLE IF EXISTS federated.t1;
DROP DATABASE federated;
DROP TABLE IF EXISTS federated.t1;
DROP DATABASE federated;
