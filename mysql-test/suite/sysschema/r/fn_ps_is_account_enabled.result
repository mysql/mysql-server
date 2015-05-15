SELECT sys.ps_is_account_enabled('foo', 'bar');
sys.ps_is_account_enabled('foo', 'bar')
YES
DELETE FROM performance_schema.setup_actors;
INSERT INTO performance_schema.setup_actors VALUES
('test', 'test', '%', 'YES', 'NO');
SELECT sys.ps_is_account_enabled('foo', 'bar');
sys.ps_is_account_enabled('foo', 'bar')
NO
SELECT sys.ps_is_account_enabled('test', 'test');
sys.ps_is_account_enabled('test', 'test')
YES
TRUNCATE TABLE performance_schema.setup_actors;
INSERT INTO performance_schema.setup_actors VALUES ('%', '%', '%', 'YES', 'YES');
