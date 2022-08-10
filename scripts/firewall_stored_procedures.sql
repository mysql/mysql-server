-- Copyright (c) 2020, 2022, Oracle and/or its affiliates.
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License, version 2.0,
-- as published by the Free Software Foundation.
--
-- This program is also distributed with certain software (including
-- but not limited to OpenSSL) that is licensed under separate terms,
-- as designated in a particular file or component or in included license
-- documentation.  The authors of MySQL hereby grant you an additional
-- permission to link the program and your derivative works with the
-- separately licensed software that they have included with MySQL.
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
-- Remove existing Firewall stored procedures
--

DROP PROCEDURE IF EXISTS sp_set_firewall_mode;
DROP PROCEDURE IF EXISTS sp_reload_firewall_rules;
DROP PROCEDURE IF EXISTS sp_set_firewall_group_mode;
DROP PROCEDURE IF EXISTS sp_set_firewall_group_mode_and_user;
DROP PROCEDURE IF EXISTS sp_reload_firewall_group_rules;
DROP PROCEDURE IF EXISTS sp_firewall_group_enlist;
DROP PROCEDURE IF EXISTS sp_firewall_group_delist;

--
-- Create new variants of Firewall stored procedures
--

DELIMITER $$

CREATE DEFINER='mysql.sys'@'localhost'
  PROCEDURE sp_set_firewall_mode (
    IN arg_userhost VARCHAR(288),
    IN arg_mode VARCHAR(12))
  SQL SECURITY INVOKER
BEGIN
  DECLARE result VARCHAR(160);
  IF arg_mode = "RECORDING" THEN
    SELECT read_firewall_whitelist(arg_userhost,FW.rule) FROM mysql.firewall_whitelist FW WHERE userhost = arg_userhost;
  END IF;
  SELECT set_firewall_mode(arg_userhost, arg_mode) INTO result;
  IF arg_mode = "RESET" THEN
    SET arg_mode = "OFF";
  END IF;
  IF result = "OK" THEN
    INSERT IGNORE INTO mysql.firewall_users VALUES (arg_userhost, arg_mode);
    UPDATE mysql.firewall_users SET mode=arg_mode WHERE userhost = arg_userhost;
  ELSE
    SELECT result;
  END IF;
  IF arg_mode = "PROTECTING" OR arg_mode = "OFF" OR arg_mode = "DETECTING" THEN
    DELETE FROM mysql.firewall_whitelist WHERE USERHOST = arg_userhost;
    INSERT INTO mysql.firewall_whitelist(USERHOST, RULE) SELECT USERHOST,RULE FROM INFORMATION_SCHEMA.mysql_firewall_whitelist WHERE USERHOST=arg_userhost;
  END IF;
END$$

CREATE DEFINER='mysql.sys'@'localhost'
  PROCEDURE sp_reload_firewall_rules (
    IN arg_userhost VARCHAR(288))
  SQL SECURITY INVOKER
BEGIN
  DECLARE result VARCHAR(160);
  SELECT set_firewall_mode(arg_userhost, "RESET") INTO result;
  IF result = "OK" THEN
    INSERT IGNORE INTO mysql.firewall_users VALUES (arg_userhost, "OFF");
    UPDATE mysql.firewall_users SET mode="OFF" WHERE userhost = arg_userhost;
    SELECT read_firewall_whitelist(arg_userhost,FW.rule) FROM mysql.firewall_whitelist FW WHERE FW.userhost=arg_userhost;
  ELSE
    SELECT result;
  END IF;
END$$

CREATE DEFINER='mysql.sys'@'localhost'
  PROCEDURE sp_set_firewall_group_mode (
    IN arg_group_name VARCHAR(288),
    IN arg_mode VARCHAR(12))
  SQL SECURITY INVOKER
BEGIN
  DECLARE result VARCHAR(160);
  IF arg_mode = "RECORDING" THEN
    SELECT read_firewall_group_allowlist(arg_group_name,FW.rule) FROM mysql.firewall_group_allowlist FW WHERE name = arg_group_name;
  END IF;
  SELECT set_firewall_group_mode(arg_group_name, arg_mode) INTO result;
  IF arg_mode = "RESET" THEN
    SET arg_mode = "OFF";
  END IF;
  IF result = "OK" THEN
    INSERT IGNORE INTO mysql.firewall_groups VALUES (arg_group_name, arg_mode, NULL);
    UPDATE mysql.firewall_groups SET mode=arg_mode WHERE name = arg_group_name;
  ELSE
    SELECT result;
  END IF;
  IF arg_mode = "PROTECTING" OR arg_mode = "OFF" OR arg_mode = "DETECTING" THEN
    DELETE FROM mysql.firewall_group_allowlist WHERE name = arg_group_name;
    INSERT INTO mysql.firewall_group_allowlist(name, rule)
      SELECT name, rule FROM performance_schema.firewall_group_allowlist
      WHERE name=arg_group_name;
  END IF;
END$$

CREATE DEFINER='mysql.sys'@'localhost'
  PROCEDURE sp_set_firewall_group_mode_and_user(
    IN arg_group_name VARCHAR(288),
    IN arg_mode VARCHAR(12),
    IN arg_userhost VARCHAR(288))
  SQL SECURITY INVOKER
BEGIN
  DECLARE result VARCHAR(160);
  IF arg_mode = "RECORDING" THEN
    SELECT read_firewall_group_allowlist(arg_group_name,FW.rule) FROM mysql.firewall_group_allowlist FW WHERE name = arg_group_name;
  END IF;
  SELECT set_firewall_group_mode(arg_group_name, arg_mode, arg_userhost) INTO result;
  IF arg_mode = "RESET" THEN
    SET arg_mode = "OFF";
  END IF;
  IF result = "OK" THEN
    INSERT IGNORE INTO mysql.firewall_groups VALUES (arg_group_name, arg_mode, arg_userhost);
    UPDATE mysql.firewall_groups SET mode=arg_mode, userhost=arg_userhost WHERE name = arg_group_name;
  ELSE
    SELECT result;
  END IF;
  IF arg_mode = "PROTECTING" OR arg_mode = "OFF" OR arg_mode = "DETECTING" THEN
    DELETE FROM mysql.firewall_group_allowlist WHERE name = arg_group_name;
    INSERT INTO mysql.firewall_group_allowlist(name, rule)
      SELECT name, rule FROM performance_schema.firewall_group_allowlist
      WHERE name=arg_group_name;
  END IF;
END$$

CREATE DEFINER='mysql.sys'@'localhost'
  PROCEDURE sp_reload_firewall_group_rules(
    IN arg_group_name VARCHAR(288))
  SQL SECURITY INVOKER
BEGIN
  DECLARE result VARCHAR(160);
  SELECT set_firewall_group_mode(arg_group_name, "RESET") INTO result;
  IF result = "OK" THEN
    INSERT IGNORE INTO mysql.firewall_groups VALUES (arg_group_name, "OFF", NULL);
    UPDATE mysql.firewall_groups SET mode="OFF" WHERE name = arg_group_name;
    SELECT read_firewall_group_allowlist(arg_group_name,FW.rule) FROM mysql.firewall_group_allowlist FW WHERE FW.name=arg_group_name;
  ELSE
    SELECT result;
  END IF;
END$$

CREATE DEFINER='mysql.sys'@'localhost'
  PROCEDURE sp_firewall_group_enlist(
    IN arg_group_name VARCHAR(288),
    IN arg_userhost VARCHAR(288))
  SQL SECURITY INVOKER
BEGIN
  DECLARE result VARCHAR(160);
  SELECT firewall_group_enlist(arg_group_name, arg_userhost) INTO result;
  IF result = "OK" THEN
    INSERT IGNORE INTO mysql.firewall_membership VALUES (arg_group_name, arg_userhost);
  ELSE
    SELECT result;
  END IF;
END$$

CREATE DEFINER='mysql.sys'@'localhost'
  PROCEDURE sp_firewall_group_delist(
    IN arg_group_name VARCHAR(288),
    IN arg_userhost VARCHAR(288))
  SQL SECURITY INVOKER
BEGIN
  DECLARE result VARCHAR(160);
  SELECT firewall_group_delist(arg_group_name, arg_userhost) INTO result;
  IF result = "OK" THEN
    DELETE IGNORE FROM mysql.firewall_membership WHERE group_id = arg_group_name AND member_id = arg_userhost;
  ELSE
    SELECT result;
  END IF;
END$$
delimiter ;
