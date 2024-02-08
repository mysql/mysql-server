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
-- Create new variant of sp_set_firewall_mode
--

-- WARNING: If the .sql files are changed in a patch release, then it is
-- possible that the Firewall plugin, after a downgrade, will not work as intended.
-- Please try to avoid changes in patch releases.

DELIMITER $$

CREATE DEFINER='mysql.sys'@'localhost'
  PROCEDURE sp_set_firewall_mode (
    IN arg_userhost VARCHAR(288),
    IN arg_mode VARCHAR(12))
  SQL SECURITY INVOKER
BEGIN
  DECLARE result VARCHAR(160);
  DECLARE prev_mode VARCHAR(12);
  DECLARE reset_done BOOLEAN DEFAULT False;
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
  IF arg_mode = "RECORDING" THEN
    SELECT read_firewall_whitelist(arg_userhost,FW.rule) FROM firewall_whitelist FW WHERE userhost = arg_userhost;
  END IF;
  SELECT set_firewall_mode(arg_userhost, arg_mode) INTO result;
  IF arg_mode = "RESET" THEN
    DELETE FROM firewall_whitelist WHERE USERHOST = arg_userhost;
    SET arg_mode = "OFF";
    SET reset_done = True;
  END IF;
  IF result = "OK" THEN
    INSERT INTO firewall_users VALUES (arg_userhost, arg_mode) ON DUPLICATE KEY UPDATE mode=arg_mode;
  ELSE
    SELECT result;
  END IF;
  IF arg_mode = "PROTECTING" OR arg_mode = "DETECTING" OR (arg_mode = "OFF" AND reset_done = False) THEN
    INSERT INTO firewall_whitelist(USERHOST, RULE)
    (
      SELECT USERHOST,RULE FROM INFORMATION_SCHEMA.mysql_firewall_whitelist WHERE USERHOST = arg_userhost
      EXCEPT
      SELECT USERHOST,RULE FROM firewall_whitelist WHERE USERHOST = arg_userhost
    );
  END IF;
  COMMIT;
  SIGNAL SQLSTATE '01000'
  SET MESSAGE_TEXT = "'sp_set_firewall_mode' is deprecated and will be removed in a future release",
  MYSQL_ERRNO = 1681;
END$$

DELIMITER ;
