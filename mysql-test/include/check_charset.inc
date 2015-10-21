
--disable_query_log
SET sql_mode='NO_ENGINE_SUBSTITUTION';
--enable_query_log

if ($check_std_csets)
{
  CREATE DATABASE db1_std_csets;
  USE db1_std_csets;
  eval SET NAMES $cset;
  eval CREATE TABLE t_$cset (a VARCHAR(30) CHARACTER SET $cset COLLATE $coll) CHARACTER SET $cset COLLATE $coll;
  eval INSERT INTO t_$cset VALUES (_$cset'MYSQLPUMP'), (_$cset'cāt?기?會意');
  eval SELECT HEX(a) , CHAR_LENGTH(a) FROM db1_std_csets.t_$cset ORDER BY 1;
  --exec $MYSQL_PUMP --databases db1_std_csets > $MYSQLTEST_VARDIR/tmp/db1_std_csets.sql

  DROP DATABASE db1_std_csets;
  CREATE DATABASE db1_std_csets;
  --exec $MYSQL db1_std_csets < $MYSQLTEST_VARDIR/tmp/db1_std_csets.sql
  --remove_file $MYSQLTEST_VARDIR/tmp/db1_std_csets.sql

  eval SET NAMES $cset;
  eval SELECT HEX(a) , CHAR_LENGTH(a) FROM db1_std_csets.t_$cset ORDER BY 1;
  DROP DATABASE db1_std_csets;
}

if ($check_ucs2_csets)
{
  CREATE DATABASE db1_ucs2_csets CHARACTER SET utf8;
  USE db1_ucs2_csets;
  SET NAMES utf8;
  eval CREATE TABLE t_$cset (a CHAR(4) CHARACTER SET utf8) CHARACTER SET utf8;

  --replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
  eval LOAD DATA INFILE '$MYSQLTEST_VARDIR/std_data/charset_utf8.txt' INTO TABLE t_$cset;
  eval ALTER TABLE t_$cset CHANGE a a CHAR(4) CHARACTER SET $cset COLLATE $coll;
  eval SELECT COUNT(*) FROM t_$cset;

  --exec $MYSQL_PUMP --databases db1_ucs2_csets > $MYSQLTEST_VARDIR/tmp/db1_ucs2_csets.sql
  eval RENAME TABLE t_$cset TO old_t_$cset;
  --exec $MYSQL db1_ucs2_csets < $MYSQLTEST_VARDIR/tmp/db1_ucs2_csets.sql
  --remove_file $MYSQLTEST_VARDIR/tmp/db1_ucs2_csets.sql

  eval SELECT COUNT(*) FROM t_$cset;
  DROP DATABASE db1_ucs2_csets;
}

if ($check_utf8_csets)
{
  eval CREATE DATABASE db1_utf8_csets CHARACTER SET $cset;
  USE db1_utf8_csets;
  eval CREATE TABLE t_$cset (a CHAR(4) CHARACTER SET $cset COLLATE $coll) CHARACTER SET $cset COLLATE $coll;

  --replace_result $MYSQLTEST_VARDIR MYSQLTEST_VARDIR
  eval LOAD DATA INFILE '$MYSQLTEST_VARDIR/std_data/charset_utf8.txt' INTO TABLE t_$cset;
  eval SELECT COUNT(*) FROM t_$cset;

  --exec $MYSQL_PUMP --databases db1_utf8_csets > $MYSQLTEST_VARDIR/tmp/db1_utf8_csets.sql
  eval RENAME TABLE t_$cset TO old_t_$cset;
  --exec $MYSQL db1_utf8_csets < $MYSQLTEST_VARDIR/tmp/db1_utf8_csets.sql
  --remove_file $MYSQLTEST_VARDIR/tmp/db1_utf8_csets.sql

  eval SELECT COUNT(*) FROM t_$cset;
  DROP DATABASE db1_utf8_csets;
}

if ($check_for_object_names)
{
  eval SET NAMES utf8;
  CREATE DATABASE `Êý¾Ý¿â`;
  USE `Êý¾Ý¿â`;
  eval CREATE TABLE ÔÁÂÌÉÃÁ (ÐÏÌÑ VARCHAR(30) CHARACTER SET $cset COLLATE $coll) CHARACTER SET $cset COLLATE $coll;
  eval CREATE VIEW `±í¸ñ` AS SELECT _$cset'×Ö¶ÎÒ»';
  eval INSERT INTO ÔÁÂÌÉÃÁ VALUES (_$cset'MYSQLPUMP'), (_$cset'한글입력기');
  eval SELECT HEX(ÐÏÌÑ) , CHAR_LENGTH(ÐÏÌÑ) FROM ÔÁÂÌÉÃÁ ORDER BY 1;
  eval CREATE FUNCTION `straÃe` ( `饽紴缽菠` CHAR(20) CHARACTER SET $cset COLLATE $coll)
       RETURNS CHAR(50)  CHARACTER SET $cset DETERMINISTIC RETURN `饽紴缽菠`;

  SELECT * FROM `±í¸ñ`;
  eval SELECT `straÃe`(_$cset'한글');

  --exec $MYSQL_PUMP --exclude-databases=mysql,mtr,sys > $MYSQLTEST_VARDIR/tmp/db1_non_ascii.sql

  DROP DATABASE `Êý¾Ý¿â`;
  --exec $MYSQL < $MYSQLTEST_VARDIR/tmp/db1_non_ascii.sql
  --remove_file $MYSQLTEST_VARDIR/tmp/db1_non_ascii.sql

  eval SET NAMES utf8;
  USE `Êý¾Ý¿â`;
  eval SELECT HEX(ÐÏÌÑ) , CHAR_LENGTH(ÐÏÌÑ) FROM ÔÁÂÌÉÃÁ ORDER BY 1;
  eval SELECT `straÃe`(_$cset'한글');
  SELECT * FROM `±í¸ñ`;
  DROP DATABASE `Êý¾Ý¿â`;
}
