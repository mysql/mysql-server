-- Copyright (c) 2020, 2024, Oracle and/or its affiliates.
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License, version 2.0,
-- as published by the Free Software Foundation.
--
-- This program is designed to work with certain software (including
-- but not limited to OpenSSL) that is licensed under separate terms,
-- as designated in a particular file or component or in included license
-- documentation.  The authors of MySQL hereby grant you an additional
-- permission to link the program and your derivative works with the
-- separately licensed software that they have either included with
-- the program or referenced in the documentation.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License, version 2.0, for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

--
-- Create new variant of sp_firewall_group_delist
--

-- WARNING: If the .sql files are changed in a patch release, then it is
-- possible that the Firewall plugin, after a downgrade, will not work as intended.
-- Please try to avoid changes in patch releases.

DELIMITER $$

CREATE DEFINER='mysql.sys'@'localhost'
  PROCEDURE sp_firewall_group_delist(
    IN arg_group_name VARCHAR(288),
    IN arg_userhost VARCHAR(288))
  SQL SECURITY INVOKER
BEGIN
  DECLARE result VARCHAR(160);
  DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
      SELECT firewall_group_enlist(arg_group_name, arg_userhost) INTO result;
      ROLLBACK;
      RESIGNAL;
    END;
  START TRANSACTION;
  SELECT firewall_group_delist(arg_group_name, arg_userhost) INTO result;
  IF result = "OK" THEN
    DELETE IGNORE FROM firewall_membership WHERE group_id = arg_group_name AND member_id = arg_userhost;
  ELSE
    SELECT result;
  END IF;
  COMMIT;
END$$

DELIMITER ;
