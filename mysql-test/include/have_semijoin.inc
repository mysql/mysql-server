-- require r/have_optimizer_switch.require
--disable_query_log
select locate('semijoin', @@optimizer_switch) > 0 AS OPTIMIZER_SWITCH;
--enable_query_log
