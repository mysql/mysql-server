--echo Verify that JSON columns do not support indexes on create table

--error 3152
CREATE TABLE bad(i INT, j1bad JSON NOT NULL PRIMARY KEY);
--error 3152
CREATE TABLE bad(i INT, j1bad JSON NOT NULL, INDEX j1badindex (j1bad));
--error 3152
CREATE TABLE bad(i INT, j1bad JSON NOT NULL, UNIQUE INDEX j1badindex (j1bad));
--error 3152
CREATE TABLE bad(i INT, j1bad JSON NOT NULL, UNIQUE INDEX j1badindex (j1bad) USING HASH);

CREATE TABLE t1(i INT PRIMARY KEY);
ALTER TABLE t1 ADD COLUMN j1 JSON;
ALTER TABLE t1 ADD COLUMN j2 JSON NOT NULL;
# A JSON column cannot be used as a key.
--error ER_JSON_USED_AS_KEY
CREATE INDEX t1_idx_j ON t1(j1);
--error ER_JSON_USED_AS_KEY
CREATE INDEX t1_idx_i_j ON t1(i, j1);
--error ER_JSON_USED_AS_KEY
CREATE INDEX t1_idx_j_i ON t1(j1, i);
CREATE INDEX t1_idx_i ON t1(i);
DROP INDEX t1_idx_i ON t1;

DROP TABLE t1;

--echo Verify that JSON columns do not support indexes on alter table
CREATE TABLE t2(i INT PRIMARY KEY, j1 JSON, j2 JSON NOT NULL);
--error 3152
ALTER TABLE t2 ADD INDEX j1badindex (j1);
--error 3152
ALTER TABLE t2 ADD UNIQUE INDEX j1badindex (j1);
--error 3152
ALTER TABLE t2 ADD UNIQUE INDEX j1badindex (j1) USING HASH;

--echo Verify that JSON columns can be dropped
ALTER TABLE t2 DROP COLUMN j1;
ALTER TABLE t2 DROP COLUMN j2;

DROP TABLE t2;

# JSON is a non-reserved keyword, so it should be possible to use it
# as an identifier.
CREATE TABLE json(json int);
INSERT INTO json(json) VALUES (1);
SELECT json FROM json;
DROP TABLE json;

# And it should be possible to use it as a label in a stored procedure.
DELIMITER |;
CREATE PROCEDURE p()
BEGIN
  json: LOOP
    LEAVE json;
  END LOOP json;
END|
DELIMITER ;|
CALL p();
DROP PROCEDURE p;
