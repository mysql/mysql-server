# AES OFB block mode is supported by OpenSSL only.

--echo # Tests of the AES ofb block mode
--disable_query_log
--disable_result_log
--error 0,ER_WRONG_VALUE_FOR_VAR
SET SESSION block_encryption_mode='aes-128-ofb';
--enable_query_log
--enable_result_log

let $block_mode=ofb;
source include/func_aes_block.inc;

SET SESSION block_encryption_mode=default;

--echo #
--echo # End of 5.7 tests
--echo #
