if (`SELECT @@binlog_format = 'ROW'`)
{
     skip Test cannot run with binlog_format row;
}
