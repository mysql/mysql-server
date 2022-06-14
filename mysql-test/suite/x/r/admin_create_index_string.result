CREATE SCHEMA xtest DEFAULT CHARSET 'utf8mb4';

command ok
1

command ok
Got expected error (code 0)

1 rows affected
Got expected error (code 0)

1 rows affected
Got expected error (code 0)

1 rows affected
Got expected error (code 0)
Verify ["EXPLAIN SELECT * FROM xtest.books1 USE INDEX(`isbn`) WHERE doc->'$.isbn' = '9780262527804'" returns in "possible_keys" column, following value isbn]
Got expected error: Collection contains document missing required field (code 5117)

command ok
Got expected error (code 0)
Verify ["EXPLAIN SELECT * FROM xtest.books1 USE INDEX(`pages`) WHERE doc->'$.pages' > 1" returns in "possible_keys" column, following value pages]

command ok
Got expected error (code 0)
Verify ["EXPLAIN SELECT * FROM xtest.books1 USE INDEX(`name`) WHERE doc->'$.name' = 'The Bla'" returns in "possible_keys" column, following value name]

command ok
Got expected error (code 0)
Verify ["EXPLAIN SELECT * FROM xtest.books1 USE INDEX(`another name`) WHERE doc->'$.name' = 'The Bla'" returns in "possible_keys" column, following value another name]

command ok
Got expected error (code 0)
Verify ["EXPLAIN SELECT * FROM xtest.books1 USE INDEX(`name_char`) WHERE doc->'$.name' = 'The Bla'" returns in "possible_keys" column, following value name_char]
Got expected error: Data too long for column '$ix_c_r_27B323C313AA758D7F01480F94271D0B13D88CE7' at row 1 (code 1406)
Got expected error: Document is missing a required field (code 5115)

command ok
Got expected error (code 0)

command ok
Got expected error (code 0)
Got expected error: Invalid value for argument 'fields.field' (code 5017)
Got expected error: Argument value '' for index name is invalid (code 5017)
Got expected error: Invalid or unsupported type specification 'text(20);drop table' (code 5017)
Got expected error: Invalid or unsupported type specification 'varchar(20)' (code 5017)
Got expected error: Invalid value for argument 'fields.field' (code 5017)
Got expected error: Invalid value for argument 'fields.field' (code 5017)
Got expected error: Invalid value for argument 'fields.field' (code 5017)

command ok

1 rows affected
Got expected error (code 0)

1 rows affected
Got expected error (code 0)

command ok
Verify ["EXPLAIN SELECT * FROM xtest.books0 USE INDEX(`mix`) WHERE doc->'$.isbn' = '9780262527804' and doc->'$.pages' > 1" returns in "possible_keys" column, following value mix]

command ok
Got expected error (code 0)
Verify ["EXPLAIN SELECT * FROM xtest.books0 USE INDEX(`name_charset`) WHERE doc->'$.name' = 'The Bla'" returns in "possible_keys" column, following value name_charset]

command ok
Got expected error (code 0)
Verify ["EXPLAIN SELECT * FROM xtest.books0 USE INDEX(`isbn_text_charset`) WHERE doc->'$.isbn' = '9780262527804'" returns in "possible_keys" column, following value isbn_text_charset]
Table	Create Table
books0	CREATE TABLE `books0` (
  `doc` json DEFAULT NULL,
  `_id` varbinary(32) GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$._id'))) STORED NOT NULL,
  `_json_schema` json GENERATED ALWAYS AS (_utf8mb4'{"type":"object"}') VIRTUAL,
  `$ix_t80_27B323C313AA758D7F01480F94271D0B13D88CE7` text GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$.isbn'))) VIRTUAL,
  `$ix_i_94F8A4D3704392CE73A5A69F07681271C98C8CB2` int GENERATED ALWAYS AS (json_extract(`doc`,_utf8mb4'$.pages')) VIRTUAL,
  `$ix_c64_r_2BAA686C5604372BA51E965E6346198F5072C3D7` char(64) CHARACTER SET latin1 COLLATE latin1_bin GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$.name'))) VIRTUAL NOT NULL,
  `$ix_t64_r_27B323C313AA758D7F01480F94271D0B13D88CE7` text CHARACTER SET latin1 COLLATE latin1_bin GENERATED ALWAYS AS (json_unquote(json_extract(`doc`,_utf8mb4'$.isbn'))) VIRTUAL NOT NULL,
  PRIMARY KEY (`_id`),
  UNIQUE KEY `name_charset` (`$ix_c64_r_2BAA686C5604372BA51E965E6346198F5072C3D7`),
  UNIQUE KEY `isbn_text_charset` (`$ix_t64_r_27B323C313AA758D7F01480F94271D0B13D88CE7`(64)),
  UNIQUE KEY `mix` (`$ix_t80_27B323C313AA758D7F01480F94271D0B13D88CE7`(80),`$ix_i_94F8A4D3704392CE73A5A69F07681271C98C8CB2`),
  CONSTRAINT `$val_strict_66FE29BD959E94BA4075737F67D513C0C2105309` CHECK (json_schema_valid(`_json_schema`,`doc`)) /*!80016 NOT ENFORCED */
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
0 rows affected
Mysqlx.Ok {
  msg: "bye!"
}
ok
