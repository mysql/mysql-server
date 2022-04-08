-- Copyright (c) 2014, 2022, Oracle and/or its affiliates.
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
-- Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

INSERT IGNORE INTO mysql.user VALUES
('localhost','mysql.sys','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','','','','',0,0,0,0,'caching_sha2_password','\$A\$005\$THISISACOMBINATIONOFINVALIDSALTANDPASSWORDTHATMUSTNEVERBRBEUSED','N',CURRENT_TIMESTAMP,NULL,'Y','N','N',NULL,NULL,NULL,NULL);

INSERT IGNORE INTO mysql.db VALUES
('localhost','sys','mysql.sys','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','N','Y');

INSERT IGNORE INTO mysql.tables_priv VALUES
('localhost','sys','mysql.sys','sys_config','root@localhost',
CURRENT_TIMESTAMP, 'Select', '');

-- GRANT SYSTEM_USER ON *.* TO 'mysql.sys'@localhost
INSERT IGNORE INTO mysql.global_grants (USER,HOST,PRIV,WITH_GRANT_OPTION)
VALUES ('mysql.sys','localhost','SYSTEM_USER','N');

-- GRANT AUDIT_ABORT_EXEMPT ON *.* TO 'mysql.sys'@localhost
INSERT IGNORE INTO mysql.global_grants (USER,HOST,PRIV,WITH_GRANT_OPTION)
VALUES ('mysql.sys','localhost','AUDIT_ABORT_EXEMPT','N');

-- GRANT FIREWALL_EXEMPT ON *.* TO 'mysql.sys'@localhost
INSERT IGNORE INTO mysql.global_grants (USER,HOST,PRIV,WITH_GRANT_OPTION)
VALUES ('mysql.sys','localhost','FIREWALL_EXEMPT','N');
