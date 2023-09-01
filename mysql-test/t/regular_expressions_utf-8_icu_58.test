SET NAMES utf8mb3;

--let $required_icu_version=58
--source include/require_icu_version.inc

SELECT regexp_like( 'abc\n123\n456\nxyz\n', '(?m)^\\d+\\R\\d+$' );
--error ER_REGEXP_RULE_SYNTAX
SELECT regexp_like( 'a\nb', '(*CR)a.b' );
SELECT regexp_like( 'a\nb', 'a\\vb' );
