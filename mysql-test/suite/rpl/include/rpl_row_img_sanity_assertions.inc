-- let IMG_STMT_TYPE=

-- let $TMP_FILE= $MYSQLTEST_VARDIR/tmp/rpl_row_img_sanity.tmp
-- let IMG_BINLOG_FILE= $TMP_FILE

-- let $binlog_file= query_get_value(SHOW MASTER STATUS, File, 1)
-- let $MYSQLD_DATADIR= `select @@datadir;`
-- exec $MYSQL_BINLOG -v $MYSQLD_DATADIR/$binlog_file > $TMP_FILE

################################################################
-- let IMG_STMT_TYPE=INSERT
-- let IMG_BI_EXPECTED={};
-- let IMG_AI_EXPECTED={1=>1, 2=>2, 3=>"\'a\'"};
-- source suite/rpl/include/rpl_row_mysqbinlog_img_check.inc

################################################################
-- let IMG_STMT_TYPE=INSERT
-- let IMG_AI_EXPECTED={};
-- let IMG_BI_EXPECTED={};

if (`SELECT @@binlog_row_image = "FULL" or @@binlog_row_image = "NOBLOB"`)
{
  -- let IMG_AI_EXPECTED={1=>10, 2=>NULL, 3=>"\'a\'"};
}
if  (`SELECT @@binlog_row_image = "MINIMAL"`)
{ 
  -- let IMG_AI_EXPECTED={1=>10, 3=>"\'a\'"};
}
-- source suite/rpl/include/rpl_row_mysqbinlog_img_check.inc

################################################################
-- let IMG_STMT_TYPE=INSERT
-- let IMG_AI_EXPECTED={};
-- let IMG_BI_EXPECTED={};

if (`SELECT @@binlog_row_image = "FULL" or @@binlog_row_image = "NOBLOB"`)
{
  -- let IMG_AI_EXPECTED={1=>100, 2=>NULL, 3=>"\'a\'"};
}
if  (`SELECT @@binlog_row_image = "MINIMAL"`)
{ 
  -- let IMG_AI_EXPECTED={1=>100, 3=>"\'a\'"};
}
-- source suite/rpl/include/rpl_row_mysqbinlog_img_check.inc

################################################################
-- let IMG_STMT_TYPE=INSERT
-- let IMG_AI_EXPECTED={};
-- let IMG_BI_EXPECTED={};

if (`SELECT @@binlog_row_image = "FULL"`)
{
  -- let IMG_AI_EXPECTED={1=>1000, 2=>NULL, 3=>NULL};
} 
if  (`SELECT @@binlog_row_image = "NOBLOB"`)
{ 
  -- let IMG_AI_EXPECTED={1=>1000, 2=>NULL};
}
if  (`SELECT @@binlog_row_image = "MINIMAL"`)
{ 
  -- let IMG_AI_EXPECTED={1=>1000};
}
-- source suite/rpl/include/rpl_row_mysqbinlog_img_check.inc

################################################################

-- let IMG_STMT_TYPE=INSERT
-- let IMG_AI_EXPECTED={};
-- let IMG_BI_EXPECTED={};

if (`SELECT @@binlog_row_image = "FULL"`)
{
  -- let IMG_AI_EXPECTED={1=>10000, 2=>NULL, 3=>NULL};
} 
if  (`SELECT @@binlog_row_image = "NOBLOB"`)
{ 
  -- let IMG_AI_EXPECTED={1=>10000, 2=>NULL};
}
if  (`SELECT @@binlog_row_image = "MINIMAL"`)
{ 
  -- let IMG_AI_EXPECTED={1=>10000};
}
-- source suite/rpl/include/rpl_row_mysqbinlog_img_check.inc

################################################################
-- let IMG_STMT_TYPE=UPDATE
-- let IMG_AI_EXPECTED={};
-- let IMG_BI_EXPECTED={};

if (`SELECT @@binlog_row_image = "FULL" OR (SELECT $iteration < 4)`)
{
  -- let IMG_BI_EXPECTED={1=>1, 2=>2, 3=>"\'a\'"};
}

# noblob with pk + uk no nullable
if (`SELECT @@binlog_row_image = "NOBLOB" AND (SELECT $iteration > 3)`)
{
  -- let IMG_BI_EXPECTED={1=>1, 2=>2};
}

if (`SELECT @@binlog_row_image = "MINIMAL" AND (SELECT $iteration > 3)`)
{
  -- let IMG_BI_EXPECTED={1=>1};
}

if (`SELECT @@binlog_row_image = "FULL"`)
{
  -- let IMG_AI_EXPECTED={1=>2, 2=>2, 3=>"\'a\'"};
}

if (`SELECT @@binlog_row_image = "NOBLOB"`)
{
  -- let IMG_AI_EXPECTED={1=>2, 2=>2};
}

if (`SELECT @@binlog_row_image = "MINIMAL"`)
{
  -- let IMG_AI_EXPECTED={1=>2};
}
-- source suite/rpl/include/rpl_row_mysqbinlog_img_check.inc

################################################################
-- let IMG_STMT_TYPE=DELETE
-- let IMG_AI_EXPECTED={};
-- let IMG_BI_EXPECTED={};

if (`SELECT @@binlog_row_image = "FULL" OR (SELECT $iteration < 4)`)
{
  -- let IMG_BI_EXPECTED={1=>2, 2=>2, 3=>"\'a\'"};
}

# noblob with pk + uk no nullable
if (`SELECT @@binlog_row_image = "NOBLOB" AND (SELECT $iteration > 3)`)
{
  -- let IMG_BI_EXPECTED={1=>2, 2=>2};
}

if (`SELECT @@binlog_row_image = "MINIMAL" AND (SELECT $iteration > 3)`)
{
  -- let IMG_BI_EXPECTED={1=>2};
}
-- source suite/rpl/include/rpl_row_mysqbinlog_img_check.inc

################################################################

-- remove_file $TMP_FILE

