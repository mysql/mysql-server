call mtr.add_suppression("When binlog_format=STATEMENT, the option binlog_row_value_options=PARTIAL_JSON");
call mtr.add_suppression("When binlog_row_image=FULL, the option binlog_row_value_options=PARTIAL_JSON");
call mtr.add_suppression("When binlog_row_value_options=PARTIAL_JSON, the option log_bin_use_v1_row_events=1");
call mtr.add_suppression("When the binary log is disabled, the option binlog_row_value_options=PARTIAL_JSON");
call mtr.add_suppression("You need to use --log-bin to make --binlog-format work.");
# binlog-format=stmt is not compatible with partial json
# restart: --log-bin --binlog-format=statement --binlog-row-value-options=PARTIAL_JSON --binlog-row-image=MINIMAL
include/assert_grep.inc [There shall be a warning when binlog_format=STATEMENT]
SET @@GLOBAL.BINLOG_ROW_VALUE_OPTIONS= PARTIAL_JSON;
Warnings:
Warning	3645	When binlog_format=STATEMENT, the option binlog_row_value_options=PARTIAL_JSON will be ignored and updates will be written in full format to binary log.
# binlog-row-image=full causes partial json to be used only in after image
# restart: --log-bin --binlog-format=row --binlog-row-value-options=PARTIAL_JSON --binlog-row-image=FULL
include/assert_grep.inc [There shall be a warning when binlog_row_image=FULL]
# log-bin-use-v1-row-events is not compatible with partial json
# restart: --log-bin --log-bin-use-v1-row-events=TRUE --binlog-row-value-options=PARTIAL_JSON --binlog-row-image=MINIMAL
include/assert_grep.inc [There shall be a warning when log_bin_use_v1_row_events=1]
SET @@GLOBAL.BINLOG_ROW_VALUE_OPTIONS= PARTIAL_JSON;
Warnings:
Warning	3646	When binlog_row_value_options=PARTIAL_JSON, the option log_bin_use_v1_row_events=1 will be ignored and row events will be written in new format to binary log.
# the binlog is disabled
# restart: --binlog-row-value-options=PARTIAL_JSON --binlog-row-image=MINIMAL --skip-log-bin --skip-log-slave-updates
include/assert_grep.inc [There shall be a warning when when the binary log is disabled]
SET @@GLOBAL.BINLOG_ROW_VALUE_OPTIONS= PARTIAL_JSON;
Warnings:
Warning	3645	When the binary log is closed, the option binlog_row_value_options=PARTIAL_JSON will be ignored and updates will be written in full format to binary log.
