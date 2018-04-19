perl;
use strict;
use warnings;
my $dest_dir = $ENV{'MYSQLTEST_VARDIR'};
my $prefix_len = $ENV{'PREFIX_LEN'};
open(DATA, ">$dest_dir/tmp/create_table.sql") or die "Couldn't open file
$dest_dir/tmp/create_table.sql, $!";
print DATA "CREATE TABLE t1(";
my $huge_cols = 196;
for (my $i=1; $i <= $huge_cols; $i++) {
        print DATA "c$i TEXT ";

        if ($i != $huge_cols) {
                print DATA ", ";
        }
}

print DATA ", ";
my $max_keys = 64;
for (my $i=1; $i <= $max_keys; $i++) {
        print DATA "KEY k$i(c$i($prefix_len))";

        if ($i != $max_keys) {
                print DATA ", ";
        }
}

print DATA ")ENGINE=INNODB ROW_FORMAT=DYNAMIC charset latin1;";
print DATA "\n";

print DATA "INSERT INTO t1 VALUES (";
for (my $i=1; $i <= $huge_cols; $i++) {
        print DATA "$i$i";
        if ($i != $huge_cols) {
                print DATA ", ";
        }
}
print DATA ");";
print DATA "\n";

print DATA "UPDATE t1 SET ";
for (my $i=1; $i <= $huge_cols; $i++) {
	print DATA " c$i=REPEAT('a', 1)";
        if ($i != $huge_cols) {
                print DATA ", ";
        }
}
print DATA ";";
print DATA "\n";

print DATA "UPDATE t1 SET ";
for (my $i=1; $i <= $huge_cols; $i++) {
	print DATA " c$i=REPEAT('a', $i)";
        if ($i != $huge_cols) {
                print DATA ", ";
        }
}
print DATA ";";
print DATA "\n";

close DATA;
EOF
