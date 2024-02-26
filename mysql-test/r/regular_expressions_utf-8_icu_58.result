SET NAMES utf8;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
SELECT regexp_like( 'abc\n123\n456\nxyz\n', '(?m)^\\d+\\R\\d+$' );
regexp_like( 'abc\n123\n456\nxyz\n', '(?m)^\\d+\\R\\d+$' )
1
SELECT regexp_like( 'a\nb', '(*CR)a.b' );
ERROR HY000: Syntax error in regular expression on line 1, character 2.
SELECT regexp_like( 'a\nb', 'a\\vb' );
regexp_like( 'a\nb', 'a\\vb' )
1
