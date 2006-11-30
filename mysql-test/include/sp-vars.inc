delimiter |;

# --------------------------------------------------------------------------

CREATE PROCEDURE sp_vars_check_dflt()
BEGIN
  DECLARE v1 TINYINT DEFAULT 1e200;
  DECLARE v1u TINYINT UNSIGNED DEFAULT 1e200;
  DECLARE v2 TINYINT DEFAULT -1e200;
  DECLARE v2u TINYINT UNSIGNED DEFAULT -1e200;
  DECLARE v3 TINYINT DEFAULT 300;
  DECLARE v3u TINYINT UNSIGNED DEFAULT 300;
  DECLARE v4 TINYINT DEFAULT -300;
  DECLARE v4u TINYINT UNSIGNED DEFAULT -300;

  DECLARE v5 TINYINT DEFAULT 10 * 10 * 10;
  DECLARE v5u TINYINT UNSIGNED DEFAULT 10 * 10 * 10;
  DECLARE v6 TINYINT DEFAULT -10 * 10 * 10;
  DECLARE v6u TINYINT UNSIGNED DEFAULT -10 * 10 * 10;

  DECLARE v7 TINYINT DEFAULT '10';
  DECLARE v8 TINYINT DEFAULT '10 ';
  DECLARE v9 TINYINT DEFAULT ' 10 ';
  DECLARE v10 TINYINT DEFAULT 'String 10 ';
  DECLARE v11 TINYINT DEFAULT 'String10';
  DECLARE v12 TINYINT DEFAULT '10 String';
  DECLARE v13 TINYINT DEFAULT '10String';
  DECLARE v14 TINYINT DEFAULT concat('10', ' ');
  DECLARE v15 TINYINT DEFAULT concat(' ', '10');
  DECLARE v16 TINYINT DEFAULT concat('Hello, ', 'world');

  DECLARE v17 DECIMAL(64, 2) DEFAULT 12;
  DECLARE v18 DECIMAL(64, 2) DEFAULT 12.123;
  DECLARE v19 DECIMAL(64, 2) DEFAULT 11 + 1;
  DECLARE v20 DECIMAL(64, 2) DEFAULT 12 + 0.123;

  SELECT v1, v1u, v2, v2u, v3, v3u, v4, v4u;
  SELECT v5, v5u, v6, v6u;
  SELECT v7, v8, v9, v10, v11, v12, v13, v14, v15, v16;
  SELECT v17, v18, v19, v20;
END|

# --------------------------------------------------------------------------

CREATE PROCEDURE sp_vars_check_assignment()
BEGIN
  DECLARE i1, i2, i3, i4 TINYINT;
  DECLARE u1, u2, u3, u4 TINYINT UNSIGNED;
  DECLARE d1, d2, d3 DECIMAL(64, 2);
 
  SET i1 = 1e200;
  SET i2 = -1e200;
  SET i3 = 300;
  SET i4 = -300;
 
  SELECT i1, i2, i3, i4;

  SET i1 = 10 * 10 * 10;
  SET i2 = -10 * 10 * 10;
  SET i3 = sign(10 * 10) * 10 * 20;
  SET i4 = sign(-10 * 10) * -10 * 20;
  
  SELECT i1, i2, i3, i4;

  SET u1 = 1e200;
  SET u2 = -1e200;
  SET u3 = 300;
  SET u4 = -300;
 
  SELECT u1, u2, u3, u4;

  SET u1 = 10 * 10 * 10;
  SET u2 = -10 * 10 * 10;
  SET u3 = sign(10 * 10) * 10 * 20;
  SET u4 = sign(-10 * 10) * -10 * 20;
  
  SELECT u1, u2, u3, u4;

  SET d1 = 1234;
  SET d2 = 1234.12;
  SET d3 = 1234.1234;

  SELECT d1, d2, d3;

  SET d1 = 12 * 100 + 34;
  SET d2 = 12 * 100 + 34 + 0.12;
  SET d3 = 12 * 100 + 34 + 0.1234;

  SELECT d1, d2, d3;
END|

# --------------------------------------------------------------------------

CREATE FUNCTION sp_vars_check_ret1() RETURNS TINYINT
BEGIN
  RETURN 1e200;
END|

# --------------------------------------------------------------------------

CREATE FUNCTION sp_vars_check_ret2() RETURNS TINYINT
BEGIN
  RETURN 10 * 10 * 10;
END|

# --------------------------------------------------------------------------

CREATE FUNCTION sp_vars_check_ret3() RETURNS TINYINT
BEGIN
  RETURN 'Hello, world';
END|

# --------------------------------------------------------------------------

CREATE FUNCTION sp_vars_check_ret4() RETURNS DECIMAL(64, 2)
BEGIN
  RETURN 12 * 10 + 34 + 0.1234;
END|

# --------------------------------------------------------------------------

CREATE FUNCTION sp_vars_div_zero() RETURNS INTEGER
BEGIN
  DECLARE div_zero INTEGER;
  SELECT 1/0 INTO div_zero;
  RETURN div_zero;
END|

# --------------------------------------------------------------------------

delimiter ;|
