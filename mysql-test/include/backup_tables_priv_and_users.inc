# ==== Purpose ====
#
# To backup/restore contents of the tables: mysql.tables_priv and mysql.user
#
# ==== Usage ====
#
#       # Backup mysql.user & mysql.tables_priv tables
#	--let $backup=1
#	--source include/backup_tables_priv_users.inc
#
#       # Restore mysql.user & mysql.tables_priv from backup
#       --let $restore=1
#       --source include/backup_tables_priv_users.inc

if ($backup)
{
  # Save a copy of the user/tables_priv tables, to restore later
  CREATE TABLE mysql.tmp_backup_tables_priv AS SELECT * FROM mysql.tables_priv;
  CREATE TABLE mysql.tmp_backup_user AS SELECT * FROM mysql.user;

  # Reset the backup flag
  --let $backup= 0;
}

if ($restore)
{
  # Restore the saved contents of mysql.tables_priv and mysql.user
  TRUNCATE TABLE mysql.tables_priv;
  INSERT INTO mysql.tables_priv (SELECT * FROM mysql.tmp_backup_tables_priv);
  DROP TABLE mysql.tmp_backup_tables_priv;

  TRUNCATE TABLE mysql.user;
  INSERT INTO mysql.user (SELECT * FROM mysql.tmp_backup_user);
  DROP TABLE mysql.tmp_backup_user;

  # Reset the restore flag
  --let $restore= 0;
}
