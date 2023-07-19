CREATE USER foo@localhost IDENTIFIED BY 'pwd';

--echo #
--echo # Blocking client
--echo # Send/Recieve a large packet with following options :-
--echo # Compress option : $COMPRESS_OPTION
--echo # Compress algorithm : $COMPRESS_ALGORITHM
--echo # Packet size : $SIZE
--echo #
if($COMPRESS_OPTION)
{
 connect(foo_con,localhost,foo,pwd,,,,SSL $COMPRESS_OPTION,,$COMPRESS_ALGORITHM);
}
if(!$COMPRESS_OPTION)
{
connect(foo_con,localhost,foo,pwd,,,,SSL,,);
}

--let $x_str = `SELECT REPEAT('X', $SIZE)`
--let $SIZE =  `SELECT ($SIZE)`

--echo ############ Verify Send and Recieve $SIZE bytes packet #################

--let $str = `SELECT "$x_str"`
--let $x_str = 0

--echo ############ Verify the packet contents #################################
--let $digest_client = `SELECT MD5("$str")`
--let $digest_server = `SELECT MD5(REPEAT('X', $SIZE))`
if($digest_client != $digest_server)
{
 die "Digests do not match";
}

connection default;
disconnect foo_con;

#cleanup
DROP USER foo@localhost;
