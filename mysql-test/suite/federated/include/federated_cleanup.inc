connection master;
--disable_warnings
DROP TABLE IF EXISTS federated.t1;
DROP DATABASE federated;

connection slave;
DROP TABLE IF EXISTS federated.t1;
DROP DATABASE federated;
--enable_warnings
