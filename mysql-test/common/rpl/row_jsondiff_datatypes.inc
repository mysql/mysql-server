# ==== Requirements ====
#
# See common/rpl/row_jsondiff_init.inc for an overview.
#
# This script sets up the following scenarios:
# - updates that insert values using all different SQL data types

--echo ******** Data types ********

--delimiter |
let $rpl_sql=
  SET SQL_LOG_BIN = 0;
  DROP TABLE IF EXISTS t;
  CREATE TABLE t (
    i INT $pk,
    j JSON,

    nullint INT DEFAULT NULL,
    emptystring CHAR(0) DEFAULT '',
    quotestring VARCHAR(100), # No default

    bit_ BIT DEFAULT 1,
    bit_50_ BIT(50) DEFAULT b'1111100000111110000011111000001111100000',
    bool_false BOOL DEFAULT false,
    bool_true BOOL DEFAULT true,

    tinyint_ TINYINT DEFAULT -100,
    tinyint_1_ TINYINT(1) DEFAULT -9,
    tinyint_u_ TINYINT UNSIGNED DEFAULT 250,
    tinyint_1_u_ TINYINT(1) UNSIGNED DEFAULT 9,
    smallint_ SMALLINT DEFAULT -12345,
    smallint_3_ SMALLINT(3) DEFAULT -987,
    smallint_u_ SMALLINT UNSIGNED DEFAULT 45678,
    smallint_3_u_ SMALLINT(3) UNSIGNED DEFAULT 987,
    int_ INT DEFAULT -1234567,
    int_6_ INT(6) DEFAULT -123456,
    int_u_ INT UNSIGNED DEFAULT 3222111000,
    int_6_u_ INT(6) UNSIGNED DEFAULT 987654,
    bigint_ BIGINT DEFAULT -5444333222111,
    bigint_12_ BIGINT(16) DEFAULT -6555444333222111,
    bigint_u_ BIGINT UNSIGNED DEFAULT 5444333222111,
    bigint_12_u_ BIGINT(12) UNSIGNED DEFAULT 6555444333222111,

    decimal_ DECIMAL DEFAULT -987654321,
    decimal_10_5_ DECIMAL(10, 5) DEFAULT -12345.6789,
    decimal_u_ DECIMAL UNSIGNED DEFAULT 987654321,
    decimal_10_5_u_ DECIMAL(10, 5) UNSIGNED DEFAULT 12345.6789,
    float_ FLOAT DEFAULT -5.4321e+20,
    float_4_2_ FLOAT(4, 2) DEFAULT -43.21,
    float_u_ FLOAT UNSIGNED DEFAULT 6.54321e+10,
    float_4_2_u_ FLOAT(4, 2) UNSIGNED DEFAULT 12.34,
    double_ DOUBLE DEFAULT -9.87654321e+300,
    double_20_10_ DOUBLE(20, 10) DEFAULT -9.8765e+9,
    double_u_ DOUBLE UNSIGNED DEFAULT 9.87654321e+300,
    double_20_10_u_ DOUBLE(20, 10) UNSIGNED DEFAULT 9.8765e+9,

    date_ DATE DEFAULT '2017-05-19',
    datetime_ DATETIME DEFAULT '2017-05-20 6:00:00',
    datetime_6_ DATETIME(6) DEFAULT '2017-05-21 7:01:01.000001',
    timestamp_ TIMESTAMP DEFAULT '2017-05-22 8:02:02',
    timestamp_6_ TIMESTAMP(6) DEFAULT '2017-05-23 9:03:03.000003',
    time_ TIME DEFAULT '10:04:04',
    time_6_ TIME(6) DEFAULT '10:05:05.000005',
    year_ YEAR DEFAULT 2018,

    binary_ BINARY DEFAULT 'a',
    binary_10_ BINARY(10) DEFAULT '\x00\x01',
    varbinary_1000_ VARBINARY(1000) DEFAULT '\xaa',
    tinyblob_ TINYBLOB, # cannot have default
    blob_ BLOB, # cannot have default
    blob_10_ BLOB(10), # cannot have default
    mediumblob_ MEDIUMBLOB, # cannot have default
    longblob_ LONGBLOB, # cannot have default

    char_ CHAR DEFAULT 'g',
    char_10_ CHAR(10) DEFAULT 'hij',
    varchar_1000_ VARCHAR(1000) DEFAULT 'klm',
    tinytext_ TINYTEXT, # cannot have default
    text_ TEXT, # cannot have default
    text_10_ TEXT(10), # cannot have default
    mediumtext_ MEDIUMTEXT, # cannot have default
    longtext_ LONGTEXT, # cannot have default

    enum_ ENUM('abc', 'def', 'ghi') DEFAULT 'def',
    set_ SET('jkl', 'mno', 'pqr') DEFAULT 'jkl,pqr',

    json_ JSON # cannot have default
  );

  # Doing explicit insert instead of setting the rows variable, since
  # mtr gets confused by escapes and to easier make use of default
  # values.
  INSERT INTO t SET
    i = 1,
    j = '["xyzw", 0]',
    # Using numeric character codes to not confuse mtr.  Omitting
    # CHAR(1) and \f because there is no escape sequence that mysql
    # understands for them.  Omitting CHAR(0) and \Z becuase
    # mysqlbinlog -v does not output the escape sequence that mysql
    # understands for them.  This is not supposed to be exact SQL
    # anyways.
    #                   backslash  quote    doublequote backtick dollar
    quotestring = CONCAT(CHAR(92), CHAR(39), CHAR(34), CHAR(96), CHAR(36), '%', '_', '\b\n\r\t'),

    tinyblob_ = CHAR(0xbb),
    blob_ = CHAR(0xcc),
    blob_10_ = CHAR(0xdd),
    mediumblob_ = CHAR(0xee),
    longblob_ = CHAR(0xff),

    tinytext_ = 'nop',
    text_ = 'qrs',
    text_10_ = 'tuv',
    mediumtext_ = 'wxy',
    longtext_ = 'z',

    json_ = '{ "a" : [ 1, 2.3, null, false, "text" ] }';
  SET SQL_LOG_BIN = 1;
|
--delimiter ;
--source include/rpl/for_each_server_stmt.inc

--let $stmt_pre= UPDATE t SET j = JSON_SET(j, '$dollar[1]',
--let $stmt_post= ) WHERE i = 1
--let $echo_select= j
--let $rows=
--let $column_def=
# Sizes may differ because decoded rows don't preserve types exactly
--let $compare_size_columns=
# Echo decoded rows only on master, skip it on slave_2 and slave_4
# because it is too big to read
--let $echo_decoded_rows= 2

# Loop variables
--let $column_name= tmp
--let $position= 3
while ($column_name)
{
  --let $column_name= `SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'test' AND TABLE_NAME = 't' AND ORDINAL_POSITION = $position`
  --let $column_type= `SELECT COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = 'test' AND TABLE_NAME = 't' AND ORDINAL_POSITION = $position`

  if ($column_name)
  {
    --let $desc= Update from $column_type column
    --let $stmt= $column_name
    --source $scenario_inc
  }

  --inc $position
}
