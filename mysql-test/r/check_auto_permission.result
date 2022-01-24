CALL mtr.add_suppression("World-writable config file .* is ignored");
CALL mtr.add_suppression("World-writable config file .* has been removed.");
# restart
include/assert.inc [File permissions should be equl to decimal 432(i.e octal 660).]
