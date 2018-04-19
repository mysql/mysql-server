SELECT sys.quote_identifier(NULL);
sys.quote_identifier(NULL)
NULL
SELECT sys.quote_identifier('abc');
sys.quote_identifier('abc')
`abc`
SELECT sys.quote_identifier('ab`c');
sys.quote_identifier('ab`c')
`ab``c`
SELECT sys.quote_identifier('ab``c');
sys.quote_identifier('ab``c')
`ab````c`
SELECT sys.quote_identifier('ab```c');
sys.quote_identifier('ab```c')
`ab``````c`
SELECT sys.quote_identifier('a`b`c');
sys.quote_identifier('a`b`c')
`a``b``c`
SELECT sys.quote_identifier('a`b``c');
sys.quote_identifier('a`b``c')
`a``b````c`
