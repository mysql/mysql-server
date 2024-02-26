--source include/force_myisam_default.inc
--source include/have_myisam.inc

--echo #
--echo # Bug #21789000 SPATIAL INDEX CAUSES INCORRECT CARDINALITY FOR ALL
--echo # NON-PRIMARY INDEXES
--echo #

# With spatial index
CREATE TABLE t1 (
  id INT NOT NULL AUTO_INCREMENT,
  a VARCHAR(10) NOT NULL,
  b VARCHAR(5) NOT NULL,
  c GEOMETRY NOT NULL SRID 0,
  PRIMARY KEY (id),
  SPATIAL INDEX c (c),
  INDEX a (a),
  INDEX b (b) 
) ENGINE=MyISAM;

# Without spatial index
CREATE TABLE t2 (
  id INT NOT NULL AUTO_INCREMENT,
  a VARCHAR(10) NOT NULL,
  b VARCHAR(5) NOT NULL,
  c GEOMETRY NOT NULL,
  PRIMARY KEY (id),
  INDEX a (a),
  INDEX b (b) 
) ENGINE=MyISAM;

INSERT INTO t1(a, b, c) VALUES
  ('a1', 'b1', POINT(0, 0)),
  ('a2', 'b2', POINT(0, 0)),
  ('a3', 'b3', POINT(0, 0)),
  ('a4', 'b4', POINT(0, 0)),
  ('a5', 'b5', POINT(0, 0)),
  ('a6', 'b6', POINT(0, 0)),
  ('a7', 'b7', POINT(0, 0));

INSERT INTO t2 SELECT * FROM t1; 

ANALYZE TABLE t1; 
SHOW INDEXES FROM t1; 

ANALYZE TABLE t2; 
SHOW INDEXES FROM t2; 

DROP TABLE t1, t2; 
