/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuFT, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

const char *toku_patent_string = "COPYING CONDITIONS NOTICE:\n\
\n\
  This program is free software; you can redistribute it and/or modify\n\
  it under the terms of version 2 of the GNU General Public License as\n\
  published by the Free Software Foundation, and provided that the\n\
  following conditions are met:\n\
\n\
      * Redistributions of source code must retain this COPYING\n\
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the\n\
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the\n\
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS\n\
        GRANT (below).\n\
\n\
      * Redistributions in binary form must reproduce this COPYING\n\
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the\n\
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the\n\
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS\n\
        GRANT (below) in the documentation and/or other materials\n\
        provided with the distribution.\n\
\n\
  You should have received a copy of the GNU General Public License\n\
  along with this program; if not, write to the Free Software\n\
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA\n\
  02110-1301, USA.\n\
\n\
COPYRIGHT NOTICE:\n\
\n\
  TokuFT, Tokutek Fractal Tree Indexing Library.\n\
  Copyright (C) 2007-2013 Tokutek, Inc.\n\
\n\
DISCLAIMER:\n\
\n\
  This program is distributed in the hope that it will be useful, but\n\
  WITHOUT ANY WARRANTY; without even the implied warranty of\n\
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU\n\
  General Public License for more details.\n\
\n\
UNIVERSITY PATENT NOTICE:\n\
\n\
  The technology is licensed by the Massachusetts Institute of\n\
  Technology, Rutgers State University of New Jersey, and the Research\n\
  Foundation of State University of New York at Stony Brook under\n\
  United States of America Serial No. 11/760379 and to the patents\n\
  and/or patent applications resulting from it.\n\
\n\
PATENT MARKING NOTICE:\n\
\n\
  This software is covered by US Patent No. 8,185,551.\n\
  This software is covered by US Patent No. 8,489,638.\n\
\n\
PATENT RIGHTS GRANT:\n\
\n\
  \"THIS IMPLEMENTATION\" means the copyrightable works distributed by\n\
  Tokutek as part of the Fractal Tree project.\n\
\n\
  \"PATENT CLAIMS\" means the claims of patents that are owned or\n\
  licensable by Tokutek, both currently or in the future; and that in\n\
  the absence of this license would be infringed by THIS\n\
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.\n\
\n\
  \"PATENT CHALLENGE\" shall mean a challenge to the validity,\n\
  patentability, enforceability and/or non-infringement of any of the\n\
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.\n\
\n\
  Tokutek hereby grants to you, for the term and geographical scope of\n\
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,\n\
  irrevocable (except as stated in this section) patent license to\n\
  make, have made, use, offer to sell, sell, import, transfer, and\n\
  otherwise run, modify, and propagate the contents of THIS\n\
  IMPLEMENTATION, where such license applies only to the PATENT\n\
  CLAIMS.  This grant does not include claims that would be infringed\n\
  only as a consequence of further modifications of THIS\n\
  IMPLEMENTATION.  If you or your agent or licensee institute or order\n\
  or agree to the institution of patent litigation against any entity\n\
  (including a cross-claim or counterclaim in a lawsuit) alleging that\n\
  THIS IMPLEMENTATION constitutes direct or contributory patent\n\
  infringement, or inducement of patent infringement, then any rights\n\
  granted to you under this License shall terminate as of the date\n\
  such litigation is filed.  If you or your agent or exclusive\n\
  licensee institute or order or agree to the institution of a PATENT\n\
  CHALLENGE, then Tokutek may terminate any rights granted to you\n\
  under this License.\n\
";
