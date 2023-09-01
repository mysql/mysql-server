--source include/have_debug.inc

--disable_warnings
DROP TABLE IF EXISTS t1;
DROP VIEW IF EXISTS v1;
--enable_warnings

CREATE TABLE t1 (id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, a VARBINARY(32));
INSERT INTO t1 (a) VALUES (''),('_'),('%'),('\_'),('\%'),('\\');
INSERT INTO t1 (a) VALUES ('a'),('c');
INSERT INTO t1 (a) VALUES ('a_'),('c_');
INSERT INTO t1 (a) VALUES ('a%'),('c%');
INSERT INTO t1 (a) VALUES ('aa'),('cc'),('ch');
INSERT INTO t1 (a) VALUES ('aa_'),('cc_'),('ch_');
INSERT INTO t1 (a) VALUES ('aa%'),('cc%'),('ch%');
INSERT INTO t1 (a) VALUES ('aaa'),('ccc'),('cch');
INSERT INTO t1 (a) VALUES ('aaa_'),('ccc_'),('cch_');
INSERT INTO t1 (a) VALUES ('aaa%'),('ccc%'),('cch%');
INSERT INTO t1 (a) VALUES ('aaaaaaaaaaaaaaaaaaaa');
INSERT INTO t1 (a) VALUES ('caaaaaaaaaaaaaaaaaaa');

CREATE VIEW v1 AS
  SELECT id, 'a' AS name, a AS val FROM t1
UNION
  SELECT id, 'mn', HEX(LIKE_RANGE_MIN(a, 16)) AS min FROM t1
UNION
  SELECT id, 'mx', HEX(LIKE_RANGE_MAX(a, 16)) AS max FROM t1
UNION
  SELECT id, 'sp', REPEAT('-', 32) AS sep FROM t1
ORDER BY id, name;
SELECT * FROM v1;

ALTER TABLE t1 MODIFY a VARCHAR(32) CHARACTER SET latin1;
SELECT * FROM v1;

ALTER TABLE t1 MODIFY a VARCHAR(32) CHARACTER SET utf8mb3;
SELECT * FROM v1;

ALTER TABLE t1 MODIFY a VARCHAR(32) CHARACTER SET utf8mb3 COLLATE utf8mb3_unicode_ci;
SELECT * FROM v1;

ALTER TABLE t1 MODIFY a VARCHAR(32) CHARACTER SET utf8mb3 COLLATE utf8mb3_czech_ci;
SELECT * FROM v1;

# Note, 16 bytes is enough for 16/3= 5 characters
# For the 'aaaaaaaa' value contraction breaks apart
# For the 'caaaaaaa' value contraction does not break apart
ALTER TABLE t1 MODIFY a VARCHAR(32) CHARACTER SET utf8mb3 COLLATE utf8mb3_danish_ci;
SELECT * FROM v1;

ALTER TABLE t1 MODIFY a VARCHAR(32) CHARACTER SET utf8mb4;
SELECT * FROM v1;

ALTER TABLE t1 MODIFY a VARCHAR(32) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
SELECT * FROM v1;

ALTER TABLE t1 MODIFY a VARCHAR(32) CHARACTER SET utf8mb4 COLLATE utf8mb4_czech_ci;
SELECT * FROM v1;

# Note, 16 bytes is enough for 16/4= 4 characters
# For the 'aaaaaaaa' value contraction does not break apart
# For the 'caaaaaaa' value contraction breaks apart
ALTER TABLE t1 MODIFY a VARCHAR(32) CHARACTER SET utf8mb4 COLLATE utf8mb4_danish_ci;
SELECT * FROM v1;

ALTER TABLE t1 MODIFY a VARCHAR(32) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_520_ci;
SELECT * FROM v1;

ALTER TABLE t1 MODIFY a VARCHAR(32) CHARACTER SET ucs2;
SELECT * FROM v1;

ALTER TABLE t1 MODIFY a VARCHAR(32) CHARACTER SET ucs2 COLLATE ucs2_unicode_ci;
SELECT * FROM v1;

ALTER TABLE t1 MODIFY a VARCHAR(32) CHARACTER SET ucs2 COLLATE ucs2_czech_ci;
SELECT * FROM v1;

ALTER TABLE t1 MODIFY a VARCHAR(32) CHARACTER SET ucs2 COLLATE ucs2_danish_ci;
SELECT * FROM v1;

ALTER TABLE t1 MODIFY a VARCHAR(32) CHARACTER SET utf16;
SELECT * FROM v1;

ALTER TABLE t1 MODIFY a VARCHAR(32) CHARACTER SET utf16 COLLATE utf16_unicode_ci;
SELECT * FROM v1;

ALTER TABLE t1 MODIFY a VARCHAR(32) CHARACTER SET utf16 COLLATE utf16_czech_ci;
SELECT * FROM v1;

# Note, 16 bytes is enough for 16/3= 5 characters
# For the 'aaaaaaaa' value contraction does not break apart
# For the 'caaaaaaa' value contraction breaks apart
ALTER TABLE t1 MODIFY a VARCHAR(32) CHARACTER SET utf16 COLLATE utf16_danish_ci;
SELECT * FROM v1;

ALTER TABLE t1 MODIFY a VARCHAR(32) CHARACTER SET utf16 COLLATE utf16_unicode_520_ci;
SELECT * FROM v1;

ALTER TABLE t1 MODIFY a VARCHAR(32) CHARACTER SET utf32;
SELECT * FROM v1;

ALTER TABLE t1 MODIFY a VARCHAR(32) CHARACTER SET utf32 COLLATE utf32_unicode_ci;
SELECT * FROM v1;

ALTER TABLE t1 MODIFY a VARCHAR(32) CHARACTER SET utf32 COLLATE utf32_czech_ci;
SELECT * FROM v1;

ALTER TABLE t1 MODIFY a VARCHAR(32) CHARACTER SET utf32 COLLATE utf32_danish_ci;
SELECT * FROM v1;

ALTER TABLE t1 MODIFY a VARCHAR(32) CHARACTER SET utf32 COLLATE utf32_unicode_520_ci;
SELECT * FROM v1;

DROP VIEW v1;
DROP TABLE t1;
