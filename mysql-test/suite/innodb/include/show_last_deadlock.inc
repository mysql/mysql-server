replace_regex
  /TRANSACTIONS[\w\W]*//
  /[\w\W]*LATEST DETECTED DEADLOCK//
  /\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2} .+/%DATE% %THREAD_ID%/
  /0x[0-9a-f]+/%HEXADECIMAL%/
  /thread handle \d+/thread handle %NUMBER%/
  /\b\d+ sec\b/%NUMBER% sec/
  /heap size \d+/heap size %NUMBER%/
  /thread id \d+/thread id %NUMBER%/
  /TRANSACTION \d+/TRANSACTION %NUMBER%/
  /hex [0-9a-f]+/hex %HEXADECIMAL%/
  /query id \d+ .* (statistics|executing)/query id %NUMBER% %ADDRESS% %USER% statistics/
  /trx id \d+/trx id %NUMBER%/
  /asc.*(\x3b\x3b)/asc %ASC%\1/
  /space id \d+/space id %NUMBER%/
;
SHOW ENGINE INNODB STATUS;
