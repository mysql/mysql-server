-- require r/is_debug_build.require
--disable_query_log
select instr(version(), "debug") > 0;
--enable_query_log
