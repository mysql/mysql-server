#
# mysqld --help
#

#
# force lower-case-table-names=1 (linux/macosx have different defaults)
# force symbolic-links=0 (valgrind build has a different default)
#
--let $MYDIR= $MYSQL_TMP_DIR/temp
--mkdir $MYDIR
--let $TMP_DATADIR= $MYDIR/data
--mkdir $TMP_DATADIR

exec $MYSQLD_BOOTSTRAP_CMD --datadir=$TMP_DATADIR --symbolic-links=0 --lower-case-table-names=1 --help --verbose --relay-log=relaylog > $MYSQL_TMP_DIR/mysqld--help.txt 2>/dev/null;

# The inline perl code below will copy $MYSQL_TMP_DIR/mysqld--help.txt
# to output, but filter away some variable stuff (e.g. paths).

perl;
  # Variables which we don't want to display in the result file since
  # their paths may vary:
  @skipvars=qw/basedir open-files-limit general-log-file plugin-dir
               pid-file slow-query-log-file
               language lc-messages-dir
               datadir slave-load-tmpdir tmpdir socket table-definition-cache table-open-cache
               tls-version secure-file-priv/;
               
  # Variables which we don't want to display in result file since
  # they depend on type of build
  @build_specific=qw/auto-generate-certs/;

  # Plugins which may or may not be there:
  @plugins=qw/innodb ngram mecab ndb ndbinfo archive blackhole federated partition ndbcluster debug ssl des-key-file
              thread-concurrency super-large-pages mutex-deadlock-detector null-audit
              sha256-password-private-key-path sha256-password-public-key-path
              sha256-password-auto-generate-rsa-keys keyring-migration-socket
              caching-sha2-password-auto-generate-rsa-keys/;

  # And substitute the content some environment variables with their
  # names:
  @env=qw/MYSQLTEST_VARDIR MYSQL_TEST_DIR MYSQL_CHARSETSDIR MYSQL_SHAREDIR/;

  $re1=join('|', @skipvars, @build_specific, @plugins);
  $re2=join('|', @build_specific, @plugins);
  $skip=0;
  open(F, '<', "$ENV{MYSQL_TMP_DIR}/mysqld--help.txt") or die;
  while (<F>) {
    next if 1../The following groups are read/;
    # formatting, skip line consisting entirely of dashes and blanks
    next if /^[\- ]+\s?$/;
    next if /Value \(after reading options\)/; # skip table header
    next if /^($re1) /;
    next if /^($re2)-/;
    $skip=0 if /^  -/;
    $skip=1 if / --($re2)\b/;
    y!\\!/!;
    s/[ ]+/ /; # squeeze spaces to remove table formatting
    # fixes for 32-bit
    s/\b4294967295\b/18446744073709551615/;
    s/\b2146435072\b/9223372036853727232/;
    # s/\b196608\b/262144/;
    # s/\b524288\b/262144/;
    # s/\b393216\b/262144/;
    s/\b573440\b/262144/;
    s/\b286720\b/262144/;
    s/\b442368\b/262144/;
    s/\b221184\b/262144/;
    # Replacing port number with string
    s/^port \d+/port ####/;
    # log-tc-size varies by platform
    s/^log-tc-size \d+/log-tc-size #####/;
    # Avoid test failures when adding new error message
    s/^performance-schema-error-size \d+/performance-schema-error-size ####/;
    foreach $var (@env) { s/$ENV{$var}/$var/ }
    next if /use --skip-(use-)?symbolic-links to disable/; # for valgrind, again
    next if $skip;
    print;
  }
  close F;
EOF

--rmdir $TMP_DATADIR
--rmdir $MYDIR
