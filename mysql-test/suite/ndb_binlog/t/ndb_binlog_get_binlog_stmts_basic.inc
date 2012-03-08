# 
# Get the mysqlbinlog tool --verbose mode to dump the Binlog contents with
# 'SQL' statements in triple-comments over multiple lines, e.g. :
#
### INSERT
###  SET
###  @1=1
###  @2=2
#
# Then munch this output into single-line statements
# e.g. :
#  INSERT SET @1=1 @2=2
#
# Then filter + sort to get deterministic order independent of Ndb table 
# fragmentation, epoch in ndb_apply_status etc.
#

--disable_query_log
let $MYSQLD_DATADIR= `select @@datadir;`;
--exec $MYSQL_BINLOG --verbose $MYSQLD_DATADIR/mysqld-bin.000001 > $MYSQLTEST_VARDIR/tmp/ndb_binlog_mysqlbinlog.sql

create table raw_binlog_rows (txt varchar(1000));

--eval load data local infile '$MYSQLTEST_VARDIR/tmp/ndb_binlog_mysqlbinlog.sql' into table raw_binlog_rows columns terminated by '\n';

create table binlog_stmt_parts_unassoc (txt varchar(1000), line_count int, stmt_boundary int);

set @line_count=0;
set @stmt_boundary=0;

# Use replace() here to get rid of any unwanted Windows
# CRs
insert into binlog_stmt_parts_unassoc
  select replace(txt, '\r', ''),
         @line_count:= @line_count + 1,  # So we can preserve order later
         (txt like '%INSERT%' or         # Identify statement boundaries
          txt like '%UPDATE%' or
          txt like '%DELETE%')
    from raw_binlog_rows
    where
      txt like '###%';                   # Discard non verbose output

#select * from binlog_stmt_parts_unassoc;

create table binlog_stmt_parts_assoc (txt varchar(1000), line_count int, stmt_num int);

set @stmt_count = 0;

insert into binlog_stmt_parts_assoc
  select txt, 
         line_count, 
         @stmt_count:= @stmt_count + stmt_boundary   # All rows from same stmt will
                                                     # have same stmt_num
    from binlog_stmt_parts_unassoc order by line_count;


#select * from binlog_stmt_parts_assoc;

create table binlog_stmts (txt varchar(1000), stmt_num int);

insert into binlog_stmts 
  select group_concat(right(txt,             # Combine rows in statment into 1
                            length(txt) - 4) # Trim ### from line start
                      order by line_count
                      separator ' '), stmt_num
    from binlog_stmt_parts_assoc
    group by stmt_num;

#select * from binlog_stmts;

# Drop ndb_apply_status entries and sort by the statment 
# text to get a deterministic order.
#
# Reasonable order would be sort by (PK-cols, stmt_num)
# - Sorting by PK-cols would give determinism between events from different
#   fragments
# - Multiple ops on same pk would be in order of application
#
# However, as that's harder, and unnecessary given that we just want
# deterministic output, not applicable SQL, we will just sort by
# the statement text
#
--enable_query_log
--eval select txt from binlog_stmts where txt $binlog_condition order by txt

--disable_query_log
drop table raw_binlog_rows;
drop table binlog_stmt_parts_unassoc;
drop table binlog_stmt_parts_assoc;
drop table binlog_stmts;
--enable_query_log