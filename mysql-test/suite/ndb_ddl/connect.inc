--source include/have_ndb.inc


# No need to include the setup commands in .result file
--disable_query_log

# Create one connection to each mysqld
let $i = $NUM_MYSQLDS;
while($i)
{
  let $_port= \$MYSQLD_PORT_$i;
  if (!$_port)
  {
    die Not all $MYSQLD_PORT_* env. variables are configured;
  }
  --connect(mysqld$i,127.0.0.1,root,,test,$_port)

  dec $i;
}

--enable_query_log
