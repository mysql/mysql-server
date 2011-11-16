-- require r/have_optimizer_switch.require
--disable_query_log
select locate('batch_key_access', @@optimizer_switch) > 0 AS OPTIMIZER_SWITCH;
--enable_query_log
