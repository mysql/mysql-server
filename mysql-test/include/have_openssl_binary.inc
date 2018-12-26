--disable_query_log
--disable_result_log
let OPENSSL_EXEC_LOG= $MYSQLTEST_VARDIR/log/openssl_exec_log.txt;
let OPENSSL_CONFIG_INC= $MYSQLTEST_VARDIR/log/openssl_binary_config.inc;
--error 0,1
--remove_file $OPENSSL_EXEC_LOG
--error 0,1
--remove_file $OPENSSL_CONFIG_INC

--error 0,1, 127
--exec openssl version 2> $OPENSSL_EXEC_LOG
let STATUS_VAR= $__error;
if ($STATUS_VAR)
{
  --error 0,1
  --remove_file $OPENSSL_EXEC_LOG
  --skip Test requires openssl binary
}

perl;
  use strict;
  my $search_file= $ENV{'OPENSSL_EXEC_LOG'};
  my $search_pattern_1= "can't open config file";
  my $search_pattern_2= "Unable to load config info";
  my $content= "";
  my $dir= $ENV{'MYSQLTEST_VARDIR'};
  open(CONFIG_INC, ">$dir/log/openssl_binary_config.inc");
  open(FILE, "$search_file") or die("Unable to open '$search_file' : $!\n");

  read(FILE, $content, 100, 0);
  close(FILE);

  if ( ($content =~ m{$search_pattern_1}) || ($content =~ m{$search_pattern_2}) ) {
     print CONFIG_INC "let \$STATUS_VAR = 1;\n";
  }
  else {
     print CONFIG_INC "let \$STATUS_VAR = 0;\n";
  }

  close(CONFIG_INC);
EOF

--source $OPENSSL_CONFIG_INC

if ($STATUS_VAR)
{
  --error 0,1
  --remove_file $OPENSSL_EXEC_LOG
  --error 0,1
  --remove_file $OPENSSL_CONFIG_INC
  --skip Test requires openssl binary but either config file for openssl is not found or openssl is unable to load config from the file
}

--error 0,1
--remove_file $OPENSSL_EXEC_LOG
--error 0,1
--remove_file $OPENSSL_CONFIG_INC

--enable_query_log
--enable_result_log
