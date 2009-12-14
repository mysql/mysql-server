SET SQL_LOG_BIN=0;

-- echo CREATING TABLE IN $CURRENT_CONNECTION

if (`SELECT $iteration = 1`)
{
  CREATE TABLE t (c1 int, c2 int, c3 blob);
}

if (`SELECT $iteration = 2`)
{
  CREATE TABLE t (c1 int, c2 int, c3 blob, key(c1));
}

if (`SELECT $iteration = 3`)
{
  CREATE TABLE t (c1 int, c2 int, c3 blob, unique key(c1));
}


# UK NON null
if (`SELECT $iteration = 4`)
{
  CREATE TABLE t (c1 int, c2 int, c3 blob, primary key(c1));
}

# PK
if (`SELECT $iteration = 5`)
{
  CREATE TABLE t (c1 int NOT NULL, c2 int, c3 blob, unique key(c1));
}

if (`SELECT $iteration = 6`)
{
  if (`SELECT @@binlog_row_image='NOBLOB'`)
  {
    CREATE TABLE t (c1 int, c2 int, c3 blob, primary key(c1,c3(512)));
  }
}

SET SQL_LOG_BIN=1;
