--require r/have_mysql_upgrade.result
--disable_query_log
select LENGTH("$MYSQL_UPGRADE")>0 as have_mysql_upgrade;
--enable_query_log
