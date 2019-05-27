--echo #
--echo # Test that --basedir, --lc-messages-dir, --character-sets-dir
--echo # and --plugin-dir are deduced correctly from the location of
--echo # mysqld executable.
--echo #
--echo # Note that deduction of --character-sets-dir and --plugin-dir does
--echo # not currently work correctly when running from a sandbox build dir,
--echo # (the expected string is printed anyway to avoid diffs).
--echo #

let HELP_VERBOSE_OUTPUT= $MYSQL_TMP_DIR/server.out;
let HVARGS= --no-defaults --secure-file-priv="" --help --verbose >$HELP_VERBOSE_OUTPUT 2>&1;

--echo #
--echo # Deduce --basedir when using full path to mysqld
--echo #
--exec $MYSQLD $HVARGS
--source include/check_dir_settings.inc
--remove_file $HELP_VERBOSE_OUTPUT

--echo #
--echo # Deduce --basedir when using path relative to CWD
--echo #
--perl
  use strict;
  use File::Basename;
  use Config;
  use File::Spec;

  my $binary= $ENV{'MYSQLD'};
  #print "# DBG: binary: $binary\n";
  my $bindir= dirname($binary);
  my $binary= basename($binary);
  #print "# DBG: bindir: $bindir\n";
  chdir("$bindir/..") || die "Cannot chdir to $bindir/..; $!";

  my $relbindir= basename($bindir);
  #print "# DBG: relbindir: $relbindir\n";
  my $exe= File::Spec->catfile($relbindir, $binary);
  my $syscmd= "$exe $ENV{'HVARGS'}";
  #print "# DBG: syscmd: $syscmd\n";

  (system($syscmd) == 0) || die "system($syscmd) failed: $!";
EOF
--source include/check_dir_settings.inc
--remove_file $HELP_VERBOSE_OUTPUT


--echo #
--echo # Deduce --basedir when using bare executable name (PATH lookup)
--echo #
--perl
  use strict;
  use File::Basename;
  use Config;

  #print "# DBG: Config{path_sep'} is $Config{'path_sep'}\n";
  my $binary= $ENV{'MYSQLD'};
  my $bindir= dirname($binary);
  $binary= basename($binary);
  chdir("$bindir/..") || die "Cannot chdir to $bindir/..; $!";

  #print "# DBG: ENV{'PATH'}=\"$bindir$Config{'path_sep'}\$ENV{'PATH'}\"\n";
  $ENV{'PATH'}="$bindir$Config{'path_sep'}$ENV{'PATH'}";
  #print "# DBG: PATH: $ENV{'PATH'}\n";
  my $syscmd= "$binary $ENV{'HVARGS'}";
  #print "# DBG: syscmd: $syscmd\n";
  (system($syscmd) == 0) || die "system($syscmd) failed: $!";
EOF
--source include/check_dir_settings.inc
--remove_file $HELP_VERBOSE_OUTPUT
