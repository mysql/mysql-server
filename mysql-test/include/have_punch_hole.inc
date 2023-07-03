<<<<<<< HEAD
--let MYSQLD_DATADIR = `SELECT @@datadir`

--disable_query_log
--error 0,ER_INNODB_COMPRESSION_FAILURE
CREATE TABLE t1(c1 INT PRIMARY KEY) COMPRESSION="ZLIB" TABLESPACE=innodb_file_per_table;
let COMPR_ZIP_ERROR= `SHOW ERRORS`;
=======
let $MYSQLD_DATADIR= `SELECT @@datadir`;
LET MYSQLD_DATADIR = `SELECT @@datadir`;

--disable_query_log
--disable_warnings
CREATE TABLE t1(c1 INT PRIMARY KEY) COMPRESSION="ZLIB";
let COMPR_ZIP_WARN= `SHOW WARNINGS`;
--enable_warnings
>>>>>>> pr/231
--enable_query_log

perl;
  use strict;
<<<<<<< HEAD
  my $has_support = ($ENV{COMPR_ZIP_ERROR} =~ /Punch hole not supported/)? 0 : 1;
### we do not expect any other error
  printf("Unexpected error: %s\n",$ENV{COMPR_ZIP_ERROR})
    if ($has_support and $ENV{COMPR_ZIP_ERROR} ne '');
  open(DHF,">$ENV{'MYSQLD_DATADIR'}/compr.inc");
  printf DHF "let \$has_support= %s;\n",$has_support;
  close(DHF);
EOF

--disable_query_log
--disable_warnings
drop table if exists t1;
--enable_warnings
--enable_query_log

--source $MYSQLD_DATADIR/compr.inc
--remove_file $MYSQLD_DATADIR/compr.inc

if (!$has_support)
=======
  my $no_holes = ($ENV{COMPR_ZIP_WARN} =~ /Punch hole not supported/)? 1 : 0;
### we do not expect any other warning
  printf("Unexpected warning: %s\n",$ENV{COMPR_ZIP_WARN})
    if (not $no_holes and $ENV{COMPR_ZIP_WARN} ne '');
  open(DHF,">$ENV{'MYSQLD_DATADIR'}/compr.inc");
  printf DHF "let \$no_holes= %s;\n",$no_holes;
  close(DHF);
EOF

--source $MYSQLD_DATADIR/compr.inc
--remove_file $MYSQLD_DATADIR/compr.inc

--disable_query_log
DROP TABLE t1;
--enable_query_log

if ($no_holes)
>>>>>>> pr/231
{
  --skip Test requires 'Punch hole support'
}
