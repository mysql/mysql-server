# Verifies that we are on linux, and that version returned by uname -r is
# at least as large as $minimum_required_linux_version.
# Otherwise it skips the test.

--source 'include/linux.inc'

if (!$minimum_required_linux_version){
  --die You must specify $minimum_required_linux_version
}

let MINIMUM_REQUIRED_LINUX_VERSION_FOR_PERL = $minimum_required_linux_version;
let LINUX_VERSION_RESULT_FILE = $MYSQLTEST_VARDIR/log/linux_version_result.inc;

perl;
  use strict;

  # To keep it simple we only look at major and minor parts of uname -r,
  # as later parts may contain non-digits and it is not clear how we
  # should handle that.
  # Don't be tempted to use $Config{osvers} here, as there are machines
  # on which `uname` correctly reports 2.6 and $Config reports 3.8.
  my $version= (split /\n/, `uname -r | cut -d '.' -f 1-2`)[0];
  my $minimum_required_version= $ENV{'MINIMUM_REQUIRED_LINUX_VERSION_FOR_PERL'};

  # Retrieving result from --perl command is non-trivial as of today, so
  # we need to create an *.inc file on the fly, that will contain result.
  open(RESULT_FILE, ">$ENV{'LINUX_VERSION_RESULT_FILE'}");

  # This would be much easier using CPAN::Version, but we can't rely on it
  # being avialable in the environment. This simple implementation cares only
  # about major and minor numbers. It was tested for 4 < 4.1, 4.1 < 4.10,
  # 4.12 < 4.111 and 3.4 < 4.1 (and their mirror images).

  my @version_parts = split /\./, $version;
  my @minimum_required_version_parts = split /\./, $minimum_required_version;

  if ((@version_parts[0] <=> @minimum_required_version_parts[0] ||
       @version_parts[1] <=> @minimum_required_version_parts[1]) < 0) {
    print RESULT_FILE "let \$linux_version_is_ok = 0;\n";
    print RESULT_FILE "let \$found_linux_version = $version;\n";
  } else {
    print RESULT_FILE "let \$linux_version_is_ok = 1;\n";
  }
  close(RESULT_FILE);
EOF

--source $LINUX_VERSION_RESULT_FILE
--remove_file $LINUX_VERSION_RESULT_FILE

if (!$linux_version_is_ok)
{
  skip Needs Linux $minimum_required_linux_version, found $found_linux_version;
}
