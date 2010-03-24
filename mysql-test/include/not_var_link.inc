perl;
  open (ISLINK, ">" . $ENV{'MYSQL_TMP_DIR'} . "/mtr_var_link");
  my $mvr= -l $ENV{'MYSQLTEST_VARDIR'} ? 1 : 0;
  print ISLINK "let \$mtr_var_link= $mvr;\n";
  close ISLINK;
EOF

--source $MYSQL_TMP_DIR/mtr_var_link
--remove_file $MYSQL_TMP_DIR/mtr_var_link

if ($mtr_var_link) {
  --skip Test does not work with var being softlink
}
