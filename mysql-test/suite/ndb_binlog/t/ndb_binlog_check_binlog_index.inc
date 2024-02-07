#
# ndb_binlog_check_binlog_index
#
# This include file determines epoch boundaries from a Binlog using the
# mysqlbinlog tool
# It then compares the calculated boundaries with the contents of
# the ndb_binlog_index table
# The intention is to prove that :
#   - for any epoch A
#     - The mysql.ndb_binlog_index next pos for A is the first binlog location
#       after the COMMIT event of epoch A
#   - Therefore the presence of a committed epoch number in a slave cluster's
#     ndb_apply_status table is taken to mean only that the transaction which
#     wrote it committed.
#   - The Slave should resume immediately after the committed transaction
# Note that :
#   - The start position (Position, File) is vaguely defined and can refer
#     to any position after the previously committed epoch's COMMIT event
#     and before the epoch's BEGIN event.  A test and fix for this
#     exists, but has been shelved.
#

--disable_query_log
set sql_log_bin=0;

let have_next_pos=query_get_value(select count(1) as have_next_file from information_schema.COLUMNS where table_schema='mysql' and table_name='ndb_binlog_index' and column_name='next_file', have_next_file, 1);
if (!$have_next_pos)
{
  --echo Nothing to verify
}
if ($have_next_pos)
{

  let $MYSQLD_DATADIR= `select @@datadir;`;
  let $tmp_file = $MYSQLTEST_VARDIR/tmp/ndb_binlog_mysqlbinlog.sql;
  --exec $MYSQL_BINLOG --verbose $MYSQLD_DATADIR/binlog.000001 > $tmp_file

  create table raw_binlog_rows(
    txt varchar(1000) character set latin1
  );

  --eval load data local infile '$tmp_file' into table raw_binlog_rows columns terminated by '\n';
  --remove_file $tmp_file

  #select count(*) FROM raw_binlog_rows;

  create table binlog_stmt_parts_unassoc(
    txt varchar(1000) character set latin1,
    line_count int NOT NULL AUTO_INCREMENT PRIMARY KEY,
    stmt_boundary int,
    tx_boundary int
  );

  # Auto incrementing line_count values preserve the order of the raw binlog rows.
  # Use replace() here to get rid of any unwanted Windows
  # CRs
  insert into binlog_stmt_parts_unassoc (txt,stmt_boundary,tx_boundary)
    select replace(txt, '\r', ''),
           (txt like '%INSERT%' or         # Identify statement boundaries
            txt like '%UPDATE%' or
            txt like '%DELETE%' or
            txt = 'BEGIN' or
            txt = 'COMMIT'),
           txt = 'BEGIN'             # Transaction boundary
      from raw_binlog_rows
      where
        txt like '###%' OR
        txt = 'BEGIN' OR
        txt = 'COMMIT' OR
        txt like '%# at%';   # Discard stuff we don't care about

  #select * from binlog_stmt_parts_unassoc;

  create table binlog_stmt_parts_assoc(
    txt varchar(1000) character set latin1,
    line_count int,
    stmt_num int,
    tx_num int,
    key(stmt_num),
    key(line_count),
    key(txt)
  );

  insert into binlog_stmt_parts_assoc
  (  select txt,
            line_count,
            SUM(stmt_boundary) OVER(ORDER BY line_count) AS stmt_count, # All rows from same stmt will
                                                                        # have same stmt_num
            SUM(tx_boundary) OVER(ORDER BY line_count) AS tx_count      # Same transaction
        from binlog_stmt_parts_unassoc order by line_count);

  create table apply_status_stmt_nums (stmt_num int primary key);

  insert into apply_status_stmt_nums
    select stmt_num from binlog_stmt_parts_assoc
    where txt like '%INSERT INTO mysql.ndb_apply_status%';

  create table relevant_info(
    txt varchar(1000) character set latin1,
    tx_num int,
    line_count int
  );

  insert into relevant_info
    # Epoch number to tx_num mapping
    #  ###  @2= <epoch>, <tx_num>, <line_count>
    select bspa.txt, bspa.tx_num, bspa.line_count
    from
      binlog_stmt_parts_assoc as bspa,
      apply_status_stmt_nums
    where
      (bspa.stmt_num = apply_status_stmt_nums.stmt_num
           and
       bspa.txt like '%@2=%')                # Epoch number
  union
    # BEGIN and COMMIT event to tx_num and file position mapping
    # BEGIN # at <offset>, <tx_num>, <line_count>
    # COMMIT # at <offset>, <tx_num>, <line_count>
    #
    select concat(bspa2.txt, " ", bspa1.txt), bspa2.tx_num, bspa1.line_count
    from
      binlog_stmt_parts_assoc as bspa1,
      binlog_stmt_parts_assoc as bspa2
    where
      (bspa2.txt = 'BEGIN' and
       bspa1.line_count = bspa2.line_count - 1) # Line before BEGIN event
      or
      (bspa2.txt = 'COMMIT' and
       bspa1.line_count = bspa2.line_count + 1); # Line after COMMIT event

  #select * from relevant_info order by line_count;

  create table epoch_info (num int NOT NULL AUTO_INCREMENT PRIMARY KEY, epoch bigint, start_pos bigint, next_pos bigint);
  set @epoch_num=0;

  insert into epoch_info (epoch , start_pos , next_pos)
    select
    (right(ri1.txt, length(ri1.txt) - length('###   @2='))) + 0, # epoch number
     (right(ri2.txt, length(ri2.txt) - length('BEGIN # at '))) + 0, # start pos
      (right(ri3.txt, length(ri3.txt) - length('COMMIT # at '))) + 0  # end pos
    from
      relevant_info as ri1, relevant_info as ri2, relevant_info as ri3
    where
      ri1.tx_num = ri2.tx_num
      and
      ri1.tx_num = ri3.tx_num
      and
      ri1.txt like '%@2=%'
      and
      ri2.txt like '%BEGIN%'
      and
      ri3.txt like '%COMMIT%';

  #select * from epoch_info order by num;

  # Insert dummy row 0 to give start pos of first epoch
  --let $first_event_pos= query_get_value(SHOW BINLOG EVENTS LIMIT 1, End_log_pos, 1)
  eval insert into epoch_info values (0,0,0,$first_event_pos);


  # Get epoch info where following epoch starts at end of previous epoch
  create table adjusted_epoch_info (epoch bigint, start_pos bigint);

  insert into adjusted_epoch_info
    select e2.epoch, e1.next_pos as start_pos
      from
        epoch_info as e1, epoch_info as e2
      where
        e2.num = e1.num + 1;

  #select * from adjusted_epoch_info;

  # Should not return any rows
  --echo ---------------------------------------------------------------------------
  --echo Mismatches between Binlog index next_pos and Binlog COMMIT event pos
  --echo ---------------------------------------------------------------------------

  select binlog.epoch,
           binlog.next_pos as calculated_pos,
           binlog_index.next_position as stored_pos
    from epoch_info as binlog,
           mysql.ndb_binlog_index as binlog_index
    where binlog.epoch = binlog_index.epoch AND
            binlog.next_pos != binlog_index.next_position;

  --echo Done

  # Following commented out as it is an (understandably) non-deterministic
  # race
  #
  #--echo ----------------------------------------------
  #--echo Any gaps between epoch n next_pos and epoch n+1 start_pos
  #--echo ----------------------------------------------
  ## This indicates that other events (e.g. DDL) were inserted between the end of
  ## one epoch and the recorded start pos of the next epoch
  ##select binlog.epoch,
  ##         binlog.start_pos as calculated_start_pos,
  ##         bi.Position as stored_start_pos
  #select count(1) > 0
  #  from
  #    adjusted_epoch_info as binlog,
  #    mysql.ndb_binlog_index as bi
  #  where
  #    binlog.epoch = bi.epoch
  #    and
  #    binlog.start_pos != bi.Position;
  #
  #--echo Done

  # Following is commented out as it is an (understandably) non-deterministic
  # race
  #
  #--echo -----------------------------------------------
  #--echo Any stored start positions different to BEGIN positions
  #--echo -----------------------------------------------
  ## This indicates that other events (e.g. DDL) were inserted between the recorded
  ## start of an epoch, and the actual start of the transaction (BEGIN)
  ##
  ##select binlog.epoch,
  ##         binlog.start_pos as calculated_pos,
  ##         binlog_index.position as stored_pos
  #select count(1) > 0
  #  from epoch_info as binlog,
  #         mysql.ndb_binlog_index as binlog_index
  #  where binlog.epoch = binlog_index.epoch AND
  #          binlog.start_pos != binlog_index.position;
  #
  #--echo Done

  # The following test is no longer relevant as epochs are
  # not guaranteed adjacent.
  #
  #create table bi_offsets (count bigint,
  #                              epoch bigint,
  #                              start_pos bigint,
  #                              next_pos bigint);

  #set @epoch_count=0;

  #insert into bi_offsets
  #  select @epoch_count:=@epoch_count+1,
  #    epoch, Position, next_position
  #    from mysql.ndb_binlog_index
  #    order by epoch asc;

  #select * from bi_offsets order by epoch;

  #--echo ----------------------------------------
  #--echo Non adjacent epochs in ndb_binlog_index
  #--echo ----------------------------------------

  #select bio1.count, bio1.epoch, bio1.start_pos, bio1.next_pos,
  #         bio2.count, bio2.epoch, bio2.start_pos, bio2.next_pos
  #  from bi_offsets as bio1, bi_offsets as bio2
  #  where
  #    bio2.count = bio1.count + 1 AND
  #    bio2.start_pos != bio1.next_pos;

  #--echo Done.

  #drop table bi_offsets;

  drop table adjusted_epoch_info;
  drop table epoch_info;
  drop table relevant_info;
  drop table apply_status_stmt_nums;
  drop table binlog_stmt_parts_assoc;
  drop table binlog_stmt_parts_unassoc;
  drop table raw_binlog_rows;
}
set sql_log_bin=1;
--enable_query_log
