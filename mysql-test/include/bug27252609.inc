# Bug #27252609 UNABLE TO CREATE INDEX WHEN SPATIAL DATATYPES ARE USED
# WITH REQUIRED OPTION

# This file contains test cases that should be run both in strict and non-strict
# modes.

#
# ALTER TABLE with generated geometry column
#

# Stored generated column with valid geometry.
CREATE TABLE t1 (doc JSON) ENGINE=InnoDB;
INSERT INTO t1 (doc) VALUES (
  '{"geojson":{"type":"Point","coordinates":[11.11,12.22]}}'
);
--error ER_ALTER_OPERATION_NOT_SUPPORTED
ALTER TABLE t1
  ADD COLUMN g1 GEOMETRY GENERATED ALWAYS AS (
    ST_GEOMFROMGEOJSON(JSON_EXTRACT(doc, '$.geojson'), 2, 4326)
  ) STORED NOT NULL,
  ALGORITHM=INPLACE;
ALTER TABLE t1
  ADD COLUMN g2 GEOMETRY GENERATED ALWAYS AS (
    ST_GEOMFROMGEOJSON(JSON_EXTRACT(doc, '$.geojson'), 2, 4326)
  ) STORED NOT NULL,
  ALGORITHM=COPY;
DROP TABLE t1;

# Stored generated column with invalid geometry value or NULL in non-nullable
# column. Both should fail since the value is invalid for the column.
CREATE TABLE t1 (doc JSON) ENGINE=InnoDB;
INSERT INTO t1 (doc) VALUES (
  '{"geojson":{"type":"Point","invalid-geojson":[11.11,12.22]}}'
);
--error ER_INVALID_GEOJSON_MISSING_MEMBER
ALTER TABLE t1
  ADD COLUMN g1 GEOMETRY GENERATED ALWAYS AS (
    ST_GEOMFROMGEOJSON(JSON_EXTRACT(doc, '$.geojson'), 2, 4326)
  ) STORED NOT NULL;
# ST_CROSSES(polygon, point) is always NULL.
--error ER_BAD_NULL_ERROR_NOT_IGNORED
ALTER TABLE t1
  ADD COLUMN g2 GEOMETRY GENERATED ALWAYS AS (
    ST_CROSSES(ST_GEOMFROMTEXT('POLYGON((0 0, 1 0, 1 1, 0 0))'),
               ST_GEOMFROMTEXT('POINT(0 0)'))
  ) STORED NOT NULL;
DROP TABLE t1;

# Virtual generated column with valid geometry.
CREATE TABLE t1 (doc JSON) ENGINE=InnoDB;
INSERT INTO t1 (doc) VALUES (
  '{"geojson":{"type":"Point","coordinates":[11.11,12.22]}}'
);
ALTER TABLE t1
  ADD COLUMN g1 GEOMETRY GENERATED ALWAYS AS (
    ST_GEOMFROMGEOJSON(JSON_EXTRACT(doc, '$.geojson'), 2, 4326)
  ) VIRTUAL NOT NULL,
  ALGORITHM=INPLACE;
ALTER TABLE t1
  ADD COLUMN g2 GEOMETRY GENERATED ALWAYS AS (
    ST_GEOMFROMGEOJSON(JSON_EXTRACT(doc, '$.geojson'), 2, 4326)
  ) VIRTUAL NOT NULL,
  ALGORITHM=COPY;
SELECT doc FROM t1;
SELECT ST_ASTEXT(g1) FROM t1;
SELECT ST_ASTEXT(g2) FROM t1;
INSERT INTO t1 (doc) VALUES (
  '{"geojson":{"type":"Point","coordinates":[11.11,12.22]}}'
);
--error ER_INVALID_GEOJSON_MISSING_MEMBER
INSERT INTO t1 (doc) VALUES (
  '{"geojson":{"type":"Point","invalid":[11.11,12.22]}}'
);
UPDATE t1 SET doc='{"geojson":{"type":"Point","coordinates":[1,2]}}';
--error ER_INVALID_GEOJSON_MISSING_MEMBER
UPDATE t1 SET doc='{"geojson":{"type":"Point","invalid":[11.11,12.22]}}';
DELETE FROM t1;
DROP TABLE t1;

# Virtual generated column with invalid geometry value or NULL in non-nullable
# column. Both are allowed with ALGORITHM=INPLACE, but should fail when the
# invalid value is used.
CREATE TABLE t1 (doc JSON) ENGINE=InnoDB;
INSERT INTO t1 (doc) VALUES (
  '{"geojson":{"type":"Point","invalid-geojson":[11.11,12.22]}}'
);
ALTER TABLE t1
  ADD COLUMN g1 GEOMETRY GENERATED ALWAYS AS (
    ST_GEOMFROMGEOJSON(JSON_EXTRACT(doc, '$.geojson'), 2, 4326)
  ) VIRTUAL NOT NULL,
  ALGORITHM=INPLACE;
# ST_CROSSES(polygon, point) is always NULL.
ALTER TABLE t1
  ADD COLUMN g2 GEOMETRY GENERATED ALWAYS AS (
    ST_CROSSES(ST_GEOMFROMTEXT('POLYGON((0 0, 1 0, 1 1, 0 0))'),
               ST_GEOMFROMTEXT('POINT(0 0)'))
  ) VIRTUAL NOT NULL,
  ALGORITHM=INPLACE;
SELECT doc FROM t1;
--error ER_INVALID_GEOJSON_MISSING_MEMBER
SELECT ST_ASTEXT(g1) FROM t1;
--error ER_BAD_NULL_ERROR_NOT_IGNORED
INSERT INTO t1 (doc) VALUES ('{"foo":"bar"}');
--error ER_INVALID_GEOJSON_MISSING_MEMBER
UPDATE t1 SET doc='{"geojson":{"type":"Point","coordinates":[1,2]}}';
--error ER_INVALID_GEOJSON_MISSING_MEMBER
UPDATE t1 SET doc='{"geojson":{"type":"Point","invalid":[11.11,12.22]}}';
# Commented out because of bug #27599073.
#--error ER_INVALID_GEOJSON_MISSING_MEMBER
#DELETE FROM t1;
DROP TABLE t1;
CREATE TABLE t1 (doc JSON) ENGINE=InnoDB;
INSERT INTO t1 (doc) VALUES (
  '{"geojson":{"type":"Point","invalid-geojson":[11.11,12.22]}}'
);
# ST_CROSSES(polygon, point) is always NULL.
ALTER TABLE t1
  ADD COLUMN g1 GEOMETRY GENERATED ALWAYS AS (
    ST_CROSSES(ST_GEOMFROMTEXT('POLYGON((0 0, 1 0, 1 1, 0 0))'),
               ST_GEOMFROMTEXT('POINT(0 0)'))
  ) VIRTUAL NOT NULL,
  ALGORITHM=INPLACE;
SELECT doc FROM t1;
--error ER_BAD_NULL_ERROR_NOT_IGNORED
SELECT ST_ASTEXT(g1) FROM t1;
--error ER_BAD_NULL_ERROR_NOT_IGNORED
INSERT INTO t1 (doc) VALUES ('{"foo":"bar"}');
# Commented out because of bug #27599073.
#--error ER_BAD_NULL_ERROR_NOT_IGNORED
#DELETE FROM t1;
DROP TABLE t1;

# Virtual generated column with invalid geometry value or NULL in non-nullable
# column. With ALGORITHM=COPY, the invalid value is computed immediately, and
# the ALTER statement fails immediately.
CREATE TABLE t1 (doc JSON) ENGINE=InnoDB;
INSERT INTO t1 (doc) VALUES (
  '{"geojson":{"type":"Point","invalid-geojson":[11.11,12.22]}}'
);
--error ER_INVALID_GEOJSON_MISSING_MEMBER
ALTER TABLE t1
  ADD COLUMN g1 GEOMETRY GENERATED ALWAYS AS (
    ST_GEOMFROMGEOJSON(JSON_EXTRACT(doc, '$.geojson'), 2, 4326)
  ) VIRTUAL NOT NULL,
  ALGORITHM=COPY;
# ST_CROSSES(polygon, point) is always NULL.
--error ER_BAD_NULL_ERROR_NOT_IGNORED
ALTER TABLE t1
  ADD COLUMN g2 GEOMETRY GENERATED ALWAYS AS (
    ST_CROSSES(ST_GEOMFROMTEXT('POLYGON((0 0, 1 0, 1 1, 0 0))'),
               ST_GEOMFROMTEXT('POINT(0 0)'))
  ) VIRTUAL NOT NULL,
  ALGORITHM=COPY;
DELETE FROM t1;
DROP TABLE t1;

#
# CREATE TABLE with generated geometry column
#

# Stored generated column
CREATE TABLE t1 (
  doc JSON,
  g1 GEOMETRY GENERATED ALWAYS AS (
    ST_GEOMFROMGEOJSON(JSON_EXTRACT(doc, '$.geojson'), 2, 4326)
  ) STORED NOT NULL
) ENGINE= InnoDB;
INSERT INTO t1 (doc) VALUES (
  '{"geojson":{"type":"Point","coordinates":[11.11,12.22]}}'
);
--error ER_INVALID_GEOJSON_MISSING_MEMBER
INSERT INTO t1 (doc) VALUES (
  '{"geojson":{"type":"Point","invalid":[11.11,12.22]}}'
);
UPDATE t1 SET doc='{"geojson":{"type":"Point","coordinates":[1,2]}}';
--error ER_INVALID_GEOJSON_MISSING_MEMBER
UPDATE t1 SET doc='{"geojson":{"type":"Point","invalid":[11.11,12.22]}}';
DELETE FROM t1;
DROP TABLE t1;
CREATE TABLE t1 (
  doc JSON,
  g1 GEOMETRY GENERATED ALWAYS AS (
    ST_CROSSES(ST_GEOMFROMTEXT('POLYGON((0 0, 1 0, 1 1, 0 0))'),
               ST_GEOMFROMTEXT('POINT(0 0)'))
  ) STORED NOT NULL
) ENGINE=InnoDB;
--error ER_BAD_NULL_ERROR_NOT_IGNORED
INSERT INTO t1 (doc) VALUES (
  '{"geojson":{"type":"Point","coordinates":[11.11,12.22]}}'
);
--error ER_BAD_NULL_ERROR_NOT_IGNORED
INSERT INTO t1 (doc) VALUES (
  '{"geojson":{"type":"Point","invalid":[11.11,12.22]}}'
);
DELETE FROM t1;
DROP TABLE t1;

# Virtual generated column
CREATE TABLE t1 (
  doc JSON,
  g1 GEOMETRY GENERATED ALWAYS AS (
    ST_GEOMFROMGEOJSON(JSON_EXTRACT(doc, '$.geojson'), 2, 4326)
  ) VIRTUAL NOT NULL
) ENGINE=InnoDB;
INSERT INTO t1 (doc) VALUES (
  '{"geojson":{"type":"Point","coordinates":[11.11,12.22]}}'
);
--error ER_INVALID_GEOJSON_MISSING_MEMBER
INSERT INTO t1 (doc) VALUES (
  '{"geojson":{"type":"Point","invalid":[11.11,12.22]}}'
);
UPDATE t1 SET doc='{"geojson":{"type":"Point","coordinates":[1,2]}}';
--error ER_INVALID_GEOJSON_MISSING_MEMBER
UPDATE t1 SET doc='{"geojson":{"type":"Point","invalid":[11.11,12.22]}}';
DELETE FROM t1;
DROP TABLE t1;
CREATE TABLE t1 (
  doc JSON,
  g1 GEOMETRY GENERATED ALWAYS AS (
    ST_CROSSES(ST_GEOMFROMTEXT('POLYGON((0 0, 1 0, 1 1, 0 0))'),
               ST_GEOMFROMTEXT('POINT(0 0)'))
  ) VIRTUAL NOT NULL
) ENGINE=InnoDB;
--error ER_BAD_NULL_ERROR_NOT_IGNORED
INSERT INTO t1 (doc) VALUES (
  '{"geojson":{"type":"Point","coordinates":[11.11,12.22]}}'
);
--error ER_BAD_NULL_ERROR_NOT_IGNORED
INSERT INTO t1 (doc) VALUES (
  '{"geojson":{"type":"Point","invalid":[11.11,12.22]}}'
);
DELETE FROM t1;
DROP TABLE t1;

CREATE TABLE t1 (col1 JSON, col2 VARBINARY(255) GENERATED ALWAYS AS (
  ST_GEOMFROMGEOJSON(col1->>'$.geojson')
) VIRTUAL);
INSERT INTO t1 (col1) VALUES (
  '{"geojson":{"type":"Point","coordinates":[11.11,12.22]}}'
);
ALTER TABLE t1
  CHANGE COLUMN col2 col2 GEOMETRY GENERATED ALWAYS AS (
    ST_GEOMFROMGEOJSON(col1->>'$.geojson')
  ) VIRTUAL NOT NULL;
DROP TABLE t1;
