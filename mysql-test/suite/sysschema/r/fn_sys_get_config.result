SET @sys.statement_truncate_len := IFNULL(@sys.statement_truncate_len, sys.sys_get_config('statement_truncate_len', 128));
SELECT @sys.statement_truncate_len;
@sys.statement_truncate_len
64
SET @sys.foo := IFNULL(@sys.foo, sys.sys_get_config('foo', 'foobar'));
SELECT @sys.foo;
@sys.foo
foobar
