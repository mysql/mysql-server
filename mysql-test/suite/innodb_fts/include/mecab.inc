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


call mtr.add_suppression("unknown variable 'loose_mecab_rc_file");
call mtr.add_suppression("Fulltext index charset 'big5' doesn't match mecab charset");
call mtr.add_suppression("Mecab: createModel.* failed: param.cpp");
call mtr.add_suppression("Plugin 'mecab' init function returned error");

-- disable_query_log

-- error 0, ER_CANT_INITIALIZE_UDF

# Restart with package dictionary
let $MYSQL_DATADIR=`select @@datadir`;
let MYSQL_BASEDIR=`select @@basedir`;
let MECABRC=$MYSQL_DATADIR/mecabrc;
let DICDIR=ipadic_$ipadic_charset;

# In standard layout libdir is
#  $basedir/lib
# while RPMS (el and sles) uses
#  $basedir/lib/mysql
#  $basedir/lib64/mysql
#
# Fedora, Debian and Ubuntu uses system mecab
#
# Fedora has
#  UTF-8 in %{_libdir}/mecab/dic/ipadic
#  EUCJP in %{_libdir}/mecab/dic/ipadic-EUCJP
#
# Debian and Ubuntu have
#  UTF-8 in /var/lib/mecab/dic/ipadic-utf8
#  EUCJP in /var/lib/mecab/dic/ipadic
#
--perl
  use strict;
  use lib "lib/";
  use My::Find;
  my $dir = "$ENV{'DICDIR'}";
  my $basedir = $ENV{'MYSQL_BASEDIR'};

  my $dicdir = my_find_dir("$basedir",
			   ['lib/mecab/dic',           # Bundled mecab
			    'lib/mysql/mecab/dic',     # 32 bits RPMS
			    'lib64/mysql/mecab/dic'] , # 64 bits RPMS
			   ["$dir"], 1);

  if ($dicdir eq '' && index($dir, 'utf-8') != -1) {
    # Fedora
    $dicdir = my_find_dir("/usr",
			  ['lib/mecab/dic',            # System mecab 32 bits
			   'lib64/mecab/dic'],         # System mecab 64 bits
			  ["ipadic"], 1);
    # Debian/Ubuntu
    if ($dicdir eq '') {
        $dicdir = my_find_dir("/var",
			      ['lib/mecab/dic'],       # System mecab
			      ["ipadic-utf8"], 1);
    }
  } elsif ($dicdir eq '' && index($dir, 'euc-jp') != -1) {
    # Fedora
    $dicdir = my_find_dir("/usr",
			  ['lib/mecab/dic/',           # System mecab 32 bits
			   'lib64/mecab/dic/', ],      # System mecab 64 bits
			  ["ipadic-EUCJP"], 1);
    # Debian/Ubuntu
    if ($dicdir eq '') {
        $dicdir = my_find_dir("/var",
			      ['lib/mecab/dic'],       # System mecab
			      ["ipadic"], 1);
    }
  }
  open(my $rc, '>', "$ENV{'MECABRC'}") || die "Could not open $ENV{'MECABRC'}: $!";
  print $rc "dicdir =  ${dicdir}\n";
  close $rc;
EOF

-- source include/shutdown_mysqld.inc
-- exec echo "restart: --loose_mecab_rc_file=$mecabrc $MECAB_OPT --innodb_ft_min_token_size=2" >$MYSQLTEST_VARDIR/tmp/mysqld.1.expect
-- source include/wait_until_connected_again.inc

-- disable_query_log

-- error 0, ER_CANT_INITIALIZE_UDF
eval INSTALL PLUGIN mecab SONAME '$MECAB';

let $mecab_charset=`SELECT variable_value FROM performance_schema.global_status WHERE VARIABLE_NAME='mecab_charset'`;

if ($mecab_charset == '') {
  --remove_file $MYSQL_DATADIR/mecabrc
  -- skip Test fail to load mecab parser, IPADIC files for $ipadic_charset might be missing.
}

--enable_query_log

SHOW STATUS LIKE 'mecab_charset';

SET NAMES utf8mb3;
eval CREATE TABLE page (
  page_id int(8) unsigned NOT NULL AUTO_INCREMENT,
  page_title varchar(255) CHARACTER SET $mysql_charset NOT NULL DEFAULT '',
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
SELECT * FROM INFORMATION_SCHEMA.INNODB_FT_INDEX_CACHE
ORDER BY WORD, DOC_ID, POSITION;
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

eval ALTER TABLE page add column c1 varchar(100) CHARACTER SET $mysql_charset DEFAULT 'ﾚﾓﾅ';
ALTER TABLE page ADD FULLTEXT INDEX idx1 (page_title,c1) WITH PARSER mecab;
SELECT * FROM page WHERE MATCH(page_title,c1) AGAINST('ﾚﾓﾅ');
SELECT * FROM page WHERE MATCH(page_title,c1) AGAINST('-ﾚﾓﾅ' IN BOOLEAN MODE);

DROP TABLE page;

# Test charset mismatch.
CREATE TABLE page (
  page_id int(8) unsigned NOT NULL AUTO_INCREMENT,
  page_title varchar(255) CHARACTER SET big5 NOT NULL DEFAULT '',
  PRIMARY KEY (page_id)
)ENGINE=InnoDB;

INSERT INTO page VALUES (1,'日本');

--error ER_ERROR_ON_WRITE
CREATE FULLTEXT INDEX ft_idx ON page(page_title) WITH PARSER mecab;

SELECT * FROM INFORMATION_SCHEMA.INNODB_FT_INDEX_CACHE
ORDER BY WORD, DOC_ID, POSITION;
SELECT * FROM INFORMATION_SCHEMA.INNODB_FT_INDEX_TABLE;

INSERT INTO page VALUES (2,'日本');

--error ER_FT_MATCHING_KEY_NOT_FOUND
SELECT * FROM page WHERE MATCH(page_title) AGAINST('日本');

DROP TABLE page;

# Test utf8mb4
if ($mecab_charset == 'utf8mb4')
{
  SET NAMES utf8mb4;

  CREATE TABLE page (
    page_id int(8) unsigned NOT NULL AUTO_INCREMENT,
    page_title varchar(255) CHARACTER SET utf8mb4 NOT NULL DEFAULT '',
    PRIMARY KEY (page_id),
    FULLTEXT KEY page_title (page_title) WITH PARSER mecab
  ) ENGINE=InnoDB;

  # insert some emoji chars with 4 bytes.
  INSERT INTO page VALUES
    (1,'😃ﾚﾓﾅ'),
    (2,'鉄道😄'),
    (3,'鉄道😊100電車');

  SET GLOBAL innodb_ft_aux_table="test/page";
  SELECT * FROM INFORMATION_SCHEMA.INNODB_FT_INDEX_CACHE
  ORDER BY WORD, DOC_ID, POSITION;
  SELECT * FROM INFORMATION_SCHEMA.INNODB_FT_INDEX_TABLE;

  SHOW CREATE TABLE page;

  SELECT * FROM page WHERE MATCH(page_title) AGAINST("ﾚﾓﾅ" IN BOOLEAN MODE);
  SELECT * FROM page WHERE MATCH(page_title) AGAINST('鉄道');
  SELECT * FROM page WHERE MATCH(page_title) AGAINST('鉄道😄');
  SELECT * FROM page WHERE MATCH(page_title) AGAINST('鉄道' WITH QUERY EXPANSION);

  DROP TABLE page;
}

# Test eucjpms & cp932
if ($mysql_win_charset)
{
  eval CREATE TABLE page (
    page_id int(8) unsigned NOT NULL AUTO_INCREMENT,
    page_title varchar(255) CHARACTER SET $mysql_win_charset NOT NULL DEFAULT '',
    PRIMARY KEY (page_id),
    FULLTEXT KEY page_title (page_title) WITH PARSER mecab
  ) ENGINE=InnoDB;

  # insert some eucjpms & cp932 chars.
  INSERT INTO page VALUES
    (1,'ⅰﾚﾓﾅ'),
    (2,'鉄道ⅹ'),
    (3,'鉄道ⅴ100電車');

  SET GLOBAL innodb_ft_aux_table="test/page";
  SELECT * FROM INFORMATION_SCHEMA.INNODB_FT_INDEX_CACHE
  ORDER BY WORD, DOC_ID, POSITION;
  SELECT * FROM INFORMATION_SCHEMA.INNODB_FT_INDEX_TABLE;

  SHOW CREATE TABLE page;

  SELECT * FROM page WHERE MATCH(page_title) AGAINST("ﾚﾓﾅ" IN BOOLEAN MODE);
  SELECT * FROM page WHERE MATCH(page_title) AGAINST('鉄道');
  SELECT * FROM page WHERE MATCH(page_title) AGAINST('鉄道ⅴ');
  SELECT * FROM page WHERE MATCH(page_title) AGAINST('鉄道' WITH QUERY EXPANSION);

  DROP TABLE page;
}

# Parser failures during deferred FTS tokenization must be visible to the
# client. This failure path is independent of the document charset, so run
# the oversized-input case only for utf8mb4 rather than repeating the costly
# parse in every charset variant. MeCab rejects sentences larger than its
# internal input limit.
if ($mysql_charset == utf8mb4)
{
  call mtr.add_suppression("Mecab: parse\\(\\) failed: too long sentence");

  SET NAMES utf8mb4;
  CREATE TABLE t1 (
    id INT PRIMARY KEY,
    doc LONGTEXT CHARACTER SET utf8mb4,
    FULLTEXT KEY doc (doc) WITH PARSER mecab
  ) ENGINE=InnoDB;

  INSERT INTO t1 VALUES
    (1, REPEAT('すもももももももものうち。', 300000));

  DROP TABLE t1;
}

SET GLOBAL innodb_ft_aux_table=default;

UNINSTALL PLUGIN mecab;
--remove_file $MYSQL_DATADIR/mecabrc
