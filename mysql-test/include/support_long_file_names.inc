--error 0,3
--perl
my $n = 254 - length($ENV{MYSQLTEST_VARDIR});
my $name = $ENV{MYSQLTEST_VARDIR} . '/' . sprintf("%0${n}d", $$);
if (!open(LONG_NAME,">$name"))
{
  # We cannot rely either on a particular error code, or error string
  # to check that we indeed hit "File name too long".
  # Instead, we will try to create a file with a short name 
  # the same way, in the same location. If it goes all right,
  # we will assume the problem was with the file name length

  open(SHORT_NAME,">$ENV{MYSQLTEST_VARDIR}/$$") ||
    # Even a shorter name could not be created, something else is wrong
    die "Could not create file $ENV{MYSQLTEST_VARDIR}/$$: $!";
  close(SHORT_NAME);
  unlink("$ENV{MYSQLTEST_VARDIR}/$$");
  exit(3);
} else {
  close(LONG_NAME);
  unlink($name);
  exit(0);
}
EOF

# If perl exited with error code 0, the check has passed,
# so the calling test will be executed. 
# If perl exited with error code 3, it will be caught by
# --error above, and processed by the check below.
# If perl exited with some other error code, something went wrong,
# so the test will fail.

if ($errno)
{
  --skip Long file names are not supported
}

