# BUG#58455
# Starting mysqld with defaults file without extension cause
# segmentation fault

source include/not_windows.inc;

--let MDF_LOG= $MYSQLTEST_VARDIR/tmp/defaults.$_server_id.log

# All these tests refer to configuration files that do not exist

--error 1
exec $MYSQLD --defaults-file=/path/with/no/extension --print-defaults 2>$MDF_LOG;
--perl
  use strict;
  my $mysqld_log= $ENV{'MDF_LOG'};
  open(MYSQLD_LOG, $mysqld_log) || die "1 - Failed to open '$mysqld_log': $!";
  while(<MYSQLD_LOG>) {
    if (!(/NOTIFY_SOCKET not set in environment/) &&
        !(/Invalid systemd notify socket, cannot send: /)) {
      s/mysqld-debug/mysqld/;
      print; }}
  close(MYSQLD_LOG);
EOF

--error 1
exec $MYSQLD --defaults-file=/path/with.ext --print-defaults 2>$MDF_LOG;
--perl
  use strict;
  my $mysqld_log= $ENV{'MDF_LOG'};
  open(MYSQLD_LOG, $mysqld_log) || die "2 - Failed to open '$mysqld_log': $!";
  while(<MYSQLD_LOG>) {
    if (!(/NOTIFY_SOCKET not set in environment/) &&
        !(/Invalid systemd notify socket, cannot send: /)) {
      s/mysqld-debug/mysqld/;
      print; }}
  close(MYSQLD_LOG);
EOF

# Using $MYSQL_TEST_DIR_ABS which contains canonical path to the
# test directory since --print-default prints the absolute path.
--error 1
exec $MYSQLD --defaults-file=relative/path/with.ext --print-defaults 2>$MDF_LOG;
--perl
  use strict;
  my $mysqld_log= $ENV{'MDF_LOG'};
  my $test_dir= $ENV{'MYSQL_TEST_DIR_ABS'};
  open(MYSQLD_LOG, $mysqld_log) || die "3 - Failed to open '$mysqld_log': $!";
  while(<MYSQLD_LOG>) {
    if (!(/NOTIFY_SOCKET not set in environment/) &&
        !(/Invalid systemd notify socket, cannot send: /)) {
      s/mysqld-debug/mysqld/;
      s/$test_dir/MYSQL_TEST_DIR/;
      print; }}
  close(MYSQLD_LOG);
EOF

--error 1
exec $MYSQLD --defaults-file=relative/path/without/extension --print-defaults 2>$MDF_LOG;
--perl
  use strict;
  my $mysqld_log= $ENV{'MDF_LOG'};
  my $test_dir= $ENV{'MYSQL_TEST_DIR_ABS'};
  open(MYSQLD_LOG, $mysqld_log) || die "4 - Failed to open '$mysqld_log': $!";
  while(<MYSQLD_LOG>) {
    if (!(/NOTIFY_SOCKET not set in environment/) &&
        !(/Invalid systemd notify socket, cannot send: /)) {
      s/mysqld-debug/mysqld/;
      s/$test_dir/MYSQL_TEST_DIR/;
      print; }}
  close(MYSQLD_LOG);
EOF

--error 1
exec $MYSQLD --defaults-file=with.ext --print-defaults 2>$MDF_LOG;
--perl
  use strict;
  my $mysqld_log= $ENV{'MDF_LOG'};
  my $test_dir= $ENV{'MYSQL_TEST_DIR_ABS'};
  open(MYSQLD_LOG, $mysqld_log) || die "5 - Failed to open '$mysqld_log': $!";
  while(<MYSQLD_LOG>) {
    if (!(/NOTIFY_SOCKET not set in environment/) &&
        !(/Invalid systemd notify socket, cannot send: /)) {
      s/mysqld-debug/mysqld/;
      s/$test_dir/MYSQL_TEST_DIR/;
      print; }}
  close(MYSQLD_LOG);
EOF

--error 1
exec $MYSQLD --defaults-file=no_extension --print-defaults 2>$MDF_LOG;
--perl
  use strict;
  my $mysqld_log= $ENV{'MDF_LOG'};
  my $test_dir= $ENV{'MYSQL_TEST_DIR_ABS'};
  open(MYSQLD_LOG, $mysqld_log) || die "6 - Failed to open '$mysqld_log': $!";
  while(<MYSQLD_LOG>) {
    if (!(/NOTIFY_SOCKET not set in environment/) &&
        !(/Invalid systemd notify socket, cannot send: /)) {
      s/mysqld-debug/mysqld/;
      s/$test_dir/MYSQL_TEST_DIR/;
      print; }}
  close(MYSQLD_LOG);
EOF

--remove_file $MDF_LOG
