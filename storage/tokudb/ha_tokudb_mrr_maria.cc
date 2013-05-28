/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
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

  TokuDB, Tokutek Fractal Tree Indexing Library.
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
/****************************************************************************
 * DS-MRR implementation, essentially copied from InnoDB/MyISAM/Maria
 ***************************************************************************/

/**
 * Multi Range Read interface, DS-MRR calls
 */

int ha_tokudb::multi_range_read_init(RANGE_SEQ_IF *seq, void *seq_init_param,
                                       uint n_ranges, uint mode, 
                                       HANDLER_BUFFER *buf)
{
  return ds_mrr.dsmrr_init(this, seq, seq_init_param, n_ranges, mode, buf);
}

int ha_tokudb::multi_range_read_next(range_id_t *range_info)
{
  return ds_mrr.dsmrr_next(range_info);
}

ha_rows ha_tokudb::multi_range_read_info_const(uint keyno, RANGE_SEQ_IF *seq,
                                                 void *seq_init_param,  
                                                 uint n_ranges, uint *bufsz,
                                                 uint *flags, 
                                                 COST_VECT *cost)
{
  /* See comments in ha_myisam::multi_range_read_info_const */
  ds_mrr.init(this, table);
  ha_rows res= ds_mrr.dsmrr_info_const(keyno, seq, seq_init_param, n_ranges,
                                       bufsz, flags, cost);
  return res;
}

ha_rows ha_tokudb::multi_range_read_info(uint keyno, uint n_ranges, uint keys,
                                           uint key_parts, uint *bufsz, 
                                           uint *flags, COST_VECT *cost)
{
  ds_mrr.init(this, table);
  ha_rows res= ds_mrr.dsmrr_info(keyno, n_ranges, keys, key_parts, bufsz, 
                                 flags, cost);
  return res;
}

int ha_tokudb::multi_range_read_explain_info(uint mrr_mode, char *str, size_t size)
{
  return ds_mrr.dsmrr_explain_info(mrr_mode, str, size);
}
