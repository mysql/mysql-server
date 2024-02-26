# Test if MYSQLTEST_VARDIR is a soft link
# If we run in parallel, we have a suffix "/$child_num", so chop off that.

perl;
  use Cwd 'abs_path';
  my $abs_path= abs_path($ENV{'MYSQLTEST_VARDIR'});
  my $path    = $ENV{'MYSQLTEST_VARDIR'};
  my $soft_link= 0;
  $soft_link= 1 if ($abs_path ne $path);
  open (ISLINK, ">" . $ENV{'MYSQL_TMP_DIR'} . "/mtr_var_link");
  print ISLINK "let \$mtr_var_link= $soft_link;\n";
  close ISLINK;
EOF

--source $MYSQL_TMP_DIR/mtr_var_link
--remove_file $MYSQL_TMP_DIR/mtr_var_link

if ($mtr_var_link) {
  --skip Test does not work with var being softlink
}
