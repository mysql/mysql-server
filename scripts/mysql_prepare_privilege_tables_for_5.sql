-- Copyright (C) 2003 MySQL AB
-- 
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; version 2 of the License.
-- 
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
-- 
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

use mysql;

--
-- merging `host` table and `db`
--

UPDATE IGNORE host SET Host='%' WHERE Host='';
DELETE FROM host WHERE Host='';

INSERT IGNORE INTO db (User, Host, Select_priv, Insert_priv, Update_priv,
    Delete_priv, Create_priv, Drop_priv, Grant_priv, References_priv,
    Index_priv, Alter_priv, Create_tmp_table_priv, Lock_tables_priv)
  SELECT d.User, h.Host,
    (d.Select_priv           = 'Y' || h.Select_priv           = 'Y') + 1,
    (d.Insert_priv           = 'Y' || h.Select_priv           = 'Y') + 1,
    (d.Update_priv           = 'Y' || h.Update_priv           = 'Y') + 1,
    (d.Delete_priv           = 'Y' || h.Delete_priv           = 'Y') + 1,
    (d.Create_priv           = 'Y' || h.Create_priv           = 'Y') + 1,
    (d.Drop_priv             = 'Y' || h.Drop_priv             = 'Y') + 1,
    (d.Grant_priv            = 'Y' || h.Grant_priv            = 'Y') + 1,
    (d.References_priv       = 'Y' || h.References_priv       = 'Y') + 1,
    (d.Index_priv            = 'Y' || h.Index_priv            = 'Y') + 1,
    (d.Alter_priv            = 'Y' || h.Alter_priv            = 'Y') + 1,
    (d.Create_tmp_table_priv = 'Y' || h.Create_tmp_table_priv = 'Y') + 1,
    (d.Lock_tables_priv      = 'Y' || h.Lock_tables_priv      = 'Y') + 1
  FROM db d, host h WHERE d.Host = '';

UPDATE IGNORE db SET Host='%' WHERE Host = '';
DELETE FROM db WHERE Host='';

TRUNCATE TABLE host;

--
-- Adding missing users to `user` table
--
-- note that invalid password causes the user to be skipped during the
-- load of grand tables (at mysqld startup) thus three following inserts
-- do not affect anything

INSERT IGNORE user (User, Host, Password) SELECT User, Host, "*" FROM db;
INSERT IGNORE user (User, Host, Password) SELECT User, Host, "*" FROM tables_priv;
INSERT IGNORE user (User, Host, Password) SELECT User, Host, "*" FROM columns_priv;

SELECT DISTINCT
"There are user accounts with the username 'PUBLIC'. In the SQL-1999
(or later) standard this name is reserved for PUBLIC role and can
not be used as a valid user name. Consider renaming these accounts before
upgrading to MySQL-5.0.
These accounts are:" x
FROM user WHERE user='PUBLIC';
SELECT CONCAT(user,'@',host) FROM user WHERE user='PUBLIC';

