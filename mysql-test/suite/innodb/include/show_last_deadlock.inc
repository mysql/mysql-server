--let status = `SHOW ENGINE INNODB STATUS`
#--let status = 'LATEST DETECTED DEADLOCK o my " got TRANSACTIONS'
--disable_query_log
--eval SET @status = "$status"
--enable_query_log
# In theory we could use replace_regex to remove everything before LATEST DETECTED DEADLOCK
# and everything after TRANSACTIONS without having to play with @status server-side,
# but in reality, it takes 6 seconds longer then (at least on my laptop).
SELECT SUBSTRING(@status,LOCATE('LATEST DETECTED DEADLOCK',@status)) INTO @status;
SELECT SUBSTRING(@status,1,LOCATE('TRANSACTIONS',@status)-1) INTO @status;

replace_regex
  /\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}/%DATE%/
  /0x[0-9a-f]+/%HEXADECIMAL%/
  /thread handle \d+/thread handle %NUMBER%/
  /\b\d+ sec\b/%NUMBER% sec/
  /thread id \d+/thread id %NUMBER%/
  /TRANSACTION \d+/TRANSACTION %NUMBER%/
  /hex [0-9a-f]+/hex %HEXADECIMAL%/
  /query id \d+ .* statistics/query id %NUMBER% %ADDRESS% %USER% statistics/
  /trx id \d+/trx id %NUMBER%/
  /asc.*(\x3b\x3b)/asc %ASC%\1/
  /space id \d+/space id %NUMBER%/
;
SELECT @status;
