# There should be no errors
Pattern "\[ERROR\]" not found
# There should be no errors
Pattern "\[ERROR\]" not found
# There should be no leftovers in the datadir.
# There should be no errors
Pattern "\[ERROR\]" not found
# There should be no errors
Pattern "\[ERROR\]" not found
# There should be no leftovers in the datadir.
SELECT PLUGIN_NAME, PLUGIN_STATUS, LOAD_OPTION FROM INFORMATION_SCHEMA.PLUGINS
WHERE PLUGIN_NAME IN ('ARCHIVE', 'BLACKHOLE', 'FEDERATED')
ORDER BY PLUGIN_NAME;
PLUGIN_NAME	PLUGIN_STATUS	LOAD_OPTION
ARCHIVE	ACTIVE	ON
BLACKHOLE	ACTIVE	ON
FEDERATED	DISABLED	OFF
# archive and blackhole should be on, federated off
Pattern "^(?i)ARCHIVE\s+ON" found
Pattern "^(?i)BLACKHOLE\s+ON" found
Pattern "^(?i)FEDERATED\s+OFF" found
# archive should be off, federated on
Pattern "^(?i)ARCHIVE\s+OFF" found
Pattern "^(?i)FEDERATED\s+ON" found
