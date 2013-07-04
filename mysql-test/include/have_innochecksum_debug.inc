let FILE= $MYSQLTEST_VARDIR/tmp/innochecksum-debug;
--exec $INNOCHECKSUM --version > $FILE
perl;
use strict;
open(FILE, $ENV{FILE}) or die;
$_ = <FILE>;
close(FILE);
my $debug = /innochecksum-debug /;
open(FILE, ">$ENV{FILE}") or die;
print FILE "let \$debug= $debug;\n";
close(FILE);
EOF

--source $FILE
--remove_file $FILE

if (!$debug) {
  --skip Test requires innochecksum-debug
}
