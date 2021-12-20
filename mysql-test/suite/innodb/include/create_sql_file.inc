# --------------------------------------------------------------------
# This file is used to generate INSTANT DDL statements based on the
# INPUT (operation type (ADD/DROP), column position).
# --------------------------------------------------------------------

#################### Perl module starts ####################
perl;

my $total_col = $ENV{'total_col'};
my $input = $ENV{'input'};

my $Columns =  $ENV{'Columns'};
my @col_name_array = split(' ', $Columns);
print "@col_name_array\n";

my $ICol1 = $ENV{'ICol1'};
my $ICol2 = $ENV{'ICol2'};
my $ICol3 = $ENV{'ICol3'};
my $ICol4 = $ENV{'ICol4'};
my $ICol5 = $ENV{'ICol5'};
my $ICol6 = $ENV{'ICol6'};
my @col_descr_array = ($ICol1, $ICol2, $ICol3, $ICol4, $ICol5, $ICol6);

my $vardir = $ENV{'MYSQLTEST_VARDIR'};
my $filename="$vardir/sql_file.inc";
open(OP_FILE, '>', $filename) or die "$!";

print OP_FILE "SET SESSION debug= '+d,show_dropped_column';\n";
scan_input();

close OP_FILE;

# Subroutine to print the validate SQL in generated test script
sub insert_validate_query {
my $validate_query_string = "
--disable_query_log
let \$table_id = `SELECT TABLE_ID FROM INFORMATION_SCHEMA.INNODB_TABLES WHERE NAME='test/t1'`;

eval SELECT NAME, POS, MTYPE, PRTYPE, VERSION_ADDED, VERSION_DROPPED
  FROM INFORMATION_SCHEMA.INNODB_COLUMNS
  WHERE TABLE_ID=\$table_id;

eval SELECT NAME, N_COLS, INSTANT_COLS, TOTAL_ROW_VERSIONS,
  INITIAL_COLUMN_COUNTS, CURRENT_COLUMN_COUNTS, TOTAL_COLUMN_COUNTS
  FROM INFORMATION_SCHEMA.INNODB_TABLES
  WHERE TABLE_ID=\$table_id;

SET SESSION debug= '+d,skip_dd_table_access_check';
SELECT ID INTO \@tid FROM mysql.tables WHERE NAME='t1';
--replace_result \$table_id TABLE_ID
SELECT NAME, ORDINAL_POSITION, TYPE, HAS_NO_DEFAULT, HIDDEN, SE_PRIVATE_DATA
  FROM mysql.columns
  WHERE TABLE_ID=\@tid ORDER BY ORDINAL_POSITION;
--enable_query_log

SELECT * FROM t1;

--echo #---------------------------------------------\n\n";

print OP_FILE $validate_query_string;
}

# Subroutine to scan input and generate INSTANT DDLs in test script
sub scan_input {
  my @token_array = split(' ', $input);

  my $count = 0;
  my $operation = $token_array[$count++];

  # Skip =
  $count++;

  my $instant_col = 0;
  $token = $token_array[$count++];
  while ($token ne '') {
    my $command = "ALTER TABLE t1 $operation COLUMN ";

    my @temp_array = split(' ', $col_descr_array[$ic]);
    my $inst_col_name = $temp_array[0];

    my @pos_array= split('', $token);
    my $final_pos = $pos_array[1];

    if ($final_pos eq ']') {
      $final_pos = $total_col;
      if ($operation eq "ADD") {
        $total_col++;
        $command = $command.$col_descr_array[$ic];
        push @col_name_array, $inst_col_name;
        $ic++;
      } else {
        die "ERROR INPUT STRING";
      }
    } else {
      if ($operation eq "ADD") {
        $total_col++;
        $command = $command."$col_descr_array[$ic] AFTER $col_name_array[$final_pos - 1] ";
        splice @col_name_array, $final_pos, 0, $inst_col_name;
        $ic++;
      } else {
        $command = $command."$col_name_array[$final_pos - 1]";
        splice @col_name_array, $final_pos - 1, 1;
      }
    }

    $command = $command.", ALGORITHM=INSTANT;";
    print OP_FILE "$command\n";
    print OP_FILE "--echo # Column name array @col_name_array\n";
    insert_validate_query();

    $token = @token_array[$count++];
  }

  # Write the final column list so that it could be taken for next test if needed.
  print OP_FILE "--let Columns = @col_name_array\n";
}

EOF
#################### Perl module ends ####################
