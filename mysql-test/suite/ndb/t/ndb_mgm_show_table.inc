#
# ndb_mgm_show_table.inc - will execute ndb_mgm -e show command and
# mask the details that are unwanted for the test(version details, node id,etc)
# and make the result consistent.
#

#
# Build strings for masking ndb_mgm output
#

#echo MYSQL_SERVER_VERSION: $MYSQL_SERVER_VERSION;

# extract X.X.X for MySQL
let $mysql_version=
  `select SUBSTRING_INDEX('$MYSQL_SERVER_VERSION','-', 1)`;
# Remove any trailing a in the X.X.X string(ugly but works)
let $mysql_version=
  `select TRIM(TRAILING 'a' FROM '$mysql_version')`;
let $mysql_version = mysql-$mysql_version;
# echo mysql_version: $mysql_version;

# load ndb-X.X.X from the "ndb_version_string" variable, trim off
# the version status indicator i.e -dmr etc.
let $ndb_version=
  `select REGEXP_SUBSTR(@@ndb_version_string, "^ndb-[0-9]*[.][0-9]*[.][0-9]*")`;
# echo ndb_version: $ndb_version;

#masking mgm version and id
--disable_query_log
let $dump_file = $MYSQLTEST_VARDIR/tmp/ndb_mgm_show.txt;

# Dump the output of ndb_mgm to file
--exec $NDB_MGM --verbose=0 -e show > $dump_file;

# Load the file into a temporary table
CREATE TEMPORARY TABLE ndb_mgm_show (a varchar(512));

eval LOAD DATA INFILE '$dump_file' INTO TABLE ndb_mgm_show
  FIELDS TERMINATED BY '\n';
--remove_file $dump_file

# Remove Windows line feeds
UPDATE ndb_mgm_show SET a = TRIM(TRAILING "\r" FROM a);


--replace_result $mysql_version MYSQL_VERSION $ndb_version NDB_VERSION
SELECT a as 'ndbd(s)' FROM ndb_mgm_show
  WHERE SUBSTR(a, 4, 3) IN (1, 2, 3, 4);


--replace_result $mysql_version MYSQL_VERSION $ndb_version NDB_VERSION
SELECT a as 'ndb_mgmd(s)' FROM ndb_mgm_show
  WHERE SUBSTR(a, 4, 3) IN (5);

--replace_result $mysql_version MYSQL_VERSION $ndb_version NDB_VERSION
SELECT a as 'mysqld(s)' FROM ndb_mgm_show
  WHERE SUBSTR(a, 4, 3)
    IN (6,7,8,9,10,11,12,13,14,15,16,17,18,19)
      ORDER BY LOCATE("accepting", a);

DROP TABLE ndb_mgm_show;
--enable_query_log
