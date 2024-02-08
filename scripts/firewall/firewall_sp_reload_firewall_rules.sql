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
-- Create new variant of sp_reload_firewall_rules
--

-- WARNING: If the .sql files are changed in a patch release, then it is
-- possible that the Firewall plugin, after a downgrade, will not work as intended.
-- Please try to avoid changes in patch releases.

DELIMITER $$

CREATE DEFINER='mysql.sys'@'localhost'
  PROCEDURE sp_reload_firewall_rules (
    IN arg_userhost VARCHAR(288))
  SQL SECURITY INVOKER
BEGIN
  DECLARE result VARCHAR(160);
  DECLARE prev_mode VARCHAR(12);
  DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
      IF (@prev_mode IS NOT NULL) THEN
        SELECT set_firewall_mode(arg_userhost, @prev_mode) INTO result;
      END IF;
      ROLLBACK;
      RESIGNAL;
    END;
  START TRANSACTION;
  SET @prev_mode = (SELECT mode FROM information_schema.mysql_firewall_users WHERE userhost = arg_userhost);
  SELECT set_firewall_mode(arg_userhost, "RESET") INTO result;
  IF result = "OK" THEN
    INSERT INTO firewall_users VALUES (arg_userhost, "OFF") ON DUPLICATE KEY UPDATE mode="OFF";
    SELECT read_firewall_whitelist(arg_userhost,FW.rule) FROM firewall_whitelist FW WHERE FW.userhost=arg_userhost;
  ELSE
    SELECT result;
  END IF;
  COMMIT;
  SIGNAL SQLSTATE '01000'
  SET MESSAGE_TEXT = "'sp_reload_firewall_rules' is deprecated and will be removed in a future release",
  MYSQL_ERRNO = 1681;
END$$

DELIMITER ;
