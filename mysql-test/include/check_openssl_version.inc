--disable_query_log
--disable_result_log

let OPENSSL_VERSION_INFO= $MYSQLTEST_VARDIR/log/openssl_version_info.txt;
let OPENSSL_CONFIG_INC= $MYSQLTEST_VARDIR/log/openssl_binary_config.inc;

--error 0,1
--remove_file $OPENSSL_VERSION_INFO
--error 0,1
--remove_file $OPENSSL_CONFIG_INC

--error 0,1, 127
--exec $OPENSSL_EXECUTABLE version > $OPENSSL_VERSION_INFO

perl;
 use strict;
 my $search_file= $ENV{'OPENSSL_VERSION_INFO'};
 my $search_pattern_1= "0\\.9.*";
 my $search_pattern_2= "1\\.0\\.0.*";
 my $search_pattern_3= "1\\.0\\.1.*";
 my $content= "";
 my $dir= $ENV{'MYSQLTEST_VARDIR'};
 open(CONFIG_INC, ">$dir/log/openssl_binary_config.inc");
 open(FILE, "$search_file") or die("Unable to open '$search_file' : $!\n");
 read(FILE, $content, 100, 0);
 close(FILE);

 if ( ($content =~ m{$search_pattern_1}) || ($content =~ m{$search_pattern_2}) ||
      ($content =~ m{$search_pattern_3}) ) {
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
 --remove_file $OPENSSL_VERSION_INFO
 --error 0,1
 --remove_file $OPENSSL_CONFIG_INC
 --skip Test requires openssl version to be 1.0.2+
}

--error 0,1
--remove_file $OPENSSL_VERSION_INFO
--error 0,1
--remove_file $OPENSSL_CONFIG_INC

--enable_query_log
--enable_result_log
