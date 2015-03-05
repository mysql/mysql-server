--source include/have_debug_sync.inc

--echo # Bug#20554017 CONCAT MAY INCORRECTLY COPY OVERLAPPING STRINGS

SET @old_debug= @@session.debug;
SET session debug='d,force_fake_uuid';

do concat('111','11111111111111111111111111',
          substring_index(uuid(),0,1.111111e+308));

do concat_ws(',','111','11111111111111111111111111',
             substring_index(uuid(),0,1.111111e+308));

SET session debug= @old_debug;
