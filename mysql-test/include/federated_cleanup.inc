connection master;
--disable_warnings
DROP TABLE IF EXISTS federated.t1;
DROP DATABASE IF EXISTS federated;
--enable_warnings

connection slave;
--disable_warnings
DROP TABLE IF EXISTS federated.t1;
DROP DATABASE IF EXISTS federated;
--enable_warnings
