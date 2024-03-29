#
# Test sending and receiving queries that saddle a MAX_PACKET_LENGTH
# boundary (aka 16mb boundary)
#


# If required parameters are not set then throw error and exit.
--let PARAM_PERL_QUIT=0
if ($CLIENT_TYPE != 'NONBLOCKING')
{
  if($CLIENT_TYPE !=  'BLOCKING')
  {
    --let VARIABLE_NAME = CLIENT_TYPE
    --let PARAM_PERL_QUIT=1
  }
}

--perl
    $variable_name = "$ENV{VARIABLE_NAME}";
    die "Error: input variable $variable_name is missing!"
             if ($ENV{PARAM_PERL_QUIT} == 1);
EOF


#
# Test async client with a packet size close to the default 16MB
#
if($CLIENT_TYPE == NONBLOCKING)
{
  --enable_async_client
}

CREATE USER foo@localhost IDENTIFIED BY 'pwd';
connect(foo_con,localhost,foo,pwd,,,,TCP,,);
let $str = `SELECT REPEAT('X', 16777208)`;
let $i = 0;

--disable_query_log
while ($i < 16)
{
  let $str = z$str;
  eval SELECT MD5("$str");
  eval SELECT LENGTH("$str");
  replace_column 1 verylongstring;
  eval SELECT "Q $str END";
  inc $i;
}
--enable_query_log
connection default;
disconnect foo_con;
DROP USER foo@localhost;

if($CLIENT_TYPE == NONBLOCKING)
{
  --disable_async_client
}


#
# Test clients with a packet size close to 64MB with compress option enabled
#
--let $COMPRESS_OPTION = COMPRESS
--let $COMPRESS_ALGORITHM = zstd
--let $SIZE = 67108851
if($CLIENT_TYPE == NONBLOCKING)
{
  --source async_client.inc
}
if($CLIENT_TYPE == BLOCKING)
{
  --source sync_client.inc
}

#
# Test clients with a packet size close to 16MB with compress option
# enabled. It verifies the packet size to be MAX_PACKET_LENGTH
#
--let $COMPRESS_OPTION = COMPRESS
--let $COMPRESS_ALGORITHM = zlib
--let $SIZE = 16777211
if($CLIENT_TYPE == NONBLOCKING)
{
  --source async_client.inc
}
if($CLIENT_TYPE == BLOCKING)
{
  --source sync_client.inc
}

# cleanup
# Wait till all disconnects are completed
--source include/wait_until_count_sessions.inc
