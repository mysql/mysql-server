#
# mysqld --help
#
--source include/not_embedded.inc

#
# force lower-case-table-names=1 (linux/macosx have different defaults)
# force symbolic-links=0 (valgrind build has a different default)
#
exec $MYSQLD_BOOTSTRAP_CMD --symbolic-links=0 --lower-case-table-names=1 --help --verbose > $MYSQL_TMP_DIR/mysqld--help.txt 2>&1;

perl;
  @skipvars=qw/basedir open-files-limit general-log-file log
               log-slow-queries pid-file slow-query-log-file
               datadir slave-load-tmpdir tmpdir/;
  @plugins=qw/innodb ndb ndbcluster safemalloc debug temp-pool ssl des-key-file
              thread-concurrency super-large-pages mutex-deadlock-detector/;
  @env=qw/MYSQLTEST_VARDIR MYSQL_TEST_DIR MYSQL_LIBDIR MYSQL_SHAREDIR/;
  $re1=join('|', @skipvars, @plugins);
  $re2=join('|', @plugins);
  $skip=0;
  open(F, '<', "$ENV{MYSQL_TMP_DIR}/mysqld--help.txt") or die;
  while (<F>) {
    next if 1../The following groups are read/;
    next if /^($re1) /;
    next if /^($re2)-/;
    $skip=0 if /^  -/;
    $skip=1 if / --($re2)\b/;
    # fixes for 32-bit
    y!\\!/!;
    s/\b4294967295\b/18446744073709551615/;
    s/\b2146435072\b/9223372036853727232/;
    s/\b196608\b/262144/;
    foreach $var (@env) { s/$ENV{$var}/$var/ }
    next if /use --skip-(use-)?symbolic-links to disable/; # for valgrind, again
    next if $skip;
    print;
  }
  close F;
EOF

