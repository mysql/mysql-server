** Connecting con1 using root **
SELECT date(now()) = date(sysdate());
date(now()) = date(sysdate())
1
SET @@session.timestamp = 1100000000;
SELECT date(now()) != date(sysdate());
date(now()) != date(sysdate())
0
** Connecting con0 using root **
SELECT @@session.timestamp != 1100000000;
@@session.timestamp != 1100000000
1
SET @@session.timestamp = 1000000000;
SELECT date(now()) != date(sysdate());
date(now()) != date(sysdate())
0
** Connection con1 **
SELECT @@session.timestamp != 1000000000;
@@session.timestamp != 1000000000
1
SELECT @@session.timestamp = 1100000000;
@@session.timestamp = 1100000000
1
** Connection default **
Disconnecting Connections con0, con1
