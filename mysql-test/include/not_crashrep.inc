# Check if CrashReporter is enabled and would open a window

perl;
sub skip_test {
  # Only relevant on Mac OS X
  return 0 unless $^O eq 'darwin';
  my $crep= `defaults read com.apple.CrashReporter DialogType`;
  return 0 if $?;
  chomp ($crep);
  $crep= lc $crep;
  return ($crep eq 'basic' || $crep eq 'developer');
}
my $skip= skip_test();
open (F, ">" . $ENV{'MYSQL_TMP_DIR'} . "/crashrep.inc");
print F "let \$crashrep= $skip;\n";
close F;
EOF

--source $MYSQL_TMP_DIR/crashrep.inc
--remove_file $MYSQL_TMP_DIR/crashrep.inc

if ($crashrep) {
  --skip CrashReporter would popup a window
}

# the test will crash mysqld. flush tables now to make sure that
# system tables aren't corrupted by the crash
--disable_query_log ONCE
FLUSH TABLES;

