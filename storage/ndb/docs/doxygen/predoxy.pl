#
#  Written by Lars Thalmann, lars@mysql.com, 2003.
#

use strict;
umask 000;

# -----------------------------------------------------------------------------
#  Fix HTML Footer
# -----------------------------------------------------------------------------

open (OUTFILE, "> footer.html");

print OUTFILE<<EOT;
<hr>
<address>
<small>
<center>
EOT
print OUTFILE "Documentation generated " . localtime() . 
    " from mysql source files.";
print OUTFILE<<EOT;
<br>
&copy; 2003-2004 MySQL AB
<br>
</center>
</small></address>
</body>
</html>
EOT

print "Preformat finished\n\n";
