-- Copyright (c) 2013, 2024, Oracle and/or its affiliates.
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

DROP VIEW IF EXISTS test.nstats_diff;
DROP VIEW IF EXISTS test.nstats_current;
DROP TABLE IF EXISTS test.nstats_baseline;

-- ndbapi slave stats in both, global_status and session_status
CREATE OR REPLACE VIEW test.nstats_current AS
        SELECT variable_name AS name, variable_value AS value
        FROM performance_schema.global_status
--        WHERE variable_name LIKE 'ndb_api%';
        WHERE variable_name LIKE 'ndb_api%count';
--        WHERE variable_name LIKE 'ndb_api%slave';
-- SELECT * FROM test.nstats_current;

CREATE TABLE IF NOT EXISTS test.nstats_baseline(
        name VARCHAR(64) PRIMARY KEY,
        value VARCHAR(1024) ) ENGINE = MEMORY;
-- REPLACE INTO test.nstats_baseline
--      SELECT * FROM test.nstats_current;
-- SELECT * FROM test.nstats_baseline;
-- \. nstats_reset.sql

CREATE OR REPLACE VIEW test.nstats_diff AS
--        SELECT c.name, c.value - b.value AS diff
--        SELECT c.name, CAST(c.value AS SIGNED) - CAST(b.value AS SIGNED) AS diff
        SELECT c.name, CAST(c.value AS DECIMAL(64)) - CAST(b.value AS DECIMAL(64)) AS diff
        FROM test.nstats_current AS c, test.nstats_baseline AS b
        WHERE c.name = b.name;
--      WHERE c.name = b.name AND (c.value - b.value > 0);
-- SELECT * FROM test.nstats_diff;
-- \. nstats_diff.sql
