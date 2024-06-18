DROP SCHEMA mysql_innodb_cluster_metadata;
DROP USER IF EXISTS 'mysqlrouter'@'%';


if ($router_id)
{
--force-rmdir $MYSQL_TMP_DIR/router.$router_id/data/
}
