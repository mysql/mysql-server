# Windows-specific tests
--source include/windows.inc

# Check if the variable MY_PERROR is set
--source include/have_perror.inc

########################################################
############# Skip if Non-English Windows

perl;
open(FILE, ">", "$ENV{MYSQL_TMP_DIR}/perror_syslocale.inc") or die $!;

if(`systeminfo /FO LIST | findstr /C:"System Locale"` =~ m/en-us/)
 {print FILE "let \$non_eng_sys= 0;\n";}
else
 {print FILE "let \$non_eng_sys= 1;\n";}
 
close FILE;
EOF

source $MYSQL_TMP_DIR/perror_syslocale.inc;
remove_file $MYSQL_TMP_DIR/perror_syslocale.inc;

if ($non_eng_sys)
{
  skip Need an English Windows Installation;
}

############# Skip if Non-English Windows
########################################################

--exec $MY_PERROR 150 2>&1
--exec $MY_PERROR 23 2>&1
--exec $MY_PERROR 1062 2>&1
--error 1
--exec $MY_PERROR 30000 2>&1

