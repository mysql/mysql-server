CREATE USER IF NOT EXISTS 'mysqlrouter'@'%' IDENTIFIED BY 'mysqlrouter';

GRANT SELECT, INSERT, UPDATE, EXECUTE ON mysql_innodb_cluster_metadata.* TO 'mysqlrouter'@'%';

GRANT SELECT ON performance_schema.* TO 'mysqlrouter'@'%';
