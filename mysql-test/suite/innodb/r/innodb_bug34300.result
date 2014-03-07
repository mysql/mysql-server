#
# Bug#34300 Tinyblob & tinytext fields currupted after export/import and alter in 5.1
#
SET @@global.max_allowed_packet=16777216;
CREATE TABLE bug34300 (
f4 TINYTEXT,
f6 MEDIUMTEXT,
f8 TINYBLOB
) ENGINE=InnoDB;
INSERT INTO bug34300 VALUES ('xxx', repeat('a', 8459264), 'zzz');
SELECT f4, f8 FROM bug34300;
f4	f8
xxx	zzz
ALTER TABLE bug34300 ADD COLUMN (f10 INT);
SELECT f4, f8 FROM bug34300;
f4	f8
xxx	zzz
# Cleanup
DROP TABLE bug34300;
