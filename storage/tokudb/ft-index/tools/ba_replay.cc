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

// Replay a block allocator trace against different strategies and compare
// the results

#include <db.h>

#include <getopt.h>
#include <math.h>
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

static int verbose = false;

static void ba_replay_assert(bool pred, const char *msg, const char *line, int line_num) {
    if (!pred) {
        fprintf(stderr, "%s, line (#%d): %s\n", msg, line_num, line);
        abort();
    }
}

static char *trim_whitespace(char *line) {
    // skip leading whitespace
    while (isspace(*line)) {
        line++;
    }
    return line;
}

static int64_t parse_number(char **ptr, int line_num, int base) {
    *ptr = trim_whitespace(*ptr);
    char *line = *ptr;

    char *new_ptr;
    int64_t n = strtoll(line, &new_ptr, base);
    ba_replay_assert(n >= 0, "malformed trace (bad numeric token)", line, line_num);
    ba_replay_assert(new_ptr > *ptr, "malformed trace (missing numeric token)", line, line_num);
    *ptr = new_ptr;
    return n;
}

static uint64_t parse_uint64(char **ptr, int line_num) {
    int64_t n = parse_number(ptr, line_num, 10);
    // we happen to know that the uint64's we deal with will
    // take less than 63 bits (they come from pointers)
    return static_cast<uint64_t>(n);
}

static string parse_token(char **ptr, int line_num) {
    *ptr = trim_whitespace(*ptr);
    char *line = *ptr;

    // parse the first token, which represents the traced function
    char token[64];
    int r = sscanf(*ptr, "%64s", token);
    ba_replay_assert(r == 1, "malformed trace (missing string token)", line, line_num);
    *ptr += strlen(token);
    return string(token);
}

static block_allocator::blockpair parse_blockpair(char **ptr, int line_num) {
    *ptr = trim_whitespace(*ptr);
    char *line = *ptr;

    uint64_t offset, size;
    int bytes_read;
    int r = sscanf(line, "[%" PRIu64 " %" PRIu64 "]%n", &offset, &size, &bytes_read);
    ba_replay_assert(r == 2, "malformed trace (bad offset/size pair)", line, line_num);
    *ptr += bytes_read;
    return block_allocator::blockpair(offset, size);
}

static char *strip_newline(char *line, bool *found) {
    char *ptr = strchr(line, '\n');
    if (ptr != nullptr) {
        if (found != nullptr) {
            *found = true;
        }
        *ptr = '\0';
    }
    return line;
}

static char *read_trace_line(FILE *file) {
    const int buf_size = 4096;
    char buf[buf_size];
    std::stringstream ss;
    while (true) {
        if (fgets(buf, buf_size, file) == nullptr) {
            break;
        }
        bool has_newline = false;
        ss << strip_newline(buf, &has_newline);
        if (has_newline) {
            // end of the line, we're done out
            break;
        }
    }
    std::string s = ss.str();
    return s.size() ? toku_strdup(s.c_str()) : nullptr;
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
    static const uint64_t ASN_NONE = (uint64_t) -1;
    typedef map<uint64_t, uint64_t> offset_seq_map;

    // raw allocator id -> offset_seq_map that tracks its allocations
    map<uint64_t, offset_seq_map> offset_to_seq_num_maps;

    int line_num = 0;
    char *line;
    while ((line = read_trace_line(file)) != nullptr) {
        line_num++;
        char *ptr = line;

        string fn = parse_token(&ptr, line_num);
        int64_t allocator_id = parse_number(&ptr, line_num, 16);

        std::stringstream ss;
        if (fn.find("ba_trace_create") != string::npos) {
            ba_replay_assert(allocator_ids.count(allocator_id) == 0, "corrupted trace: double create", line, line_num);
            ba_replay_assert(fn == "ba_trace_create" || fn == "ba_trace_create_from_blockpairs",
                             "corrupted trace: bad fn", line, line_num);

            // we only convert the allocator_id to an allocator_id_seq_num
            // in the canonical trace and leave the rest of the line as-is.
            allocator_ids[allocator_id] = allocator_id_seq_num;
            ss << fn << ' ' << allocator_id_seq_num << ' ' << trim_whitespace(ptr) << std::endl;
            allocator_id_seq_num++;

            // First, read passed the reserve / alignment values.
            (void) parse_uint64(&ptr, line_num);
            (void) parse_uint64(&ptr, line_num);
            if (fn == "ba_trace_create_from_blockpairs") {
                // For each blockpair created by this traceline, add its offset to the offset seq map
                // with asn ASN_NONE so that later canonicalizations of `free' know whether to write
                // down the asn or the raw offset.
                offset_seq_map *map = &offset_to_seq_num_maps[allocator_id];
                while (*trim_whitespace(ptr) != '\0') {
                    const block_allocator::blockpair bp = parse_blockpair(&ptr, line_num);
                    (*map)[bp.offset] = ASN_NONE;
                }
            }
        } else {
            ba_replay_assert(allocator_ids.count(allocator_id) > 0, "corrupted trace: unknown allocator", line, line_num);
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

                // if there's an asn, then a corresponding ba_trace_alloc occurred and we should
                // write `free(asn)'. otherwise, the blockpair was initialized from create_from_blockpairs
                // and we write the original offset.
                if (asn != ASN_NONE) {
                    ss << "ba_trace_free_asn" << ' ' << canonical_allocator_id << ' ' << asn << std::endl;
                } else {
                    ss << "ba_trace_free_offset" << ' ' << canonical_allocator_id << ' ' << offset << std::endl;
                }
            } else if (fn == "ba_trace_destroy") {
                // Remove this allocator from both maps
                allocator_ids.erase(allocator_id);
                offset_to_seq_num_maps.erase(allocator_id);

                // translate `destroy(ptr_id) to destroy(canonical_id)'
                ss << fn << ' ' << canonical_allocator_id << ' ' << std::endl;
            } else {
                ba_replay_assert(false, "corrupted trace: bad fn", line, line_num);
            }
        }
        canonicalized_trace.push_back(ss.str());

        toku_free(line);
    }

    if (allocator_ids.size() != 0) {
        fprintf(stderr, "warning: leaked allocators. this might be ok if the tracing process is still running");
    }

    return canonicalized_trace;
}

struct streaming_variance_calculator {
    int64_t n_samples;
    int64_t mean;
    int64_t variance;

    // math credit: AoCP, Donald Knuth, '62
    void add_sample(int64_t x) {
        n_samples++;
        if (n_samples == 1) {
            mean = x;
            variance = 0;
        } else {
            int64_t old_mean = mean;
            mean = old_mean + ((x - old_mean) / n_samples);
            variance = (((n_samples - 1) * variance) +
                        ((x - old_mean) * (x - mean))) / n_samples;
        }
    }
};

struct canonical_trace_stats {
    uint64_t n_lines_replayed;

    uint64_t n_create;
    uint64_t n_create_from_blockpairs;
    uint64_t n_alloc_hot;
    uint64_t n_alloc_cold;
    uint64_t n_free;
    uint64_t n_destroy;

    struct streaming_variance_calculator alloc_hot_bytes;
    struct streaming_variance_calculator alloc_cold_bytes;

    canonical_trace_stats() {
        memset(this, 0, sizeof(*this));
    }
};

struct fragmentation_report {
    TOKU_DB_FRAGMENTATION_S beginning;
    TOKU_DB_FRAGMENTATION_S end;
    fragmentation_report() {
        memset(this, 0, sizeof(*this));
    }
    void merge(const struct fragmentation_report &src_report) {
        for (int i = 0; i < 2; i++) {
            TOKU_DB_FRAGMENTATION_S *dst = i == 0 ? &beginning : &end;
            const TOKU_DB_FRAGMENTATION_S *src = i == 0 ? &src_report.beginning : &src_report.end;
            dst->file_size_bytes += src->file_size_bytes;
            dst->data_bytes += src->data_bytes;
            dst->data_blocks += src->data_blocks;
            dst->checkpoint_bytes_additional += src->checkpoint_bytes_additional;
            dst->checkpoint_blocks_additional += src->checkpoint_blocks_additional;
            dst->unused_bytes += src->unused_bytes;
            dst->unused_blocks += src->unused_blocks;
            dst->largest_unused_block += src->largest_unused_block;
        }
    }
};

static void replay_canonicalized_trace(const vector<string> &canonicalized_trace,
                                       block_allocator::allocation_strategy strategy,
                                       map<uint64_t, struct fragmentation_report> *reports,
                                       struct canonical_trace_stats *stats) {
    // maps an allocator id to its block allocator
    map<uint64_t, block_allocator *> allocator_map;

    // maps allocation seq num to allocated offset
    map<uint64_t, uint64_t> seq_num_to_offset;

    for (vector<string>::const_iterator it = canonicalized_trace.begin();
         it != canonicalized_trace.end(); it++) {
        const int line_num = stats->n_lines_replayed++;

        char *line = toku_strdup(it->c_str());
        line = strip_newline(line, nullptr);

        char *ptr = trim_whitespace(line);

        // canonical allocator id is in base 10, not 16
        string fn = parse_token(&ptr, line_num);
        int64_t allocator_id = parse_number(&ptr, line_num, 10);

        if (fn.find("ba_trace_create") != string::npos) {
            const uint64_t reserve_at_beginning = parse_uint64(&ptr, line_num);
            const uint64_t alignment = parse_uint64(&ptr, line_num);
            ba_replay_assert(allocator_map.count(allocator_id) == 0,
                             "corrupted canonical trace: double create", line, line_num);

            block_allocator *ba = new block_allocator();
            if (fn == "ba_trace_create") {
                ba->create(reserve_at_beginning, alignment);
                stats->n_create++;
            } else {
                ba_replay_assert(fn == "ba_trace_create_from_blockpairs",
                                 "corrupted canonical trace: bad create fn", line, line_num);
                vector<block_allocator::blockpair> pairs;
                while (*trim_whitespace(ptr) != '\0') {
                    const block_allocator::blockpair bp = parse_blockpair(&ptr, line_num);
                    pairs.push_back(bp);
                }
                ba->create_from_blockpairs(reserve_at_beginning, alignment, &pairs[0], pairs.size());
                stats->n_create_from_blockpairs++;
            }
            ba->set_strategy(strategy);

            TOKU_DB_FRAGMENTATION_S report;
            ba->get_statistics(&report);
            (*reports)[allocator_id].beginning = report;
            allocator_map[allocator_id] = ba;
        } else {
            ba_replay_assert(allocator_map.count(allocator_id) > 0,
                             "corrupted canonical trace: no such allocator", line, line_num);

            block_allocator *ba = allocator_map[allocator_id];
            if (fn == "ba_trace_alloc") {
                // replay an `alloc' whose result will be associated with a certain asn
                const uint64_t size = parse_uint64(&ptr, line_num);
                const uint64_t heat = parse_uint64(&ptr, line_num);
                const uint64_t asn = parse_uint64(&ptr, line_num);
                ba_replay_assert(seq_num_to_offset.count(asn) == 0,
                                 "corrupted canonical trace: double alloc (asn in use)", line, line_num);

                uint64_t offset;
                ba->alloc_block(size, heat, &offset);
                seq_num_to_offset[asn] = offset;
                heat ? stats->n_alloc_hot++ : stats->n_alloc_cold++;
                heat ? stats->alloc_hot_bytes.add_sample(size) : stats->alloc_cold_bytes.add_sample(size);
            } else if (fn == "ba_trace_free_asn") {
                // replay a `free' on a block whose offset is the result of an alloc with an asn
                const uint64_t asn = parse_uint64(&ptr, line_num);
                ba_replay_assert(seq_num_to_offset.count(asn) == 1,
                                 "corrupted canonical trace: double free (asn unused)", line, line_num);

                const uint64_t offset = seq_num_to_offset[asn];
                ba->free_block(offset);
                seq_num_to_offset.erase(asn);
                stats->n_free++;
            } else if (fn == "ba_trace_free_offset") {
                // replay a `free' on a block whose offset was explicitly set during a create_from_blockpairs
                const uint64_t offset = parse_uint64(&ptr, line_num);
                ba->free_block(offset);
                stats->n_free++;
            } else if (fn == "ba_trace_destroy") {
                TOKU_DB_FRAGMENTATION_S report;
                ba->get_statistics(&report);
                ba->destroy();
                (*reports)[allocator_id].end = report;
                allocator_map.erase(allocator_id);
                stats->n_destroy++;
            } else {
                ba_replay_assert(false, "corrupted canonical trace: bad fn", line, line_num);
            }
        }

        toku_free(line);
    }
}

static const char *strategy_to_cstring(block_allocator::allocation_strategy strategy) {
    switch (strategy) {
    case block_allocator::allocation_strategy::BA_STRATEGY_FIRST_FIT:
        return "first-fit";
    case block_allocator::allocation_strategy::BA_STRATEGY_BEST_FIT:
        return "best-fit";
    case block_allocator::allocation_strategy::BA_STRATEGY_HEAT_ZONE:
        return "heat-zone";
    case block_allocator::allocation_strategy::BA_STRATEGY_PADDED_FIT:
        return "padded-fit";
    default:
        abort();
    }
}

static block_allocator::allocation_strategy cstring_to_strategy(const char *str) {
    if (strcmp(str, "first-fit") == 0) {
        return block_allocator::allocation_strategy::BA_STRATEGY_FIRST_FIT;
    }
    if (strcmp(str, "best-fit") == 0) {
        return block_allocator::allocation_strategy::BA_STRATEGY_BEST_FIT;
    }
    if (strcmp(str, "heat-zone") == 0) {
        return block_allocator::allocation_strategy::BA_STRATEGY_HEAT_ZONE;
    }
    if (strcmp(str, "padded-fit") != 0) {
        fprintf(stderr, "bad strategy string: %s\n", str);
        abort();
    }
    return block_allocator::allocation_strategy::BA_STRATEGY_PADDED_FIT;
}

static void print_result_verbose(uint64_t allocator_id,
                                 block_allocator::allocation_strategy strategy,
                                 const struct fragmentation_report &report) {
    if (report.end.data_bytes + report.end.unused_bytes +
        report.beginning.data_bytes + report.beginning.unused_bytes
        < 32UL * 1024 * 1024) {
        printf(" ...skipping allocator_id %" PRId64 " (total bytes < 32mb)\n", allocator_id);
        return;
    }

    printf(" allocator_id:   %20" PRId64 "\n", allocator_id);
    printf(" strategy:       %20s\n", strategy_to_cstring(strategy));

    for (int i = 0; i < 2; i++) {
        const TOKU_DB_FRAGMENTATION_S *r = i == 0 ? &report.beginning : &report.end;
        printf("%s\n", i == 0 ? "BEFORE" : "AFTER");

        uint64_t total_bytes = r->data_bytes + r->unused_bytes;
        uint64_t total_blocks = r->data_blocks + r->unused_blocks;

        // byte statistics
        printf(" total bytes:    %20" PRId64 "\n", total_bytes);
        printf(" used bytes:     %20" PRId64 " (%.3lf)\n", r->data_bytes,
               static_cast<double>(r->data_bytes) / total_bytes);
        printf(" unused bytes:   %20" PRId64 " (%.3lf)\n", r->unused_bytes,
               static_cast<double>(r->unused_bytes) / total_bytes);

        // block statistics
        printf(" total blocks:   %20" PRId64 "\n", total_blocks);
        printf(" used blocks:    %20" PRId64 " (%.3lf)\n", r->data_blocks,
               static_cast<double>(r->data_blocks) / total_blocks);
        printf(" unused blocks:  %20" PRId64 " (%.3lf)\n", r->unused_blocks,
               static_cast<double>(r->unused_blocks) / total_blocks);

        // misc
        printf(" largest unused: %20" PRId64 "\n", r->largest_unused_block);
    }
}

static void print_result(uint64_t allocator_id,
                         block_allocator::allocation_strategy strategy,
                         const struct fragmentation_report &report) {
    const TOKU_DB_FRAGMENTATION_S *beginning = &report.beginning;
    const TOKU_DB_FRAGMENTATION_S *end = &report.end;

    uint64_t total_beginning_bytes = beginning->data_bytes + beginning->unused_bytes;
    uint64_t total_end_bytes = end->data_bytes + end->unused_bytes;
    if (total_end_bytes + total_beginning_bytes < 32UL * 1024 * 1024) {
        if (verbose) {
            printf("\n");
            printf(" ...skipping allocator_id %" PRId64 " (total bytes < 32mb)\n", allocator_id);
        }
        return;
    }
    printf("\n");
    if (verbose) {
        print_result_verbose(allocator_id, strategy, report);
    } else {
        printf(" %-15s: allocator %" PRId64 ", %.3lf used bytes (%.3lf before)\n",
               strategy_to_cstring(strategy), allocator_id,
               static_cast<double>(report.end.data_bytes) / total_end_bytes,
               static_cast<double>(report.beginning.data_bytes) / total_beginning_bytes);
    }
}

static int only_aggregate_reports;

static struct option getopt_options[] = {
    { "verbose", no_argument, &verbose, 1 },
    { "only-aggregate-reports", no_argument, &only_aggregate_reports, 1 },
    { "include-strategy", required_argument, nullptr, 'i' },
    { "exclude-strategy", required_argument, nullptr, 'x' },
    { nullptr, 0, nullptr, 0 },
};

int main(int argc, char *argv[]) {
    int opt;
    set<block_allocator::allocation_strategy> candidate_strategies, excluded_strategies;
    while ((opt = getopt_long(argc, argv, "", getopt_options, nullptr)) != -1) {
        switch (opt) {
        case 0:
            break;
        case 'i':
            candidate_strategies.insert(cstring_to_strategy(optarg));
            break;
        case 'x':
            excluded_strategies.insert(cstring_to_strategy(optarg));
            break;
        case '?':
        default:
            abort();
        };
    }
    // Default to everything if nothing was explicitly included.
    if (candidate_strategies.empty()) {
        candidate_strategies.insert(block_allocator::allocation_strategy::BA_STRATEGY_FIRST_FIT);
        candidate_strategies.insert(block_allocator::allocation_strategy::BA_STRATEGY_BEST_FIT);
        candidate_strategies.insert(block_allocator::allocation_strategy::BA_STRATEGY_PADDED_FIT);
        candidate_strategies.insert(block_allocator::allocation_strategy::BA_STRATEGY_HEAT_ZONE);
    }
    // ..but remove anything that was explicitly excluded
    for (set<block_allocator::allocation_strategy>::const_iterator it = excluded_strategies.begin();
         it != excluded_strategies.end(); it++) {
        candidate_strategies.erase(*it);
    }

    // Run the real trace
    //
    // First, read the raw trace from stdin
    vector<string> canonicalized_trace = canonicalize_trace_from(stdin);

    if (!only_aggregate_reports) {
        printf("\n");
        printf("Individual reports, by allocator:\n");
    }

    struct canonical_trace_stats stats;
    map<block_allocator::allocation_strategy, struct fragmentation_report> reports_by_strategy; 
    for (set<block_allocator::allocation_strategy>::const_iterator it = candidate_strategies.begin();
         it != candidate_strategies.end(); it++) {
        const block_allocator::allocation_strategy strategy(*it);

        // replay the canonicalized trace against the current strategy.
        //
        // we provided the allocator map so we can gather statistics later
        struct canonical_trace_stats dummy_stats;
        map<uint64_t, struct fragmentation_report> reports;
        replay_canonicalized_trace(canonicalized_trace, strategy, &reports,
                                   // Only need to gather canonical trace stats once
                                   it == candidate_strategies.begin() ? &stats : &dummy_stats);

        struct fragmentation_report aggregate_report;
        memset(&aggregate_report, 0, sizeof(aggregate_report));
        for (map<uint64_t, struct fragmentation_report>::iterator rp = reports.begin();
             rp != reports.end(); rp++) {
            const struct fragmentation_report &report = rp->second;
            aggregate_report.merge(report);
            if (!only_aggregate_reports) {
                print_result(rp->first, strategy, report);
            }
        }
        reports_by_strategy[strategy] = aggregate_report;
    }

    printf("\n");
    printf("Aggregate reports, by strategy:\n");

    for (map<block_allocator::allocation_strategy, struct fragmentation_report>::iterator it = reports_by_strategy.begin();
         it != reports_by_strategy.end(); it++) {
        print_result(0, it->first, it->second);
    }

    printf("\n");
    printf("Overall trace stats:\n");
    printf("\n");
    printf(" n_lines_played:            %15" PRIu64 "\n", stats.n_lines_replayed);
    printf(" n_create:                  %15" PRIu64 "\n", stats.n_create);
    printf(" n_create_from_blockpairs:  %15" PRIu64 "\n", stats.n_create_from_blockpairs);
    printf(" n_alloc_hot:               %15" PRIu64 "\n", stats.n_alloc_hot);
    printf(" n_alloc_cold:              %15" PRIu64 "\n", stats.n_alloc_cold);
    printf(" n_free:                    %15" PRIu64 "\n", stats.n_free);
    printf(" n_destroy:                 %15" PRIu64 "\n", stats.n_destroy);
    printf("\n");
    printf(" avg_alloc_hot:             %15" PRIu64 "\n", stats.alloc_hot_bytes.mean);
    printf(" stddev_alloc_hot:          %15" PRIu64 "\n", (uint64_t) sqrt(stats.alloc_hot_bytes.variance));
    printf(" avg_alloc_cold:            %15" PRIu64 "\n", stats.alloc_cold_bytes.mean);
    printf(" stddev_alloc_cold:         %15" PRIu64 "\n", (uint64_t) sqrt(stats.alloc_cold_bytes.variance));
    printf("\n");

    return 0;
}
