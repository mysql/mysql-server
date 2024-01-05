-- Copyright (c) 2020, 2024, Oracle and/or its affiliates.
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

CREATE DEFINER='mysql.sys'@'localhost'
  PROCEDURE sp_set_firewall_group_mode (
    IN arg_group_name VARCHAR(288),
    IN arg_mode VARCHAR(12))
  SQL SECURITY INVOKER
BEGIN
  DECLARE result VARCHAR(160);
  DECLARE prev_mode VARCHAR(12);
  DECLARE prev_user VARCHAR(288);
  DECLARE reset_done BOOLEAN DEFAULT False;
  DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
      IF (@prev_mode IS NOT NULL) THEN
        SELECT set_firewall_group_mode(arg_group_name, @prev_mode, IF(@prev_user IS NOT NULL, @prev_user, '')) INTO result;
      END IF;
      ROLLBACK;
      RESIGNAL;
    END;
  START TRANSACTION;
  SET @prev_mode = (SELECT mode FROM performance_schema.firewall_groups WHERE name = arg_group_name);
  SET @prev_user = (SELECT userhost FROM performance_schema.firewall_groups WHERE name = arg_group_name);
  IF arg_mode = "RECORDING" THEN
    SELECT read_firewall_group_allowlist(arg_group_name,FW.rule) FROM firewall_group_allowlist FW WHERE name = arg_group_name;
  END IF;
  SELECT set_firewall_group_mode(arg_group_name, arg_mode) INTO result;
  IF arg_mode = "RESET" THEN
    DELETE FROM firewall_group_allowlist WHERE name = arg_group_name;
    SET arg_mode = "OFF";
    SET reset_done = True;
  END IF;
  IF result = "OK" THEN
    INSERT INTO firewall_groups VALUES (arg_group_name, arg_mode, NULL) ON DUPLICATE KEY UPDATE mode=arg_mode;
  ELSE
    SELECT result;
  END IF;
  IF arg_mode = "PROTECTING" OR arg_mode = "DETECTING" OR (arg_mode = "OFF" AND reset_done = False) THEN
    INSERT INTO firewall_group_allowlist(name, rule)
    (
      SELECT name, rule FROM performance_schema.firewall_group_allowlist WHERE name=arg_group_name
      EXCEPT
      SELECT name, rule FROM firewall_group_allowlist WHERE name = arg_group_name
    );
  END IF;
  COMMIT;
END$$

CREATE DEFINER='mysql.sys'@'localhost'
  PROCEDURE sp_set_firewall_group_mode_and_user(
    IN arg_group_name VARCHAR(288),
    IN arg_mode VARCHAR(12),
    IN arg_userhost VARCHAR(288))
  SQL SECURITY INVOKER
BEGIN
  DECLARE result VARCHAR(160);
  DECLARE prev_mode VARCHAR(12);
  DECLARE prev_user VARCHAR(288);
  DECLARE reset_done BOOLEAN DEFAULT False;
  DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
      IF (@prev_mode IS NOT NULL) THEN
        SELECT set_firewall_group_mode(arg_group_name, @prev_mode, IF(@prev_user IS NOT NULL, @prev_user, '')) INTO result;
      END IF;
      ROLLBACK;
      RESIGNAL;
    END;
  START TRANSACTION;
  SET @prev_mode = (SELECT mode FROM performance_schema.firewall_groups WHERE name = arg_group_name);
  SET @prev_user = (SELECT userhost FROM performance_schema.firewall_groups WHERE name = arg_group_name);
  IF arg_mode = "RECORDING" THEN
    SELECT read_firewall_group_allowlist(arg_group_name,FW.rule) FROM firewall_group_allowlist FW WHERE name = arg_group_name;
  END IF;
  SELECT set_firewall_group_mode(arg_group_name, arg_mode, arg_userhost) INTO result;
  IF arg_mode = "RESET" THEN
    DELETE FROM firewall_group_allowlist WHERE name = arg_group_name;
    SET arg_mode = "OFF";
    SET reset_done = True;
  END IF;
  IF result = "OK" THEN
    INSERT INTO firewall_groups VALUES (arg_group_name, arg_mode, arg_userhost) ON DUPLICATE KEY UPDATE mode=arg_mode, userhost=arg_userhost;
  ELSE
    SELECT result;
  END IF;
  IF arg_mode = "PROTECTING" OR arg_mode = "DETECTING" OR (arg_mode = "OFF" AND reset_done = False) THEN
    INSERT INTO firewall_group_allowlist(name, rule)
    (
      SELECT name, rule FROM performance_schema.firewall_group_allowlist WHERE name=arg_group_name
      EXCEPT
      SELECT name, rule FROM firewall_group_allowlist WHERE name = arg_group_name
    );
  END IF;
  COMMIT;
END$$

CREATE DEFINER='mysql.sys'@'localhost'
  PROCEDURE sp_reload_firewall_group_rules(
    IN arg_group_name VARCHAR(288))
  SQL SECURITY INVOKER
BEGIN
  DECLARE result VARCHAR(160);
  DECLARE prev_mode VARCHAR(12);
  DECLARE prev_user VARCHAR(288);
  DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
      IF (@prev_mode IS NOT NULL) THEN
        SELECT set_firewall_group_mode(arg_group_name, @prev_mode, IF(@prev_user IS NOT NULL, @prev_user, '')) INTO result;
      END IF;
      ROLLBACK;
      RESIGNAL;
    END;
  START TRANSACTION;
  SET @prev_mode = (SELECT mode FROM performance_schema.firewall_groups WHERE name = arg_group_name);
  SET @prev_user = (SELECT userhost FROM performance_schema.firewall_groups WHERE name = arg_group_name);
  SELECT set_firewall_group_mode(arg_group_name, "RESET") INTO result;
  IF result = "OK" THEN
    INSERT INTO firewall_groups VALUES (arg_group_name, "OFF", NULL) ON DUPLICATE KEY UPDATE mode="OFF";
    SELECT read_firewall_group_allowlist(arg_group_name,FW.rule) FROM firewall_group_allowlist FW WHERE FW.name=arg_group_name;
  ELSE
    SELECT result;
  END IF;
  COMMIT;
END$$

CREATE DEFINER='mysql.sys'@'localhost'
  PROCEDURE sp_firewall_group_enlist(
    IN arg_group_name VARCHAR(288),
    IN arg_userhost VARCHAR(288))
  SQL SECURITY INVOKER
BEGIN
  DECLARE result VARCHAR(160);
  DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
      SELECT firewall_group_delist(arg_group_name, arg_userhost) INTO result;
      ROLLBACK;
      RESIGNAL;
    END;
  START TRANSACTION;
  SELECT firewall_group_enlist(arg_group_name, arg_userhost) INTO result;
  IF result = "OK" THEN
    INSERT IGNORE INTO firewall_membership VALUES (arg_group_name, arg_userhost);
  ELSE
    SELECT result;
  END IF;
  COMMIT;
END$$

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
delimiter ;
