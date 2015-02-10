#
#-----------------------------------------------------------------------------
# wl6607 : InnoDB FULLTEXT SEARCH: CJK support (mecab parser)
# Adding FTS check with mecab parser
# - Table with mecab parser
# - Basic FTS query ( mainly BOOLEAN MODE check)
# - FTS index with single and two columns
# - external parser index using alter table
#
#------------------------------------------------------------------------------

# restart does not work with embedded
-- source include/not_embedded.inc

call mtr.add_suppression("unknown variable 'loose_mecab_rc_file");
call mtr.add_suppression("Fulltext index charset 'eucjpms' doesn't match mecab charset");
call mtr.add_suppression("Mecab: param.cpp");
call mtr.add_suppression("Plugin 'mecab' init function returned error");

-- disable_query_log

-- error 0, ER_CANT_INITIALIZE_UDF
eval INSTALL PLUGIN mecab SONAME '$MECAB';

let $mecab_charset=`SELECT variable_value FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME='mecab_charset'`;
if ($mecab_charset == '') {
  # Restart with package dictionary.
  let $MYSQL_DATADIR=`select @@datadir`;
  let $MYSQL_BASEDIR=`select @@basedir`;

  let $mecabrc = $MYSQL_DATADIR/mecabrc;
  let $dicdir  = $MYSQL_BASEDIR/lib/mecab/dic/ipadic_$ipadic_charset;

  -- exec echo "dicdir=$dicdir" > $mecabrc

  -- source include/shutdown_mysqld.inc
  -- exec echo "restart: --loose_mecab_rc_file=$mecabrc $MECAB_OPT --innodb_ft_min_token_size=2" >$MYSQLTEST_VARDIR/tmp/mysqld.1.expect
  -- enable_reconnect
  -- source include/wait_until_connected_again.inc
  -- disable_reconnect

  -- disable_query_log

  -- error 0, ER_CANT_INITIALIZE_UDF
  eval INSTALL PLUGIN mecab SONAME '$MECAB';

  let $mecab_charset=`SELECT variable_value FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE VARIABLE_NAME='mecab_charset'`;
}

if ($mecab_charset == '') {
  -- skip Test fail to load mecab parser, please set correct 'loose_mecab_rc_file'.
}

if ($mecab_charset != $mysql_charset) {
  UNINSTALL PLUGIN mecab;
  -- skip Test mecab charset mismatch.
}

--enable_query_log

SHOW STATUS LIKE 'mecab_charset';

SET NAMES utf8;
eval CREATE TABLE page (
  page_id int(8) unsigned NOT NULL AUTO_INCREMENT,
  page_title varchar(255) CHARACTER SET $mecab_charset NOT NULL DEFAULT '',
  PRIMARY KEY (page_id),
  FULLTEXT KEY page_title (page_title) WITH PARSER mecab
) ENGINE=InnoDB;

INSERT INTO page VALUES
(1,'ﾚﾓﾅ'),
(2,'SDレモナ'),
(3,'レモナ'),
(4,'データベース管理システム'),
(5,'キム'),
(6,'鉄道100電車'),
(7,'100'),
(8,'ラフィタ・カスティージョ'),
(9,'ツル科_(Sibley)'),
(10,'ツル'),
(11,'鉄道');

SET GLOBAL innodb_ft_aux_table="test/page";
SELECT * FROM INFORMATION_SCHEMA.INNODB_FT_INDEX_CACHE;
SELECT * FROM INFORMATION_SCHEMA.INNODB_FT_INDEX_TABLE;

SHOW CREATE TABLE page;

SELECT * FROM page WHERE MATCH(page_title) AGAINST("ﾚﾓﾅ" IN BOOLEAN MODE);
SELECT * FROM page WHERE MATCH(page_title) AGAINST("キムショウカン" IN BOOLEAN MODE);
SELECT * FROM page WHERE MATCH(page_title) AGAINST("100" IN BOOLEAN MODE);
SELECT * FROM page WHERE MATCH(page_title) AGAINST("ツル*" IN BOOLEAN MODE);
SELECT * FROM page WHERE MATCH(page_title) AGAINST("ラフィタ・カスティージョ");
SELECT * FROM page WHERE MATCH(page_title) AGAINST("ラフィタ・カスティージョ" IN BOOLEAN MODE);
SELECT * FROM page WHERE MATCH(page_title) AGAINST("+ラフィタ・カスティージョ +データベース" IN BOOLEAN MODE);
SELECT * FROM page WHERE MATCH(page_title) AGAINST("+ラフィタ・カスティージョ +(データベース)" IN BOOLEAN MODE);
SELECT * FROM page WHERE MATCH(page_title) AGAINST("-ラフィタ・カスティージョ +(データベース)" IN BOOLEAN MODE);
SELECT * FROM page WHERE MATCH(page_title) AGAINST("ラフィタ・カスティージョ 鉄道" IN BOOLEAN MODE);
SELECT * FROM page WHERE MATCH(page_title) AGAINST("<ラフィタ・カスティージョ >鉄道" IN BOOLEAN MODE);
SELECT * FROM page WHERE MATCH(page_title) AGAINST('鉄道');
SELECT * FROM page WHERE MATCH(page_title) AGAINST('鉄道' WITH QUERY EXPANSION);

eval ALTER TABLE page add column c1 varchar(100) CHARACTER SET $mecab_charset DEFAULT 'ﾚﾓﾅ';
ALTER TABLE page ADD FULLTEXT INDEX idx1 (page_title,c1) WITH PARSER mecab;
SELECT * FROM page WHERE MATCH(page_title,c1) AGAINST('ﾚﾓﾅ');
SELECT * FROM page WHERE MATCH(page_title,c1) AGAINST('-ﾚﾓﾅ' IN BOOLEAN MODE);

DROP TABLE page;

# Test charset mismatch.
CREATE TABLE page (
  page_id int(8) unsigned NOT NULL AUTO_INCREMENT,
  page_title varchar(255) CHARACTER SET eucjpms NOT NULL DEFAULT '',
  PRIMARY KEY (page_id)
)ENGINE=InnoDB;

INSERT INTO page VALUES (1,'ﾚﾓﾅ');

# We don't return charset mismatch error because of parallel fts build.
CREATE FULLTEXT INDEX ft_idx ON page(page_title) WITH PARSER mecab;

SELECT * FROM INFORMATION_SCHEMA.INNODB_FT_INDEX_CACHE;
SELECT * FROM INFORMATION_SCHEMA.INNODB_FT_INDEX_TABLE;

--error ER_ERROR_ON_WRITE
INSERT INTO page VALUES (2,'ﾚﾓﾅ');

--error ER_ERROR_ON_WRITE
SELECT * FROM page WHERE MATCH(page_title) AGAINST("ﾚﾓﾅ");

DROP TABLE page;

SET GLOBAL innodb_ft_aux_table=default;

UNINSTALL PLUGIN mecab;
