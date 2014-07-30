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

// Replay a block allocator trace against different strategies and compare
// the results

#include <db.h>

#include <stdio.h>
#include <string.h>

#include <map>
#include <set>
#include <string>
#include <sstream>
#include <vector>

#include <portability/memory.h>
#include <portability/toku_assert.h>
#include <portability/toku_stdlib.h>

#include "ft/serialize/block_allocator.h"

using std::map;
using std::set;
using std::string;
using std::vector;

static void ba_replay_assert(bool pred, const char *msg, const char *line, int line_num) {
    if (!pred) {
        fprintf(stderr, "%s, line (#%d): %s\n", msg, line_num, line);
        abort();
    }
}

// return line with whitespace skipped, and any newline replaced with a null byte
static char *tidy_line(char *line) {
    // skip leading whitespace
    while (isspace(*line)) {
        line++;
    }
    char *ptr = strchr(line, '\n');
    if (ptr != nullptr) {
        *ptr = '\0';
    }
    return line;
}

static int64_t parse_number(char **ptr, int line_num, int base) {
    *ptr = tidy_line(*ptr);

    char *new_ptr;
    int64_t n = strtoll(*ptr, &new_ptr, base);
    ba_replay_assert(n >= 0, "malformed trace", *ptr, line_num);
    *ptr = new_ptr;
    return n;
}

static uint64_t parse_uint64(char **ptr, int line_num) {
    int64_t n = parse_number(ptr, line_num, 10);
    ba_replay_assert(n >= 0, "malformed trace", *ptr, line_num);
    // we happen to know that the uint64's we deal with will
    // take less than 63 bits (they come from pointers)
    return static_cast<uint64_t>(n);
}

static string parse_token(char **ptr, int line_num) {
    char *line = *ptr;

    // parse the first token, which represents the traced function
    char token[64];
    int r = sscanf(line, "%64s", token);
    ba_replay_assert(r == 1, "malformed trace", line, line_num);
    *ptr += strlen(token);
    return string(token);
}

static vector<string> canonicalize_trace_from(FILE *file) {
    // new trace, canonicalized from a raw trace
    vector<string> canonicalized_trace;

    // raw allocator id -> canonical allocator id
    //
    // keeps track of allocators that were created as part of the trace,
    // and therefore will be part of the canonicalized trace.
    uint64_t allocator_id_seq_num = 0;
    map<uint64_t, uint64_t> allocator_ids;

    // allocated offset -> allocation seq num
    //
    uint64_t allocation_seq_num = 0;
    typedef map<uint64_t, uint64_t> offset_seq_map;

    // raw allocator id -> offset_seq_map that tracks its allocations
    map<uint64_t, offset_seq_map> offset_to_seq_num_maps;

    int line_num = 0;
    const int max_line = 512;
    char line[max_line];
    while (fgets(line, max_line, file) != nullptr) {
        line_num++;

        // removes leading whitespace and trailing newline
        char *ptr = tidy_line(line);

        string fn = parse_token(&ptr, line_num);
        int64_t allocator_id = parse_number(&ptr, line_num, 16);

        std::stringstream ss;
        if (fn == "ba_trace_create") {
            // only allocators created in the raw traec will be part of the
            // canonical trace, so save the next canonical allocator id here.
            ba_replay_assert(allocator_ids.count(allocator_id) == 0, "corrupted trace: double create", line, line_num);
            allocator_ids[allocator_id] = allocator_id_seq_num;
            ss << fn << ' ' << allocator_id_seq_num << ' ' << std::endl;
            allocator_id_seq_num++;
        } else if (allocator_ids.count(allocator_id) > 0) {
            // this allocator is part of the canonical trace
            uint64_t canonical_allocator_id = allocator_ids[allocator_id];

            // this is the map that tracks allocations for this allocator
            offset_seq_map *map = &offset_to_seq_num_maps[allocator_id];

            if (fn == "ba_trace_alloc") {
                const uint64_t size = parse_uint64(&ptr, line_num);
                const uint64_t heat = parse_uint64(&ptr, line_num);
                const uint64_t offset = parse_uint64(&ptr, line_num);
                ba_replay_assert(map->count(offset) == 0, "corrupted trace: double alloc", line, line_num);

                // remember that an allocation at `offset' has the current alloc seq num
                (*map)[offset] = allocation_seq_num;

                // translate `offset = alloc(size)' to `asn = alloc(size)'
                ss << fn << ' ' << canonical_allocator_id << ' ' << size << ' ' << heat << ' ' << allocation_seq_num << std::endl;
                allocation_seq_num++;
            } else if (fn == "ba_trace_free") {
                const uint64_t offset = parse_uint64(&ptr, line_num);
                ba_replay_assert(map->count(offset) != 0, "corrupted trace: invalid free", line, line_num);

                // get the alloc seq num for an allcation that occurred at `offset'
                const uint64_t asn = (*map)[offset];
                map->erase(offset);

                // translate `free(offset)' to `free(asn)'
                ss << fn << ' ' << canonical_allocator_id << ' ' << asn << std::endl;
            } else if (fn == "ba_trace_destroy") {
                // Remove this allocator from both maps
                allocator_ids.erase(allocator_id);
                offset_to_seq_num_maps.erase(allocator_id);

                // translate `destroy(ptr_id) to destroy(canonical_id)'
                ss << fn << ' ' << canonical_allocator_id << ' ' << std::endl;
            }
        } else {
            // traced an alloc/free for an allocator not created as part of this trace, skip
            continue;
        }
        canonicalized_trace.push_back(ss.str());
    }

    return canonicalized_trace;
}

static void replay_canonicalized_trace(const vector<string> &canonicalized_trace,
                                       block_allocator::allocation_strategy strategy,
                                       map<uint64_t, block_allocator *> *allocator_map) {
    // maps allocation seq num to allocated offset
    map<uint64_t, uint64_t> seq_num_to_offset;

    int line_num = 0;
    for (vector<string>::const_iterator it = canonicalized_trace.begin();
         it != canonicalized_trace.end(); it++) {
        line_num++;

        char *line = toku_strdup(it->c_str());

        printf("playing canonical trace line #%d: %s", line_num, line);
        char *ptr = tidy_line(line);

        // canonical allocator id is in base 10, not 16
        string fn = parse_token(&ptr, line_num);
        int64_t allocator_id = parse_number(&ptr, line_num, 10);

        if (fn == "ba_trace_create") {
            ba_replay_assert(allocator_map->count(allocator_id) == 0,
                             "corrupted canonical trace: double create", line, line_num);

            block_allocator *ba = new block_allocator();
            ba->create(8096, 4096); // header reserve, alignment - taken from block_table.cc
            ba->set_strategy(strategy);

            // caller owns the allocator_map and its contents
            (*allocator_map)[allocator_id] = ba;
        } else {
            ba_replay_assert(allocator_map->count(allocator_id) > 0,
                             "corrupted canonical trace: no such allocator", line, line_num);

            block_allocator *ba = (*allocator_map)[allocator_id];
            if (fn == "ba_trace_alloc") {
                const uint64_t size = parse_uint64(&ptr, line_num);
                const uint64_t heat = parse_uint64(&ptr, line_num);
                const uint64_t asn = parse_uint64(&ptr, line_num);
                ba_replay_assert(seq_num_to_offset.count(asn) == 0,
                                 "corrupted canonical trace: double alloc (asn in use)", line, line_num);

                uint64_t offset;
                ba->alloc_block(size, heat, &offset);
                seq_num_to_offset[asn] = offset;
            } else if (fn == "ba_trace_free") {
                const uint64_t asn = parse_uint64(&ptr, line_num);
                ba_replay_assert(seq_num_to_offset.count(asn) == 1,
                                 "corrupted canonical trace: double free (asn unused)", line, line_num);

                uint64_t offset = seq_num_to_offset[asn];
                ba->free_block(offset);
                seq_num_to_offset.erase(asn);
            } else if (fn == "ba_trace_destroy") {
                // TODO: Clean this up - we won't be able to catch no such allocator errors
                // if we don't actually not the destroy. We only do it here so that the caller
                // can gather statistics on all closed allocators at the end of the run.
                // allocator_map->erase(allocator_id);
            } else {
                ba_replay_assert(false, "corrupted canonical trace: bad fn", line, line_num);
            }
        }

        toku_free(line);
    }
}

// TODO: Put this in the allocation strategy class
static const char *strategy_str(block_allocator::allocation_strategy strategy) {
    switch (strategy) {
    case block_allocator::allocation_strategy::BA_STRATEGY_FIRST_FIT:
        return "first-fit";
    case block_allocator::allocation_strategy::BA_STRATEGY_BEST_FIT:
        return "best-fit";
    case block_allocator::allocation_strategy::BA_STRATEGY_HEAT_ZONE:
        return "heat-zone";
    default:
        abort();
    }
}

static void print_result(uint64_t allocator_id,
                         block_allocator::allocation_strategy strategy,
                         TOKU_DB_FRAGMENTATION report) {
    uint64_t total_bytes = report->data_bytes + report->unused_bytes;
    uint64_t total_blocks = report->data_blocks + report->unused_blocks;
    if (total_bytes < 32UL * 1024 * 1024) {
        printf("skipping allocator_id %" PRId64 " (total bytes < 32mb)\n", allocator_id);
        return;
    }

    printf("\n");
    printf("allocator_id:   %20" PRId64 "\n", allocator_id);
    printf("strategy:       %20s\n", strategy_str(strategy));

    // byte statistics
    printf("total bytes:    %20" PRId64 "\n", total_bytes);
    printf("used bytes:     %20" PRId64 " (%.3lf)\n", report->data_bytes,
           static_cast<double>(report->data_bytes) / total_bytes);
    printf("unused bytes:   %20" PRId64 " (%.3lf)\n", report->unused_bytes,
           static_cast<double>(report->unused_bytes) / total_bytes);

    // block statistics
    printf("total blocks:   %20" PRId64 "\n", total_blocks);
    printf("used blocks:    %20" PRId64 " (%.3lf)\n", report->data_blocks,
           static_cast<double>(report->data_blocks) / total_blocks);
    printf("unused blocks:  %20" PRId64 " (%.3lf)\n", report->unused_blocks,
           static_cast<double>(report->unused_blocks) / total_blocks);

    // misc
    printf("largest unused: %20" PRId64 "\n", report->largest_unused_block);
}

int main(void) {
    // Read the raw trace from stdin
    vector<string> canonicalized_trace = canonicalize_trace_from(stdin);

    vector<enum block_allocator::allocation_strategy> candidate_strategies;
    candidate_strategies.push_back(block_allocator::allocation_strategy::BA_STRATEGY_FIRST_FIT);
    candidate_strategies.push_back(block_allocator::allocation_strategy::BA_STRATEGY_BEST_FIT);
    candidate_strategies.push_back(block_allocator::allocation_strategy::BA_STRATEGY_PADDED_FIT);
    candidate_strategies.push_back(block_allocator::allocation_strategy::BA_STRATEGY_HEAT_ZONE);

    for (vector<enum block_allocator::allocation_strategy>::const_iterator it = candidate_strategies.begin();
         it != candidate_strategies.end(); it++) {
        const block_allocator::allocation_strategy strategy(*it);

        // replay the canonicalized trace against the current strategy.
        //
        // we provided the allocator map so we can gather statistics later
        map<uint64_t, block_allocator *> allocator_map;
        replay_canonicalized_trace(canonicalized_trace, strategy, &allocator_map);

        for (map<uint64_t, block_allocator *>::iterator al = allocator_map.begin();
             al != allocator_map.end(); al++) {
            block_allocator *ba = al->second;

            TOKU_DB_FRAGMENTATION_S report;
            ba->get_statistics(&report);
            ba->destroy();

            print_result(al->first, strategy,&report);
        }
    }

    return 0;
}
