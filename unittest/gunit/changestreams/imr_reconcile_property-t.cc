/* Copyright (c) 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/// @file
/// Property test for the Reconcile_Operation.
///
/// **Property: Reconcile converges to the requested configuration.**
///
/// mysql::csa::Imr_service::reconcile(Relay_log_info*) reads the channel's
/// selection, CSA state and bounds from an rli and drives the owning runtime
/// entry in Imr_service::m_runtimes (keyed by channel_instance_id), keeping the
/// non-owning pointer cached on the rli (rli->set_imr_ptr()) in sync. An rli is
/// not constructible in a unit harness (it needs a running server -- no gunit
/// test builds one), so the transition LOGIC is exercised through the
/// production decision it is factored around:
/// Imr_runtime::reconcile_action(want, has, matches). reconcile() itself just
/// computes those three inputs from the rli and then, per the returned action,
/// leaves / drops / rebuilds / creates the runtime.
///
/// This test drives that exact tail against a real runtime slot:
/// apply_reconcile() below mirrors reconcile()'s switch (the decision is the
/// production reconcile_action(); the runtime is the real Imr_runtime, so its
/// queue carries the requested bounds; the local std::unique_ptr slot stands in
/// for the m_runtimes entry plus the cached rli pointer), and the properties
/// are asserted over random sequences of configuration changes:
///
///   - the runtime exists exactly when selection is ON and the channel is
///     CSA-enabled (want_runtime),
///   - when it exists its queue bounds and spill directory equal the requested
///     values, and
///   - an unchanged configuration leaves the runtime object identity intact.
///
/// The spill directory is fixed for the server's lifetime, so it is not treated
/// as a changing dimension by reconcile; it is held constant
/// here and covered separately only to confirm it participates in the
/// bounds-match comparison.

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>  // std::bad_alloc
#include <random>
#include <string>

#include "sql/changestreams/apply/storage/in_memory/imr_runtime.h"
#include "sql/changestreams/apply/storage/in_memory/imr_types.h"
#include "sql/changestreams/apply/storage/in_memory/trx_envelope_queue.h"

namespace mysql::csa::unittests {

namespace {
using Action = Imr_runtime::Reconcile_action;

/// A requested per-channel configuration: the reconcile() inputs a channel
/// supplies through its rli.
struct Channel_config {
  bool want{false};      ///< selection ON and CSA-enabled
  std::size_t limit{0};  ///< IN_MEMORY_RELAYLOG_LIMIT
  std::size_t spill{0};  ///< IN_MEMORY_RELAYLOG_SPILL_THRESHOLD
  std::string dir;       ///< relay-log-derived spill directory
};

/// Mirror of Imr_service::reconcile()'s tail: decide via the production
/// reconcile_action(), then leave / destroy / rebuild / create the runtime slot
/// exactly as reconcile() does. The local slot stands in for the m_runtimes
/// entry plus the cached rli pointer (set_imr_ptr()). Only the decision and the
/// runtime construction are production. A rebuild clears the slot before it
/// builds the replacement, as reconcile() does; destroy clears the slot.
/// @p fail_build makes the build throw std::bad_alloc.
void apply_reconcile(std::unique_ptr<Imr_runtime> &slot,
                     const Channel_config &c, bool fail_build = false) {
  const bool has = slot != nullptr;
  const bool matches = has && slot->queue()->memory_limit() == c.limit &&
                       slot->queue()->spill_threshold() == c.spill &&
                       slot->queue()->relay_log_dir() == c.dir;

  Imr_config cfg;
  cfg.memory_limit = c.limit;
  cfg.spill_threshold = c.spill;

  switch (Imr_runtime::reconcile_action(c.want, has, matches)) {
    case Action::kLeave:
      return;
    case Action::kDestroy:
      slot.reset();
      return;
    case Action::kRebuild:
    case Action::kCreate: {
      slot.reset();  // a rebuild drops the old runtime first
      if (fail_build) throw std::bad_alloc();
      slot = std::make_unique<Imr_runtime>(cfg, c.dir,
                                           /*memory_resource=*/nullptr);
      return;
    }
  }
}

constexpr int kIterations = 500;
}  // namespace

// The decision table is total and matches reconcile()'s four transitions for
// every combination of (want_runtime, has_runtime, config_matches).
// config_matches is only consulted when a runtime exists.
TEST(ImrReconcilePropertyTest, DecisionTableIsExhaustiveAndCorrect) {
  // No runtime wanted -> always destroy, regardless of the other inputs.
  EXPECT_EQ(Imr_runtime::reconcile_action(false, false, false),
            Action::kDestroy);
  EXPECT_EQ(Imr_runtime::reconcile_action(false, false, true),
            Action::kDestroy);
  EXPECT_EQ(Imr_runtime::reconcile_action(false, true, false),
            Action::kDestroy);
  EXPECT_EQ(Imr_runtime::reconcile_action(false, true, true), Action::kDestroy);

  // Runtime wanted, none present -> create.
  EXPECT_EQ(Imr_runtime::reconcile_action(true, false, false), Action::kCreate);
  EXPECT_EQ(Imr_runtime::reconcile_action(true, false, true), Action::kCreate);

  // Runtime wanted and present -> leave when matching, rebuild otherwise.
  EXPECT_EQ(Imr_runtime::reconcile_action(true, true, true), Action::kLeave);
  EXPECT_EQ(Imr_runtime::reconcile_action(true, true, false), Action::kRebuild);
}

// A runtime built from a config exposes exactly those bounds and spill
// directory through its queue (the "bounds equal the requested values" half of
// the property, isolated).
TEST(ImrReconcilePropertyTest, CreatePropagatesRequestedBounds) {
  Imr_config cfg;
  cfg.memory_limit = std::size_t{64} << 20;
  cfg.spill_threshold = std::size_t{8} << 20;
  Imr_runtime imr(cfg, "/tmp/relay/", /*memory_resource=*/nullptr);

  EXPECT_EQ(imr.queue()->memory_limit(), cfg.memory_limit);
  EXPECT_EQ(imr.queue()->spill_threshold(), cfg.spill_threshold);
  EXPECT_EQ(imr.queue()->relay_log_dir(), std::string("/tmp/relay/"));
}

// A rebuild drops the old runtime before it builds the replacement, so a
// failed build leaves no runtime, never one holding a freed resource entry. The
// next reconcile with the same configuration then creates a runtime again.
TEST(ImrReconcilePropertyTest, RebuildFailureLeavesNoRuntime) {
  const Channel_config old_cfg{true, std::size_t{128} << 20,
                               std::size_t{16} << 20, "/tmp/relay/"};
  const Channel_config new_cfg{true, std::size_t{64} << 20,
                               std::size_t{8} << 20, "/tmp/relay/"};

  std::unique_ptr<Imr_runtime> slot;
  apply_reconcile(slot, old_cfg);
  ASSERT_NE(slot, nullptr);

  bool threw = false;
  try {
    apply_reconcile(slot, new_cfg, /*fail_build=*/true);  // kRebuild
  } catch (const std::bad_alloc &) {
    threw = true;
  }
  EXPECT_TRUE(threw);
  EXPECT_EQ(slot, nullptr) << "a failed rebuild must leave no runtime";

  // With no runtime left, the same configuration takes kCreate.
  EXPECT_EQ(Imr_runtime::reconcile_action(new_cfg.want,
                                          /*has_runtime=*/false,
                                          /*config_matches=*/false),
            Action::kCreate);
  apply_reconcile(slot, new_cfg);
  ASSERT_NE(slot, nullptr);
  EXPECT_EQ(slot->queue()->memory_limit(), new_cfg.limit);
  EXPECT_EQ(slot->queue()->spill_threshold(), new_cfg.spill);
}

// Reconciling with an unchanged configuration is a no-op: the runtime object
// (and therefore its queue address) is left intact.
TEST(ImrReconcilePropertyTest, UnchangedConfigLeavesRuntimeIdentityIntact) {
  std::unique_ptr<Imr_runtime> slot;
  const Channel_config c{true, std::size_t{128} << 20, std::size_t{16} << 20,
                         "/tmp/relay/"};

  apply_reconcile(slot, c);
  ASSERT_NE(slot, nullptr);
  const Imr_runtime *before = slot.get();
  const Trx_envelope_queue *queue_before = slot->queue();

  apply_reconcile(slot, c);  // identical config
  ASSERT_NE(slot, nullptr);
  EXPECT_EQ(slot.get(), before)
      << "an unchanged reconcile must not replace the "
         "runtime";
  EXPECT_EQ(slot->queue(), queue_before);
}

// Over random sequences of configuration changes, after every reconcile the
// runtime exists exactly when wanted, its bounds equal the requested values,
// and an unchanged step preserves the object identity.
TEST(ImrReconcilePropertyTest, ConvergesOverRandomConfigSequences) {
  std::mt19937 rng(20260916u);
  const std::size_t limits[] = {std::size_t{32} << 20, std::size_t{64} << 20,
                                std::size_t{128} << 20, std::size_t{256} << 20};
  const std::size_t spills[] = {std::size_t{8} << 20, std::size_t{16} << 20};
  const char *dirs[] = {"", "/tmp/a/", "/tmp/b/"};

  std::uniform_int_distribution<int> want_dist(0, 1);
  std::uniform_int_distribution<std::size_t> limit_dist(0, 3);
  std::uniform_int_distribution<std::size_t> spill_dist(0, 1);
  std::uniform_int_distribution<std::size_t> dir_dist(0, 2);

  std::unique_ptr<Imr_runtime> slot;
  bool have_prev = false;
  Channel_config prev;

  for (int i = 0; i < kIterations; ++i) {
    Channel_config c;
    c.want = want_dist(rng) != 0;
    c.limit = limits[limit_dist(rng)];
    c.spill = spills[spill_dist(rng)];
    c.dir = dirs[dir_dist(rng)];

    // Capture identity before, to check the "unchanged config" leg.
    const Imr_runtime *before = slot.get();
    const bool same_as_prev = have_prev && prev.want == c.want &&
                              prev.limit == c.limit && prev.spill == c.spill &&
                              prev.dir == c.dir;

    apply_reconcile(slot, c);

    // Existence tracks want exactly.
    EXPECT_EQ(slot != nullptr, c.want) << "iteration " << i;

    if (slot != nullptr) {
      // Bounds and spill dir equal the request.
      EXPECT_EQ(slot->queue()->memory_limit(), c.limit) << "iteration " << i;
      EXPECT_EQ(slot->queue()->spill_threshold(), c.spill) << "iteration " << i;
      EXPECT_EQ(slot->queue()->relay_log_dir(), c.dir) << "iteration " << i;

      // An unchanged configuration on an existing runtime is left untouched:
      // identity preserved.
      if (same_as_prev && before != nullptr) {
        EXPECT_EQ(slot.get(), before)
            << "unchanged config must not rebuild, iteration " << i;
      }
    }

    prev = c;
    have_prev = true;
  }
}

}  // namespace mysql::csa::unittests
