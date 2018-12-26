# Print the size of the given file in bytes. The file name
# is given in the environment variable FILE_TO_CHECK. The
# file name FILE_TO_CHECK is given relative to the data directory.
#
# Usage:
#
# let MYSQLD_DATADIR=`SELECT @@datadir`;
# let FILE_TO_CHECK=test/t1.ibd;
# --source include/show_file_size.inc
#

perl;
 my $file = $ENV{'MYSQLD_DATADIR'}.$ENV{'FILE_TO_CHECK'};
 my $size = -s "$file";
 print "The size of file '" . $ENV{FILE_TO_CHECK} . "' is $size bytes.\n";
EOF

