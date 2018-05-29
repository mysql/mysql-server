SET @session_start_value = @@collation_database;
CREATE DATABASE collation_test DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_as_ci;
USE collation_test;
SET @@session.collation_database='utf8mb4_vi_0900_as_cs';
Warnings:
Warning	1681	Updating 'collation_database' is deprecated. It will be made read-only in a future release.
DROP DATABASE collation_test;
