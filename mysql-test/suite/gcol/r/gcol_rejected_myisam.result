SET @@session.default_storage_engine = 'MyISAM';
CREATE TABLE t1 (
pk INTEGER PRIMARY KEY,
i8 BIGINT,
i4 INTEGER,
i2 SMALLINT,
i1 TINYINT,
i8u BIGINT UNSIGNED,
i4u INTEGER UNSIGNED,
i2u SMALLINT UNSIGNED,
i1u TINYINT UNSIGNED,
f4 REAL,
f8 DOUBLE PRECISION,
dc DECIMAL(10, 2),
fc CHAR(10),
vc VARCHAR(10),
fb BINARY(8),
vb VARBINARY(8),
d  DATE,
t  TIME,
dt DATETIME,
ts TIMESTAMP
);
INSERT INTO t1 VALUES
(0, -9223372036854775808, -2147483648, -32768, -128,
0, 0, 0, 0, -99999999.99, -99999999.99, -99999999.99,
'abcdefgh', 'abcdefgh', 'abcdefgh', 'abcdefgh',
DATE '1000-01-01', TIME '00:00:00', TIMESTAMP '1000-01-01 00:00:00',
TIMESTAMP '1970-01-01 12:00:00'),
(1, 9223372036854775807, 2147483647, 32767, 127,
18446744073709551615, 4294967295, 65535, 255, 99999999.99, 99999999.99,
99999999.99, 'abcdefgh', 'abcdefgh', 'abcdefgh', 'abcdefgh',
DATE '9999-12-31', TIME '23:59:59', TIMESTAMP '9999-12-31 23:59:59',
TIMESTAMP '2038-01-01 23:59:59');
ALTER TABLE t1 ADD gc BIGINT UNSIGNED AS (i8) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc BIGINT AS (fc) STORED;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc BIGINT AS (vc) STORED;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc BIGINT AS (fb) STORED;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc BIGINT AS (vb) STORED;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc INTEGER AS (i8) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc INTEGER AS (i8u) STORED;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc INTEGER UNSIGNED AS (i8) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc INTEGER UNSIGNED AS (i8u) STORED;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc INTEGER UNSIGNED AS (i4) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc INTEGER AS (fc) STORED;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc INTEGER AS (vc) STORED;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc INTEGER AS (fb) STORED;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc INTEGER AS (vb) STORED;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc INTEGER AS (d) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc INTEGER AS (dt) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc INTEGER AS (ts) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT AS (i8) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT AS (i8u) STORED;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc SMALLINT AS (i4) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT AS (i4u) STORED;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc SMALLINT AS (dc) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT AS (f4) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT AS (f8) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT UNSIGNED AS (i8) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT UNSIGNED AS (i8u) STORED;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc SMALLINT UNSIGNED AS (i4) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT UNSIGNED AS (i4u) STORED;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc SMALLINT UNSIGNED AS (i2) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT UNSIGNED AS (dc) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT UNSIGNED AS (f4) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT UNSIGNED AS (f8) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT AS (fc) STORED;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT AS (vc) STORED;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT AS (fb) STORED;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT AS (vb) STORED;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT AS (t) STORED;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc SMALLINT AS (d) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT AS (dt) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT AS (ts) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT AS (i8) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT AS (i8u) STORED;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc TINYINT AS (i4) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT AS (i4u) STORED;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc TINYINT AS (i2) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT AS (i2u) STORED;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc TINYINT AS (dc) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT AS (f4) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT AS (f8) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT UNSIGNED AS (i8) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT UNSIGNED AS (i8u) STORED;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc TINYINT UNSIGNED AS (i4) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT UNSIGNED AS (i4u) STORED;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc TINYINT UNSIGNED AS (i2) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT UNSIGNED AS (i2u) STORED;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc TINYINT UNSIGNED AS (i1) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT UNSIGNED AS (dc) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT UNSIGNED AS (f4) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT UNSIGNED AS (f8) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT AS (fc) STORED;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT AS (vc) STORED;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT AS (fb) STORED;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT AS (vb) STORED;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT AS (t) STORED;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc TINYINT AS (d) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT AS (dt) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT AS (ts) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc DECIMAL(10,2) AS (i8) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc DECIMAL(10,2) AS (i8u) STORED;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc DECIMAL(10,2) AS (i4) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc DECIMAL(10,2) AS (i4u) STORED;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc DECIMAL(10,2) AS (fc) STORED;
ERROR HY000: Incorrect decimal value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DECIMAL(10,2) AS (vc) STORED;
ERROR HY000: Incorrect decimal value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DECIMAL(10,2) AS (fb) STORED;
ERROR HY000: Incorrect decimal value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DECIMAL(10,2) AS (vb) STORED;
ERROR HY000: Incorrect decimal value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DECIMAL(10,2) AS (dt) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc DECIMAL(10,2) AS (ts) STORED;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATE AS (i8) STORED;
ERROR 22007: Incorrect date value: '-9223372036854775808' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATE AS (i4) STORED;
ERROR 22007: Incorrect date value: '-2147483648' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATE AS (i2) STORED;
ERROR 22007: Incorrect date value: '-32768' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATE AS (i1) STORED;
ERROR 22007: Incorrect date value: '-128' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATE AS (dc) STORED;
ERROR 22007: Incorrect date value: '-99999999.99' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATE AS (f4) STORED;
ERROR 22007: Incorrect date value: '-99999999.99' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATE AS (fc) STORED;
ERROR 22007: Incorrect date value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATE AS (vc) STORED;
ERROR 22007: Incorrect date value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATE AS (fb) STORED;
ERROR 22007: Incorrect date value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATE AS (vb) STORED;
ERROR 22007: Incorrect date value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIME AS (i8) STORED;
ERROR 22007: Incorrect time value: '-9223372036854775808' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIME AS (i4) STORED;
ERROR 22007: Incorrect time value: '-2147483648' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIME AS (i2) STORED;
ERROR 22007: Incorrect time value: '-32768' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIME AS (dc) STORED;
ERROR 22007: Incorrect time value: '-99999999.99' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIME AS (f4) STORED;
ERROR 22007: Incorrect time value: '-99999999.99' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIME AS (fc) STORED;
ERROR 22007: Incorrect time value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIME AS (vc) STORED;
ERROR 22007: Incorrect time value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIME AS (fb) STORED;
ERROR 22007: Incorrect time value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIME AS (vb) STORED;
ERROR 22007: Incorrect time value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATETIME AS (i8) STORED;
ERROR 22007: Incorrect datetime value: '-9223372036854775808' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATETIME AS (i4) STORED;
ERROR 22007: Incorrect datetime value: '-2147483648' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATETIME AS (i2) STORED;
ERROR 22007: Incorrect datetime value: '-32768' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATETIME AS (i1) STORED;
ERROR 22007: Incorrect datetime value: '-128' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATETIME AS (dc) STORED;
ERROR 22007: Incorrect datetime value: '-99999999.99' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATETIME AS (f4) STORED;
ERROR 22007: Incorrect datetime value: '-99999999.99' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATETIME AS (fc) STORED;
ERROR 22007: Incorrect datetime value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATETIME AS (vc) STORED;
ERROR 22007: Incorrect datetime value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATETIME AS (fb) STORED;
ERROR 22007: Incorrect datetime value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATETIME AS (vb) STORED;
ERROR 22007: Incorrect datetime value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIMESTAMP AS (i8) STORED;
ERROR 22007: Incorrect datetime value: '-9223372036854775808' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIMESTAMP AS (i4) STORED;
ERROR 22007: Incorrect datetime value: '-2147483648' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIMESTAMP AS (i2) STORED;
ERROR 22007: Incorrect datetime value: '-32768' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIMESTAMP AS (i1) STORED;
ERROR 22007: Incorrect datetime value: '-128' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIMESTAMP AS (dc) STORED;
ERROR 22007: Incorrect datetime value: '-99999999.99' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIMESTAMP AS (f4) STORED;
ERROR 22007: Incorrect datetime value: '-99999999.99' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIMESTAMP AS (fc) STORED;
ERROR 22007: Incorrect datetime value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIMESTAMP AS (vc) STORED;
ERROR 22007: Incorrect datetime value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIMESTAMP AS (fb) STORED;
ERROR 22007: Incorrect datetime value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIMESTAMP AS (vb) STORED;
ERROR 22007: Incorrect datetime value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc BIGINT UNSIGNED AS (i8) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc BIGINT AS (fc) VIRTUAL;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc BIGINT AS (vc) VIRTUAL;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc BIGINT AS (fb) VIRTUAL;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc BIGINT AS (vb) VIRTUAL;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc INTEGER AS (i8) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc INTEGER AS (i8u) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc INTEGER UNSIGNED AS (i8) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc INTEGER UNSIGNED AS (i8u) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc INTEGER UNSIGNED AS (i4) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc INTEGER AS (fc) VIRTUAL;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc INTEGER AS (vc) VIRTUAL;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc INTEGER AS (fb) VIRTUAL;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc INTEGER AS (vb) VIRTUAL;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc INTEGER AS (d) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc INTEGER AS (dt) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc INTEGER AS (ts) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT AS (i8) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT AS (i8u) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc SMALLINT AS (i4) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT AS (i4u) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc SMALLINT AS (dc) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT AS (f4) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT AS (f8) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT UNSIGNED AS (i8) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT UNSIGNED AS (i8u) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc SMALLINT UNSIGNED AS (i4) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT UNSIGNED AS (i4u) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc SMALLINT UNSIGNED AS (i2) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT UNSIGNED AS (dc) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT UNSIGNED AS (f4) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT UNSIGNED AS (f8) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT AS (fc) VIRTUAL;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT AS (vc) VIRTUAL;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT AS (fb) VIRTUAL;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT AS (vb) VIRTUAL;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT AS (t) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc SMALLINT AS (d) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT AS (dt) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc SMALLINT AS (ts) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT AS (i8) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT AS (i8u) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc TINYINT AS (i4) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT AS (i4u) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc TINYINT AS (i2) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT AS (i2u) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc TINYINT AS (dc) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT AS (f4) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT AS (f8) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT UNSIGNED AS (i8) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT UNSIGNED AS (i8u) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc TINYINT UNSIGNED AS (i4) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT UNSIGNED AS (i4u) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc TINYINT UNSIGNED AS (i2) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT UNSIGNED AS (i2u) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc TINYINT UNSIGNED AS (i1) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT UNSIGNED AS (dc) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT UNSIGNED AS (f4) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT UNSIGNED AS (f8) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT AS (fc) VIRTUAL;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT AS (vc) VIRTUAL;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT AS (fb) VIRTUAL;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT AS (vb) VIRTUAL;
ERROR HY000: Incorrect integer value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT AS (t) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc TINYINT AS (d) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT AS (dt) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc TINYINT AS (ts) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc DECIMAL(10,2) AS (i8) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc DECIMAL(10,2) AS (i8u) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc DECIMAL(10,2) AS (i4) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc DECIMAL(10,2) AS (i4u) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 2
ALTER TABLE t1 ADD gc DECIMAL(10,2) AS (fc) VIRTUAL;
ERROR HY000: Incorrect decimal value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DECIMAL(10,2) AS (vc) VIRTUAL;
ERROR HY000: Incorrect decimal value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DECIMAL(10,2) AS (fb) VIRTUAL;
ERROR HY000: Incorrect decimal value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DECIMAL(10,2) AS (vb) VIRTUAL;
ERROR HY000: Incorrect decimal value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DECIMAL(10,2) AS (dt) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc DECIMAL(10,2) AS (ts) VIRTUAL;
ERROR 22003: Out of range value for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATE AS (i8) VIRTUAL;
ERROR 22007: Incorrect date value: '-9223372036854775808' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATE AS (i4) VIRTUAL;
ERROR 22007: Incorrect date value: '-2147483648' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATE AS (i2) VIRTUAL;
ERROR 22007: Incorrect date value: '-32768' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATE AS (i1) VIRTUAL;
ERROR 22007: Incorrect date value: '-128' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATE AS (dc) VIRTUAL;
ERROR 22007: Incorrect date value: '-99999999.99' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATE AS (f4) VIRTUAL;
ERROR 22007: Incorrect date value: '-99999999.99' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATE AS (fc) VIRTUAL;
ERROR 22007: Incorrect date value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATE AS (vc) VIRTUAL;
ERROR 22007: Incorrect date value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATE AS (fb) VIRTUAL;
ERROR 22007: Incorrect date value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATE AS (vb) VIRTUAL;
ERROR 22007: Incorrect date value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIME AS (i8) VIRTUAL;
ERROR 22007: Incorrect time value: '-9223372036854775808' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIME AS (i4) VIRTUAL;
ERROR 22007: Incorrect time value: '-2147483648' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIME AS (i2) VIRTUAL;
ERROR 22007: Incorrect time value: '-32768' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIME AS (dc) VIRTUAL;
ERROR 22007: Incorrect time value: '-99999999.99' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIME AS (f4) VIRTUAL;
ERROR 22007: Incorrect time value: '-99999999.99' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIME AS (fc) VIRTUAL;
ERROR 22007: Incorrect time value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIME AS (vc) VIRTUAL;
ERROR 22007: Incorrect time value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIME AS (fb) VIRTUAL;
ERROR 22007: Incorrect time value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIME AS (vb) VIRTUAL;
ERROR 22007: Incorrect time value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATETIME AS (i8) VIRTUAL;
ERROR 22007: Incorrect datetime value: '-9223372036854775808' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATETIME AS (i4) VIRTUAL;
ERROR 22007: Incorrect datetime value: '-2147483648' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATETIME AS (i2) VIRTUAL;
ERROR 22007: Incorrect datetime value: '-32768' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATETIME AS (i1) VIRTUAL;
ERROR 22007: Incorrect datetime value: '-128' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATETIME AS (dc) VIRTUAL;
ERROR 22007: Incorrect datetime value: '-99999999.99' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATETIME AS (f4) VIRTUAL;
ERROR 22007: Incorrect datetime value: '-99999999.99' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATETIME AS (fc) VIRTUAL;
ERROR 22007: Incorrect datetime value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATETIME AS (vc) VIRTUAL;
ERROR 22007: Incorrect datetime value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATETIME AS (fb) VIRTUAL;
ERROR 22007: Incorrect datetime value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc DATETIME AS (vb) VIRTUAL;
ERROR 22007: Incorrect datetime value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIMESTAMP AS (i8) VIRTUAL;
ERROR 22007: Incorrect datetime value: '-9223372036854775808' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIMESTAMP AS (i4) VIRTUAL;
ERROR 22007: Incorrect datetime value: '-2147483648' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIMESTAMP AS (i2) VIRTUAL;
ERROR 22007: Incorrect datetime value: '-32768' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIMESTAMP AS (i1) VIRTUAL;
ERROR 22007: Incorrect datetime value: '-128' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIMESTAMP AS (dc) VIRTUAL;
ERROR 22007: Incorrect datetime value: '-99999999.99' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIMESTAMP AS (f4) VIRTUAL;
ERROR 22007: Incorrect datetime value: '-99999999.99' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIMESTAMP AS (fc) VIRTUAL;
ERROR 22007: Incorrect datetime value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIMESTAMP AS (vc) VIRTUAL;
ERROR 22007: Incorrect datetime value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIMESTAMP AS (fb) VIRTUAL;
ERROR 22007: Incorrect datetime value: 'abcdefgh' for column 'gc' at row 1
ALTER TABLE t1 ADD gc TIMESTAMP AS (vb) VIRTUAL;
ERROR 22007: Incorrect datetime value: 'abcdefgh' for column 'gc' at row 1
drop table t1;
DROP VIEW  IF EXISTS v1,v2;
DROP TABLE IF EXISTS t1,t2,t3;
DROP PROCEDURE IF EXISTS p1;
DROP FUNCTION IF EXISTS f1;
DROP TRIGGER IF EXISTS trg1;
DROP TRIGGER IF EXISTS trg2;
set sql_warnings = 0;
