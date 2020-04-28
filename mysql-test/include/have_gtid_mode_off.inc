--let $_tmp_gtid_mode= `SELECT @@GLOBAL.GTID_MODE`
if ($_tmp_gtid_mode != 'OFF')
{
   --skip Test requires gtid_mode = OFF
}
