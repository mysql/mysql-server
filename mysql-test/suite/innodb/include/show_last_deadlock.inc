DELIMITER !;

replace_regex
  /TRANSACTIONS.*//
  /.*LATEST DETECTED DEADLOCK//
  /[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}/%DATE%/
  /0x[0-9a-f]+/%HEXADECIMAL%/
  /thread handle [0-9]+/thread handle %NUMBER%/
  /\b[0-9]+ sec\b/%NUMBER% sec/
  /heap size [0-9]+/heap size %NUMBER%/
  /thread id [0-9]+/thread id %NUMBER%/
  /TRANSACTION [0-9]+/TRANSACTION %NUMBER%/
  /hex [0-9a-f]+/hex %HEXADECIMAL%/
  /query id [0-9]+ ([^ ]+|[^ ]+ [^ ]+) [^ ]+ statistics/query id %NUMBER% %ADDRESS% %USER% statistics/
  /trx id [0-9]+/trx id %NUMBER%/
  /asc [^;]*;;;?([^;])/asc %ASC%;;\1/
  /space id [0-9]+/space id %NUMBER%/
!
SHOW ENGINE INNODB STATUS!
DELIMITER ;!
