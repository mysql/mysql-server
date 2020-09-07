# Create 5.7 mysql.user table layout

ALTER TABLE mysql.user DROP COLUMN Drop_role_priv;
ALTER TABLE mysql.user DROP COLUMN Create_role_priv;
ALTER TABLE mysql.user DROP COLUMN Password_reuse_history;
ALTER TABLE mysql.user DROP COLUMN Password_reuse_time;
ALTER TABLE mysql.user DROP COLUMN Password_require_current;
ALTER TABLE mysql.user DROP COLUMN User_attributes;

# Remove the SYSTEM_USER privilege granted to existing users.
DELETE FROM mysql.global_grants WHERE PRIV= 'SYSTEM_USER';