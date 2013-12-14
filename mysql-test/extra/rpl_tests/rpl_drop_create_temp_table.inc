--source include/have_innodb.inc
--disable_abort_on_error

if (`SELECT HEX(@commands) = HEX('configure')`)
{
  connection master;

  #
  # Index that is used as an Id to tables that trigger errors.
  #
  --let $tt_error_counter=0
  --let $nt_error_counter=0

  #
  # Creates a T-table that is never dropped.
  #
  --eval CREATE TABLE tt_xx_1 ( id INT ) ENGINE = Innodb

  #
  # Creates a N-table that is never dropped.
  #
  --eval CREATE TABLE nt_xx_1 ( id INT ) ENGINE = MyIsam

  #
  # Creates a T-table that is never dropped.
  #
  --eval CREATE TABLE tt_error_1 ( id INT, PRIMARY KEY (id) ) ENGINE = Innodb

  #
  # Creates a N-table that is never dropped.
  #
  --eval CREATE TABLE nt_error_1 ( id INT, PRIMARY KEY (id) ) ENGINE = MyIsam

  #
  # Creates a T-table that is never dropped.
  #
  --eval CREATE TABLE tt_error_2 ( id INT, PRIMARY KEY (id) ) ENGINE = Innodb

  #
  # Creates a N-table that is never dropped.
  #
  --eval CREATE TABLE nt_error_2 ( id INT, PRIMARY KEY (id) ) ENGINE = MyIsam

  #
  # Create a trigger nt_error_2 --> tt_error_2
  #
  DELIMITER |;
  CREATE TRIGGER tr_i_nt_2_to_tt_2 AFTER INSERT ON nt_error_2 FOR EACH ROW
  BEGIN
    DECLARE in_stmt_id INTEGER;
    INSERT INTO tt_error_2(id) VALUES (NEW.id);
  END|
  DELIMITER ;|

  #
  # Creates a Temporary N-table that is never dropped.
  #
  --eval CREATE TEMPORARY TABLE nt_tmp_xx_1 ( id INT ) ENGINE = MyIsam

  #
  # Creates a Temporary N-table that is never dropped.
  #
  --eval CREATE TEMPORARY TABLE tt_tmp_xx_1 ( id INT ) ENGINE = Innodb

  #
  # In what follows, we create a set of tables that are used
  # throughout this test case. The number of tables to be
  # created is give by the variable $tot_table.
  #
  #
  # Creates Temporay N-tables that are automatically dropped and recreated
  # when a command ends.
  #
  --let $n= $tot_table
  while ($n)
  { 
    --eval DROP TEMPORARY TABLE IF EXISTS nt_tmp_$n
    --eval CREATE TEMPORARY TABLE nt_tmp_$n ( id INT ) ENGINE = MyIsam
    --disable_query_log
    --eval SET @check_temp='$available_n_temp'
    --enable_query_log
    #
    # Updates the $available_n_temp that keeps track of the created
    # temporary N-tables.
    # 
    if (`SELECT HEX(@check_temp) != HEX('')`)
    {
      --let $available_n_temp= $available_n_temp,nt_tmp_$n
    }
    if (`SELECT HEX(@check_temp) = HEX('')`)
    {
      --let $available_n_temp= nt_tmp_$n
    }
    --dec $n
  }

  #
  # Creates Temporay T-tables that are automatically dropped and recreated
  # when a command ends.
  #
  --let $n= $tot_table
  while ($n)
  { 
    --eval DROP TEMPORARY TABLE IF EXISTS tt_tmp_$n
    --eval CREATE TEMPORARY TABLE tt_tmp_$n ( id INT ) ENGINE = Innodb
    --disable_query_log
    --eval SET @check_temp='$available_t_temp'
    --enable_query_log
    #
    # Updates the $available_t_temp that keeps track of the created
    # temporary T-tables.
    # 
    if (`SELECT HEX(@check_temp) != HEX('')`)
    {
      --let $available_t_temp= $available_t_temp,tt_tmp_$n
    }
    if (`SELECT HEX(@check_temp) = HEX('')`)
    {
      --let $available_t_temp= tt_tmp_$n
    }
    --dec $n
  }

  #
  # Creates N-tables that are automatically dropped and recreated
  # when a command ends.
  #
  --let $n= $tot_table
  while ($n)
  { 
    --eval DROP TABLE IF EXISTS nt_$n
    --eval CREATE TABLE nt_$n ( id INT ) ENGINE = MyIsam
    --disable_query_log
    --eval SET @check_temp='$available_n'
    --enable_query_log
    #
    # Updates the $available_n that keeps track of the created
    # N-tables.
    # 
    if (`SELECT HEX(@check_temp) != HEX('')`)
    {
      --let $available_n= $available_n,nt_$n
    }
    if (`SELECT HEX(@check_temp) = HEX('')`)
    {
      --let $available_n= nt_$n
    }
    --dec $n
  }

  #
  # Creates T-tables that are automatically dropped and recreated
  # when a command ends.
  #
  --let $n= $tot_table
  while ($n)
  { 
    --eval DROP TABLE IF EXISTS tt_$n
    --eval CREATE TABLE tt_$n ( id INT ) ENGINE = Innodb
    --disable_query_log
    --eval SET @check_temp='$available_t'
    --enable_query_log
    #
    # Updates the $available_t that keeps track of the created
    # T-tables.
    # 
    if (`SELECT HEX(@check_temp) != HEX('')`)
    {
      --let $available_t= $available_t,tt_$n
    }
    if (`SELECT HEX(@check_temp) = HEX('')`)
    {
      --let $available_t= tt_$n
    }
    --dec $n
  }

  --let $dropped_t_temp=
  --let $dropped_n_temp=

  --let $dropped_t=
  --let $dropped_n=

  let $pos_trans_command= query_get_value("SHOW MASTER STATUS", Position, 1);

  SET @commands= '';
}

#
# Drops tables and synchronizes master and slave.
#

if (`SELECT HEX(@commands) = HEX('clean')`)
{
  connection master;

  DROP TABLE IF EXISTS tt_xx_1;

  DROP TABLE IF EXISTS nt_xx_1;

  DROP TABLE IF EXISTS tt_error_1;

  DROP TABLE IF EXISTS nt_error_1;

  DROP TABLE IF EXISTS tt_error_2;

  DROP TABLE IF EXISTS nt_error_2;

  DROP TEMPORARY TABLE IF EXISTS tt_tmp_xx_1;
  DROP TEMPORARY TABLE IF EXISTS nt_tmp_xx_1;

  --let $n= $tot_table
  while ($n)
  { 
    --eval DROP TABLE IF EXISTS nt_$n
    --eval DROP TEMPORARY TABLE IF EXISTS tt_tmp_$n
    --eval DROP TEMPORARY TABLE IF EXISTS nt_tmp_$n
    --dec $n
  }

  --let $n= $tot_table
  while ($n)
  { 
    --eval DROP TABLE IF EXISTS tt_$n
    --dec $n
  }

  sync_slave_with_master;

  SET @commands= '';
}

#
# This is the core of the test is responsible for processing
# the following commands:
#
# B - Begin
# C - Commit
# R - Rollback
#
#
# T - Inserts a row into a T-table
# N-Temp - Inserts a row into a temporary N-table.
#
#
# T-SELECT-N-Temp - Selects from a temporary N-table and inserts
#                   into a T-table.
# N-SELECT-N-Temp - Selects from a temporary N-table and inserts
#                   into a N-table.
# T-SELECT-T-Temp - Selects from a temporary T-table and inserts
#                   into a T-table.
# N-SELECT-T-Temp - Selects from a temporary T-table and inserts
#                   into a N-table.
#
#
# Create-N-Temp - Creates a temporary N-table if a temporary N-table
#                 was dropped before
# Create-T-Temp - Creates a temporary T-table if a temporary T-table
#                 was dropped before
#
#
# Drop-Temp-T-Temp - Drops a temporary T-table if there is any
# Drop-Temp-N-Temp - Drops a temporary N-table if there is any
# Drop-Temp-TN-Temp - Drops both a temporary T-table and N-table if there
#                     is any
# Drop-Temp-TT-Temp - Drops two temporary T-tables if there is any
# Drop-Temp-NN-Temp - Drops two temporary N-tables if there is any
# Drop-Temp-Xe-Temp - Tries to drop a temporary table that does not exist
# Drop-Temp-NXe-Temp - Drops a temporary N-table if there is any and
#                      a temporary table that does not exist
# Drop-Temp-TXe-Temp - Drops a temporary T-table if there is any and
#                      a temporary table that does not exist
#
#
# Drop-Temp-If-Xe-Temp - Tries to drop a temporary table that does not exist
# Drop-Temp-If-TXe-Temp - Drops a temporary T-table if there is any and
#                         a temporary table that does not exist
#
#
# Drop-T - Drops a T-table if there is any
# Drop-N - Drops a  N-table if there is any
# Drop-Xe - Tries to drop a table that does not exist
# Drop-TXe - Drops a T-table if there is any and a table that does
#            not exist
# Drop-NXe - Drops a N-table if there is any and a table that does
#            not exist
# Drop-TN - Drops both a T-table and N-table if there is any
# Drop-TT - Drops two T-tables if there is any
# Drop-NN - Drops two N-tables if there is any
# Drop-N-TN-Temp - Drops a N-table and both a temporary T-table and
#                  N-table if there is any
#
#
# Drop-If-Xe - Tries to drop a table that does not exist
# Drop-If-TXe - Drops a T-table if there is any and a table that does
#               not exist
# Drop-If-NXe - Drops a N-table if there is any and a table that does
#               not exist
#
while (`SELECT HEX(@commands) != HEX('')`)
{
  --disable_query_log
  SET @command= SUBSTRING_INDEX(@commands, ' ', 1);
  let $command= `SELECT @command`;
  --eval SET @check_commands= '$commands'
  if (`SELECT HEX(@check_commands) = HEX('''')`)
  {
    let $commands= `SELECT @commands`;
  }

  if (`SELECT HEX(@command) = HEX('B')`)
  {
    --enable_query_log
    eval BEGIN;
   --disable_query_log
  }

  if (`SELECT HEX(@command) = HEX('T')`)
  {
    --enable_query_log
    eval INSERT INTO tt_xx_1() VALUES (1);
   --disable_query_log
  }

  if (`SELECT HEX(@command) = HEX('N')`)
  {
    --enable_query_log
    eval INSERT INTO nt_xx_1() VALUES (1);
   --disable_query_log
  }

  if (`SELECT HEX(@command) = HEX('Te')`)
  {
    --enable_query_log
    --inc $tt_error_counter
    eval INSERT INTO tt_error_1() VALUES ($tt_error_counter), ($tt_error_counter);
   --disable_query_log
  }

  if (`SELECT HEX(@command) = HEX('Ne')`)
  {
    --enable_query_log
    --inc $nt_error_counter
    eval INSERT INTO nt_error_1() VALUES ($nt_error_counter), ($nt_error_counter);
   --disable_query_log
  }

  if (`SELECT HEX(@command) = HEX('NeT-trig')`)
  {
    --enable_query_log
    --inc $nt_error_counter
    eval INSERT INTO nt_error_1() VALUES ($nt_error_counter), ($nt_error_counter);
   --disable_query_log
  }

  if (`SELECT HEX(@command) = HEX('N-Temp')`)
  {
    --enable_query_log
    eval INSERT INTO nt_tmp_xx_1() VALUES (1);
   --disable_query_log
  }

  if (`SELECT HEX(@command) = HEX('N-SELECT-N-Temp')`)
  {
    --enable_query_log
    eval INSERT INTO nt_xx_1 SELECT * FROM nt_tmp_xx_1;
   --disable_query_log
  }

  if (`SELECT HEX(@command) = HEX('N-SELECT-T-Temp')`)
  {
    --enable_query_log
    eval INSERT INTO nt_xx_1 SELECT * FROM tt_tmp_xx_1;
   --disable_query_log
  }

  if (`SELECT HEX(@command) = HEX('T-SELECT-N-Temp')`)
  {
    --enable_query_log
    eval INSERT INTO tt_xx_1 SELECT * FROM nt_tmp_xx_1;
   --disable_query_log
  }

  if (`SELECT HEX(@command) = HEX('T-SELECT-T-Temp')`)
  {
    --enable_query_log
    eval INSERT INTO tt_xx_1 SELECT * FROM tt_tmp_xx_1;
   --disable_query_log
  }

  if (`SELECT HEX(@command) = HEX('Create-N-Temp') || HEX(@command) = HEX('Create-T-Temp')`)
  {
    if (`SELECT HEX(@command) = HEX('Create-N-Temp')`)
    {
      --let $dropped_temp=$dropped_n_temp
      --let $available_temp=$available_n_temp
    }
    if (`SELECT HEX(@command) = HEX('Create-T-Temp')`)
    {
      --let $dropped_temp=$dropped_t_temp
      --let $available_temp=$available_t_temp
    }

    --eval SET @check_temp='$dropped_temp'
    if (`SELECT HEX(@check_temp) != HEX('')`)
    {
      SET @temp= SUBSTRING_INDEX(@check_temp, ',', 1);
      let $table=`SELECT @temp`;
      --eval SET @check_temp='$available_temp'
      if (`SELECT HEX(@check_temp) != HEX('')`)
      {
        --let $available_temp= $available_temp,$table
      }
      if (`SELECT HEX(@check_temp) = HEX('')`)
      {
        --let $available_temp= $table
      }
      --eval SET @check_temp='$dropped_temp'
      --eval SET @table_temp='$table'
      SET @check_temp= LTRIM(SUBSTRING(@check_temp, LENGTH(@table_temp) + 2));
      --let $dropped_temp= `SELECT @check_temp`

      if (`SELECT HEX(@command) = HEX('Create-N-Temp')`)
      {
        --enable_query_log
        --eval CREATE TEMPORARY TABLE $table ( id INT ) engine= MyIsam
        --disable_query_log

        --let $available_n_temp= $available_temp
        --let $dropped_n_temp= $dropped_temp
      }
      if (`SELECT HEX(@command) = HEX('Create-T-Temp')`)
      {
        --enable_query_log
        --eval CREATE TEMPORARY TABLE $table ( id INT ) engine= Innodb
        --disable_query_log

        --let $available_t_temp= $available_temp
        --let $dropped_t_temp= $dropped_temp
      }
    }
  }

  if (`SELECT HEX(@command) = HEX('Drop-Temp-N-Temp') || HEX(@command) = HEX('Drop-Temp-T-Temp') || HEX(@command) = HEX('Drop-T') || HEX(@command) = HEX('Drop-N')`)
  {
    if (`SELECT HEX(@command) = HEX('Drop-Temp-N-Temp')`)
    {
      --let $dropped_temp=$dropped_n_temp
      --let $available_temp=$available_n_temp
    }
    if (`SELECT HEX(@command) = HEX('Drop-Temp-T-Temp')`)
    {
      --let $dropped_temp=$dropped_t_temp
      --let $available_temp=$available_t_temp
    }
    if (`SELECT HEX(@command) = HEX('Drop-N')`)
    {
      --let $dropped_temp=$dropped_n
      --let $available_temp=$available_n
    }
    if (`SELECT HEX(@command) = HEX('Drop-T')`)
    {
      --let $dropped_temp=$dropped_t
      --let $available_temp=$available_t
    }

    --eval SET @check_temp='$available_temp'
    if (`SELECT HEX(@check_temp) != HEX('')`)
    {
      SET @temp= SUBSTRING_INDEX(@check_temp, ',', 1);
      let $table=`SELECT @temp`;
      --eval SET @check_temp='$dropped_temp'
      if (`SELECT HEX(@check_temp) != HEX('')`)
      {
        --let $dropped_temp= $dropped_temp,$table
      }
      if (`SELECT HEX(@check_temp) = HEX('')`)
      {
        --let $dropped_temp= $table
      }
      --eval SET @check_temp='$available_temp'
      --eval SET @table_temp='$table'
      SET @check_temp= LTRIM(SUBSTRING(@check_temp, LENGTH(@table_temp) + 2));
      --let $available_temp= `SELECT @check_temp`

      if (`SELECT HEX(@command) = HEX('Drop-Temp-N-Temp')`)
      {
        --enable_query_log
        --eval DROP TEMPORARY TABLE $table
        --disable_query_log

        --let $available_n_temp= $available_temp
        --let $dropped_n_temp= $dropped_temp
      }
      if (`SELECT HEX(@command) = HEX('Drop-Temp-T-Temp')`)
      {
        --enable_query_log
        --eval DROP TEMPORARY TABLE $table
        --disable_query_log

        --let $available_t_temp= $available_temp
        --let $dropped_t_temp= $dropped_temp
      }
      if (`SELECT HEX(@command) = HEX('Drop-N')`)
      {
        --enable_query_log
        --eval DROP TABLE $table
        --disable_query_log

        --let $available_n= $available_temp
        --let $dropped_n= $dropped_temp
      }
      if (`SELECT HEX(@command) = HEX('Drop-T')`)
      {
        --enable_query_log
        --eval DROP TABLE $table
        --disable_query_log

        --let $available_t= $available_temp
        --let $dropped_t= $dropped_temp
      }
    }
  }

  if (`SELECT HEX(@command) = HEX('Drop-Temp-Xe-Temp')`)
  {
    --enable_query_log
    --eval DROP TEMPORARY TABLE tt_xx_1
    --disable_query_log
  }

  if (`SELECT HEX(@command) = HEX('Drop-Temp-If-Xe-Temp')`)
  {
    --enable_query_log
    --eval DROP TEMPORARY TABLE IF EXISTS tt_xx_1
    --disable_query_log
  }

  if (`SELECT HEX(@command) = HEX('Drop-Xe')`)
  {
    --enable_query_log
    --eval DROP TABLE xx_1
    --disable_query_log
  }

  if (`SELECT HEX(@command) = HEX('Drop-If-Xe')`)
  {
    --enable_query_log
    --eval DROP TABLE IF EXISTS xx_1
    --disable_query_log
  }

  if (`SELECT HEX(@command) = HEX('Drop-Temp-NXe-Temp') || HEX(@command) = HEX('Drop-Temp-TXe-Temp') || HEX(@command) = HEX('Drop-NXe') || HEX(@command) = HEX('Drop-TXe') || HEX(@command) = HEX('Drop-Temp-If-NXe-Temp') || HEX(@command) = HEX('Drop-Temp-If-TXe-Temp') || HEX(@command) = HEX('Drop-If-NXe') || HEX(@command) = HEX('Drop-If-TXe')`)
  {
    if (`SELECT HEX(@command) = HEX('Drop-Temp-NXe-Temp') || HEX(@command) = HEX('Drop-Temp-If-NXe-Temp')`)
    {
      --let $dropped_temp=$dropped_n_temp
      --let $available_temp=$available_n_temp
    }
    if (`SELECT HEX(@command) = HEX('Drop-Temp-TXe-Temp') || HEX(@command) = HEX('Drop-Temp-If-TXe-Temp')`)
    {
      --let $dropped_temp=$dropped_t_temp
      --let $available_temp=$available_t_temp
    }
    if (`SELECT HEX(@command) = HEX('Drop-NXe') || HEX(@command) = HEX('Drop-If-NXe')`)
    {
      --let $dropped_temp=$dropped_n
      --let $available_temp=$available_n
    }
    if (`SELECT HEX(@command) = HEX('Drop-TXe') || HEX(@command) = HEX('Drop-If-TXe')`)
    {
      --let $dropped_temp=$dropped_t
      --let $available_temp=$available_t
    }

    --eval SET @check_temp='$available_temp'
    if (`SELECT HEX(@check_temp) != HEX('')`)
    {
      SET @temp= SUBSTRING_INDEX(@check_temp, ',', 1);
      let $table=`SELECT @temp`;
      --eval SET @check_temp='$dropped_temp'
      if (`SELECT HEX(@check_temp) != HEX('')`)
      {
        --let $dropped_temp= $dropped_temp,$table
      }
      if (`SELECT HEX(@check_temp) = HEX('')`)
      {
        --let $dropped_n_temp= $table
      }
      --eval SET @check_temp='$available_temp'
      --eval SET @table_temp='$table'
      SET @check_temp= LTRIM(SUBSTRING(@check_temp, LENGTH(@table_temp) + 2));
      --let $available_temp= `SELECT @check_temp`

      if (`SELECT HEX(@command) = HEX('Drop-Temp-NXe-Temp')`)
      {
        --enable_query_log
        --eval DROP TEMPORARY TABLE $table, tt_1
        --disable_query_log

        --let $available_n_temp= $available_temp
        --let $dropped_n_temp= $dropped_temp
      }
      if (`SELECT HEX(@command) = HEX('Drop-Temp-If-NXe-Temp')`)
      {
        --enable_query_log
        --eval DROP TEMPORARY TABLE IF EXISTS $table, tt_1
        --disable_query_log

        --let $available_n_temp= $available_temp
        --let $dropped_n_temp= $dropped_temp
      }
      if (`SELECT HEX(@command) = HEX('Drop-Temp-TXe-Temp')`)
      {
        --enable_query_log
        --eval DROP TEMPORARY TABLE $table, tt_1
        --disable_query_log

        --let $available_t_temp= $available_temp
        --let $dropped_t_temp= $dropped_temp
      }
      if (`SELECT HEX(@command) = HEX('Drop-Temp-If-TXe-Temp')`)
      {
        --enable_query_log
        --eval DROP TEMPORARY TABLE IF EXISTS $table, tt_1
        --disable_query_log

        --let $available_t_temp= $available_temp
        --let $dropped_t_temp= $dropped_temp
      }
      if (`SELECT HEX(@command) = HEX('Drop-NXe')`)
      {
        --enable_query_log
        --eval DROP TABLE $table, xx_1
        --disable_query_log

        --let $available_n= $available_temp
        --let $dropped_n= $dropped_temp
      }
      if (`SELECT HEX(@command) = HEX('Drop-If-NXe')`)
      {
        --enable_query_log
        --eval DROP TABLE IF EXISTS $table, xx_1
        --disable_query_log

        --let $available_n= $available_temp
        --let $dropped_n= $dropped_temp
      }
      if (`SELECT HEX(@command) = HEX('Drop-TXe')`)
      {
        --enable_query_log
        --eval DROP TABLE $table, xx_1
        --disable_query_log

        --let $available_t= $available_temp
        --let $dropped_t= $dropped_temp
      }
      if (`SELECT HEX(@command) = HEX('Drop-If-TXe')`)
      {
        --enable_query_log
        --eval DROP TABLE IF EXISTS $table, xx_1
        --disable_query_log

        --let $available_t= $available_temp
        --let $dropped_t= $dropped_temp
      }
    }
  }

  if (`SELECT HEX(@command) = HEX('Drop-Temp-NN-Temp') || HEX(@command) = HEX('Drop-Temp-TT-Temp') || HEX(@command) = HEX('Drop-NN') || HEX(@command) = HEX('Drop-TT')`)
  {
    if (`SELECT HEX(@command) = HEX('Drop-Temp-NN-Temp')`)
    {
      --let $dropped_temp=$dropped_n_temp
      --let $available_temp=$available_n_temp
    }
    if (`SELECT HEX(@command) = HEX('Drop-Temp-TT-Temp')`)
    {
      --let $dropped_temp=$dropped_t_temp
      --let $available_temp=$available_t_temp
    }
    if (`SELECT HEX(@command) = HEX('Drop-NN')`)
    {
      --let $dropped_temp=$dropped_n
      --let $available_temp=$available_n
    }
    if (`SELECT HEX(@command) = HEX('Drop-TT')`)
    {
      --let $dropped_temp=$dropped_t
      --let $available_temp=$available_t
    }

    --eval SET @check_temp='$available_temp'
    if (`SELECT HEX(@check_temp) != HEX('')`)
    {
      --let $n= 2
      while (`SELECT HEX(@check_temp) != HEX('') && $n != 0`)
      {
        SET @temp= SUBSTRING_INDEX(@check_temp, ',', 1);
        let $table=`SELECT @temp`;
        --eval SET @check_temp='$dropped_temp'
        if (`SELECT HEX(@check_temp) != HEX('')`)
        {
          --let $dropped_temp= $dropped_temp,$table
        }
        if (`SELECT HEX(@check_temp) = HEX('')`)
        {
          --let $dropped_temp= $table
        }
        if ($n == 1)
        {
          --let $table_1= $table
        }
        if ($n == 2)
        {
          --let $table_2= $table
        }

        --dec $n
        --eval SET @check_temp='$available_temp'
        --eval SET @table_temp='$table'
        SET @check_temp= LTRIM(SUBSTRING(@check_temp, LENGTH(@table_temp) + 2));
        --let $available_temp= `SELECT @check_temp`
      }

      if (`SELECT HEX(@command) = HEX('Drop-Temp-NN-Temp') && $n = 0`)
      {
        --enable_query_log
        --eval DROP TEMPORARY TABLE $table_1, $table_2
        --disable_query_log

        --let $available_n_temp= $available_temp
        --let $dropped_n_temp= $dropped_temp
      }
      if (`SELECT HEX(@command) = HEX('Drop-Temp-TT-Temp') && $n= 0`)
      {
        --enable_query_log
        --eval DROP TEMPORARY TABLE $table_1, $table_2
        --disable_query_log

        --let $available_t_temp= $available_temp
        --let $dropped_t_temp= $dropped_temp
      }
      if (`SELECT HEX(@command) = HEX('Drop-NN') && $n = 0`)
      {
        --enable_query_log
        --eval DROP TABLE $table_1, $table_2
        --disable_query_log

        --let $available_n= $available_temp
        --let $dropped_n= $dropped_temp
      }
      if (`SELECT HEX(@command) = HEX('Drop-TT') && $n= 0`)
      {
        --enable_query_log
        --eval DROP TABLE $table_1, $table_2
        --disable_query_log

        --let $available_t= $available_temp
        --let $dropped_t= $dropped_temp
      }
    }
  }

  if (`SELECT HEX(@command) = HEX('Drop-Temp-TN-Temp')`)
  {
    --eval SET @check_temp_t='$available_t_temp'
    --eval SET @check_temp_n='$available_n_temp'
    if (`SELECT HEX(@check_temp_t) != HEX('') && HEX(@check_temp_n) != HEX('')`)
    {
      SET @temp_t= SUBSTRING_INDEX(@check_temp_t, ',', 1);
      let $table_t=`SELECT @temp_t`;
      --eval SET @check_temp_t='$dropped_t_temp'
      if (`SELECT HEX(@check_temp_t) != HEX('')`)
      {
        --let $dropped_t_temp= $dropped_t_temp,$table_t
      }
      if (`SELECT HEX(@check_temp_t) = HEX('')`)
      {
        --let $dropped_t_temp= $table_t
      }
      --eval SET @check_temp='$available_t_temp'
      --eval SET @table_temp='$table_t'
      SET @check_temp= LTRIM(SUBSTRING(@check_temp, LENGTH(@table_temp) + 2));
      --let $available_t_temp= `SELECT @check_temp`

      SET @temp_n= SUBSTRING_INDEX(@check_temp_n, ',', 1);
      let $table_n=`SELECT @temp_n`;
      --eval SET @check_temp_n='$dropped_n_temp'
      if (`SELECT HEX(@check_temp_n) != HEX('')`)
      {
        --let $dropped_n_temp= $dropped_n_temp,$table_n
      }
      if (`SELECT HEX(@check_temp_n) = HEX('')`)
      {
        --let $dropped_n_temp= $table_n
      }
      --eval SET @check_temp='$available_n_temp'
      --eval SET @table_temp='$table_n'
      SET @check_temp= LTRIM(SUBSTRING(@check_temp, LENGTH(@table_temp) + 2));
      --let $available_n_temp= `SELECT @check_temp`

      --enable_query_log
      --eval DROP TEMPORARY TABLE $table_t, $table_n
      --disable_query_log
    }
  }

  if (`SELECT HEX(@command) = HEX('Drop-TN')`)
  {
    --eval SET @check_temp_t='$available_t'
    --eval SET @check_temp_n='$available_n'
    if (`SELECT HEX(@check_temp_t) != HEX('') && HEX(@check_temp_n) != HEX('')`)
    {
      SET @temp_t= SUBSTRING_INDEX(@check_temp_t, ',', 1);
      let $table_t=`SELECT @temp_t`;
      --eval SET @check_temp_t='$dropped_t'
      if (`SELECT HEX(@check_temp_t) != HEX('')`)
      {
        --let $dropped_t= $dropped_t,$table_t
      }
      if (`SELECT HEX(@check_temp_t) = HEX('')`)
      {
        --let $dropped_t= $table_t
      }
      --eval SET @check_temp='$available_t'
      --eval SET @table_temp='$table_t'
      SET @check_temp= LTRIM(SUBSTRING(@check_temp, LENGTH(@table_temp) + 2));
      --let $available_t= `SELECT @check_temp`

      SET @temp_n= SUBSTRING_INDEX(@check_temp_n, ',', 1);
      let $table_n=`SELECT @temp_n`;
      --eval SET @check_temp_n='$dropped_n'
      if (`SELECT HEX(@check_temp_n) != HEX('')`)
      {
        --let $dropped_n= $dropped_n,$table_n
      }
      if (`SELECT HEX(@check_temp_n) = HEX('')`)
      {
        --let $dropped_n= $table_n
      }
      --eval SET @check_temp='$available_n'
      --eval SET @table_temp='$table_n'
      SET @check_temp= LTRIM(SUBSTRING(@check_temp, LENGTH(@table_temp) + 2));
      --let $available_t= `SELECT @check_temp`

      --enable_query_log
      --eval DROP TABLE $table_t, $table_n
      --disable_query_log
    }
  }

  if (`SELECT HEX(@command) = HEX('Drop-N-TN-Temp') || HEX(@command) = HEX('Drop-TN-Temp')`)
  {
    --eval SET @check_temp_t='$available_t_temp'
    --eval SET @check_temp_n='$available_n_temp'
    if (`SELECT HEX(@command) = HEX('Drop-N-TN-Temp')`)
    {
      --eval SET @check_n='$available_n'
    }
    if (`SELECT HEX(@command) = HEX('Drop-TN-Temp')`)
    {
      #
      # Just to be possible to go through the next if...
      #
      --eval SET @check_n='...'
    }
    if (`SELECT HEX(@check_temp_t) != HEX('') && HEX(@check_temp_n) != HEX('') && HEX(@check_n) != HEX('')`)
    {
      SET @temp_t= SUBSTRING_INDEX(@check_temp_t, ',', 1);
      let $table_temp_t=`SELECT @temp_t`;
      --eval SET @check_temp_t='$dropped_t_temp'
      if (`SELECT HEX(@check_temp_t) != HEX('')`)
      {
        --let $dropped_t_temp= $dropped_t_temp,$table_temp_t
      }
      if (`SELECT HEX(@check_temp_t) = HEX('')`)
      {
        --let $dropped_t_temp= $table_temp_t
      }
      --eval SET @check_temp='$available_t_temp'
      --eval SET @table_temp='$table_temp_t'
      SET @check_temp= LTRIM(SUBSTRING(@check_temp, LENGTH(@table_temp) + 2));
      --let $available_t_temp= `SELECT @check_temp`

      SET @temp_n= SUBSTRING_INDEX(@check_temp_n, ',', 1);
      let $table_temp_n=`SELECT @temp_n`;
      --eval SET @check_temp_n='$dropped_n_temp'
      if (`SELECT HEX(@check_temp_n) != HEX('')`)
      {
        --let $dropped_n_temp= $dropped_n_temp,$table_temp_n
      }
      if (`SELECT HEX(@check_temp_n) = HEX('')`)
      {
        --let $dropped_n_temp= $table_temp_n
      }
      --eval SET @check_temp='$available_n_temp'
      --eval SET @table_temp='$table_temp_n'
      SET @check_temp= LTRIM(SUBSTRING(@check_temp, LENGTH(@table_temp) + 2));
      --let $available_n_temp= `SELECT @check_temp`

      if (`SELECT HEX(@command) = HEX('Drop-N-TN-Temp')`)
      {
        SET @temp_n= SUBSTRING_INDEX(@check_n, ',', 1);
        let $table_n=`SELECT @temp_n`;
        --eval SET @check_n='$dropped_n'
        if (`SELECT HEX(@check_n) != HEX('')`)
        {
          --let $dropped_n= $dropped_n,$table_n
        }
        if (`SELECT HEX(@check_n) = HEX('')`)
        {
          --let $dropped_n= $table_n
        }
        --eval SET @check_temp='$available_n'
        --eval SET @table_temp='$table_n'
        SET @check_temp= LTRIM(SUBSTRING(@check_temp, LENGTH(@table_temp) + 2));
        --let $available_n= `SELECT @check_temp`

        --enable_query_log
        --eval DROP TABLE $table_temp_t, $table_temp_n, $table_n
        --disable_query_log
      }
      if (`SELECT HEX(@command) = HEX('Drop-TN-Temp')`)
      {
        --enable_query_log
        --eval DROP TABLE $table_temp_t, $table_temp_n
        --disable_query_log
      }
    }
  }

  if (`SELECT HEX(@command) = HEX('C')`)
  {
    --enable_query_log
    --error 0, ER_GET_ERRMSG
    eval COMMIT;
    --disable_query_log
  }

  if (`SELECT HEX(@command) = HEX('R')`)
  {
    --enable_query_log
    --error 0, ER_GET_ERRMSG
    --replace_column 2 #
    eval ROLLBACK;
    --disable_query_log
  }

  SET @commands= LTRIM(SUBSTRING(@commands, LENGTH(@command) + 1));
  if (`SELECT HEX(@commands) = HEX('')`)
  {
    let $binlog_start= $pos_trans_command;
    --echo -b-b-b-b-b-b-b-b-b-b-b- >> $commands << -b-b-b-b-b-b-b-b-b-b-b-
    # Use other connection because include/show_binlog_events.inc
    # executes SELECT UUID(), which switches to row-logging when
    # binlog_format=mixed, if there are open temporary tables. This is
    # due to BUG#13692513.
    --connection server_1
    --source include/show_binlog_events.inc
    --connection master
    --echo -e-e-e-e-e-e-e-e-e-e-e- >> $commands << -e-e-e-e-e-e-e-e-e-e-e-
    --echo
 
    --disable_warnings 
    --let $available_n_temp=
    --let $dropped_n_temp=
    --let $n= $tot_table
    while ($n)
    { 
      --eval DROP TEMPORARY TABLE IF EXISTS nt_tmp_$n
      --eval CREATE TEMPORARY TABLE nt_tmp_$n ( id INT ) ENGINE = MyIsam
      --eval SET @check_temp='$available_n_temp'
      if (`SELECT HEX(@check_temp) != HEX('')`)
      {
        --let $available_n_temp= $available_n_temp,nt_tmp_$n
      }
      if (`SELECT HEX(@check_temp) = HEX('')`)
      {
        --let $available_n_temp= nt_tmp_$n
      }
      --dec $n
    }

    --let $available_t_temp=
    --let $dropped_t_temp=
    --let $n= $tot_table
    while ($n)
    { 
      --eval DROP TEMPORARY TABLE IF EXISTS tt_tmp_$n
      --eval CREATE TEMPORARY TABLE tt_tmp_$n ( id INT ) ENGINE = Innodb
      --eval SET @check_temp='$available_t_temp'
      if (`SELECT HEX(@check_temp) != HEX('')`)
      {
        --let $available_t_temp= $available_t_temp,tt_tmp_$n
      }
      if (`SELECT HEX(@check_temp) = HEX('')`)
      {
        --let $available_t_temp= tt_tmp_$n
      }
      --dec $n
    }

    --let $available_t=
    --let $dropped_t=
    --let $n= $tot_table
    while ($n)
    { 
      --eval DROP TABLE IF EXISTS tt_$n
      --eval CREATE TABLE tt_$n ( id INT ) ENGINE = Innodb
      --eval SET @check_temp='$available_t'
      if (`SELECT HEX(@check_temp) != HEX('')`)
      {
        --let $available_t= $available_t,tt_$n
      }
      if (`SELECT HEX(@check_temp) = HEX('')`)
      {
        --let $available_t= tt_$n
      }
      --dec $n
    }

    --let $available_n=
    --let $dropped_n=
    --let $n= $tot_table
    while ($n)
    { 
      --eval DROP TABLE IF EXISTS nt_$n
      --eval CREATE TABLE nt_$n ( id INT ) ENGINE = MyIsam
      --eval SET @check_temp='$available_n'
      if (`SELECT HEX(@check_temp) != HEX('')`)
      {
        --let $available_n= $available_n,nt_$n
      }
      if (`SELECT HEX(@check_temp) = HEX('')`)
      {
        --let $available_n= nt_$n
      }
      --dec $n
    }
    --enable_warnings 

    let $pos_trans_command= query_get_value("SHOW MASTER STATUS", Position, 1);
    let $commands= '';
  }
  --enable_query_log
}
