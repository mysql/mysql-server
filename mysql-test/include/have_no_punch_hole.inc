let $MYSQLD_DATADIR= `SELECT @@datadir`;
LET MYSQLD_DATADIR = `SELECT @@datadir`;

--disable_query_log
--disable_warnings
CREATE TABLE t1(c1 INT PRIMARY KEY) COMPRESSION="ZLIB";
let COMPR_ZIP_WARN= `SHOW WARNINGS`;
--enable_warnings
--enable_query_log

perl;
  use strict;
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

if (!$no_holes)
{
  --skip Test requires 'No punch hole support'
}
