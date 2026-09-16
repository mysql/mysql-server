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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "mysql/components/library_mysys/system_info.h"
#ifdef __APPLE__
#include <libproc.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <unistd.h>

#include "components/library_mysys/my_system_api/system_info_macos.h"
#endif
#ifdef __linux__
#include <unistd.h>
#endif
#if defined(__linux__) || defined(__APPLE__)
#include "components/library_mysys/my_system_api/system_info_linux_procfs.h"
#include "components/library_mysys/my_system_api/system_info_platform.h"
#endif

namespace mysql::system_info {
namespace {

#ifdef __APPLE__
[[nodiscard]] std::optional<mach_msg_type_number_t>
current_mach_port_name_count() {
  mach_port_name_array_t names{nullptr};
  mach_msg_type_number_t name_count{0};
  mach_port_type_array_t types{nullptr};
  mach_msg_type_number_t type_count{0};
  if (mach_port_names(mach_task_self(), &names, &name_count, &types,
                      &type_count) != KERN_SUCCESS) {
    return std::nullopt;
  }

  kern_return_t name_cleanup = KERN_SUCCESS;
  if (names != nullptr) {
    name_cleanup =
        vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(names),
                      static_cast<vm_size_t>(name_count) * sizeof(*names));
  }
  kern_return_t type_cleanup = KERN_SUCCESS;
  if (types != nullptr) {
    type_cleanup =
        vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(types),
                      static_cast<vm_size_t>(type_count) * sizeof(*types));
  }
  if (name_cleanup != KERN_SUCCESS || type_cleanup != KERN_SUCCESS ||
      name_count != type_count) {
    return std::nullopt;
  }
  return name_count;
}

struct Fake_macos_thread_deallocator {
  std::vector<mach_port_t> ports;
  size_t memory_calls{0};
  vm_address_t memory_address{0};
  vm_size_t memory_size{0};
  kern_return_t port_result{KERN_SUCCESS};
  kern_return_t memory_result{KERN_SUCCESS};
};

kern_return_t record_macos_port_deallocation(void *context, mach_port_t port) {
  auto &fake = *static_cast<Fake_macos_thread_deallocator *>(context);
  fake.ports.push_back(port);
  return fake.port_result;
}

kern_return_t record_macos_memory_deallocation(void *context,
                                               vm_address_t address,
                                               vm_size_t size) {
  auto &fake = *static_cast<Fake_macos_thread_deallocator *>(context);
  ++fake.memory_calls;
  fake.memory_address = address;
  fake.memory_size = size;
  return fake.memory_result;
}

[[nodiscard]] internal::macOS_thread_deallocator fake_macos_deallocator(
    Fake_macos_thread_deallocator &fake) {
  return {&fake, record_macos_port_deallocation,
          record_macos_memory_deallocation};
}
#endif

static_assert(std::is_same_v<
              decltype(std::declval<System_result<std::string> &&>().value()),
              std::string>);
static_assert(
    std::is_same_v<
        decltype(std::declval<const System_result<std::string> &&>().value()),
        std::string>);
static_assert(
    std::is_same_v<decltype(std::declval<const System_result<int> &>().error()),
                   const std::error_code &>);
static_assert(
    std::is_same_v<decltype(std::declval<System_result<int> &&>().error()),
                   std::error_code>);
static_assert(std::is_same_v<
              decltype(std::declval<const System_result<int> &&>().error()),
              std::error_code>);

struct Throw_on_copy_construction {
  Throw_on_copy_construction() = default;
  Throw_on_copy_construction(const Throw_on_copy_construction &) {
    throw std::runtime_error("copy construction failed");
  }
  Throw_on_copy_construction(Throw_on_copy_construction &&) = default;
  Throw_on_copy_construction &operator=(const Throw_on_copy_construction &) =
      default;
  Throw_on_copy_construction &operator=(Throw_on_copy_construction &&) =
      default;
};

struct Throw_on_copy_assignment {
  Throw_on_copy_assignment() = default;
  Throw_on_copy_assignment(const Throw_on_copy_assignment &) = default;
  Throw_on_copy_assignment(Throw_on_copy_assignment &&) = default;
  Throw_on_copy_assignment &operator=(const Throw_on_copy_assignment &) {
    throw std::runtime_error("copy assignment failed");
  }
  Throw_on_copy_assignment &operator=(Throw_on_copy_assignment &&) = default;
};

#if defined(__linux__) || defined(__APPLE__)
[[nodiscard]] std::string linux_thread_stat_fixture(
    char state = 'S', std::string_view name = "worker",
    const std::vector<std::pair<size_t, std::string>> &fields = {}) {
  std::vector<std::string> values;
  for (size_t value = 1; value <= 41; ++value) {
    values.push_back(std::to_string(value));
  }
  for (const auto &[field, value] : fields) values.at(field - 4) = value;

  std::string result = "10 (" + std::string{name} + ") " + state;
  for (const auto &value : values) result += " " + value;
  return result + " ignored\n";
}

class Linux_storage_fixture {
 public:
  Linux_storage_fixture()
      : m_root(std::filesystem::temp_directory_path() /
               ("mysys-storage-" +
                std::to_string(reinterpret_cast<uintptr_t>(this)))) {
    std::error_code error;
    std::filesystem::remove_all(m_root, error);
    std::filesystem::create_directories(m_root / "proc");
    std::filesystem::create_directories(m_root / "sys/class/block");
    std::filesystem::create_directories(m_root / "sys/devices/block");
  }

  ~Linux_storage_fixture() {
    std::error_code error;
    std::filesystem::remove_all(m_root, error);
  }

  Linux_storage_fixture(const Linux_storage_fixture &) = delete;
  Linux_storage_fixture &operator=(const Linux_storage_fixture &) = delete;

  void add_disk(std::string_view name, std::string_view size = "8",
                std::string_view sector_size = "4096") {
    const auto device = m_root / "sys/devices/block" / name;
    std::filesystem::create_directories(device / "queue");
    write(device / "size", size);
    write(device / "queue/hw_sector_size", sector_size);
    std::filesystem::create_directory_symlink(
        device, m_root / "sys/class/block" / name);
  }

  void add_partition(std::string_view parent, std::string_view name,
                     std::string_view size = "4") {
    const auto device = m_root / "sys/devices/block" / parent / name;
    std::filesystem::create_directories(device);
    write(device / "partition", "1");
    write(device / "size", size);
    std::filesystem::create_directory_symlink(
        device, m_root / "sys/class/block" / name);
  }

  void write_partitions(std::string_view content) {
    write(m_root / "proc/partitions", content);
  }

  void write_diskstats(std::string_view content) {
    write(m_root / "proc/diskstats", content);
  }

  void write_size(std::string_view name, std::string_view content) {
    write(m_root / "sys/devices/block" / name / "size", content);
  }

  [[nodiscard]] internal::Linux_storage_paths paths() const {
    return {(m_root / "proc/partitions").string(),
            (m_root / "proc/diskstats").string(),
            (m_root / "sys/class/block").string()};
  }

  [[nodiscard]] std::filesystem::path class_device(
      std::string_view name) const {
    return m_root / "sys/class/block" / name;
  }

 private:
  static void write(const std::filesystem::path &path,
                    std::string_view content) {
    std::ofstream output{path};
    output << content;
  }

  std::filesystem::path m_root;
};
#endif

TEST(MysysSystemInfo, AvailableCapabilityContainsValue) {
  auto result = System_result<std::string>::success("available");

  EXPECT_EQ(Result_state::available, result.state());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("available", result.value());

  result.value() = "updated";
  EXPECT_EQ("updated", result.value());
}

TEST(MysysSystemInfo, AvailableCapabilitySupportsMoveOnlyValue) {
  auto result =
      System_result<std::unique_ptr<int>>::success(std::make_unique<int>(42));

  ASSERT_TRUE(result.has_value());
  auto value = std::move(result).value();
  ASSERT_NE(nullptr, value);
  EXPECT_EQ(42, *value);
}

TEST(MysysSystemInfo, ZeroIsAnAvailableValue) {
  auto result = System_result<uint64_t>::success(0);

  EXPECT_EQ(Result_state::available, result.state());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(0, result.value());
}

TEST(MysysSystemInfo, UnavailableCapabilityHasNoValue) {
  auto result = System_result<int>::unavailable();

  EXPECT_EQ(Result_state::unavailable, result.state());
  EXPECT_FALSE(result.has_value());
}

TEST(MysysSystemInfo, FailedCapabilityPreservesError) {
  const auto error = std::make_error_code(std::errc::invalid_argument);
  auto result = System_result<int>::failure(error);

  EXPECT_EQ(Result_state::failed, result.state());
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(error, result.error());
}

TEST(MysysSystemInfo, SnapshotCapabilitiesDefaultToUnavailable) {
  const Filesystem_snapshot filesystem;
  EXPECT_EQ(Result_state::unavailable, filesystem.identity.state());
  EXPECT_EQ(Result_state::unavailable, filesystem.usage.state());

  const Host_memory_snapshot memory;
  EXPECT_EQ(Result_state::unavailable, memory.memory.state());
  EXPECT_EQ(Result_state::unavailable, memory.breakdown.state());
  EXPECT_EQ(Result_state::unavailable, memory.swap_capacity.state());
  EXPECT_EQ(Result_state::unavailable, memory.swap_activity.state());
}

TEST(MysysSystemInfo, ThrowingCopyKeepsUnavailableDestinationConsistent) {
  const auto source = System_result<Throw_on_copy_construction>::success(
      Throw_on_copy_construction{});
  System_result<Throw_on_copy_construction> destination;

  EXPECT_THROW(destination = source, std::runtime_error);
  EXPECT_EQ(Result_state::unavailable, destination.state());
  EXPECT_FALSE(destination.has_value());
}

TEST(MysysSystemInfo, ThrowingAssignmentKeepsAvailableDestinationEngaged) {
  const auto source = System_result<Throw_on_copy_assignment>::success(
      Throw_on_copy_assignment{});
  auto destination = System_result<Throw_on_copy_assignment>::success(
      Throw_on_copy_assignment{});

  EXPECT_THROW(destination = source, std::runtime_error);
  EXPECT_EQ(Result_state::available, destination.state());
  EXPECT_TRUE(destination.has_value());
}

#if defined(__linux__) || defined(__APPLE__)
TEST(MysysSystemInfo, CpuTickConversionUsesBooleanErrorConvention) {
  std::chrono::milliseconds value{-1};

  EXPECT_FALSE(internal::cpu_ticks_to_milliseconds(0, 100, value));
  EXPECT_EQ(std::chrono::milliseconds{0}, value);
  EXPECT_FALSE(internal::cpu_ticks_to_milliseconds(129, 128, value));
  EXPECT_EQ(std::chrono::milliseconds{1007}, value);

  value = std::chrono::milliseconds{-1};
  EXPECT_TRUE(internal::cpu_ticks_to_milliseconds(1, 0, value));
  EXPECT_EQ(std::chrono::milliseconds{-1}, value);
  EXPECT_TRUE(internal::cpu_ticks_to_milliseconds(
      std::numeric_limits<uint64_t>::max(), 1, value));
  EXPECT_EQ(std::chrono::milliseconds{-1}, value);
}

TEST(MysysSystemInfo, FilesystemQueryReturnsCompleteSnapshot) {
  const auto first = query_filesystem(".");
  const auto second = query_filesystem(".");

  ASSERT_EQ(Result_state::available, first.identity.state());
  ASSERT_EQ(Result_state::available, first.usage.state());
  EXPECT_FALSE(first.identity.value().filesystem_id.empty());
  EXPECT_FALSE(first.identity.value().mount_point.empty());
  EXPECT_FALSE(first.identity.value().filesystem_type.empty());
  EXPECT_GT(first.usage.value().capacity_bytes, 0);
  EXPECT_LE(first.usage.value().available_bytes,
            first.usage.value().capacity_bytes);
  ASSERT_EQ(Result_state::available, second.identity.state());
  EXPECT_EQ(first.identity.value().filesystem_id,
            second.identity.value().filesystem_id);
}

TEST(MysysSystemInfo, FilesystemQueryReportsFailedPath) {
  const auto result = query_filesystem("");

  ASSERT_EQ(Result_state::failed, result.identity.state());
  ASSERT_EQ(Result_state::failed, result.usage.state());
  EXPECT_TRUE(result.identity.error());
  EXPECT_TRUE(result.usage.error());
}

TEST(MysysSystemInfo, FilesystemValidationRejectsInvalidUsage) {
  Filesystem_snapshot zero_capacity;
  zero_capacity.usage = System_result<Filesystem_usage>::success({0, 0});
  const auto zero_result =
      internal::validate_filesystem_snapshot(std::move(zero_capacity));

  EXPECT_EQ(Result_state::failed, zero_result.usage.state());

  Filesystem_snapshot excess_available;
  excess_available.usage = System_result<Filesystem_usage>::success({100, 101});
  const auto excess_result =
      internal::validate_filesystem_snapshot(std::move(excess_available));

  EXPECT_EQ(Result_state::failed, excess_result.usage.state());
}

TEST(MysysSystemInfo, LinuxPartitionsParsesAndSortsCompleteFixture) {
  std::istringstream input{
      "major minor  #blocks  name\n\n"
      "   8       1       1024 sda1  \n"
      "   8       0       2048 sda\n"};

  const auto result = internal::read_linux_partitions(input);

  ASSERT_EQ(Result_state::available, result.state());
  ASSERT_EQ(2U, result.value().size());
  EXPECT_EQ("sda", result.value()[0].name);
  EXPECT_EQ(8U, result.value()[0].major);
  EXPECT_EQ(0U, result.value()[0].minor);
  EXPECT_EQ("sda1", result.value()[1].name);
}

TEST(MysysSystemInfo, LinuxPartitionsRejectsMalformedAndDuplicateRows) {
  for (const std::string_view content : {
           "8 0 1\n",
           "-1 0 1 sda\n",
           "8 0 18446744073709551616 sda\n",
           "8 0 1 sda extra\n",
           "8 0 1 ../sda\n",
           "8 0 1 sda\n8 1 1 sda\n",
           "8 0 1 sda\n8 0 1 other\n",
       }) {
    std::istringstream input{std::string{content}};
    EXPECT_EQ(Result_state::failed,
              internal::read_linux_partitions(input).state());
  }
}

TEST(MysysSystemInfo, LinuxStorageDeviceNamesAllowKernelPunctuation) {
  for (const std::string_view name : {"dm-0", "md_0", "disk.0", "cciss!0"}) {
    EXPECT_TRUE(internal::valid_linux_storage_device_name(name));
  }
  for (const std::string_view name : {"", ".", "..", "a/b"}) {
    EXPECT_FALSE(internal::valid_linux_storage_device_name(name));
  }
  const std::string embedded_nul{"sda\0evil", 8};
  EXPECT_FALSE(internal::valid_linux_storage_device_name(embedded_nul));
}

TEST(MysysSystemInfo, LinuxStorageCapacityUsesFixed512ByteUnits) {
  std::istringstream size{"8\n"};
  std::istringstream sector_size{"4096\n"};

  const auto result =
      internal::read_linux_storage_capacity(size, sector_size, "sda");

  ASSERT_EQ(Result_state::available, result.state());
  EXPECT_EQ("sda", result.value().device);
  EXPECT_EQ(4096U, result.value().capacity_bytes);
  EXPECT_EQ(4096U, result.value().sector_size_bytes);
}

TEST(MysysSystemInfo, LinuxStorageCapacityValidatesInputs) {
  for (const auto &[size_text, sector_text] :
       std::vector<std::pair<std::string, std::string>>{
           {"1 trailing\n", "512\n"},
           {"1\n", "0\n"},
           {"18446744073709551615\n", "512\n"},
           {"bad\n", "512\n"},
       }) {
    std::istringstream size{size_text};
    std::istringstream sector_size{sector_text};
    EXPECT_EQ(Result_state::failed,
              internal::read_linux_storage_capacity(size, sector_size, "sda")
                  .state());
  }
  std::istringstream zero_size{"0\n"};
  std::istringstream valid_sector_size{"512\n"};
  EXPECT_EQ(Result_state::available, internal::read_linux_storage_capacity(
                                         zero_size, valid_sector_size, "sda")
                                         .state());
}

TEST(MysysSystemInfo, LinuxPartitionRelationshipUsesCanonicalParent) {
  const auto result = internal::linux_partition_relationship(
      "/sys/devices/pci/block/sda/sda1", "sda1");

  ASSERT_EQ(Result_state::available, result.state());
  EXPECT_EQ("sda1", result.value().device);
  EXPECT_EQ("sda", result.value().parent_device);
  EXPECT_EQ(Result_state::failed,
            internal::linux_partition_relationship("/block/sda1/sda1", "sda1")
                .state());
}

TEST(MysysSystemInfo, LinuxPartitionSectorSizeUsesParentQueue) {
  const auto partition = internal::linux_storage_sector_size_path(
      "/sys/devices/pci/block/sda/sda1", true);
  const auto disk = internal::linux_storage_sector_size_path(
      "/sys/devices/pci/block/sda", false);

  ASSERT_EQ(Result_state::available, partition.state());
  EXPECT_EQ("/sys/devices/pci/block/sda/queue/hw_sector_size",
            partition.value());
  ASSERT_EQ(Result_state::available, disk.state());
  EXPECT_EQ("/sys/devices/pci/block/sda/queue/hw_sector_size", disk.value());
}

TEST(MysysSystemInfo, LinuxDiskstatsMapsCountersAndExcludesPartitionFlush) {
  std::istringstream input{
      "8 1 sda1 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17\n"
      "8 0 sda 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18\n"};
  const std::unordered_set<std::string> whole_devices{"sda"};

  const auto result = internal::read_linux_diskstats(input, whole_devices);

  ASSERT_EQ(Result_state::available, result.read_write.state());
  ASSERT_EQ(2U, result.read_write.value().devices.size());
  const auto &disk = result.read_write.value().devices[0];
  EXPECT_EQ("sda", disk.device);
  EXPECT_EQ(2U, disk.read_count);
  EXPECT_EQ(4U * 512U, disk.read_bytes);
  EXPECT_EQ(std::chrono::milliseconds{5}, disk.read_time);
  EXPECT_EQ(6U, disk.write_count);
  EXPECT_EQ(8U * 512U, disk.write_bytes);
  EXPECT_EQ(std::chrono::milliseconds{9}, disk.write_time);
  ASSERT_EQ(Result_state::available, result.flush.state());
  ASSERT_EQ(1U, result.flush.value().devices.size());
  EXPECT_EQ("sda", result.flush.value().devices[0].device);
  EXPECT_EQ(17U, result.flush.value().devices[0].flush_count);
  EXPECT_EQ(std::chrono::milliseconds{18},
            result.flush.value().devices[0].flush_time);
}

TEST(MysysSystemInfo, LinuxDiskstatsSupportsOlderRowsAndZeroCounters) {
  std::istringstream input{"8 0 sda 0 0 0 0 0 0 0 0 0 0 0 1 2 3 4\n"};

  const auto result = internal::read_linux_diskstats(input, {"sda"});

  ASSERT_EQ(Result_state::available, result.read_write.state());
  EXPECT_EQ(0U, result.read_write.value().devices[0].read_bytes);
  EXPECT_EQ(0U, result.read_write.value().devices[0].write_bytes);
  EXPECT_EQ(Result_state::unavailable, result.flush.state());
}

TEST(MysysSystemInfo, LinuxDiskstatsAcceptsForwardCompatibleFields) {
  std::istringstream input{
      "8 0 sda 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 extra fields\n"};

  const auto result = internal::read_linux_diskstats(input, {"sda"});

  EXPECT_EQ(Result_state::available, result.read_write.state());
  EXPECT_EQ(Result_state::available, result.flush.state());
}

TEST(MysysSystemInfo, LinuxDiskstatsCapabilityFailuresAreIndependent) {
  std::istringstream bad_read_write{
      "8 0 sda 1 2 bad 4 5 6 7 8 9 10 11 12 13 14 15 16 17\n"};
  const auto read_write_result =
      internal::read_linux_diskstats(bad_read_write, {"sda"});
  EXPECT_EQ(Result_state::failed, read_write_result.read_write.state());
  EXPECT_EQ(Result_state::available, read_write_result.flush.state());

  std::istringstream bad_flush{
      "8 0 sda 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 bad 17\n"};
  const auto flush_result = internal::read_linux_diskstats(bad_flush, {"sda"});
  EXPECT_EQ(Result_state::available, flush_result.read_write.state());
  EXPECT_EQ(Result_state::failed, flush_result.flush.state());

  std::istringstream bad_identity{
      "8 0 ../sda 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17\n"};
  const auto identity_result = internal::read_linux_diskstats(bad_identity, {});
  EXPECT_EQ(Result_state::failed, identity_result.read_write.state());
  EXPECT_EQ(Result_state::failed, identity_result.flush.state());
}

TEST(MysysSystemInfo, LinuxDiskstatsRejectsDuplicatesAndOverflow) {
  for (const std::string_view content : {
           "8 0 sda 1 2 3 4 5 6 7 8 9 10 11\n"
           "8 1 sda 1 2 3 4 5 6 7 8 9 10 11\n",
           "8 0 sda 1 2 3 4 5 6 7 8 9 10 11\n"
           "8 0 other 1 2 3 4 5 6 7 8 9 10 11\n",
           "8 0 sda 1 2 18446744073709551615 4 5 6 7 8 9 10 11\n",
           "8 0 sda 1 2 3 18446744073709551615 5 6 7 8 9 10 11\n",
       }) {
    std::istringstream input{std::string{content}};
    EXPECT_EQ(
        Result_state::failed,
        internal::read_linux_diskstats(input, {"sda"}).read_write.state());
  }
}

TEST(MysysSystemInfo, LinuxStorageParsersDoNotRetainState) {
  std::istringstream complete_partitions{"8 0 1 sda\n"};
  EXPECT_EQ(Result_state::available,
            internal::read_linux_partitions(complete_partitions).state());
  std::istringstream empty_partitions;
  EXPECT_EQ(Result_state::failed,
            internal::read_linux_partitions(empty_partitions).state());

  std::istringstream complete_diskstats{"8 0 sda 1 2 3 4 5 6 7 8 9 10 11\n"};
  EXPECT_EQ(Result_state::available,
            internal::read_linux_diskstats(complete_diskstats, {"sda"})
                .read_write.state());
  std::istringstream empty_diskstats;
  EXPECT_EQ(Result_state::failed,
            internal::read_linux_diskstats(empty_diskstats, {"sda"})
                .read_write.state());
}

TEST(MysysSystemInfo, LinuxStorageAssemblyOmitsEnoentHotplugDevice) {
  Linux_storage_fixture fixture;
  fixture.add_disk("sda");
  fixture.add_disk("sdb");
  fixture.write_partitions("8 0 8 sda\n8 16 8 sdb\n");
  fixture.write_diskstats(
      "8 0 sda 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17\n"
      "8 16 sdb 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17\n");

  const auto result = internal::query_linux_storage_devices(
      fixture.paths(),
      [&] { std::filesystem::remove(fixture.class_device("sdb")); });

  ASSERT_EQ(Result_state::available, result.inventory.state());
  ASSERT_EQ(1U, result.inventory.value().devices.size());
  EXPECT_EQ("sda", result.inventory.value().devices[0].name);
  ASSERT_EQ(Result_state::available, result.capacities.state());
  EXPECT_EQ(1U, result.capacities.value().devices.size());
  EXPECT_EQ(Result_state::available, result.hierarchy.state());
  ASSERT_EQ(Result_state::available, result.read_write.state());
  EXPECT_EQ(2U, result.read_write.value().devices.size());
}

TEST(MysysSystemInfo, LinuxStorageAssemblyFailsOnNonEnoentSysfsError) {
  Linux_storage_fixture fixture;
  fixture.add_disk("sda");
  fixture.add_disk("sdb");
  fixture.write_partitions("8 0 8 sda\n8 16 8 sdb\n");
  fixture.write_diskstats(
      "8 0 sda 1 2 3 4 5 6 7 8 9 10 11\n"
      "8 16 sdb 1 2 3 4 5 6 7 8 9 10 11\n");

  const auto result =
      internal::query_linux_storage_devices(fixture.paths(), [&] {
        const auto device = fixture.class_device("sdb");
        std::filesystem::remove(device);
        std::filesystem::create_symlink(device, device);
      });

  ASSERT_EQ(Result_state::failed, result.inventory.state());
  EXPECT_NE(std::errc::no_such_file_or_directory, result.inventory.error());
  EXPECT_EQ(Result_state::failed, result.capacities.state());
  EXPECT_EQ(Result_state::failed, result.hierarchy.state());
  EXPECT_EQ(Result_state::available, result.read_write.state());
}

TEST(MysysSystemInfo, LinuxStorageAssemblyRejectsUnknownPartitionParent) {
  Linux_storage_fixture fixture;
  fixture.add_disk("sda");
  fixture.add_partition("sda", "sda1");
  fixture.write_partitions("8 1 4 sda1\n");
  fixture.write_diskstats("8 1 sda1 1 2 3 4 5 6 7 8 9 10 11\n");

  const auto result = internal::query_linux_storage_devices(fixture.paths());

  ASSERT_EQ(Result_state::available, result.inventory.state());
  ASSERT_EQ(1U, result.inventory.value().devices.size());
  EXPECT_EQ(Storage_device_kind::partition,
            result.inventory.value().devices[0].kind);
  EXPECT_EQ(Result_state::available, result.capacities.state());
  EXPECT_EQ(Result_state::failed, result.hierarchy.state());
  EXPECT_EQ(Result_state::available, result.read_write.state());
}

TEST(MysysSystemInfo, LinuxStorageAssemblyIsolatesCapabilityFailures) {
  Linux_storage_fixture fixture;
  fixture.add_disk("sda", "bad");
  fixture.write_partitions("8 0 8 sda\n");
  fixture.write_diskstats("8 0 sda 1 2 3 4 5 6 7 8 9 10 11\n");

  const auto capacity_failure =
      internal::query_linux_storage_devices(fixture.paths());
  EXPECT_EQ(Result_state::available, capacity_failure.inventory.state());
  EXPECT_EQ(Result_state::failed, capacity_failure.capacities.state());
  EXPECT_EQ(Result_state::available, capacity_failure.hierarchy.state());
  EXPECT_EQ(Result_state::available, capacity_failure.read_write.state());

  fixture.write_partitions("malformed\n");
  const auto inventory_failure =
      internal::query_linux_storage_devices(fixture.paths());
  EXPECT_EQ(Result_state::failed, inventory_failure.inventory.state());
  EXPECT_EQ(Result_state::failed, inventory_failure.capacities.state());
  EXPECT_EQ(Result_state::failed, inventory_failure.hierarchy.state());
  EXPECT_EQ(Result_state::available, inventory_failure.read_write.state());

  fixture.write_partitions("8 0 8 sda\n");
  fixture.write_size("sda", "8");
  fixture.write_diskstats("malformed\n");
  const auto statistics_failure =
      internal::query_linux_storage_devices(fixture.paths());
  EXPECT_EQ(Result_state::available, statistics_failure.inventory.state());
  EXPECT_EQ(Result_state::available, statistics_failure.capacities.state());
  EXPECT_EQ(Result_state::available, statistics_failure.hierarchy.state());
  EXPECT_EQ(Result_state::failed, statistics_failure.read_write.state());
}

TEST(MysysSystemInfo, StorageValidationRejectsOnlyInvalidCapabilities) {
  Storage_snapshot snapshot;
  snapshot.inventory = System_result<Storage_inventory>::success(
      {{{"sda", Storage_device_kind::disk},
        {"sda1", Storage_device_kind::partition}}});
  snapshot.capacities = System_result<Storage_capacities>::success(
      {{{"sda1", 10, 512}, {"sda", 20, 512}}});
  snapshot.hierarchy =
      System_result<Storage_hierarchy>::success({{{"sda1", "missing"}}});
  snapshot.read_write = System_result<Storage_read_write_snapshot>::success(
      {{{"sda", 0, 0, {}, 0, 0, {}}}});

  const auto result = internal::validate_storage_snapshot(std::move(snapshot));

  EXPECT_EQ(Result_state::available, result.inventory.state());
  EXPECT_EQ(Result_state::failed, result.capacities.state());
  EXPECT_EQ(Result_state::failed, result.hierarchy.state());
  EXPECT_EQ(Result_state::available, result.read_write.state());
  EXPECT_EQ(Result_state::unavailable, result.flush.state());
}

TEST(MysysSystemInfo, StorageValidationAllowsEmptyHierarchy) {
  Storage_snapshot snapshot;
  snapshot.inventory = System_result<Storage_inventory>::success(
      {{{"sda", Storage_device_kind::disk}}});
  snapshot.hierarchy = System_result<Storage_hierarchy>::success({});

  const auto result = internal::validate_storage_snapshot(std::move(snapshot));

  EXPECT_EQ(Result_state::available, result.inventory.state());
  EXPECT_EQ(Result_state::available, result.hierarchy.state());
}

TEST(MysysSystemInfo, StorageValidationAcceptsEveryKnownDeviceKind) {
  for (const auto kind :
       {Storage_device_kind::disk, Storage_device_kind::partition,
        Storage_device_kind::volume, Storage_device_kind::virtual_device}) {
    Storage_snapshot snapshot;
    snapshot.inventory =
        System_result<Storage_inventory>::success({{{"device", kind}}});

    const auto result =
        internal::validate_storage_snapshot(std::move(snapshot));

    EXPECT_EQ(Result_state::available, result.inventory.state());
  }
}

TEST(MysysSystemInfo, StorageValidationRejectsUnknownDeviceKind) {
  Storage_snapshot snapshot;
  snapshot.inventory = System_result<Storage_inventory>::success(
      {{{"device", Storage_device_kind::unknown}}});

  const auto result = internal::validate_storage_snapshot(std::move(snapshot));

  EXPECT_EQ(Result_state::failed, result.inventory.state());
}

TEST(MysysSystemInfo, StorageValidationRequiresAvailableInventoryForHierarchy) {
  Storage_snapshot unavailable;
  unavailable.hierarchy = System_result<Storage_hierarchy>::success({});
  const auto unavailable_result =
      internal::validate_storage_snapshot(std::move(unavailable));
  EXPECT_EQ(Result_state::unavailable, unavailable_result.inventory.state());
  EXPECT_EQ(Result_state::failed, unavailable_result.hierarchy.state());

  Storage_snapshot failed;
  failed.inventory = System_result<Storage_inventory>::failure(
      std::make_error_code(std::errc::io_error));
  failed.hierarchy = System_result<Storage_hierarchy>::success({});
  const auto failed_result =
      internal::validate_storage_snapshot(std::move(failed));
  EXPECT_EQ(Result_state::failed, failed_result.inventory.state());
  EXPECT_EQ(Result_state::failed, failed_result.hierarchy.state());
}

TEST(MysysSystemInfo, StorageValidationRejectsEmptyDeviceCapabilities) {
  Storage_snapshot snapshot;
  snapshot.inventory = System_result<Storage_inventory>::success({});
  snapshot.capacities = System_result<Storage_capacities>::success({});
  snapshot.read_write = System_result<Storage_read_write_snapshot>::success({});
  snapshot.flush = System_result<Storage_flush_snapshot>::success({});

  const auto result = internal::validate_storage_snapshot(std::move(snapshot));

  EXPECT_EQ(Result_state::failed, result.inventory.state());
  EXPECT_EQ(Result_state::failed, result.capacities.state());
  EXPECT_EQ(Result_state::failed, result.read_write.state());
  EXPECT_EQ(Result_state::failed, result.flush.state());
}

TEST(MysysSystemInfo, LinuxMountInfoSelectsMostSpecificEscapedMount) {
  std::istringstream input{
      "20 1 8:1 / /mnt rw shared:1 - ext4 /dev/root rw\n"
      "21 20 8:1 /data /mnt/mysql\\040data rw shared:2 master:1 - xfs "
      "/dev/mapper/mysql\\040data rw\n"};

  const auto result =
      internal::read_linux_mount_info(input, "8:1", "/mnt/mysql data/database");

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ("21", result.value().filesystem_id);
  EXPECT_EQ("/mnt/mysql data", result.value().mount_point);
  EXPECT_EQ("/dev/mapper/mysql data", result.value().mount_source);
  EXPECT_EQ("xfs", result.value().filesystem_type);
}

TEST(MysysSystemInfo, LinuxMountInfoRejectsMalformedRecord) {
  std::istringstream input{"bad mount information\n"};

  const auto result = internal::read_linux_mount_info(input, "8:1", "/mnt");

  ASSERT_EQ(Result_state::failed, result.state());
  EXPECT_EQ(std::errc::invalid_argument, result.error());
}

TEST(MysysSystemInfo, LinuxMountInfoRejectsMalformedEscape) {
  std::istringstream input{"20 1 8:1 / /mnt\\04xdata rw - ext4 /dev/root rw\n"};

  const auto result =
      internal::read_linux_mount_info(input, "8:1", "/mnt data/database");

  ASSERT_EQ(Result_state::failed, result.state());
  EXPECT_EQ(std::errc::invalid_argument, result.error());
}

TEST(MysysSystemInfo, LinuxMountInfoRejectsMissingSeparator) {
  std::istringstream input{"20 1 8:1 / /mnt rw shared:1 ext4 /dev/root rw\n"};

  const auto result = internal::read_linux_mount_info(input, "8:1", "/mnt");

  ASSERT_EQ(Result_state::failed, result.state());
  EXPECT_EQ(std::errc::invalid_argument, result.error());
}

TEST(MysysSystemInfo, LinuxMountInfoDistinguishesNonmatchingEntries) {
  std::istringstream input{
      "20 1 8:2 / /mnt rw - ext4 /dev/other rw\n"
      "21 1 8:1 / /mnt/mysql rw - ext4 /dev/root rw\n"};

  const auto result =
      internal::read_linux_mount_info(input, "8:1", "/mnt/mysql-old");

  ASSERT_EQ(Result_state::failed, result.state());
  EXPECT_EQ(std::errc::no_such_device, result.error());
}

TEST(MysysSystemInfo, LinuxMeminfoParsesCapabilitiesInAnyOrder) {
  std::istringstream input{
      "Ignored: 99 kB\nSwapFree: 4 kB\nSlab: 5 kB\n"
      "MemAvailable: 70 kB\nCached: 6 kB\nMemTotal: 100 kB\n"
      "SwapTotal: 8 kB\nBuffers: 7 kB\nMemFree: 20 kB\n"};

  const auto result = internal::read_linux_meminfo(input);

  ASSERT_TRUE(result.memory.has_value());
  EXPECT_EQ(100U * 1024U, result.memory.value().total_bytes);
  EXPECT_EQ(20U * 1024U, result.memory.value().free_bytes);
  EXPECT_EQ(70U * 1024U, result.memory.value().available_bytes);
  ASSERT_TRUE(result.breakdown.has_value());
  EXPECT_EQ(7U * 1024U, result.breakdown.value().buffer_bytes);
  EXPECT_EQ(6U * 1024U, result.breakdown.value().cache_bytes);
  EXPECT_EQ(5U * 1024U, result.breakdown.value().slab_bytes);
  ASSERT_TRUE(result.swap_capacity.has_value());
  EXPECT_EQ(8U * 1024U, result.swap_capacity.value().total_bytes);
  EXPECT_EQ(4U * 1024U, result.swap_capacity.value().free_bytes);
}

TEST(MysysSystemInfo, LinuxMeminfoFailureIsLimitedToAffectedCapability) {
  std::istringstream input{
      "MemTotal: 100 kB\nMemFree: 20 kB\nMemAvailable: 70 kB\n"
      "Buffers: 7 kB\nCached: bad kB\nSlab: 5 kB\n"
      "SwapTotal: 0 kB\nSwapFree: 0 kB\n"};

  const auto result = internal::read_linux_meminfo(input);

  EXPECT_EQ(Result_state::available, result.memory.state());
  EXPECT_EQ(Result_state::failed, result.breakdown.state());
  EXPECT_EQ(Result_state::available, result.swap_capacity.state());
  EXPECT_EQ(0, result.swap_capacity.value().total_bytes);
}

TEST(MysysSystemInfo, LinuxMeminfoRejectsMissingDuplicateAndNegativeFields) {
  std::istringstream input{
      "MemTotal: 100 kB\nMemTotal: 100 kB\nMemFree: -1 kB\n"
      "Buffers: 7 kB\nCached: 6 kB\nSlab: 5 kB\n"
      "SwapTotal: 8 kB\nSwapFree: 4 kB\n"};

  const auto result = internal::read_linux_meminfo(input);

  EXPECT_EQ(Result_state::failed, result.memory.state());
  EXPECT_EQ(Result_state::available, result.breakdown.state());
  EXPECT_EQ(Result_state::available, result.swap_capacity.state());
}

TEST(MysysSystemInfo, LinuxMeminfoRejectsKilobyteOverflow) {
  const auto overflow = std::numeric_limits<uint64_t>::max() / 1024U + 1U;
  std::istringstream input{"MemTotal: " + std::to_string(overflow) +
                           " kB\nMemFree: 1 kB\nMemAvailable: 1 kB\n"
                           "Buffers: 1 kB\nCached: 1 kB\nSlab: 1 kB\n"
                           "SwapTotal: 0 kB\nSwapFree: 0 kB\n"};

  const auto result = internal::read_linux_meminfo(input);

  EXPECT_EQ(Result_state::failed, result.memory.state());
}

TEST(MysysSystemInfo, LinuxVmstatConvertsKibibytesToBytes) {
  std::istringstream input{"ignored 9\npgpgout 3\npgpgin 2\n"};

  const auto result = internal::read_linux_vmstat(input);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(2U * 1024U, result.value().bytes_in);
  EXPECT_EQ(3U * 1024U, result.value().bytes_out);

  std::istringstream zero_input{"pgpgin 0\npgpgout 0\n"};
  const auto zero_result = internal::read_linux_vmstat(zero_input);
  ASSERT_TRUE(zero_result.has_value());
  EXPECT_EQ(0, zero_result.value().bytes_in);
  EXPECT_EQ(0, zero_result.value().bytes_out);
}

TEST(MysysSystemInfo, LinuxVmstatRejectsMalformedDuplicateAndOverflow) {
  std::istringstream malformed{"pgpgin -1\npgpgout 0\n"};
  std::istringstream duplicate{"pgpgin 1\npgpgin 2\npgpgout 3\n"};
  const auto overflow = std::numeric_limits<uint64_t>::max() / 1024U + 1U;
  std::istringstream out_of_range{"pgpgin " + std::to_string(overflow) +
                                  "\npgpgout 0\n"};

  EXPECT_EQ(Result_state::failed,
            internal::read_linux_vmstat(malformed).state());
  EXPECT_EQ(Result_state::failed,
            internal::read_linux_vmstat(duplicate).state());
  EXPECT_EQ(Result_state::failed,
            internal::read_linux_vmstat(out_of_range).state());
}

TEST(MysysSystemInfo, LinuxMemoryParsersDoNotRetainState) {
  std::istringstream complete{
      "MemTotal: 100 kB\nMemFree: 20 kB\nMemAvailable: 70 kB\n"
      "Buffers: 7 kB\nCached: 6 kB\nSlab: 5 kB\n"
      "SwapTotal: 8 kB\nSwapFree: 4 kB\n"};
  std::istringstream empty;

  EXPECT_EQ(Result_state::available,
            internal::read_linux_meminfo(complete).memory.state());
  EXPECT_EQ(Result_state::failed,
            internal::read_linux_meminfo(empty).memory.state());
}

TEST(MysysSystemInfo, HostMemoryValidationRejectsInvalidValues) {
  Host_memory_snapshot snapshot;
  snapshot.memory = System_result<Host_memory_info>::success({100, 101, 50});
  snapshot.swap_capacity = System_result<Swap_capacity_info>::success({10, 11});

  const auto result =
      internal::validate_host_memory_snapshot(std::move(snapshot));

  EXPECT_EQ(Result_state::failed, result.memory.state());
  EXPECT_EQ(Result_state::failed, result.swap_capacity.state());

  Host_memory_snapshot zero_total;
  zero_total.memory = System_result<Host_memory_info>::success({0, 0, 0});
  EXPECT_EQ(Result_state::failed,
            internal::validate_host_memory_snapshot(std::move(zero_total))
                .memory.state());

  Host_memory_snapshot excess_available;
  excess_available.memory =
      System_result<Host_memory_info>::success({100, 50, 101});
  EXPECT_EQ(Result_state::failed,
            internal::validate_host_memory_snapshot(std::move(excess_available))
                .memory.state());
}

TEST(MysysSystemInfo, LinuxCpuStatParsesAggregateAndProcessors) {
  std::istringstream input{
      "intr 123\n"
      "cpu 100 2 30 400 5 6 7 8 9 10 999\n"
      "ctxt 456\n"
      "cpu0 40 1 10 200 2 3 4 5 6 7\n"
      "cpu1 60 1 20 200 3 3 3 3 3 3\n"};

  const auto result = internal::read_linux_cpu_stat(input, 100);

  ASSERT_EQ(Result_state::available, result.times.state());
  ASSERT_EQ(3, result.times.value().cpus.size());
  EXPECT_EQ("cpu", result.times.value().cpus[0].cpu);
  EXPECT_EQ(std::chrono::milliseconds{1000}, result.times.value().cpus[0].user);
  EXPECT_EQ(std::chrono::milliseconds{20}, result.times.value().cpus[0].nice);
  EXPECT_EQ(std::chrono::milliseconds{300},
            result.times.value().cpus[0].system);
  EXPECT_EQ(std::chrono::milliseconds{4000}, result.times.value().cpus[0].idle);
  EXPECT_EQ("cpu0", result.times.value().cpus[1].cpu);
  EXPECT_EQ(std::chrono::milliseconds{400}, result.times.value().cpus[1].user);
  EXPECT_EQ(std::chrono::milliseconds{10}, result.times.value().cpus[1].nice);
  EXPECT_EQ(std::chrono::milliseconds{100},
            result.times.value().cpus[1].system);
  EXPECT_EQ(std::chrono::milliseconds{2000}, result.times.value().cpus[1].idle);
  EXPECT_EQ("cpu1", result.times.value().cpus[2].cpu);
  EXPECT_EQ(std::chrono::milliseconds{600}, result.times.value().cpus[2].user);
  EXPECT_EQ(std::chrono::milliseconds{10}, result.times.value().cpus[2].nice);
  EXPECT_EQ(std::chrono::milliseconds{200},
            result.times.value().cpus[2].system);
  EXPECT_EQ(std::chrono::milliseconds{2000}, result.times.value().cpus[2].idle);

  ASSERT_EQ(Result_state::available, result.extended_times.state());
  ASSERT_EQ(3, result.extended_times.value().cpus.size());
  const auto &aggregate = result.extended_times.value().cpus[0];
  EXPECT_EQ("cpu", aggregate.cpu);
  EXPECT_EQ(std::chrono::milliseconds{50}, aggregate.io_wait);
  EXPECT_EQ(std::chrono::milliseconds{60}, aggregate.irq);
  EXPECT_EQ(std::chrono::milliseconds{70}, aggregate.soft_irq);
  EXPECT_EQ(std::chrono::milliseconds{80}, aggregate.steal);
  EXPECT_EQ(std::chrono::milliseconds{90}, aggregate.guest);
  EXPECT_EQ(std::chrono::milliseconds{100}, aggregate.guest_nice);
  const auto &cpu0 = result.extended_times.value().cpus[1];
  EXPECT_EQ("cpu0", cpu0.cpu);
  EXPECT_EQ(std::chrono::milliseconds{20}, cpu0.io_wait);
  EXPECT_EQ(std::chrono::milliseconds{30}, cpu0.irq);
  EXPECT_EQ(std::chrono::milliseconds{40}, cpu0.soft_irq);
  EXPECT_EQ(std::chrono::milliseconds{50}, cpu0.steal);
  EXPECT_EQ(std::chrono::milliseconds{60}, cpu0.guest);
  EXPECT_EQ(std::chrono::milliseconds{70}, cpu0.guest_nice);
  const auto &cpu1 = result.extended_times.value().cpus[2];
  EXPECT_EQ("cpu1", cpu1.cpu);
  EXPECT_EQ(std::chrono::milliseconds{30}, cpu1.io_wait);
  EXPECT_EQ(std::chrono::milliseconds{30}, cpu1.irq);
  EXPECT_EQ(std::chrono::milliseconds{30}, cpu1.soft_irq);
  EXPECT_EQ(std::chrono::milliseconds{30}, cpu1.steal);
  EXPECT_EQ(std::chrono::milliseconds{30}, cpu1.guest);
  EXPECT_EQ(std::chrono::milliseconds{30}, cpu1.guest_nice);
}

TEST(MysysSystemInfo, LinuxCpuStatFloorsFractionalMilliseconds) {
  std::istringstream input{
      "cpu 1 2 3 129 4 5 6 7 8 9\n"
      "cpu0 1 2 3 129 4 5 6 7 8 9\n"};

  const auto result = internal::read_linux_cpu_stat(input, 128);

  ASSERT_EQ(Result_state::available, result.times.state());
  EXPECT_EQ(std::chrono::milliseconds{7}, result.times.value().cpus[0].user);
  EXPECT_EQ(std::chrono::milliseconds{1007}, result.times.value().cpus[0].idle);
}

TEST(MysysSystemInfo, LinuxCpuStatRejectsMissingAndInvalidLabels) {
  std::istringstream missing_aggregate{"cpu0 1 2 3 4 5 6 7 8 9 10\n"};
  std::istringstream invalid_label{
      "cpu 1 2 3 4 5 6 7 8 9 10\n"
      "cpuX 1 2 3 4 5 6 7 8 9 10\n"};
  std::istringstream duplicate{
      "cpu 1 2 3 4 5 6 7 8 9 10\n"
      "cpu0 1 2 3 4 5 6 7 8 9 10\n"
      "cpu0 1 2 3 4 5 6 7 8 9 10\n"};

  for (auto *input : {&missing_aggregate, &invalid_label, &duplicate}) {
    const auto result = internal::read_linux_cpu_stat(*input, 100);
    EXPECT_EQ(Result_state::failed, result.times.state());
    EXPECT_EQ(Result_state::failed, result.extended_times.state());
  }
}

TEST(MysysSystemInfo, LinuxCpuStatCapabilityFailuresAreIndependent) {
  std::istringstream bad_extended{
      "cpu 1 2 3 4 5 6 bad 8 9 10\n"
      "cpu0 1 2 3 4 5 6 7 8 9 10\n"};
  const auto extended_result = internal::read_linux_cpu_stat(bad_extended, 100);
  EXPECT_EQ(Result_state::available, extended_result.times.state());
  EXPECT_EQ(Result_state::failed, extended_result.extended_times.state());

  std::istringstream bad_portable{
      "cpu bad 2 3 4 5 6 7 8 9 10\n"
      "cpu0 1 2 3 4 5 6 7 8 9 10\n"};
  const auto portable_result = internal::read_linux_cpu_stat(bad_portable, 100);
  EXPECT_EQ(Result_state::failed, portable_result.times.state());
  EXPECT_EQ(Result_state::available, portable_result.extended_times.state());
}

TEST(MysysSystemInfo, LinuxCpuStatRejectsMalformedAndOverflowValues) {
  std::istringstream negative{
      "cpu -1 2 3 4 5 6 7 8 9 10\n"
      "cpu0 1 2 3 4 5 6 7 8 9 10\n"};
  const auto negative_result = internal::read_linux_cpu_stat(negative, 100);
  EXPECT_EQ(Result_state::failed, negative_result.times.state());
  EXPECT_EQ(Result_state::available, negative_result.extended_times.state());

  const auto maximum = std::numeric_limits<uint64_t>::max();
  std::istringstream overflow{"cpu " + std::to_string(maximum) +
                              " 2 3 4 5 6 7 8 9 10\n"
                              "cpu0 1 2 3 4 5 6 7 8 9 10\n"};
  EXPECT_EQ(Result_state::failed,
            internal::read_linux_cpu_stat(overflow, 1).times.state());

  std::istringstream out_of_range{
      "cpu 18446744073709551616 2 3 4 5 6 7 8 9 10\n"
      "cpu0 1 2 3 4 5 6 7 8 9 10\n"};
  EXPECT_EQ(Result_state::failed,
            internal::read_linux_cpu_stat(out_of_range, 100).times.state());

  std::istringstream invalid_rate{
      "cpu 1 2 3 4 5 6 7 8 9 10\n"
      "cpu0 1 2 3 4 5 6 7 8 9 10\n"};
  const auto rate_result = internal::read_linux_cpu_stat(invalid_rate, 0);
  EXPECT_EQ(Result_state::failed, rate_result.times.state());
  EXPECT_EQ(Result_state::failed, rate_result.extended_times.state());
}

TEST(MysysSystemInfo, LinuxCpuStatDoesNotRetainState) {
  std::istringstream complete{
      "cpu 1 2 3 4 5 6 7 8 9 10\n"
      "cpu0 1 2 3 4 5 6 7 8 9 10\n"};
  std::istringstream empty;

  EXPECT_EQ(Result_state::available,
            internal::read_linux_cpu_stat(complete, 100).times.state());
  EXPECT_EQ(Result_state::failed,
            internal::read_linux_cpu_stat(empty, 100).times.state());
}

TEST(MysysSystemInfo, LinuxProcessStatusParsesCompleteFixture) {
  std::istringstream input{
      "Name:\tmysqld worker  \n"
      "Pid:\t123\n"
      "VmRSS:\t10 kB\n"
      "VmData:\t30 kB\n"
      "VmSwap:\t40 kB\n"};

  const auto result = internal::read_linux_process_status(input);

  ASSERT_EQ(Result_state::available, result.identity.state());
  EXPECT_EQ(123, result.identity.value().pid);
  EXPECT_EQ("mysqld worker", result.identity.value().name);
  ASSERT_EQ(Result_state::available, result.residency.state());
  EXPECT_EQ(10U * 1024U, result.residency.value().resident_bytes);
  ASSERT_EQ(Result_state::available, result.details.state());
  EXPECT_EQ(30U * 1024U, result.details.value().data_bytes);
  EXPECT_EQ(40U * 1024U, result.details.value().swap_bytes);
}

TEST(MysysSystemInfo, LinuxProcessStatusAcceptsReorderedAndZeroFields) {
  std::istringstream input{
      "Ignored:\tvalue\n"
      "VmSwap:\t0 kB\n"
      "Pid:\t7\n"
      "VmData:\t0 kB\n"
      "Name:\t worker process \t\n"
      "VmRSS:\t0 kB\n"};

  const auto result = internal::read_linux_process_status(input);

  ASSERT_EQ(Result_state::available, result.identity.state());
  EXPECT_EQ(7, result.identity.value().pid);
  EXPECT_EQ("worker process", result.identity.value().name);
  ASSERT_EQ(Result_state::available, result.residency.state());
  EXPECT_EQ(0, result.residency.value().resident_bytes);
  ASSERT_EQ(Result_state::available, result.details.state());
  EXPECT_EQ(0, result.details.value().data_bytes);
  EXPECT_EQ(0, result.details.value().swap_bytes);
}

TEST(MysysSystemInfo, LinuxProcessStatusFailuresAreCapabilityLocal) {
  std::istringstream missing_identity{
      "Pid:\t10\nVmRSS:\t1 kB\nVmData:\t3 kB\nVmSwap:\t4 kB\n"};
  const auto identity_result =
      internal::read_linux_process_status(missing_identity);
  EXPECT_EQ(Result_state::failed, identity_result.identity.state());
  EXPECT_EQ(Result_state::available, identity_result.residency.state());
  EXPECT_EQ(Result_state::available, identity_result.details.state());

  std::istringstream missing_pid{
      "Name:\tmysqld\nVmRSS:\t1 kB\nVmData:\t3 kB\nVmSwap:\t4 kB\n"};
  const auto pid_result = internal::read_linux_process_status(missing_pid);
  EXPECT_EQ(Result_state::failed, pid_result.identity.state());
  EXPECT_EQ(Result_state::available, pid_result.residency.state());
  EXPECT_EQ(Result_state::available, pid_result.details.state());

  std::istringstream empty_name{
      "Name:\t \nPid:\t10\nVmRSS:\t1 kB\nVmData:\t3 kB\n"
      "VmSwap:\t4 kB\n"};
  const auto name_result = internal::read_linux_process_status(empty_name);
  EXPECT_EQ(Result_state::failed, name_result.identity.state());
  EXPECT_EQ(Result_state::available, name_result.residency.state());
  EXPECT_EQ(Result_state::available, name_result.details.state());

  std::istringstream missing_residency{
      "Name:\tmysqld\nPid:\t10\nVmData:\t3 kB\nVmSwap:\t4 kB\n"};
  const auto residency_result =
      internal::read_linux_process_status(missing_residency);
  EXPECT_EQ(Result_state::available, residency_result.identity.state());
  EXPECT_EQ(Result_state::failed, residency_result.residency.state());
  EXPECT_EQ(Result_state::available, residency_result.details.state());

  std::istringstream missing_details{
      "Name:\tmysqld\nPid:\t10\nVmRSS:\t1 kB\nVmData:\t3 kB\n"};
  const auto details_result =
      internal::read_linux_process_status(missing_details);
  EXPECT_EQ(Result_state::available, details_result.identity.state());
  EXPECT_EQ(Result_state::available, details_result.residency.state());
  EXPECT_EQ(Result_state::failed, details_result.details.state());

  std::istringstream missing_data{
      "Name:\tmysqld\nPid:\t10\nVmRSS:\t1 kB\nVmSwap:\t4 kB\n"};
  const auto data_result = internal::read_linux_process_status(missing_data);
  EXPECT_EQ(Result_state::available, data_result.identity.state());
  EXPECT_EQ(Result_state::available, data_result.residency.state());
  EXPECT_EQ(Result_state::failed, data_result.details.state());
}

TEST(MysysSystemInfo, LinuxProcessStatusRejectsDuplicateFieldsLocally) {
  std::istringstream duplicate_identity{
      "Name:\tmysqld\nName:\tmysqld\nPid:\t10\nVmRSS:\t1 kB\n"
      "VmData:\t3 kB\nVmSwap:\t4 kB\n"};
  const auto identity_result =
      internal::read_linux_process_status(duplicate_identity);
  EXPECT_EQ(Result_state::failed, identity_result.identity.state());
  EXPECT_EQ(Result_state::available, identity_result.residency.state());
  EXPECT_EQ(Result_state::available, identity_result.details.state());

  std::istringstream duplicate_residency{
      "Name:\tmysqld\nPid:\t10\nVmRSS:\t1 kB\nVmRSS:\t1 kB\n"
      "VmData:\t3 kB\nVmSwap:\t4 kB\n"};
  const auto residency_result =
      internal::read_linux_process_status(duplicate_residency);
  EXPECT_EQ(Result_state::available, residency_result.identity.state());
  EXPECT_EQ(Result_state::failed, residency_result.residency.state());
  EXPECT_EQ(Result_state::available, residency_result.details.state());

  std::istringstream duplicate_details{
      "Name:\tmysqld\nPid:\t10\nVmRSS:\t1 kB\nVmData:\t3 kB\n"
      "VmData:\t3 kB\nVmSwap:\t4 kB\n"};
  const auto details_result =
      internal::read_linux_process_status(duplicate_details);
  EXPECT_EQ(Result_state::available, details_result.identity.state());
  EXPECT_EQ(Result_state::available, details_result.residency.state());
  EXPECT_EQ(Result_state::failed, details_result.details.state());

  std::istringstream duplicate_swap{
      "Name:\tmysqld\nPid:\t10\nVmRSS:\t1 kB\nVmData:\t3 kB\n"
      "VmSwap:\t4 kB\nVmSwap:\t4 kB\n"};
  const auto swap_result = internal::read_linux_process_status(duplicate_swap);
  EXPECT_EQ(Result_state::available, swap_result.identity.state());
  EXPECT_EQ(Result_state::available, swap_result.residency.state());
  EXPECT_EQ(Result_state::failed, swap_result.details.state());
}

TEST(MysysSystemInfo, LinuxProcessStatusRejectsInvalidValuesAndUnits) {
  const auto status_with_memory_value = [](std::string_view field,
                                           std::string_view value) {
    const auto select = [&](std::string_view expected,
                            std::string_view default_value) {
      return std::string{field == expected ? value : default_value};
    };
    return "Name:\tmysqld\nPid:\t10\nVmRSS:\t" + select("VmRSS", "1 kB") +
           "\nVmData:\t" + select("VmData", "3 kB") + "\nVmSwap:\t" +
           select("VmSwap", "4 kB") + "\n";
  };
  for (const std::string_view field : {"VmRSS", "VmData", "VmSwap"}) {
    for (const std::string_view value :
         {"bad kB", "-1 kB", "18446744073709551616 kB", "1 KB", "1 kB extra"}) {
      std::istringstream input{status_with_memory_value(field, value)};
      const auto result = internal::read_linux_process_status(input);
      EXPECT_EQ(Result_state::available, result.identity.state());
      if (field == "VmRSS") {
        EXPECT_EQ(Result_state::failed, result.residency.state());
        EXPECT_EQ(Result_state::available, result.details.state());
      } else {
        EXPECT_EQ(Result_state::available, result.residency.state());
        EXPECT_EQ(Result_state::failed, result.details.state());
      }
    }
  }

  for (const std::string_view pid :
       {"-1", "0", "bad", "1 extra", "18446744073709551616"}) {
    std::istringstream input{"Name:\tmysqld\nPid:\t" + std::string{pid} +
                             "\nVmRSS:\t1 kB\nVmData:\t3 kB\nVmSwap:\t4 kB\n"};
    const auto result = internal::read_linux_process_status(input);
    EXPECT_EQ(Result_state::failed, result.identity.state());
    EXPECT_EQ(Result_state::available, result.residency.state());
    EXPECT_EQ(Result_state::available, result.details.state());
  }
}

TEST(MysysSystemInfo, LinuxProcessStatusRejectsMemoryOverflow) {
  const auto overflow = std::numeric_limits<uint64_t>::max() / 1024U + 1U;
  std::istringstream input{"Name:\tmysqld\nPid:\t10\nVmRSS:\t" +
                           std::to_string(overflow) +
                           " kB\nVmData:\t3 kB\nVmSwap:\t4 kB\n"};

  const auto result = internal::read_linux_process_status(input);

  EXPECT_EQ(Result_state::available, result.identity.state());
  EXPECT_EQ(Result_state::failed, result.residency.state());
  EXPECT_EQ(Result_state::available, result.details.state());

  std::istringstream details_input{
      "Name:\tmysqld\nPid:\t10\nVmRSS:\t1 kB\nVmData:\t" +
      std::to_string(overflow) + " kB\nVmSwap:\t4 kB\n"};
  const auto details_result =
      internal::read_linux_process_status(details_input);
  EXPECT_EQ(Result_state::available, details_result.residency.state());
  EXPECT_EQ(Result_state::failed, details_result.details.state());
}

TEST(MysysSystemInfo, LinuxProcessStatusIgnoresRemovedMemoryFields) {
  std::istringstream input{
      "Name:\tmysqld\nPid:\t10\nVmRSS:\t1 kB\nVmSize:\tbad\n"
      "VmData:\t3 kB\nVmSwap:\t4 kB\n"};

  const auto result = internal::read_linux_process_status(input);

  EXPECT_EQ(Result_state::available, result.identity.state());
  EXPECT_EQ(Result_state::available, result.residency.state());
  EXPECT_EQ(Result_state::available, result.details.state());
}

TEST(MysysSystemInfo, LinuxProcessStatusDoesNotRetainState) {
  std::istringstream complete{
      "Name:\tmysqld\nPid:\t10\nVmRSS:\t1 kB\nVmData:\t3 kB\n"
      "VmSwap:\t4 kB\n"};
  std::istringstream empty;

  EXPECT_EQ(Result_state::available,
            internal::read_linux_process_status(complete).residency.state());
  const auto empty_result = internal::read_linux_process_status(empty);
  EXPECT_EQ(Result_state::failed, empty_result.identity.state());
  EXPECT_EQ(Result_state::failed, empty_result.residency.state());
  EXPECT_EQ(Result_state::failed, empty_result.details.state());
}

TEST(MysysSystemInfo, ProcessMemoryValidationRejectsInvalidIdentityOnly) {
  Process_memory_snapshot zero_pid;
  zero_pid.identity = System_result<Process_identity>::success({0, "mysqld"});
  zero_pid.residency = System_result<Process_memory_residency>::success({0});
  zero_pid.details = System_result<Process_memory_details>::success({0, 0});
  zero_pid.page_faults = System_result<Process_page_faults>::success({0});
  const auto zero_result =
      internal::validate_process_memory_snapshot(std::move(zero_pid));
  EXPECT_EQ(Result_state::failed, zero_result.identity.state());
  ASSERT_EQ(Result_state::available, zero_result.residency.state());
  EXPECT_EQ(0, zero_result.residency.value().resident_bytes);
  ASSERT_EQ(Result_state::available, zero_result.details.state());
  EXPECT_EQ(0, zero_result.details.value().data_bytes);
  EXPECT_EQ(0, zero_result.details.value().swap_bytes);
  ASSERT_EQ(Result_state::available, zero_result.page_faults.state());
  EXPECT_EQ(0, zero_result.page_faults.value().major_faults);

  Process_memory_snapshot empty_name;
  empty_name.identity = System_result<Process_identity>::success({1, ""});
  EXPECT_EQ(Result_state::failed,
            internal::validate_process_memory_snapshot(std::move(empty_name))
                .identity.state());
}

TEST(MysysSystemInfo, ProcessMemoryValidationPreservesCapabilityStates) {
  Process_memory_snapshot unavailable;
  unavailable.identity = System_result<Process_identity>::unavailable();
  unavailable.residency =
      System_result<Process_memory_residency>::unavailable();
  unavailable.details = System_result<Process_memory_details>::unavailable();
  unavailable.page_faults = System_result<Process_page_faults>::unavailable();
  const auto unavailable_result =
      internal::validate_process_memory_snapshot(std::move(unavailable));
  EXPECT_EQ(Result_state::unavailable, unavailable_result.identity.state());
  EXPECT_EQ(Result_state::unavailable, unavailable_result.residency.state());
  EXPECT_EQ(Result_state::unavailable, unavailable_result.details.state());
  EXPECT_EQ(Result_state::unavailable, unavailable_result.page_faults.state());

  Process_memory_snapshot failed;
  const auto error = std::make_error_code(std::errc::io_error);
  failed.identity = System_result<Process_identity>::failure(error);
  failed.residency = System_result<Process_memory_residency>::failure(error);
  failed.details = System_result<Process_memory_details>::failure(error);
  failed.page_faults = System_result<Process_page_faults>::failure(error);
  const auto failed_result =
      internal::validate_process_memory_snapshot(std::move(failed));
  EXPECT_EQ(Result_state::failed, failed_result.identity.state());
  EXPECT_EQ(error, failed_result.identity.error());
  EXPECT_EQ(Result_state::failed, failed_result.residency.state());
  EXPECT_EQ(error, failed_result.residency.error());
  EXPECT_EQ(Result_state::failed, failed_result.details.state());
  EXPECT_EQ(error, failed_result.details.error());
  EXPECT_EQ(Result_state::failed, failed_result.page_faults.state());
  EXPECT_EQ(error, failed_result.page_faults.error());
}

TEST(MysysSystemInfo, ProcessMemoryValidationDoesNotRetainState) {
  Process_memory_snapshot available;
  available.identity = System_result<Process_identity>::success({1, "mysqld"});
  available.residency = System_result<Process_memory_residency>::success({1});
  available.details = System_result<Process_memory_details>::success({2, 3});
  available.page_faults = System_result<Process_page_faults>::success({4});
  const auto available_result =
      internal::validate_process_memory_snapshot(std::move(available));
  EXPECT_EQ(Result_state::available, available_result.identity.state());
  EXPECT_EQ(Result_state::available, available_result.residency.state());
  EXPECT_EQ(Result_state::available, available_result.details.state());
  EXPECT_EQ(Result_state::available, available_result.page_faults.state());

  const auto empty_result =
      internal::validate_process_memory_snapshot(Process_memory_snapshot{});
  EXPECT_EQ(Result_state::unavailable, empty_result.identity.state());
  EXPECT_EQ(Result_state::unavailable, empty_result.residency.state());
  EXPECT_EQ(Result_state::unavailable, empty_result.details.state());
  EXPECT_EQ(Result_state::unavailable, empty_result.page_faults.state());
}

TEST(MysysSystemInfo, LinuxThreadStatMapsCompleteRecord) {
  std::istringstream input{
      "10 (worker ) name) S 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 "
      "17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 "
      "37 38 39 40 41 42 extra\n"};

  const auto result =
      internal::read_linux_thread_stat(input, 10, 10, 100, 4096);

  ASSERT_EQ(Result_state::available, result.threads.state());
  ASSERT_EQ(1U, result.threads.value().threads.size());
  EXPECT_EQ(10U, result.threads.value().threads[0].tid);
  EXPECT_EQ("worker ) name", result.threads.value().threads[0].name);
  EXPECT_EQ(Thread_state::sleeping, result.threads.value().threads[0].state);
  ASSERT_EQ(Result_state::available, result.cpu.state());
  EXPECT_EQ(std::chrono::milliseconds{110}, result.cpu.value().threads[0].user);
  EXPECT_EQ(std::chrono::milliseconds{120},
            result.cpu.value().threads[0].system);
  ASSERT_EQ(Result_state::available, result.extended_cpu.state());
  EXPECT_EQ(std::chrono::milliseconds{130},
            result.extended_cpu.value().threads[0].child_user);
  EXPECT_EQ(std::chrono::milliseconds{140},
            result.extended_cpu.value().threads[0].child_system);
  EXPECT_EQ(std::chrono::milliseconds{400},
            result.extended_cpu.value().threads[0].guest);
  EXPECT_EQ(std::chrono::milliseconds{410},
            result.extended_cpu.value().threads[0].child_guest);
  ASSERT_EQ(Result_state::available, result.scheduler.state());
  EXPECT_EQ(36U, result.scheduler.value().threads[0].last_cpu);
  EXPECT_EQ(std::chrono::milliseconds{390},
            result.scheduler.value().threads[0].block_io_delay);
  ASSERT_EQ(Result_state::available, result.runtime.state());
  EXPECT_EQ(17U, result.runtime.value().thread_count);
  EXPECT_EQ(20U, result.runtime.value().virtual_bytes);
  EXPECT_EQ(21U * 4096U, result.runtime.value().resident_bytes);
  EXPECT_EQ(22U, result.runtime.value().resident_limit_bytes);
}

TEST(MysysSystemInfo, LinuxThreadIoParsesExactFields) {
  std::istringstream input{
      "write_bytes: 22\nrchar: 100\nread_bytes: 11\nsyscr: 3\n"};

  const auto result = internal::read_linux_thread_io(input, 10);

  ASSERT_EQ(Result_state::available, result.state());
  EXPECT_EQ(10U, result.value().tid);
  EXPECT_EQ(11U, result.value().read_bytes);
  EXPECT_EQ(22U, result.value().write_bytes);
}

TEST(MysysSystemInfo, LinuxThreadStatMapsStates) {
  for (const auto &[state, expected] :
       std::vector<std::pair<char, Thread_state>>{
           {'R', Thread_state::running},
           {'S', Thread_state::sleeping},
           {'D', Thread_state::waiting},
           {'T', Thread_state::stopped},
           {'t', Thread_state::stopped},
           {'Z', Thread_state::zombie},
           {'I', Thread_state::unknown}}) {
    std::istringstream input{linux_thread_stat_fixture(state)};
    const auto result =
        internal::read_linux_thread_stat(input, 10, 20, 100, 4096);
    ASSERT_EQ(Result_state::available, result.threads.state());
    EXPECT_EQ(expected, result.threads.value().threads[0].state);
    EXPECT_EQ(Result_state::unavailable, result.runtime.state());
  }
}

TEST(MysysSystemInfo, LinuxThreadStatUsesRuntimeConversionValues) {
  std::istringstream input{linux_thread_stat_fixture(
      'S', "worker", {{14, "1"}, {15, "2"}, {24, "3"}})};

  const auto result =
      internal::read_linux_thread_stat(input, 10, 10, 128, 4096);

  ASSERT_EQ(Result_state::available, result.cpu.state());
  EXPECT_EQ(std::chrono::milliseconds{7}, result.cpu.value().threads[0].user);
  EXPECT_EQ(std::chrono::milliseconds{15},
            result.cpu.value().threads[0].system);
  ASSERT_EQ(Result_state::available, result.runtime.state());
  EXPECT_EQ(3U * 4096U, result.runtime.value().resident_bytes);
}

TEST(MysysSystemInfo, LinuxThreadStatIsolatesMalformedFieldGroups) {
  std::istringstream bad_cpu{
      linux_thread_stat_fixture('S', "worker", {{14, "-1"}})};
  const auto cpu_result =
      internal::read_linux_thread_stat(bad_cpu, 10, 10, 100, 4096);
  EXPECT_EQ(Result_state::available, cpu_result.threads.state());
  EXPECT_EQ(Result_state::failed, cpu_result.cpu.state());
  EXPECT_EQ(Result_state::available, cpu_result.extended_cpu.state());
  EXPECT_EQ(Result_state::available, cpu_result.scheduler.state());
  EXPECT_EQ(Result_state::available, cpu_result.runtime.state());

  std::istringstream bad_extended{
      linux_thread_stat_fixture('S', "worker", {{43, "bad"}})};
  const auto extended_result =
      internal::read_linux_thread_stat(bad_extended, 10, 10, 100, 4096);
  EXPECT_EQ(Result_state::available, extended_result.cpu.state());
  EXPECT_EQ(Result_state::failed, extended_result.extended_cpu.state());
  EXPECT_EQ(Result_state::available, extended_result.scheduler.state());

  std::istringstream bad_scheduler{
      linux_thread_stat_fixture('S', "worker", {{39, "4294967296"}})};
  const auto scheduler_result =
      internal::read_linux_thread_stat(bad_scheduler, 10, 10, 100, 4096);
  EXPECT_EQ(Result_state::available, scheduler_result.cpu.state());
  EXPECT_EQ(Result_state::failed, scheduler_result.scheduler.state());

  std::istringstream bad_runtime{
      linux_thread_stat_fixture('S', "worker", {{20, "0"}})};
  const auto runtime_result =
      internal::read_linux_thread_stat(bad_runtime, 10, 10, 100, 4096);
  EXPECT_EQ(Result_state::available, runtime_result.cpu.state());
  EXPECT_EQ(Result_state::failed, runtime_result.runtime.state());
}

TEST(MysysSystemInfo, LinuxThreadStatRejectsTruncatedAndAmbiguousRecords) {
  std::istringstream truncated{"10 (worker) S 1 2 3\n"};
  const auto truncated_result =
      internal::read_linux_thread_stat(truncated, 10, 10, 100, 4096);
  EXPECT_EQ(Result_state::available, truncated_result.threads.state());
  EXPECT_EQ(Result_state::failed, truncated_result.cpu.state());
  EXPECT_EQ(Result_state::failed, truncated_result.runtime.state());

  for (const std::string_view record :
       {"10 worker) S 1 2 3\n", "10 (worker S 1 2 3\n",
        "11 (worker) S 1 2 3\n"}) {
    std::istringstream input{std::string{record}};
    const auto result =
        internal::read_linux_thread_stat(input, 10, 10, 100, 4096);
    EXPECT_EQ(Result_state::failed, result.threads.state());
    EXPECT_EQ(Result_state::failed, result.cpu.state());
    EXPECT_EQ(Result_state::failed, result.runtime.state());
  }
}

TEST(MysysSystemInfo, LinuxThreadStatTruncationIsCapabilityLocal) {
  const std::string complete = linux_thread_stat_fixture();
  const auto truncate_after_field = [&](size_t field) {
    std::istringstream words{complete};
    std::string result;
    std::string word;
    const size_t word_count = 3 + (field - 3);
    for (size_t index = 0; index < word_count && words >> word; ++index) {
      if (!result.empty()) result += " ";
      result += word;
    }
    return result + "\n";
  };

  std::istringstream through_cpu{truncate_after_field(15)};
  const auto cpu_result =
      internal::read_linux_thread_stat(through_cpu, 10, 10, 100, 4096);
  EXPECT_EQ(Result_state::available, cpu_result.threads.state());
  EXPECT_EQ(Result_state::available, cpu_result.cpu.state());
  EXPECT_EQ(Result_state::failed, cpu_result.runtime.state());
  EXPECT_EQ(Result_state::failed, cpu_result.scheduler.state());
  EXPECT_EQ(Result_state::failed, cpu_result.extended_cpu.state());

  std::istringstream through_runtime{truncate_after_field(25)};
  const auto runtime_result =
      internal::read_linux_thread_stat(through_runtime, 10, 10, 100, 4096);
  EXPECT_EQ(Result_state::available, runtime_result.cpu.state());
  EXPECT_EQ(Result_state::available, runtime_result.runtime.state());
  EXPECT_EQ(Result_state::failed, runtime_result.scheduler.state());
  EXPECT_EQ(Result_state::failed, runtime_result.extended_cpu.state());

  std::istringstream through_scheduler{truncate_after_field(42)};
  const auto scheduler_result =
      internal::read_linux_thread_stat(through_scheduler, 10, 10, 100, 4096);
  EXPECT_EQ(Result_state::available, scheduler_result.cpu.state());
  EXPECT_EQ(Result_state::available, scheduler_result.runtime.state());
  EXPECT_EQ(Result_state::available, scheduler_result.scheduler.state());
  EXPECT_EQ(Result_state::failed, scheduler_result.extended_cpu.state());
}

TEST(MysysSystemInfo, LinuxThreadStatRejectsConversionOverflow) {
  std::istringstream tick_overflow{
      linux_thread_stat_fixture('S', "worker", {{14, "18446744073709551615"}})};
  const auto tick_result =
      internal::read_linux_thread_stat(tick_overflow, 10, 10, 1, 4096);
  EXPECT_EQ(Result_state::failed, tick_result.cpu.state());
  EXPECT_EQ(Result_state::available, tick_result.runtime.state());

  std::istringstream page_overflow{
      linux_thread_stat_fixture('S', "worker", {{24, "18446744073709551615"}})};
  const auto page_result =
      internal::read_linux_thread_stat(page_overflow, 10, 10, 100, 2);
  EXPECT_EQ(Result_state::available, page_result.cpu.state());
  EXPECT_EQ(Result_state::failed, page_result.runtime.state());
}

TEST(MysysSystemInfo, LinuxThreadIoRejectsInvalidFields) {
  for (const std::string_view content :
       {"read_bytes: 1\n", "write_bytes: 2\n",
        "read_bytes: 1\nread_bytes: 2\nwrite_bytes: 3\n",
        "read_bytes: -1\nwrite_bytes: 2\n", "read_bytes: bad\nwrite_bytes: 2\n",
        "read_bytes: 18446744073709551616\nwrite_bytes: 2\n"}) {
    std::istringstream input{std::string{content}};
    EXPECT_EQ(Result_state::failed,
              internal::read_linux_thread_io(input, 10).state());
  }
}

TEST(MysysSystemInfo, LinuxThreadIoAcceptsZeroAndDoesNotRetainState) {
  std::istringstream zero{"read_bytes: 0\nwrite_bytes: 0\n"};
  const auto zero_result = internal::read_linux_thread_io(zero, 10);
  ASSERT_EQ(Result_state::available, zero_result.state());
  EXPECT_EQ(0U, zero_result.value().read_bytes);
  EXPECT_EQ(0U, zero_result.value().write_bytes);

  std::istringstream empty;
  EXPECT_EQ(Result_state::failed,
            internal::read_linux_thread_io(empty, 10).state());
}

TEST(MysysSystemInfo, ProcessThreadsValidationRejectsInvalidRowsLocally) {
  Process_threads_snapshot snapshot;
  snapshot.identity = System_result<Process_identity>::success({0, "mysqld"});
  snapshot.threads = System_result<Thread_inventory>::success(
      {{{1, "one", Thread_state::running},
        {1, "duplicate", Thread_state::sleeping}}});
  snapshot.runtime = System_result<Process_runtime_info>::success({});
  snapshot.cpu = System_result<Thread_cpu_snapshot>::success({{{1, {}, {}}}});

  const auto result =
      internal::validate_process_threads_snapshot(std::move(snapshot));

  EXPECT_EQ(Result_state::failed, result.identity.state());
  EXPECT_EQ(Result_state::failed, result.threads.state());
  EXPECT_EQ(Result_state::failed, result.runtime.state());
  EXPECT_EQ(Result_state::available, result.cpu.state());
}

TEST(MysysSystemInfo, ProcessThreadsValidationRejectsMisalignedCapabilityOnly) {
  Process_threads_snapshot snapshot;
  snapshot.threads = System_result<Thread_inventory>::success(
      {{{1, "one", Thread_state::running},
        {2, "two", Thread_state::sleeping}}});
  snapshot.cpu =
      System_result<Thread_cpu_snapshot>::success({{{2, {}, {}}, {1, {}, {}}}});
  snapshot.scheduler = System_result<Thread_scheduler_snapshot>::success(
      {{{1, 0, {}}, {2, 0, {}}}});

  const auto result =
      internal::validate_process_threads_snapshot(std::move(snapshot));

  EXPECT_EQ(Result_state::available, result.threads.state());
  EXPECT_EQ(Result_state::failed, result.cpu.state());
  EXPECT_EQ(Result_state::available, result.scheduler.state());
}

TEST(MysysSystemInfo, ProcessThreadsValidationPreservesUnavailableAndFailed) {
  Process_threads_snapshot snapshot;
  const auto error = std::make_error_code(std::errc::io_error);
  snapshot.threads = System_result<Thread_inventory>::unavailable();
  snapshot.cpu = System_result<Thread_cpu_snapshot>::failure(error);

  const auto result =
      internal::validate_process_threads_snapshot(std::move(snapshot));

  EXPECT_EQ(Result_state::unavailable, result.threads.state());
  EXPECT_EQ(Result_state::failed, result.cpu.state());
  EXPECT_EQ(error, result.cpu.error());
}

TEST(MysysSystemInfo, ProcessThreadsValidationRejectsEmptyAndZeroTidRows) {
  Process_threads_snapshot snapshot;
  snapshot.threads = System_result<Thread_inventory>::success({});
  snapshot.cpu = System_result<Thread_cpu_snapshot>::success({{{0, {}, {}}}});
  snapshot.extended_cpu = System_result<Thread_extended_cpu_snapshot>::success(
      {{{0, {}, {}, {}, {}}}});
  snapshot.scheduler =
      System_result<Thread_scheduler_snapshot>::success({{{0, 0, {}}}});
  snapshot.io = System_result<Thread_io_snapshot>::success({{{0, 0, 0}}});

  const auto result =
      internal::validate_process_threads_snapshot(std::move(snapshot));

  EXPECT_EQ(Result_state::failed, result.threads.state());
  EXPECT_EQ(Result_state::failed, result.cpu.state());
  EXPECT_EQ(Result_state::failed, result.extended_cpu.state());
  EXPECT_EQ(Result_state::failed, result.scheduler.state());
  EXPECT_EQ(Result_state::failed, result.io.state());

  Process_threads_snapshot zero_inventory;
  zero_inventory.threads = System_result<Thread_inventory>::success(
      {{{0, "zero", Thread_state::unknown}}});
  EXPECT_EQ(Result_state::failed, internal::validate_process_threads_snapshot(
                                      std::move(zero_inventory))
                                      .threads.state());
}

TEST(MysysSystemInfo, HostCpuValidationRejectsInvalidLabelsAndOrdering) {
  Host_cpu_snapshot empty;
  empty.times = System_result<Cpu_times_snapshot>::success({});
  EXPECT_EQ(
      Result_state::failed,
      internal::validate_host_cpu_snapshot(std::move(empty)).times.state());

  Host_cpu_snapshot duplicate;
  duplicate.times =
      System_result<Cpu_times_snapshot>::success({{{"cpu", {}, {}, {}, {}},
                                                   {"cpu0", {}, {}, {}, {}},
                                                   {"cpu0", {}, {}, {}, {}}}});
  EXPECT_EQ(
      Result_state::failed,
      internal::validate_host_cpu_snapshot(std::move(duplicate)).times.state());

  Host_cpu_snapshot mismatch;
  mismatch.times = System_result<Cpu_times_snapshot>::success(
      {{{"cpu", {}, {}, {}, {}}, {"cpu0", {}, {}, {}, {}}}});
  mismatch.extended_times = System_result<Cpu_extended_snapshot>::success(
      {{{"cpu", {}, {}, {}, {}, {}, {}}, {"cpu1", {}, {}, {}, {}, {}, {}}}});
  const auto mismatch_result =
      internal::validate_host_cpu_snapshot(std::move(mismatch));
  EXPECT_EQ(Result_state::available, mismatch_result.times.state());
  EXPECT_EQ(Result_state::failed, mismatch_result.extended_times.state());

  Host_cpu_snapshot reordered;
  reordered.times =
      System_result<Cpu_times_snapshot>::success({{{"cpu", {}, {}, {}, {}},
                                                   {"cpu0", {}, {}, {}, {}},
                                                   {"cpu1", {}, {}, {}, {}}}});
  reordered.extended_times = System_result<Cpu_extended_snapshot>::success(
      {{{"cpu", {}, {}, {}, {}, {}, {}},
        {"cpu1", {}, {}, {}, {}, {}, {}},
        {"cpu0", {}, {}, {}, {}, {}, {}}}});
  const auto reordered_result =
      internal::validate_host_cpu_snapshot(std::move(reordered));
  EXPECT_EQ(Result_state::available, reordered_result.times.state());
  EXPECT_EQ(Result_state::failed, reordered_result.extended_times.state());
}

#ifdef __linux__
TEST(MysysSystemInfo, StorageQueryReturnsLinuxCapabilities) {
  const auto result = query_storage_devices();

  ASSERT_EQ(Result_state::available, result.inventory.state());
  ASSERT_FALSE(result.inventory.value().devices.empty());
  ASSERT_EQ(Result_state::available, result.capacities.state());
  ASSERT_EQ(result.inventory.value().devices.size(),
            result.capacities.value().devices.size());
  ASSERT_EQ(Result_state::available, result.hierarchy.state());
  ASSERT_EQ(Result_state::available, result.read_write.state());
  ASSERT_FALSE(result.read_write.value().devices.empty());
  for (size_t i = 0; i < result.inventory.value().devices.size(); ++i) {
    EXPECT_EQ(result.inventory.value().devices[i].name,
              result.capacities.value().devices[i].device);
    EXPECT_GT(result.capacities.value().devices[i].sector_size_bytes, 0U);
    if (i != 0) {
      EXPECT_LT(result.inventory.value().devices[i - 1].name,
                result.inventory.value().devices[i].name);
    }
  }
  EXPECT_TRUE(result.flush.state() == Result_state::available ||
              result.flush.state() == Result_state::unavailable);
}

TEST(MysysSystemInfo, HostMemoryQueryReturnsLinuxCapabilities) {
  const auto result = query_host_memory();

  ASSERT_EQ(Result_state::available, result.memory.state());
  EXPECT_GT(result.memory.value().total_bytes, 0);
  EXPECT_LE(result.memory.value().free_bytes,
            result.memory.value().total_bytes);
  EXPECT_LE(result.memory.value().available_bytes,
            result.memory.value().total_bytes);

  EXPECT_EQ(Result_state::available, result.breakdown.state());

  ASSERT_EQ(Result_state::available, result.swap_capacity.state());
  EXPECT_LE(result.swap_capacity.value().free_bytes,
            result.swap_capacity.value().total_bytes);

  EXPECT_EQ(Result_state::available, result.swap_activity.state());
}

TEST(MysysSystemInfo, HostCpuQueryReturnsLinuxCapabilities) {
  const auto result = query_host_cpu();

  ASSERT_EQ(Result_state::available, result.times.state());
  ASSERT_GE(result.times.value().cpus.size(), 2);
  EXPECT_EQ("cpu", result.times.value().cpus.front().cpu);
  ASSERT_EQ(Result_state::available, result.extended_times.state());
  ASSERT_EQ(result.times.value().cpus.size(),
            result.extended_times.value().cpus.size());
  for (size_t i = 0; i < result.times.value().cpus.size(); ++i) {
    EXPECT_EQ(result.times.value().cpus[i].cpu,
              result.extended_times.value().cpus[i].cpu);
  }
}

TEST(MysysSystemInfo, ProcessMemoryQueryReturnsLinuxCapabilities) {
  const auto result = query_process_memory();

  ASSERT_EQ(Result_state::available, result.identity.state());
  EXPECT_EQ(static_cast<uint64_t>(getpid()), result.identity.value().pid);
  EXPECT_FALSE(result.identity.value().name.empty());
  EXPECT_EQ(Result_state::available, result.residency.state());
  EXPECT_EQ(Result_state::available, result.details.state());
  EXPECT_EQ(Result_state::available, result.page_faults.state());
}

TEST(MysysSystemInfo, ProcessThreadsQueryReturnsLinuxCapabilities) {
  const auto result = query_process_threads();

  ASSERT_EQ(Result_state::available, result.identity.state());
  EXPECT_EQ(static_cast<uint64_t>(getpid()), result.identity.value().pid);
  ASSERT_EQ(Result_state::available, result.threads.state());
  ASSERT_EQ(Result_state::available, result.runtime.state());
  ASSERT_EQ(Result_state::available, result.cpu.state());
  ASSERT_EQ(Result_state::available, result.extended_cpu.state());
  ASSERT_EQ(Result_state::available, result.scheduler.state());
  ASSERT_EQ(Result_state::available, result.io.state());
  ASSERT_FALSE(result.threads.value().threads.empty());
  EXPECT_EQ(result.threads.value().threads.size(),
            result.cpu.value().threads.size());
  EXPECT_EQ(result.threads.value().threads.size(),
            result.extended_cpu.value().threads.size());
  EXPECT_EQ(result.threads.value().threads.size(),
            result.scheduler.value().threads.size());
  EXPECT_EQ(result.threads.value().threads.size(),
            result.io.value().threads.size());
  bool leader_found = false;
  for (size_t index = 0; index < result.threads.value().threads.size();
       ++index) {
    const uint64_t tid = result.threads.value().threads[index].tid;
    leader_found = leader_found || tid == static_cast<uint64_t>(getpid());
    EXPECT_EQ(tid, result.cpu.value().threads[index].tid);
    EXPECT_EQ(tid, result.extended_cpu.value().threads[index].tid);
    EXPECT_EQ(tid, result.scheduler.value().threads[index].tid);
    EXPECT_EQ(tid, result.io.value().threads[index].tid);
  }
  EXPECT_TRUE(leader_found);
}
#endif

#ifdef __APPLE__
TEST(MysysSystemInfo, StorageQueryReturnsmacOSUnavailable) {
  const auto result = query_storage_devices();

  EXPECT_EQ(Result_state::unavailable, result.inventory.state());
  EXPECT_EQ(Result_state::unavailable, result.capacities.state());
  EXPECT_EQ(Result_state::unavailable, result.hierarchy.state());
  EXPECT_EQ(Result_state::unavailable, result.read_write.state());
  EXPECT_EQ(Result_state::unavailable, result.flush.state());
}

TEST(MysysSystemInfo, MachErrorsUseNativeCategoryAndMessage) {
  const auto error = internal::mach_error_code(KERN_INVALID_ARGUMENT);

  EXPECT_STREQ("mach", error.category().name());
  EXPECT_EQ(mach_error_string(KERN_INVALID_ARGUMENT), error.message());
  EXPECT_NE(std::system_category(), error.category());
  EXPECT_NE(std::generic_category(), error.category());
}

TEST(MysysSystemInfo, HostMemoryQueryReturnsmacOSCapabilities) {
  const auto result = query_host_memory();

  ASSERT_EQ(Result_state::available, result.memory.state());
  EXPECT_GT(result.memory.value().total_bytes, 0);
  EXPECT_LE(result.memory.value().free_bytes,
            result.memory.value().total_bytes);
  EXPECT_LE(result.memory.value().available_bytes,
            result.memory.value().total_bytes);
  EXPECT_EQ(Result_state::unavailable, result.breakdown.state());
  ASSERT_EQ(Result_state::available, result.swap_capacity.state())
      << result.swap_capacity.error().message();
  EXPECT_LE(result.swap_capacity.value().free_bytes,
            result.swap_capacity.value().total_bytes);
  EXPECT_EQ(Result_state::unavailable, result.swap_activity.state());
}

TEST(MysysSystemInfo, HostCpuQueryReturnsmacOSCapabilities) {
  const auto first = query_host_cpu();
  const auto second = query_host_cpu();

  ASSERT_EQ(Result_state::available, first.times.state());
  ASSERT_GE(first.times.value().cpus.size(), 2);
  EXPECT_EQ("cpu", first.times.value().cpus.front().cpu);
  for (size_t i = 1; i < first.times.value().cpus.size(); ++i) {
    EXPECT_EQ("cpu" + std::to_string(i - 1), first.times.value().cpus[i].cpu);
  }
  EXPECT_EQ(Result_state::unavailable, first.extended_times.state());
  EXPECT_EQ(Result_state::available, second.times.state());
  EXPECT_EQ(Result_state::unavailable, second.extended_times.state());
}

TEST(MysysSystemInfo, ProcessMemoryQueryReturnsmacOSCapabilities) {
  const auto first = query_process_memory();
  const auto second = query_process_memory();
  struct proc_taskinfo native_task_information {};
  const int native_task_size =
      proc_pidinfo(getpid(), PROC_PIDTASKINFO, 0, &native_task_information,
                   sizeof(native_task_information));

  ASSERT_EQ(Result_state::available, first.identity.state());
  EXPECT_EQ(static_cast<uint64_t>(getpid()), first.identity.value().pid);
  EXPECT_FALSE(first.identity.value().name.empty());
  ASSERT_EQ(Result_state::available, first.residency.state());
  EXPECT_GT(first.residency.value().resident_bytes, 0);
  EXPECT_EQ(Result_state::unavailable, first.details.state());
  EXPECT_EQ(Result_state::unavailable, first.page_faults.state());

  ASSERT_EQ(Result_state::available, second.identity.state());
  EXPECT_EQ(static_cast<uint64_t>(getpid()), second.identity.value().pid);
  EXPECT_FALSE(second.identity.value().name.empty());
  ASSERT_EQ(Result_state::available, second.residency.state());
  EXPECT_GT(second.residency.value().resident_bytes, 0);
  ASSERT_EQ(static_cast<int>(sizeof(native_task_information)),
            native_task_size);
  EXPECT_EQ(native_task_information.pti_resident_size,
            second.residency.value().resident_bytes);
  EXPECT_EQ(Result_state::unavailable, second.details.state());
  EXPECT_EQ(Result_state::unavailable, second.page_faults.state());
}

TEST(MysysSystemInfo, macOSThreadStateMapping) {
  EXPECT_EQ(Thread_state::running,
            internal::macos_thread_state(TH_STATE_RUNNING));
  EXPECT_EQ(Thread_state::sleeping,
            internal::macos_thread_state(TH_STATE_WAITING));
  EXPECT_EQ(Thread_state::waiting,
            internal::macos_thread_state(TH_STATE_UNINTERRUPTIBLE));
  EXPECT_EQ(Thread_state::stopped,
            internal::macos_thread_state(TH_STATE_STOPPED));
  EXPECT_EQ(Thread_state::stopped,
            internal::macos_thread_state(TH_STATE_HALTED));
  EXPECT_EQ(Thread_state::unknown, internal::macos_thread_state(-1));
}

TEST(MysysSystemInfo, macOSThreadTimeConversionIsChecked) {
  std::chrono::milliseconds value{-1};
  EXPECT_TRUE(internal::macos_time_value_to_milliseconds(0, 0, value));
  EXPECT_EQ(std::chrono::milliseconds{0}, value);
  EXPECT_TRUE(internal::macos_time_value_to_milliseconds(2, 345678, value));
  EXPECT_EQ(std::chrono::milliseconds{2345}, value);

  const int64_t overflowing_seconds =
      std::numeric_limits<int64_t>::max() / 1000 + 1;
  EXPECT_FALSE(internal::macos_time_value_to_milliseconds(-1, 0, value));
  EXPECT_FALSE(internal::macos_time_value_to_milliseconds(0, -1, value));
  EXPECT_FALSE(internal::macos_time_value_to_milliseconds(0, 1000000, value));
  EXPECT_FALSE(internal::macos_time_value_to_milliseconds(overflowing_seconds,
                                                          0, value));
}

TEST(MysysSystemInfo, macOSThreadNamesRemainBounded) {
  const char terminated[] = {'w', 'o', 'r', 'k', '\0', 'x'};
  EXPECT_EQ("work", internal::copy_bounded_macos_thread_name(
                        terminated, sizeof(terminated)));

  const char full[] = {'f', 'u', 'l', 'l'};
  EXPECT_EQ("full",
            internal::copy_bounded_macos_thread_name(full, sizeof(full)));

  const char empty[] = {'\0'};
  EXPECT_TRUE(
      internal::copy_bounded_macos_thread_name(empty, sizeof(empty)).empty());
}

TEST(MysysSystemInfo, macOSThreadSamplesRemainAlignedAndDoNotRetainState) {
  std::vector<internal::macOS_thread_sample> samples{
      {internal::macOS_thread_sample_state::complete, 20, "twenty",
       TH_STATE_WAITING, 2, 3000, 4, 5000},
      {internal::macOS_thread_sample_state::disappeared, 15, "gone",
       TH_STATE_RUNNING, 1, 0, 1, 0},
      {internal::macOS_thread_sample_state::complete, 10, "", TH_STATE_RUNNING,
       0, 0, 0, 0}};
  Thread_inventory inventory;
  Thread_cpu_snapshot cpu;
  ASSERT_FALSE(internal::map_macos_thread_samples(std::move(samples), {},
                                                  inventory, cpu));
  ASSERT_EQ(2U, inventory.threads.size());
  ASSERT_EQ(inventory.threads.size(), cpu.threads.size());
  EXPECT_EQ(10U, inventory.threads[0].tid);
  EXPECT_TRUE(inventory.threads[0].name.empty());
  EXPECT_EQ(Thread_state::running, inventory.threads[0].state);
  EXPECT_EQ(std::chrono::milliseconds{0}, cpu.threads[0].user);
  EXPECT_EQ(std::chrono::milliseconds{0}, cpu.threads[0].system);
  EXPECT_EQ(20U, inventory.threads[1].tid);
  EXPECT_EQ(Thread_state::sleeping, inventory.threads[1].state);
  EXPECT_EQ(std::chrono::milliseconds{2003}, cpu.threads[1].user);
  EXPECT_EQ(std::chrono::milliseconds{4005}, cpu.threads[1].system);

  ASSERT_FALSE(internal::map_macos_thread_samples(
      {{internal::macOS_thread_sample_state::complete, 30, "new",
        TH_STATE_HALTED, 0, 0, 0, 0}},
      {}, inventory, cpu));
  ASSERT_EQ(1U, inventory.threads.size());
  ASSERT_EQ(1U, cpu.threads.size());
  EXPECT_EQ(30U, inventory.threads[0].tid);
  EXPECT_EQ(30U, cpu.threads[0].tid);
}

TEST(MysysSystemInfo, macOSThreadSamplesRejectInvalidAlignedRows) {
  Thread_inventory inventory;
  Thread_cpu_snapshot cpu;
  EXPECT_EQ(std::make_error_code(std::errc::invalid_argument),
            internal::map_macos_thread_samples(
                {{internal::macOS_thread_sample_state::complete, 0, "zero",
                  TH_STATE_RUNNING, 0, 0, 0, 0}},
                {}, inventory, cpu));
  EXPECT_EQ(std::make_error_code(std::errc::invalid_argument),
            internal::map_macos_thread_samples(
                {{internal::macOS_thread_sample_state::complete, 1, "one",
                  TH_STATE_RUNNING, 0, 0, 0, 0},
                 {internal::macOS_thread_sample_state::complete, 1, "duplicate",
                  TH_STATE_WAITING, 0, 0, 0, 0}},
                {}, inventory, cpu));
  EXPECT_EQ(std::make_error_code(std::errc::invalid_argument),
            internal::map_macos_thread_samples(
                {{internal::macOS_thread_sample_state::complete, 1,
                  "invalid-time", TH_STATE_RUNNING, 0, 1000000, 0, 0}},
                {}, inventory, cpu));
  const auto disappeared = internal::mach_error_code(KERN_TERMINATED);
  EXPECT_EQ(disappeared, internal::map_macos_thread_samples(
                             {{internal::macOS_thread_sample_state::disappeared,
                               1, "gone", TH_STATE_RUNNING, 0, 0, 0, 0}},
                             disappeared, inventory, cpu));
  EXPECT_TRUE(inventory.threads.empty());
  EXPECT_TRUE(cpu.threads.empty());
}

TEST(MysysSystemInfo, macOSDisappearanceDoesNotMaskInvalidSurvivingRow) {
  const auto disappeared = internal::mach_error_code(KERN_TERMINATED);
  const int64_t overflowing_seconds =
      std::numeric_limits<int64_t>::max() / 1000 + 1;
  const std::vector<std::vector<internal::macOS_thread_sample>> invalid_rows{
      {{internal::macOS_thread_sample_state::complete, 0, "zero",
        TH_STATE_RUNNING, 0, 0, 0, 0}},
      {{internal::macOS_thread_sample_state::complete, 1, "one",
        TH_STATE_RUNNING, 0, 0, 0, 0},
       {internal::macOS_thread_sample_state::complete, 1, "duplicate",
        TH_STATE_RUNNING, 0, 0, 0, 0}},
      {{internal::macOS_thread_sample_state::complete, 2, "invalid-time",
        TH_STATE_RUNNING, 0, 1000000, 0, 0}},
      {{internal::macOS_thread_sample_state::complete, 3, "overflow",
        TH_STATE_RUNNING, overflowing_seconds, 0, 0, 0}}};
  for (auto invalid : invalid_rows) {
    invalid.push_back({internal::macOS_thread_sample_state::disappeared, 9,
                       "gone", TH_STATE_RUNNING, 0, 0, 0, 0});
    Thread_inventory inventory;
    Thread_cpu_snapshot cpu;
    EXPECT_EQ(std::make_error_code(std::errc::invalid_argument),
              internal::map_macos_thread_samples(std::move(invalid),
                                                 disappeared, inventory, cpu));
  }
}

TEST(MysysSystemInfo, macOSThreadResourcesReleaseExactlyOnce) {
  thread_t threads[]{11, 22, 33};
  Fake_macos_thread_deallocator fake;
  internal::macOS_thread_resources resources{
      threads, static_cast<mach_msg_type_number_t>(std::size(threads)),
      fake_macos_deallocator(fake)};

  EXPECT_FALSE(resources.release());
  EXPECT_EQ((std::vector<mach_port_t>{11, 22, 33}), fake.ports);
  EXPECT_EQ(1U, fake.memory_calls);
  EXPECT_EQ(reinterpret_cast<vm_address_t>(threads), fake.memory_address);
  EXPECT_EQ(sizeof(threads), fake.memory_size);

  EXPECT_FALSE(resources.release());
  EXPECT_EQ(3U, fake.ports.size());
  EXPECT_EQ(1U, fake.memory_calls);
}

TEST(MysysSystemInfo, macOSThreadCleanupFailuresBlockPublication) {
  for (const bool fail_port : {true, false}) {
    thread_t threads[]{41, 42};
    Fake_macos_thread_deallocator fake;
    fake.port_result = fail_port ? KERN_FAILURE : KERN_SUCCESS;
    fake.memory_result = fail_port ? KERN_SUCCESS : KERN_FAILURE;
    internal::macOS_thread_resources resources{
        threads, static_cast<mach_msg_type_number_t>(std::size(threads)),
        fake_macos_deallocator(fake)};
    Process_threads_snapshot result;

    internal::complete_macos_thread_collection(
        resources, {}, {{{41, "one", Thread_state::running}}},
        {{{41, std::chrono::milliseconds{1}, std::chrono::milliseconds{2}}}},
        result);

    ASSERT_EQ(Result_state::failed, result.threads.state());
    ASSERT_EQ(Result_state::failed, result.cpu.state());
    EXPECT_EQ(internal::mach_error_code(KERN_FAILURE), result.threads.error());
    EXPECT_EQ(result.threads.error(), result.cpu.error());
    EXPECT_EQ((std::vector<mach_port_t>{41, 42}), fake.ports);
    EXPECT_EQ(1U, fake.memory_calls);

    EXPECT_EQ(result.threads.error(), resources.release());
    EXPECT_EQ(2U, fake.ports.size());
    EXPECT_EQ(1U, fake.memory_calls);
  }
}

TEST(MysysSystemInfo, ProcessThreadsQueryReturnsmacOSCapabilities) {
  const auto names_before = current_mach_port_name_count();
  ASSERT_TRUE(names_before.has_value());
  for (int iteration = 0; iteration < 32; ++iteration) {
    const auto result = query_process_threads();

    ASSERT_EQ(Result_state::available, result.identity.state());
    EXPECT_EQ(static_cast<uint64_t>(getpid()), result.identity.value().pid);
    EXPECT_FALSE(result.identity.value().name.empty());
    ASSERT_EQ(Result_state::available, result.threads.state());
    ASSERT_EQ(Result_state::available, result.cpu.state());
    ASSERT_FALSE(result.threads.value().threads.empty());
    ASSERT_EQ(result.threads.value().threads.size(),
              result.cpu.value().threads.size());
    std::unordered_set<uint64_t> tids;
    for (size_t index = 0; index < result.threads.value().threads.size();
         ++index) {
      const auto tid = result.threads.value().threads[index].tid;
      EXPECT_NE(0U, tid);
      EXPECT_TRUE(tids.insert(tid).second);
      EXPECT_EQ(tid, result.cpu.value().threads[index].tid);
      EXPECT_GE(result.cpu.value().threads[index].user.count(), 0);
      EXPECT_GE(result.cpu.value().threads[index].system.count(), 0);
    }
    EXPECT_EQ(Result_state::unavailable, result.runtime.state());
    EXPECT_EQ(Result_state::unavailable, result.extended_cpu.state());
    EXPECT_EQ(Result_state::unavailable, result.scheduler.state());
    EXPECT_EQ(Result_state::unavailable, result.io.state());
  }
  const auto names_after = current_mach_port_name_count();
  ASSERT_TRUE(names_after.has_value());
  EXPECT_EQ(*names_before, *names_after);
}

TEST(MysysSystemInfo, macOSShortLivedThreadDoesNotBreakAlignment) {
  for (int iteration = 0; iteration < 32; ++iteration) {
    std::thread short_lived([] {});
    const auto result = query_process_threads();
    short_lived.join();

    ASSERT_EQ(Result_state::available, result.threads.state());
    ASSERT_EQ(Result_state::available, result.cpu.state());
    ASSERT_EQ(result.threads.value().threads.size(),
              result.cpu.value().threads.size());
    for (size_t index = 0; index < result.threads.value().threads.size();
         ++index) {
      EXPECT_EQ(result.threads.value().threads[index].tid,
                result.cpu.value().threads[index].tid);
    }
  }
}

TEST(MysysSystemInfo, macOSUnsupportedThreadCapabilitiesRemainUnavailable) {
  const auto result = query_process_threads();

  EXPECT_EQ(Result_state::unavailable, result.runtime.state());
  EXPECT_EQ(Result_state::unavailable, result.extended_cpu.state());
  EXPECT_EQ(Result_state::unavailable, result.scheduler.state());
  EXPECT_EQ(Result_state::unavailable, result.io.state());
  EXPECT_FALSE(result.runtime.has_value());
  EXPECT_FALSE(result.extended_cpu.has_value());
  EXPECT_FALSE(result.scheduler.has_value());
  EXPECT_FALSE(result.io.has_value());
}
#endif
#endif

}  // namespace
}  // namespace mysql::system_info
