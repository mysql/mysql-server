--let MYSQLD_DATADIR = `SELECT @@datadir`

--disable_query_log
--let $innodb_file_per_table_orig=`select @@innodb_file_per_table`
SET GLOBAL innodb_file_per_table=1;

--disable_query_log
--error 0,ER_INNODB_COMPRESSION_FAILURE
CREATE TABLE t1(c1 INT PRIMARY KEY) COMPRESSION="ZLIB";
let COMPR_ZIP_ERROR= `SHOW ERRORS`;
--enable_query_log

perl;
  use strict;
  my $has_support = ($ENV{COMPR_ZIP_ERROR} =~ /Punch hole not supported/)? 0 : 1;
### we do not expect any other error
  printf("Unexpected error: %s\n",$ENV{COMPR_ZIP_ERROR})
    if ($has_support and $ENV{COMPR_ZIP_ERROR} ne '');
  open(DHF,">$ENV{'MYSQLD_DATADIR'}/compr.inc");
  printf DHF "let \$has_support= %s;\n",$has_support;
  close(DHF);
EOF

--disable_query_log
EVAL SET GLOBAL innodb_file_per_table=$innodb_file_per_table_orig;

--disable_warnings
drop table if exists t1;
--enable_warnings
--enable_query_log

--source $MYSQLD_DATADIR/compr.inc
--remove_file $MYSQLD_DATADIR/compr.inc

if (!$has_support)
{
  --skip Test requires 'Punch hole support'
}
