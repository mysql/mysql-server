TYPE=TRIGGERS
triggers='CREATE DEFINER=`root`@`localhost` TRIGGER ins_sum BEFORE INSERT ON account FOR EACH ROW SET @sum = @sum + NEW.amount'
sql_modes=96468992
definers='root@localhost'
client_cs_names='utf8'
connection_cl_names='utf8_general_ci'
db_cl_names='latin1_swedish_ci'
