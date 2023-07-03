# Copyright (c) 2003, 2023, Oracle and/or its affiliates.
#  Use is subject to license terms.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
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
