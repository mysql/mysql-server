-- Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.
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
-- Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

# This set of commands will modify the predefined accounts of a MySQL installation
# to increase security.

# 1) Set passwords for the root account.
# Note that the password 'ABC123xyz' will be replaced by a random string
# when these commands are transferred to the server.
SET @@old_passwords=1; 
UPDATE mysql.user SET Password=PASSWORD('ABC123xyz') WHERE User='root' and plugin='mysql_old_password';
SET @@old_passwords=0; 
UPDATE mysql.user SET Password=PASSWORD('ABC123xyz') WHERE User='root' and plugin in ('', 'mysql_native_password');
SET @@old_passwords=2; 
UPDATE mysql.user SET authentication_string=PASSWORD('ABC123xyz') WHERE User='root' and plugin='sha256_password'; 

# 2) Drop the anonymous account.
DELETE FROM mysql.user WHERE User=''; 

# 3) Force the root user to change the password on first connect.
UPDATE mysql.user SET Password_expired='Y' WHERE User='root'; 

# In case this file is sent to a running server.
FLUSH PRIVILEGES;
