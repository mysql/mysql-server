SET @sys.debug                        = 'ON',
@sys.diagnostics.allow_i_s_tables = 'ON',
@sys.diagnostics.include_raw      = 'ON';
CALL sys.diagnostics(4, 2, 'full');
SET @sys.debug                        = 'OFF',
@sys.diagnostics.allow_i_s_tables = 'OFF',
@sys.diagnostics.include_raw      = 'OFF';
CALL sys.diagnostics(0, 0, 'full');
summary
Disabled 1 thread
ERROR 45000: in_max_runtime must be greater than 0
CALL sys.diagnostics(2, 0, 'full');
ERROR 45000: in_interval must be greater than 0
CALL sys.diagnostics(1, 2, 'full');
ERROR 45000: in_max_runtime must be greater than or equal to in_interval
