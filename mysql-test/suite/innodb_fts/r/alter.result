# Case 1 : WITHOUT FTS_DOC_ID
CREATE TABLE t1 (
id INT,
title VARCHAR(200),
FULLTEXT fidx(title)
) ENGINE=InnoDB;
INSERT INTO t1 (id, title) VALUES (1, 'MySQL Tutorial');
SELECT id, title FROM t1 WHERE MATCH(title) AGAINST('mysql');
id	title
1	MySQL Tutorial
ALTER TABLE t1 ALTER COLUMN id DROP DEFAULT;
INSERT INTO t1 (id, title) VALUES (2, 'MySQL Tutorial');
SELECT id, title FROM t1 WHERE MATCH(title) AGAINST('mysql');
id	title
1	MySQL Tutorial
2	MySQL Tutorial
# restart
ALTER TABLE t1 DROP KEY fidx;
ALTER TABLE t1 ALTER COLUMN id DROP DEFAULT;
DELETE FROM t1 WHERE id < 2;
INSERT INTO t1 (id, title) VALUES (3, 'MySQL Tutorial');
SELECT id, title FROM t1 WHERE MATCH(title) AGAINST('mysql');
ERROR HY000: Can't find FULLTEXT index matching the column list
# restart
INSERT INTO t1 (id, title) VALUES (4, 'MySQL Tutorial');
SELECT id, title FROM t1 WHERE MATCH(title) AGAINST('mysql');
ERROR HY000: Can't find FULLTEXT index matching the column list
ALTER TABLE t1 ADD FULLTEXT KEY fidx (title);
INSERT INTO t1 (id, title) VALUES (5, 'MySQL Tutorial');
SELECT id, title FROM t1 WHERE MATCH(title) AGAINST('mysql');
id	title
2	MySQL Tutorial
3	MySQL Tutorial
4	MySQL Tutorial
5	MySQL Tutorial
ALTER TABLE t1 DROP KEY fidx;
ALTER TABLE t1 ALTER COLUMN id DROP DEFAULT;
DELETE FROM t1 WHERE id < 5;
INSERT INTO t1 (id, title) VALUES (6, 'MySQL Tutorial');
SELECT id, title FROM t1 WHERE MATCH(title) AGAINST('mysql');
ERROR HY000: Can't find FULLTEXT index matching the column list
ALTER TABLE t1 ADD FULLTEXT KEY fidx(title);
INSERT INTO t1 (id, title) VALUES (7, 'MySQL Tutorial');
SELECT id, title FROM t1 WHERE MATCH(title) AGAINST('mysql');
id	title
5	MySQL Tutorial
6	MySQL Tutorial
7	MySQL Tutorial
DROP TABLE t1;
# Case 2 : WITH FTS_DOC_ID PRIMARY KEY
CREATE TABLE t1 (
FTS_DOC_ID BIGINT UNSIGNED AUTO_INCREMENT NOT NULL,
id INT,
title VARCHAR(200),
PRIMARY KEY(FTS_DOC_ID),
FULLTEXT fidx(title)
) ENGINE=InnoDB;
INSERT INTO t1 (id, title) VALUES (1, 'MySQL Tutorial');
SELECT id, title FROM t1 WHERE MATCH(title) AGAINST('mysql');
id	title
1	MySQL Tutorial
ALTER TABLE t1 ALTER COLUMN id DROP DEFAULT;
INSERT INTO t1 (id, title) VALUES (2, 'MySQL Tutorial');
SELECT id, title FROM t1 WHERE MATCH(title) AGAINST('mysql');
id	title
1	MySQL Tutorial
2	MySQL Tutorial
# restart
ALTER TABLE t1 DROP KEY fidx;
ALTER TABLE t1 ALTER COLUMN id DROP DEFAULT;
DELETE FROM t1 WHERE id < 2;
INSERT INTO t1 (id, title) VALUES (3, 'MySQL Tutorial');
SELECT id, title FROM t1 WHERE MATCH(title) AGAINST('mysql');
ERROR HY000: Can't find FULLTEXT index matching the column list
# restart
INSERT INTO t1 (id, title) VALUES (4, 'MySQL Tutorial');
SELECT id, title FROM t1 WHERE MATCH(title) AGAINST('mysql');
ERROR HY000: Can't find FULLTEXT index matching the column list
ALTER TABLE t1 ADD FULLTEXT KEY fidx (title);
INSERT INTO t1 (id, title) VALUES (5, 'MySQL Tutorial');
SELECT id, title FROM t1 WHERE MATCH(title) AGAINST('mysql');
id	title
2	MySQL Tutorial
3	MySQL Tutorial
4	MySQL Tutorial
5	MySQL Tutorial
ALTER TABLE t1 DROP KEY fidx;
ALTER TABLE t1 ALTER COLUMN id DROP DEFAULT;
DELETE FROM t1 WHERE id < 5;
INSERT INTO t1 (id, title) VALUES (6, 'MySQL Tutorial');
SELECT id, title FROM t1 WHERE MATCH(title) AGAINST('mysql');
ERROR HY000: Can't find FULLTEXT index matching the column list
ALTER TABLE t1 ADD FULLTEXT KEY fidx(title);
INSERT INTO t1 (id, title) VALUES (7, 'MySQL Tutorial');
SELECT id, title FROM t1 WHERE MATCH(title) AGAINST('mysql');
id	title
5	MySQL Tutorial
6	MySQL Tutorial
7	MySQL Tutorial
DROP TABLE t1;
# Case 3 : WITH FTS_DOC_ID FTS_DOC_ID_INDEX KEY
CREATE TABLE t1 (
FTS_DOC_ID BIGINT UNSIGNED AUTO_INCREMENT NOT NULL,
id INT,
title VARCHAR(200),
UNIQUE KEY FTS_DOC_ID_INDEX(FTS_DOC_ID),
FULLTEXT fidx(title)
) ENGINE=InnoDB;
INSERT INTO t1 (id, title) VALUES (1, 'MySQL Tutorial');
SELECT id, title FROM t1 WHERE MATCH(title) AGAINST('mysql');
id	title
1	MySQL Tutorial
ALTER TABLE t1 ALTER COLUMN id DROP DEFAULT;
INSERT INTO t1 (id, title) VALUES (2, 'MySQL Tutorial');
SELECT id, title FROM t1 WHERE MATCH(title) AGAINST('mysql');
id	title
1	MySQL Tutorial
2	MySQL Tutorial
# restart
ALTER TABLE t1 DROP KEY fidx;
ALTER TABLE t1 ALTER COLUMN id DROP DEFAULT;
DELETE FROM t1 WHERE id < 2;
INSERT INTO t1 (id, title) VALUES (3, 'MySQL Tutorial');
SELECT id, title FROM t1 WHERE MATCH(title) AGAINST('mysql');
ERROR HY000: Can't find FULLTEXT index matching the column list
# restart
INSERT INTO t1 (id, title) VALUES (4, 'MySQL Tutorial');
SELECT id, title FROM t1 WHERE MATCH(title) AGAINST('mysql');
ERROR HY000: Can't find FULLTEXT index matching the column list
ALTER TABLE t1 ADD FULLTEXT KEY fidx (title);
INSERT INTO t1 (id, title) VALUES (5, 'MySQL Tutorial');
SELECT id, title FROM t1 WHERE MATCH(title) AGAINST('mysql');
id	title
2	MySQL Tutorial
3	MySQL Tutorial
4	MySQL Tutorial
5	MySQL Tutorial
ALTER TABLE t1 DROP KEY fidx;
ALTER TABLE t1 ALTER COLUMN id DROP DEFAULT;
DELETE FROM t1 WHERE id < 5;
INSERT INTO t1 (id, title) VALUES (6, 'MySQL Tutorial');
SELECT id, title FROM t1 WHERE MATCH(title) AGAINST('mysql');
ERROR HY000: Can't find FULLTEXT index matching the column list
ALTER TABLE t1 ADD FULLTEXT KEY fidx(title);
INSERT INTO t1 (id, title) VALUES (7, 'MySQL Tutorial');
SELECT id, title FROM t1 WHERE MATCH(title) AGAINST('mysql');
id	title
5	MySQL Tutorial
6	MySQL Tutorial
7	MySQL Tutorial
DROP TABLE t1;
