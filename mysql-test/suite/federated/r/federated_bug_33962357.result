# Bug#33962357: Improve federated tables code
CREATE DATABASE federated;
CREATE DATABASE federated;
DROP TABLE IF EXISTS federated.t1;
CREATE TABLE federated.t1 (
a int
);
INSERT INTO federated.t1 (a) VALUES (42);
SET @debug_save= @@GLOBAL.DEBUG;
SET @@GLOBAL.DEBUG='+d,bug33962357_simulate_null_cache';
CREATE SERVER fedlink
FOREIGN DATA WRAPPER mysql
OPTIONS (
HOST '127.0.0.1',
PORT SLAVE_PORT,
USER 'root',
PASSWORD '',
DATABASE 'federated');
DROP TABLE IF EXISTS federated.t1;
CREATE TABLE federated.t1 (
a int
) ENGINE="FEDERATED"
    CONNECTION='fedlink/t1';
ERROR HY000: server name: 'fedlink' doesn't exist!
SELECT * FROM federated.t1;
ERROR 42S02: Table 'federated.t1' doesn't exist
DROP TABLE IF EXISTS federated.t1;
SET @@GLOBAL.DEBUG='+d,bug33962357_simulate_null_server';
CREATE TABLE federated.t1 (
a int
) ENGINE="FEDERATED"
    CONNECTION='fedlink/t1';
ERROR HY000: server name: 'fedlink' doesn't exist!
SELECT * FROM federated.t1;
ERROR 42S02: Table 'federated.t1' doesn't exist
SET GLOBAL DEBUG= @debug_save;
DROP SERVER fedlink;
DROP TABLE IF EXISTS federated.t1;
DROP DATABASE federated;
DROP TABLE IF EXISTS federated.t1;
DROP DATABASE federated;
