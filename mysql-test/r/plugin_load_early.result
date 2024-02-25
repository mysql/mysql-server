#
# Test for WL#12935: Control what plugins can be passed to --early-plugin-load
#
call mtr.add_suppression("Plugin .* is not to be used as an .early. plugin");
call mtr.add_suppression("Couldn't load plugin named .* with soname ");
# plugin should not be loaded as it's not enabled for -early-plugin-load
SELECT @@global.example_enum_var = 'e2';
ERROR HY000: Unknown system variable 'example_enum_var'
