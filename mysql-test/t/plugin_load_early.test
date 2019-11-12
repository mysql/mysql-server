--echo #
--echo # Test for WL#12935: Control what plugins can be passed to --early-plugin-load
--echo #

call mtr.add_suppression("Plugin .* is not to be used as an .early. plugin");
call mtr.add_suppression("Couldn't load plugin named .* with soname ");

--echo # plugin should not be loaded as it's not enabled for -early-plugin-load
--error ER_UNKNOWN_SYSTEM_VARIABLE
SELECT @@global.example_enum_var = 'e2';
