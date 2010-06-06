# Test if MYSQLTEST_VARDIR is a soft link
# If we run in parallel, we have a suffix "/$child_num", so chop off that.

perl;
  my $path= $ENV{'MYSQLTEST_VARDIR'};
  $path=~ s|/\d+$||;
  open (ISLINK, ">" . $ENV{'MYSQL_TMP_DIR'} . "/mtr_var_link");
  my $mvr= -l $path ? 1 : 0;
  print ISLINK "let \$mtr_var_link= $mvr;\n";
  close ISLINK;
EOF

--source $MYSQL_TMP_DIR/mtr_var_link
--remove_file $MYSQL_TMP_DIR/mtr_var_link

if ($mtr_var_link) {
  --skip Test does not work with var being softlink
}
