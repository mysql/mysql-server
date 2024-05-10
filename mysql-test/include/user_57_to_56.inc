# Create 5.6 mysql.user table layout

ALTER TABLE mysql.user ADD COLUMN Password
char(41) character set latin1 collate latin1_bin DEFAULT '' NOT NULL
AFTER user;

ALTER TABLE mysql.user DROP COLUMN password_last_changed;
ALTER TABLE mysql.user DROP COLUMN password_lifetime;
ALTER TABLE mysql.user DROP COLUMN account_locked;
