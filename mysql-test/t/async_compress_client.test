##############################################################################
#  WL#13510 Compression protocol for asynchronous client                     #
#  -----------------------------------------------------                     #
#  Tests to verify the compress option with asynchronous clients.            #
#                                                                            #
#  Test requires to set following the two flags                              #
#  $COMPRESS_OPTION : Valid value is 'compress'.                             #
#  $COMPRESS_ALGORITHM : Valid values are 'zstd','zlib','uncompressed'.      #
#                                                                            #
##############################################################################

# Save the initial number of concurrent sessions
--source include/count_sessions.inc

--let $COMPRESS_OPTION = COMPRESS

--let $COMPRESS_ALGORITHM = zlib
--echo ############ Test with $COMPRESS_ALGORITHM algorithm ############
--source async_compress_client.inc

--let $COMPRESS_ALGORITHM = zstd
--echo  ############  Test with $COMPRESS_ALGORITHM algorithm ############
--source async_compress_client.inc
