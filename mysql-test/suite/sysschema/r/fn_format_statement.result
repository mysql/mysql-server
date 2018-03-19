SELECT sys.format_statement(NULL);
sys.format_statement(NULL)
NULL
SELECT sys.format_statement('SELECT foo, bar, baz, foobar, foobaz FROM foo JOIN bar USING (foobar) JOIN baz USING (foobar) WHERE foo = \'foo\' AND bar = \'bar\'') AS statement;
statement
SELECT foo, bar, baz, foobar,  ... RE foo = 'foo' AND bar = 'bar'
SET @sys.statement_truncate_len = 80;
SELECT sys.format_statement('SELECT foo, bar, baz, foobar, foobaz FROM foo JOIN bar USING (foobar) JOIN baz USING (foobar) WHERE foo = \'foo\' AND bar = \'bar\'') AS statement;
statement
SELECT foo, bar, baz, foobar, foobaz F ... bar) WHERE foo = 'foo' AND bar = 'bar'
