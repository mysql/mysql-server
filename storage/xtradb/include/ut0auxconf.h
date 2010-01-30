/* Do not remove this file even though it is empty.
This file is included in univ.i and will cause compilation failure
if not present.
A custom checks have been added in the generated
storage/innobase/Makefile.in that is shipped with the InnoDB Plugin
source archive. These checks eventually define some macros and put
them in this file.
This is a hack that has been developed in order to deploy new compile
time checks without the need to regenerate the ./configure script that is
distributed in the MySQL 5.1 official source archives.
If by any chance Makefile.in and ./configure are regenerated and thus
the hack from Makefile.in wiped away then the "real" checks from plug.in
will take over.
*/
