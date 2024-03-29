CREATE SCHEMA s;
CREATE TABLE s.t (pk INTEGER PRIMARY KEY);
Got one of the listed errors
DROP SCHEMA s;
CREATE SCHEMA s;
CREATE TABLE s.t_myisam (pk INTEGER PRIMARY KEY);
ALTER TABLE s.t_myisam ADD COLUMN c INTEGER;
ERROR HY000: Can't find file: 't_myisam' (errno: 2 - No such file or directory)
DROP SCHEMA s;
CREATE SCHEMA s;
CREATE TABLE s.t_myisam (pk INTEGER PRIMARY KEY);
SHOW CREATE TABLE s.t_myisam;
ERROR HY000: Can't find file: 't_myisam' (errno: 2 - No such file or directory)
DROP SCHEMA s;
CREATE SCHEMA s;
CREATE TABLE s.t_myisam (pk INTEGER PRIMARY KEY);
DROP TABLE s.t_myisam;
ERROR HY000: Storage engine can't drop table 's.t_myisam' because it is missing. Use DROP TABLE IF EXISTS to remove it from data-dictionary.
DROP SCHEMA s;
