# Copyright (C) 2003, 2005 MySQL AB, 2010 Sun Microsystems, Inc.
#  All rights reserved. Use is subject to license terms.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

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
&copy; 2003, 2005 MySQL AB, 2010 Sun Microsystems, Inc.
<br>
</center>
</small></address>
</body>
</html>
EOT

print "Preformat finished\n\n";
