# Helper file that has scenarios to verify shynchronous as well as
# asynchronous clients.

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


--echo #
--echo # This test is around a 32mb send/receive, testing payloads that cross
--echo # multiple packets.
--echo #

if($CLIENT_TYPE == NONBLOCKING)
{
  enable_async_client;
}

CREATE USER foo@localhost IDENTIFIED BY 'pwd';
connect(foo_con,localhost,foo,pwd,,,,TCP,,);
let $str = `SELECT REPEAT('X', 33554432)`;
let $str = zzzzzzzzzzzzzzzz$str;

--disable_query_log
--eval SELECT MD5("$str");
--eval SELECT LENGTH("$str");
--replace_column 1 verylongstring
--eval SELECT "Q $str END";
--enable_query_log
connection default;
disconnect foo_con;
DROP USER foo@localhost;

if($CLIENT_TYPE == NONBLOCKING)
{
  disable_async_client;
}


#
# Test clients with a packet size of 64MB and compress option enabled
#
--let $COMPRESS_OPTION = COMPRESS
--let $COMPRESS_ALGORITHM = zstd
--let $SIZE = 64*1024*1024

if($CLIENT_TYPE == NONBLOCKING)
{
  --source async_client.inc
}
if($CLIENT_TYPE == BLOCKING)
{
  --source sync_client.inc
}

#
# Test clients with a packet size of 64MB without compression
#
# Commented the following test due to Bug#30888680. Uncomment once that bug if
# fixed.
#--let $COMPRESS_OPTION =
#--let $COMPRESS_ALGORITHM = uncompressed
#--let $SIZE = 64*1024*1024
# if($CLIENT_TYPE == NONBLOCKING)
# {
#   --source async_client.inc
# }
# if($CLIENT_TYPE == BLOCKING)
# {
#   --source sync_client.inc
# }
