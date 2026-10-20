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

#include "components/library_mysys/my_system_api/system_info_linux_procfs.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "components/library_mysys/my_system_api/system_info_platform.h"
#include "components/library_mysys/my_system_api/system_info_utils.h"

// clang-format off
namespace mysql::system_info::internal {
namespace {

/**
  Linux metadata read by this implementation.

  Source                                  Contents
  --------------------------------------  -----------------------------------
  /proc/partitions                        Block device identities
  `/sys/class/block/<device>`             Device type and parent relationship
  `/sys/class/block/<device>/size`        Capacity in 512-byte sectors
  queue/hw_sector_size                    Hardware sector size in bytes
  /proc/diskstats                         Block device I/O counters
  /proc/self/mountinfo                    Filesystem and mount identity
  /proc/meminfo                           Host memory and swap capacity
  /proc/vmstat                            Host paging activity
  /proc/stat                              Aggregate and per-CPU times
  /proc/self/status                       Process identity and memory use
  `/proc/self/task/<tid>/stat`            Thread state, CPU and runtime data
  `/proc/self/task/<tid>/io`              Thread I/O counters

  The readers validate and convert these text records into system
  information snapshots.
*/

constexpr uint64_t k_bytes_per_kibibyte{1024};
constexpr uint64_t k_bytes_per_diskstats_sector{512};

struct Parsed_value {
  std::optional<uint64_t> value;
  bool invalid{false};
};

struct Parsed_text {
  std::optional<std::string> value;
  bool invalid{false};
};

[[nodiscard]] bool convert_kibibytes(std::string_view text, uint64_t &bytes) {
  uint64_t kibibytes{0};
  const auto conversion =
      std::from_chars(text.data(), text.data() + text.size(), kibibytes);
  if (conversion.ec != std::errc{} ||
      conversion.ptr != text.data() + text.size() ||
      kibibytes > std::numeric_limits<uint64_t>::max() / k_bytes_per_kibibyte) {
    return false;
  }
  bytes = kibibytes * k_bytes_per_kibibyte;
  return true;
}

void assign_value(Parsed_value &field, std::string_view text) {
  uint64_t bytes{0};
  if (field.value || field.invalid || !convert_kibibytes(text, bytes)) {
    field.value.reset();
    field.invalid = true;
    return;
  }
  field.value = bytes;
}

[[nodiscard]] std::string_view trim(std::string_view text) {
  const auto whitespace = [](unsigned char ch) {
    return std::isspace(ch) != 0;
  };
  while (!text.empty() && whitespace(text.front())) text.remove_prefix(1);
  while (!text.empty() && whitespace(text.back())) text.remove_suffix(1);
  return text;
}

void assign_integer(Parsed_value &field, std::string_view text) {
  text = trim(text);
  uint64_t value{0};
  const auto conversion = std::from_chars(text.data(), text.data() + text.size(), value);
  if (field.value || field.invalid || conversion.ec != std::errc{} ||
      conversion.ptr != text.data() + text.size()) {
    field.value.reset();
    field.invalid = true;
    return;
  }
  field.value = value;
}

void assign_text(Parsed_text &field, std::string_view text) {
  text = trim(text);
  if (field.value || field.invalid || text.empty()) {
    field.value.reset();
    field.invalid = true;
    return;
  }
  field.value = std::string{text};
}

void assign_status_memory(Parsed_value &field, std::string_view text) {
  std::istringstream fields{std::string{text}};
  std::string value;
  std::string unit;
  std::string extra;
  if (!(fields >> value >> unit) || unit != "kB" || fields >> extra) {
    field.value.reset();
    field.invalid = true;
    return;
  }
  assign_value(field, value);
}

[[nodiscard]] std::error_code invalid_input() {
  return std::make_error_code(std::errc::invalid_argument);
}

[[nodiscard]] bool is_decimal(std::string_view value) {
  return !value.empty() &&
         std::all_of(value.begin(), value.end(),
                     [](unsigned char ch) { return std::isdigit(ch) != 0; });
}

/**
  Decode one escaped field from /proc/self/mountinfo.

  Mountinfo separates fields with whitespace. Spaces, tabs newlines and
  backslashes are encoded as three-digit octal escapes such as \040, \011, \012
  and \134. Every backslash must start a complete octal escape.

  Return std::nullopt if an escape is incomplete or contains a non-octal digit.
*/
[[nodiscard]] std::optional<std::string> decode_mount_field(
    std::string_view field) {
  std::string decoded;
  decoded.reserve(field.size());
  for (size_t i = 0; i < field.size(); ++i) {
    if (field[i] != '\\') {
      decoded.push_back(field[i]);
      continue;
    }
    if (i + 3 >= field.size() || field[i + 1] < '0' || field[i + 1] > '7' ||
        field[i + 2] < '0' || field[i + 2] > '7' || field[i + 3] < '0' ||
        field[i + 3] > '7') {
      return std::nullopt;
    }
    const unsigned int value =
        static_cast<unsigned int>(field[i + 1] - '0') * 64U +
        static_cast<unsigned int>(field[i + 2] - '0') * 8U +
        static_cast<unsigned int>(field[i + 3] - '0');
    decoded.push_back(static_cast<char>(value));
    i += 3;
  }
  return decoded;
}

[[nodiscard]] bool contains_path(std::string_view mount_point,
                                 std::string_view target_path) {
  if (mount_point == "/") return !target_path.empty() && target_path[0] == '/';
  return target_path == mount_point ||
         (target_path.size() > mount_point.size() &&
          target_path.compare(0, mount_point.size(), mount_point) == 0 &&
          target_path[mount_point.size()] == '/');
}

[[nodiscard]] bool is_cpu_label(std::string_view label) {
  return label == "cpu" ||
         (label.size() > 3 && label.starts_with("cpu") &&
          std::all_of(label.begin() + 3, label.end(),
                      [](unsigned char ch) { return std::isdigit(ch) != 0; }));
}

[[nodiscard]] bool parse_cpu_time(std::string_view text,
                                  uint64_t ticks_per_second,
                                  std::chrono::milliseconds &time) {
  uint64_t ticks{0};
  const auto conversion = std::from_chars(text.data(), text.data() + text.size(), ticks);
  return conversion.ec == std::errc{} &&
         conversion.ptr == text.data() + text.size() &&
         !cpu_ticks_to_milliseconds(ticks, ticks_per_second, time);
}

[[nodiscard]] bool parse_unsigned(std::string_view text, uint64_t &value) {
  const auto conversion = std::from_chars(text.data(), text.data() + text.size(), value);
  return conversion.ec == std::errc{} &&
         conversion.ptr == text.data() + text.size();
}

[[nodiscard]] bool parse_unsigned(std::string_view text, uint32_t &value) {
  const auto conversion = std::from_chars(text.data(), text.data() + text.size(), value);
  return conversion.ec == std::errc{} &&
         conversion.ptr == text.data() + text.size();
}

[[nodiscard]] Thread_state thread_state(char state) {
  switch (state) {
    case 'R':
      return Thread_state::running;
    case 'S':
      return Thread_state::sleeping;
    case 'D':
      return Thread_state::waiting;
    case 'T':
    case 't':
      return Thread_state::stopped;
    case 'Z':
      return Thread_state::zombie;
    default:
      return Thread_state::unknown;
  }
}

template <typename Value>
[[nodiscard]] System_result<Value> failed_result() {
  return System_result<Value>::failure(invalid_input());
}

[[nodiscard]] std::error_code storage_io_error() {
  auto error = errno_code();
  if (!error) error = std::make_error_code(std::errc::io_error);
  return error;
}

struct Linux_block_metadata {
  std::filesystem::path class_path;     // /sys/class/block entry
  std::filesystem::path canonical_path; // resolved target of the class link
  bool partition{false};
};

}  // namespace

// A valid Linux storage device name is one nonempty path component. It can
// contain device name punctuation but not `/`, a null byte, `.` or `..`.
bool valid_linux_storage_device_name(std::string_view name) {
  return !name.empty() && name != "." && name != ".." &&
         name.find('/') == std::string_view::npos &&
         name.find('\0') == std::string_view::npos;
}

/**
  Read the block device identity table from /proc/partitions.

  After the optional header, each row has four fields:
     1  Major number
     2  Minor number
     3  Blocks
     4  Device name

  Major:Minor - kernel device number
  Device Name - sysfs device name
  Blocks      - capacity in 1024-byte blocks, not used here because
                `/sys/class/block/<name>/size` preserves the 512-byte sector
                count

  See https://man7.org/linux/man-pages/man5/proc_partitions.5.html
*/
System_result<std::vector<Linux_block_device>> read_linux_partitions(
    std::istream &input) {
  std::vector<Linux_block_device> devices;
  // Device names and major:minor numbers must each be unique across the
  // block device inventory, including disks and partitions.
  std::unordered_set<std::string> names;
  std::unordered_set<std::string> numbers;
  // Continue after an invalid row to consume the input, but reject the full
  // table rather than return a partial inventory.
  bool invalid = false;
  std::string line;

  // Read and process one `/proc/partitions` row at a time until EOF or a read
  // error. Ignore blank lines.
  while (std::getline(input, line)) {
    const auto content = trim(line);
    if (content.empty()) continue;

    std::istringstream line_input{std::string{content}};
    std::vector<std::string> fields;
    for (std::string field; line_input >> field;) fields.push_back(field);
    // Skip the optional column header when it has the expected complete form.
    if (fields.size() == 4 && fields[0] == "major" && fields[1] == "minor" &&
        fields[2] == "#blocks" && fields[3] == "name") {
      continue;
    }
    uint64_t major{0};
    uint64_t minor{0};
    uint64_t blocks{0};
    // Parse `blocks` to validate the row even though capacity is read from
    // sysfs and this value is not copied to Linux_block_device.
    if (fields.size() != 4 || !parse_unsigned(fields[0], major) ||
        !parse_unsigned(fields[1], minor) ||
        !parse_unsigned(fields[2], blocks) ||
        !valid_linux_storage_device_name(fields[3])) {
      invalid = true;
      continue;
    }
    const std::string number = std::to_string(major) + ":" + std::to_string(minor);
    // Reject a duplicate name or number because the device identity is no
    // longer one-to-one.
    if (!names.insert(fields[3]).second || !numbers.insert(number).second) {
      invalid = true;
      continue;
    }
    devices.push_back({major, minor, std::move(fields[3])});
  }
  if (invalid || devices.empty()) {
    return failed_result<std::vector<Linux_block_device>>();
  }
  // Sort by device name to ensure a deterministic inventory of devices.
  std::sort(devices.begin(), devices.end(),
            [](const auto &left, const auto &right) {
              return left.name < right.name;
            });
  return System_result<std::vector<Linux_block_device>>::success(
      std::move(devices));
}

/**
  Read capacity metadata from `/sys/class/block/<device>`.
  Block device size in sectors.

  size - contains the decimal count of 512-byte sectors. The 512-byte unit is
  fixed by the the sysfs ABI and does not change with the physical sector size.

  queue/hw_sector_size - contains the hardware sector size in bytes. A
  partition uses the queue attribute of its parent disk.

  See https://www.kernel.org/doc/html/latest/admin-guide/abi-stable.html
*/
System_result<Storage_device_capacity> read_linux_storage_capacity(
    std::istream &size_input, std::istream &sector_size_input,
    std::string_view device) {
  std::string size_text;
  std::string sector_size_text;
  if (!valid_linux_storage_device_name(device) ||
      !std::getline(size_input, size_text) ||
      !std::getline(sector_size_input, sector_size_text)) {
    return failed_result<Storage_device_capacity>();
  }
  uint64_t sectors{0};
  uint64_t sector_size{0};
  if (!parse_unsigned(trim(size_text), sectors) ||
      !parse_unsigned(trim(sector_size_text), sector_size) ||
      sector_size == 0 ||
      sectors >
          std::numeric_limits<uint64_t>::max() / k_bytes_per_diskstats_sector) {
    return failed_result<Storage_device_capacity>();
  }
  size_input >> std::ws;
  sector_size_input >> std::ws;
  if (!size_input.eof() || !sector_size_input.eof()) {
    return failed_result<Storage_device_capacity>();
  }
  return System_result<Storage_device_capacity>::success(
      {std::string{device}, sectors * k_bytes_per_diskstats_sector,
       sector_size});
}

/**
  Find the parent disk of a Linux partition from its canonical sysfs path.

  `/sys/class/block/<device>` is normally a symbolic link. A canonical path
  resolves symbolic links and removes `.` and `..` elements.

  The resolved path places a partition directly below its parent disk.
  For example, a path ending in `/sda/sda1` identifies `sda1` as a partition of
  `sda`.

  Return a failed result if the device name or path is invalid, or if the path
  does not identify a different parent device.
*/
System_result<Storage_device_relationship> linux_partition_relationship(
    std::string_view canonical_path, std::string_view device) {
  if (!valid_linux_storage_device_name(device) || canonical_path.empty()) {
    return failed_result<Storage_device_relationship>();
  }
  const std::filesystem::path path{canonical_path};
  const std::string parent = path.parent_path().filename().string();
  if (!valid_linux_storage_device_name(parent) || parent == device) {
    return failed_result<Storage_device_relationship>();
  }
  return System_result<Storage_device_relationship>::success(
      {std::string{device}, parent});
}

/**
  Build the sysfs path for a device's hardware sector size.

  A non-partition block device (aka whole device) exposes this value in
  `queue/hw_sector_size` below its sysfs directory.
  A partition has no separate queue directory, so the path uses the parent
  disk's directory before appending `queue/hw_sector_size`.

  Return a failed result if the canonical path is empty or does not end in a
  valid device name.
*/
System_result<std::string> linux_storage_sector_size_path(
    std::string_view canonical_path, bool partition) {
  if (canonical_path.empty()) return failed_result<std::string>();
  std::filesystem::path device_path{canonical_path};
  if (partition) device_path = device_path.parent_path();
  if (!valid_linux_storage_device_name(device_path.filename().string())) {
    return failed_result<std::string>();
  }
  return System_result<std::string>::success(
      (device_path / "queue" / "hw_sector_size").generic_string());
}

/**
  Read cumulative block I/O counters from /proc/diskstats.
  
  Read, write and flush counters for each block device. Fields in order.
  Sectors converted to bytes.
     1  Major number
     2  Minor number
     3  Device name
     4  Reads completed successfully
        ...
     6  Sectors read
     7  Time spent reading (ms)
     8  Writes completed
        ...
    10  Sectors written
    11  Time spent writing (ms)
        ...
    19  Flush requests completed successfully
    20  Time spent flushing
  See https://docs.kernel.org/admin-guide/iostats.html

  Each row starts with `major`, `minor` and `name`. The fields used here are:

      4  reads completed       8  writes completed
      6  sectors read         10  sectors written
      7  read time (ms)       11  write time (ms)
     19  flushes completed    20  flush time (ms)

  - Fields 5, 9 and 12-18 are are not part of this API.
  - Sector counters use a fixed 512-byte unit and are converted to bytes.
  - Flush fields apply only to whole devices, not partitions.
*/
Linux_diskstats_snapshot read_linux_diskstats(
    std::istream &input, const std::unordered_set<std::string> &whole_devices) {
  Linux_diskstats_snapshot result;
  Storage_read_write_snapshot read_write;
  Storage_flush_snapshot flush;
  std::unordered_set<std::string> names;
  std::unordered_set<std::string> numbers;
  // Track the `read_write` and `flush` capabilities separately. Invalid device
  // identity data affects both because every counter must belong to one device.
  bool identity_valid = true;
  bool read_write_valid = true;
  bool flush_valid = true;
  bool saw_row = false;
  bool saw_flush = false;
  std::string line;
  // Read and process one block device record at a time until EOF or an input
  // error. Blank lines are ignored.
  while (std::getline(input, line)) {
    const auto content = trim(line);
    if (content.empty()) continue;
    saw_row = true;
    // Assemble major:minor device identifier.
    std::istringstream line_input{std::string{content}};
    std::vector<std::string> fields;
    for (std::string field; line_input >> field;) fields.push_back(field);
    uint64_t major{0};
    uint64_t minor{0};
    if (fields.size() < 3 || !parse_unsigned(fields[0], major) ||
        !parse_unsigned(fields[1], minor) ||
        !valid_linux_storage_device_name(fields[2])) {
      identity_valid = false;
      continue;
    }
    const std::string number = std::to_string(major) + ":" + std::to_string(minor);
    // A device name and a major:minor number must each identify only one row.
    if (!names.insert(fields[2]).second || !numbers.insert(number).second) {
      identity_valid = false;
      continue;
    }

    std::vector<uint64_t> statistics;
    statistics.reserve(std::min<size_t>(fields.size() - 3, 17));
    // Parse the 11 counters in the base diskstats format. Some are skipped
    // when the snapshot is built, but parsing all of them preserves the field
    // positions and detects malformed records.
    bool required_valid = fields.size() >= 14;
    for (size_t field = 3; required_valid && field < 14; ++field) {
      uint64_t value{0};
      if (!parse_unsigned(fields[field], value)) {
        required_valid = false;
      } else {
        statistics.push_back(value);
      }
    }
    if (required_valid) {
      // Validate sector-to-byte multiplication and the millisecond range
      // before constructing the API values.
      const auto maximum_milliseconds =
          static_cast<uint64_t>(std::chrono::milliseconds::max().count());
      required_valid = statistics[2] <= std::numeric_limits<uint64_t>::max() /
                                            k_bytes_per_diskstats_sector &&
                       statistics[6] <= std::numeric_limits<uint64_t>::max() /
                                            k_bytes_per_diskstats_sector &&
                       statistics[3] <= maximum_milliseconds &&
                       statistics[7] <= maximum_milliseconds;
    }
    // Add the device's read and write counters. Convert the 512-byte sector
    // counts to bytes in fields 6 and 10. Fields 7 and 11 are already
    // milliseconds.
    if (required_valid) {
      read_write.devices.push_back(
          {fields[2], statistics[0],
           statistics[2] * k_bytes_per_diskstats_sector,
           std::chrono::milliseconds{statistics[3]}, statistics[4],
           statistics[6] * k_bytes_per_diskstats_sector,
           std::chrono::milliseconds{statistics[7]}});
    } else {
      read_write_valid = false;
    }

    // Flush counters belong only to non-partition devices. The set comes from
    // sysfs, where partitions are identified by a `partition` attribute.
    const bool is_whole_device = whole_devices.contains(fields[2]);
    if (is_whole_device && fields.size() >= 20) {
      saw_flush = true;
      uint64_t flush_count{0};
      uint64_t flush_time{0};
      const auto maximum_milliseconds =
          static_cast<uint64_t>(std::chrono::milliseconds::max().count());
      if (!parse_unsigned(fields[18], flush_count) ||
          !parse_unsigned(fields[19], flush_time) ||
          flush_time > maximum_milliseconds) {
        flush_valid = false;
      } else {
        flush.devices.push_back(
            {fields[2], flush_count, std::chrono::milliseconds{flush_time}});
      }
    } else if (is_whole_device && fields.size() == 19) {
      // A 19-field row has a flush count but no paired flush time, so `flush`
      // fails. A shorter row has neither field and leaves `flush` unavailable.
      saw_flush = true;
      flush_valid = false;
    }
  }

  // Sort both `read_write` and `flush` by device name to ensure a deterministic
  // snapshot.
  const auto by_device = [](const auto &left, const auto &right) {
    return left.device < right.device;
  };
  std::sort(read_write.devices.begin(), read_write.devices.end(), by_device);
  std::sort(flush.devices.begin(), flush.devices.end(), by_device);
  if (!saw_row || !identity_valid || !read_write_valid || read_write.devices.empty()) {
    result.read_write = failed_result<Storage_read_write_snapshot>();
  } else {
    result.read_write = System_result<Storage_read_write_snapshot>::success(
        std::move(read_write));
  }
  // If no non-partition device has flush fields, then the `flush` capability is
  // unavailable. If the flush fields are incomplete or invalid, then the
  // `flush` capability fails.
  // An invalid device identity fails both `read_write` and `flush`.
  if (!identity_valid || (saw_flush && !flush_valid)) {
    result.flush = failed_result<Storage_flush_snapshot>();
  } else if (!saw_flush || flush.devices.empty()) {
    result.flush = System_result<Storage_flush_snapshot>::unavailable();
  } else {
    result.flush = System_result<Storage_flush_snapshot>::success(std::move(flush));
  }
  return result;
}

/**
  Assemble Linux storage metadata from procfs and sysfs.

  How the snapshot is assembled:
  - Device names and major:minor device numbers come from /proc/partitions.
  - Entries in /sys/class/block are joined by device name.
  - A partition attribute marks a partition, and the sysfs path places it below
    its parent disk.
  - The size and hw_sector_size attributes provide capacity metadata.
  - /proc/diskstats is joined by device name to add I/O counters.

  Devices can disappear while these files are read. An entry that vanishes is
  omitted. Invalid metadata or an I/O error causes the capability to fail.
*/
Storage_snapshot query_linux_storage_devices(
    const Linux_storage_paths &paths,
    const std::function<void()> &after_metadata) {
  Storage_snapshot result;
  const auto invalid = invalid_input();

  // Index current sysfs block entries by device name. `whole_devices` records
  // non-partition devices because only they can provide `flush` counters.
  std::unordered_map<std::string, Linux_block_metadata> metadata;
  std::unordered_set<std::string> whole_devices;
  std::error_code metadata_error;
  std::filesystem::directory_iterator entries{paths.sys_class_block,
                                              metadata_error};
  for (std::filesystem::directory_iterator end;
       !metadata_error && entries != end; entries.increment(metadata_error)) {
    const std::string name = entries->path().filename().string();
    if (!valid_linux_storage_device_name(name)) {
      metadata_error = invalid;
      break;
    }
    // Resolve the class link so a partition's canonical path identifies its
    // parent disk. A missing target means the device disappeared.
    std::error_code canonical_error;
    auto canonical_path = std::filesystem::canonical(entries->path(), canonical_error);
    if (canonical_error == std::errc::no_such_file_or_directory) continue;
    if (canonical_error) {
      metadata_error = canonical_error;
      break;
    }
    std::error_code partition_error;
    const bool partition = std::filesystem::exists(entries->path() / "partition", partition_error);
    if (partition_error == std::errc::no_such_file_or_directory) continue;
    if (partition_error) {
      metadata_error = partition_error;
      break;
    }
    metadata.emplace(
        name, Linux_block_metadata{entries->path(), std::move(canonical_path),
                                   partition});
    if (!partition) whole_devices.insert(name);
  }
  // Tests can use this callback to remove a device after sysfs enumeration and
  // exercise the device removal race checks below. Production callers leave it
  // empty.
  if (after_metadata) after_metadata();

  // `inventory`, `capacities` and `hierarchy` require both the partitions
  // table and the sysfs index. A source failure therefore fails all three.
  errno = 0;
  std::ifstream partitions{paths.proc_partitions};
  if (!partitions.is_open()) {
    const auto error = storage_io_error();
    result.inventory = System_result<Storage_inventory>::failure(error);
    result.capacities = System_result<Storage_capacities>::failure(error);
    result.hierarchy = System_result<Storage_hierarchy>::failure(error);
  } else {
    auto parsed_partitions = read_linux_partitions(partitions);
    if (partitions.bad()) {
      const auto error = storage_io_error();
      result.inventory = System_result<Storage_inventory>::failure(error);
      result.capacities = System_result<Storage_capacities>::failure(error);
      result.hierarchy = System_result<Storage_hierarchy>::failure(error);
    } else if (!parsed_partitions.has_value()) {
      result.inventory = System_result<Storage_inventory>::failure(invalid);
      result.capacities = System_result<Storage_capacities>::failure(invalid);
      result.hierarchy = System_result<Storage_hierarchy>::failure(invalid);
    } else if (metadata_error) {
      result.inventory = System_result<Storage_inventory>::failure(metadata_error);
      result.capacities = System_result<Storage_capacities>::failure(metadata_error);
      result.hierarchy = System_result<Storage_hierarchy>::failure(metadata_error);
    } else {
      Storage_inventory inventory;
      Storage_capacities capacities;
      Storage_hierarchy hierarchy;
      bool capacities_valid = true;
      bool hierarchy_valid = true;
      std::error_code inventory_error;
      std::error_code capacity_error;
      std::unordered_set<std::string> retained_devices;

      // Build the snapshot from devices present in both /proc/partitions and
      // /sys/class/block. A device present in only one source is omitted.
      for (const auto &device : parsed_partitions.value()) {
        const auto found = metadata.find(device.name);
        if (found == metadata.end()) continue;
        const auto &device_metadata = found->second;
        std::error_code exists_error;
        // Recheck the class entry because it can disappear after enumeration.
        if (!std::filesystem::exists(device_metadata.class_path, exists_error)) {
          if (exists_error &&
              exists_error != std::errc::no_such_file_or_directory) {
            if (!inventory_error) inventory_error = exists_error;
          }
          continue;
        }

        System_result<Storage_device_relationship> relationship;
        if (device_metadata.partition) {
          relationship = linux_partition_relationship(
              device_metadata.canonical_path.generic_string(), device.name);
          if (!relationship.has_value()) hierarchy_valid = false;
        }
        // Partitions use the parent disk's queue when locating hw_sector_size.
        auto sector_path = linux_storage_sector_size_path(
            device_metadata.canonical_path.generic_string(),
            device_metadata.partition);
        // Read capacity from `size`, which uses 512-byte sectors. Store
        // `hw_sector_size` separately as the hardware sector size.
        errno = 0;
        std::ifstream size{device_metadata.class_path / "size"};
        const auto size_error = size.is_open() ? std::error_code{} : storage_io_error();
        errno = 0;
        std::ifstream sector_size;
        if (sector_path.has_value()) sector_size.open(sector_path.value());
        const auto sector_error =
            sector_size.is_open() ? std::error_code{}
                                  : (sector_path.state() == Result_state::failed
                                         ? sector_path.error()
                                         : storage_io_error());
        System_result<Storage_device_capacity> capacity;
        if (size_error || sector_error) {
          const auto error = size_error ? size_error : sector_error;
          // ENOENT may mean that the device disappeared between enumeration and
          // the metadata read, so recheck its class entry.
          // If the device is gone, omit it.
          // If it still exists, retain it in inventory and record a capacity
          // failure.
          // Because `capacities` is all-or-nothing, one such failure fails that
          // capability.
          if (error == std::errc::no_such_file_or_directory) {
            std::error_code recheck_error;
            if (!std::filesystem::exists(device_metadata.class_path,
                                         recheck_error)) {
              if (!recheck_error ||
                  recheck_error == std::errc::no_such_file_or_directory) {
                continue;
              }
              if (!inventory_error) inventory_error = recheck_error;
              continue;
            }
          }
          capacity = System_result<Storage_device_capacity>::failure(error);
        } else {
          capacity = read_linux_storage_capacity(size, sector_size, device.name);
          if (size.bad() || sector_size.bad()) {
            capacity = System_result<Storage_device_capacity>::failure(storage_io_error());
          } else if (!capacity.has_value()) {
            // A parse failure can also follow removal during the read. Recheck
            // the class entry before treating it as invalid.
            std::error_code recheck_error;
            if (!std::filesystem::exists(device_metadata.class_path, recheck_error)) {
              if (!recheck_error ||
                  recheck_error == std::errc::no_such_file_or_directory) {
                continue;
              }
              if (!inventory_error) inventory_error = recheck_error;
              continue;
            }
          }
        }
        // Retain the device in inventory. A capacity error fails `capacities`,
        // and a partition relationship error fails `hierarchy`. Neither error
        // removes the device from inventory.
        retained_devices.insert(device.name);
        inventory.devices.push_back(
            {device.name, device_metadata.partition
                              ? Storage_device_kind::partition
                              : Storage_device_kind::disk});
        if (capacity.has_value()) {
          capacities.devices.push_back(std::move(capacity).value());
        } else {
          capacities_valid = false;
          if (!capacity_error) {
            capacity_error = capacity.state() == Result_state::failed
                                 ? capacity.error()
                                 : invalid;
          }
        }
        // Add a parent relationship only for partitions. If any relationship
        // cannot be resolved, continue collecting devices but fail the
        // `hierarchy` capability.
        if (device_metadata.partition) {
          if (!relationship.has_value()) {
            hierarchy_valid = false;
          } else {
            hierarchy.relationships.push_back(std::move(relationship).value());
          }
        }
      }

      if (inventory_error) {
        result.inventory = System_result<Storage_inventory>::failure(inventory_error);
        result.capacities = System_result<Storage_capacities>::failure(inventory_error);
        result.hierarchy = System_result<Storage_hierarchy>::failure(inventory_error);
      } else if (inventory.devices.empty()) {
        const auto error = std::make_error_code(std::errc::no_such_device);
        result.inventory = System_result<Storage_inventory>::failure(error);
        result.capacities = System_result<Storage_capacities>::failure(error);
        result.hierarchy = System_result<Storage_hierarchy>::failure(error);
      } else {
        // Every partition relationship must refer to a parent that survived
        // the same collection pass.
        for (const auto &relationship : hierarchy.relationships) {
          if (!retained_devices.contains(relationship.parent_device)) {
            hierarchy_valid = false;
            break;
          }
        }
        // `inventory` can succeed by itself.
        result.inventory = System_result<Storage_inventory>::success(std::move(inventory));
        // `capacity is all-or-nothing.
        result.capacities =
            capacities_valid && capacities.devices.size() == retained_devices.size()
                ? System_result<Storage_capacities>::success(std::move(capacities))
                : System_result<Storage_capacities>::failure(
                      capacity_error ? capacity_error : invalid);
        // `hierarchy` is all-or-nothing.
        result.hierarchy =
            hierarchy_valid
                ? System_result<Storage_hierarchy>::success(std::move(hierarchy))
                : System_result<Storage_hierarchy>::failure(invalid);
      }
    }
  }

  // `read_write` comes directly from diskstats. `flush` also depends on the
  // sysfs classification of a non-partitioned (whole) device.
  errno = 0;
  std::ifstream diskstats{paths.proc_diskstats};
  if (!diskstats.is_open()) {
    const auto error = storage_io_error();
    result.read_write = System_result<Storage_read_write_snapshot>::failure(error);
    result.flush = System_result<Storage_flush_snapshot>::failure(error);
  } else {
    auto statistics = read_linux_diskstats(diskstats, whole_devices);
    if (diskstats.bad()) {
      const auto error = storage_io_error();
      result.read_write = System_result<Storage_read_write_snapshot>::failure(error);
      result.flush = System_result<Storage_flush_snapshot>::failure(error);
    } else {
      // Preserve `read_write` because diskstats contains all data it needs.
      // If sysfs enumeration failed, then fail `flush` because the
      // non-partition devices cannot be identified.
      result.read_write = std::move(statistics.read_write);
      result.flush =
          metadata_error
              ? System_result<Storage_flush_snapshot>::failure(metadata_error)
              : std::move(statistics.flush);
    }
  }
  return result;
}

/**
  Find a filesystem in /proc/self/mountinfo.

  Mount point info for each device. Fields in order.
     1 mount-id      - Mount point ID
     2 parent-id     - Parent mount point ID
     3 device-id     - Device ID "major:minor"
     4 root          - Root path
     5 mount-point   - Mount point path
     6 mount-options - Per-mount options
     7 optional      - Optional fields `tag[:value]`, can be empty
     8 separator     - End of the optional fields `-`
     9 fs_type       - Filesystem type `type[.subtype]`
    10 mount-source  - Filesystem specific or `none`
    11 super-options - Per-superblock options
  See https://man7.org/linux/man-pages/man5/proc_pid_mountinfo.5.html

  Example entries:
    19 43 0:19 / /sys rw,nosuid,nodev,noexec,relatime shared:6 - sysfs sysfs rw
    21 43 0:6 / /dev rw,nosuid shared:2 - devtmpfs devtmpfs rw,size=263331160k,nr_inodes=65832790,mode=755
    129 119 0:54 / /home/arclight rw,relatime shared:105 - nfs4 server:/export/home rw
  Example from WSL2 with empty 'optional' field:
    1076 77 7:0 / /mnt/xfs10 rw,relatime - xfs /dev/loop0 rw,attr2,inode64,logbufs=8,logbsize=32k,noquota

  A mountinfo row starts with these fixed fields:
    mount-id parent-id major:minor root mount-point mount-options

  Zero or more tag[:value] fields follow. A `-` separator introduces
  filesystem-type, mount-source and super-options.
  Path fields escape whitespace and backslashes as octal, e.g. \040 and \134.

  A device can have more than one mount, so the reader selects the matching
  major:minor row whose mount point is the longest prefix of the target path.

  From that, the mount ID, point, source and filesystem type form the identity.
*/
System_result<Filesystem_identity> read_linux_mount_info(
    std::istream &input, std::string_view device_id,
    std::string_view target_path) {
  // The same device can be mounted more than once. Keep the matching mount
  // with the longest mount point path because it is the most specific match.
  std::optional<Filesystem_identity> best;
  size_t best_length = 0;
  // Remember malformed input so a missing match can be reported as invalid
  // data rather than as an absent device.
  bool malformed = false;
  std::string line;

  // Read and process one mountinfo record at a time until EOF or a read error.
  while (std::getline(input, line)) {
    std::istringstream line_input{line};
    std::vector<std::string> fields;
    for (std::string field; line_input >> field;) fields.push_back(field);

    // Every record must contain the 6 fixed fields that precede the optional
    // fields and separator.
    if (fields.size() < 6) {
      malformed = true;
      continue;
    }
    // `st_dev` from the target path selects records for the same major:minor
    // device number.
    if (fields[2] != device_id) continue;
    // Optional fields vary in number. Locate their terminating `-`, then
    // require filesystem type, mount source and superblock options after it.
    const auto separator = std::find(fields.begin(), fields.end(), "-");
    if (separator == fields.end() || separator - fields.begin() < 6 ||
        fields.end() - separator < 4 || !is_decimal(fields[0])) {
      malformed = true;
      continue;
    }
    // Decode fields that become part of `Filesystem_identity`. An invalid
    // octal escape makes the record malformed.
    auto mount_point = decode_mount_field(fields[4]);
    auto mount_source = decode_mount_field(*(separator + 2));
    if (!mount_point || !mount_source) {
      malformed = true;
      continue;
    }
    // Match only the components of the path so that `/data` does not match
    // `/database`. If nested mount points match, keep the longest one.    
    if (!contains_path(*mount_point, target_path) ||
        mount_point->size() < best_length) {
      continue;
    }

    best = Filesystem_identity{fields[0], std::move(*mount_point),
                               std::move(*mount_source), *(separator + 1)};
    best_length = best->mount_point.size();
  }
  if (best) {
    return System_result<Filesystem_identity>::success(std::move(*best));
  }
  // No mountinfo record matched. Return `invalid_argument` if a malformed
  // record was seen. Otherwise, return `no_such_device`.
  return System_result<Filesystem_identity>::failure(std::make_error_code(
      malformed ? std::errc::invalid_argument : std::errc::no_such_device));
}

/**
  Read labeled host memory values from /proc/meminfo.

  System memory stats. Fields are labeled.
    MemTotal     - Total usable RAM
    MemFree      - Sum of LowFree + High Free
    MemAvailable - Memory available for starting an application
    Buffers      - Temporary storage for disk blocks
    Cached       - In-memory page cache
    Slab         - In-kernel data structures cache
    SwapTotal    - Swap space available
    SwapFree     - Unused swap space
    Slab         - In-kernel data structures cache
  See https://man7.org/linux/man-pages/man5/proc_meminfo.5.html

  - The reader uses MemTotal, MemFree, MemAvailable, Buffers, Cached, Slab, SwapTotal and SwapFree.
  - Linux reports these fields in kB which are then converted to bytes.
  - Unknown labels are ignored because the kernel can add fields.
*/
Host_memory_snapshot read_linux_meminfo(std::istream &input) {
  // Each Parsed_value records whether its field is missing, valid or
  // invalid. A duplicate field is invalid rather than replacing the first.
  Parsed_value total;
  Parsed_value free;
  Parsed_value available;
  Parsed_value buffers;
  Parsed_value cached;
  Parsed_value slab;
  Parsed_value swap_total;
  Parsed_value swap_free;

  std::string line;
  // Read one labeled meminfo record at a time until EOF or a read error.
  while (std::getline(input, line)) {
    std::istringstream fields{line};
    std::string name;
    std::string value;
    std::string unit;
    std::string extra;
    fields >> name;
    // Select only fields used by `memory`, `breakdown` or `swap_capacity`.
    // Ignore other fields because the kernel can add or remove them.
    Parsed_value *target = nullptr;
    if (name == "MemTotal:") target = &total;
    if (name == "MemFree:") target = &free;
    if (name == "MemAvailable:") target = &available;
    if (name == "Buffers:") target = &buffers;
    if (name == "Cached:") target = &cached;
    if (name == "Slab:") target = &slab;
    if (name == "SwapTotal:") target = &swap_total;
    if (name == "SwapFree:") target = &swap_free;
    if (target == nullptr) continue;
    // A selected record must contain exactly one decimal value followed by
    // the kernel's `kB` unit.
    if (!(fields >> value >> unit) || unit != "kB" || fields >> extra) {
      target->value.reset();
      target->invalid = true;
      continue;
    }
    // Convert the value from 1024-byte units to bytes. `assign_value` also
    // rejects duplicate fields and conversion overflow.
    assign_value(*target, value);
  }

  Host_memory_snapshot result;
  // `memory` requires all three general memory fields.
  if (!total.invalid && !free.invalid && !available.invalid && total.value &&
      free.value && available.value) {
    result.memory = System_result<Host_memory_info>::success(
        {*total.value, *free.value, *available.value});
  } else {
    result.memory = System_result<Host_memory_info>::failure(invalid_input());
  }
  // `breakdown` is independent of `memory` and requires buffers, cache and
  // slab values.
  if (!buffers.invalid && !cached.invalid && !slab.invalid && buffers.value &&
      cached.value && slab.value) {
    result.breakdown = System_result<Host_memory_breakdown>::success(
        {*buffers.value, *cached.value, *slab.value});
  } else {
    result.breakdown =
        System_result<Host_memory_breakdown>::failure(invalid_input());
  }
  // `swap_capacity` is independent of the other capabilities and requires
  // both total and free swap values.
  if (!swap_total.invalid && !swap_free.invalid && swap_total.value &&
      swap_free.value) {
    result.swap_capacity = System_result<Swap_capacity_info>::success(
        {*swap_total.value, *swap_free.value});
  } else {
    result.swap_capacity = System_result<Swap_capacity_info>::failure(invalid_input());
  }
  return result;
}

/**
  Read cumulative paging activity from /proc/vmstat.

  Virtual memory statistics. Fields used:
    pgpgin  - cumulative KiB paged in from disk
    pgpgout - cumulative KiB paged out to disk

  See https://man7.org/linux/man-pages/man5/proc_vmstat.5.html

  These are paging counters, not rates or swap-specific counters. The
  swap-specific page counters are pswpin and pswpout.

  The reader converts them to bytes, so a caller can subtract successive
  snapshots to calculate interval activity.
*/
System_result<Swap_activity_info> read_linux_vmstat(std::istream &input) {
  Parsed_value bytes_in;
  Parsed_value bytes_out;
  std::string line;
  while (std::getline(input, line)) {
    std::istringstream fields{line};
    std::string name;
    std::string value;
    std::string extra;
    fields >> name;
    Parsed_value *target = nullptr;
    if (name == "pgpgin") target = &bytes_in;
    if (name == "pgpgout") target = &bytes_out;
    if (target == nullptr) continue;
    if (!(fields >> value) || fields >> extra) {
      target->value.reset();
      target->invalid = true;
      continue;
    }
    assign_value(*target, value);
  }
  if (bytes_in.invalid || bytes_out.invalid || !bytes_in.value || !bytes_out.value) {
    return System_result<Swap_activity_info>::failure(invalid_input());
  }
  return System_result<Swap_activity_info>::success(
      {*bytes_in.value, *bytes_out.value});
}

/**
  Read CPU time rows from /proc/stat.

  Kernel system statistics, CPU-specific stats. Fields in order.
  Values converted from clock ticks to milliseconds.
     1 user       - Time in user mode
     2 nice       - Time in niced user mode
     3 system     - Time in system mode
     4 idle       - Time in the idle task
     5 iowait     - Time waiting for I/O to complete
     6 irq        - Time servicing interrupts
     7 softirqs   - Time servicing softirqs
     8 steal      - Stolen time
     9 guest      - Time spent running a virtual CPU
    10 guest_nice - Time spent running a niced guest

  See https://man7.org/linux/man-pages/man5/proc_stat.5.html

  The aggregate row is labeled `cpu`, per-CPU rows are labeled `cpu0`, `cpu1`
  and so on. Values after the label are cumulative clock ticks in this order:

      user nice system idle iowait irq softirq steal guest guest_nice

  - The first four fields form the portable CPU snapshot. The remaining six
    form the Linux extended snapshot.
  - All values are converted to milliseconds with the _SC_CLK_TCK value supplied
    by the caller.
  - Other /proc/stat rows are outside this reader and are ignored.
  - Linux includes guest time in `user` and guest-nice time in `nice`, so the
    reader preserves those kernel counters without subtracting the guest fields.
*/
Host_cpu_snapshot read_linux_cpu_stat(std::istream &input,
                                      uint64_t ticks_per_second) {
  Host_cpu_snapshot result;
  // Both `times` and `extended_times` use the same tick conversion. Reject a
  // zero rate and any rate that could overflow.
  if (ticks_per_second == 0 ||
      ticks_per_second > std::numeric_limits<uint64_t>::max() / 1000U) {
    const auto error = invalid_input();
    result.times = System_result<Cpu_times_snapshot>::failure(error);
    result.extended_times = System_result<Cpu_extended_snapshot>::failure(error);
    return result;
  }

  Cpu_times_snapshot times;
  Cpu_extended_snapshot extended_times;
  // Validate `times` and `extended_times` independently. CPU labels and their
  // ordering are shared by both.
  bool times_valid = true;
  bool extended_valid = true;
  bool labels_valid = true;
  bool first_label = true;
  bool aggregate_first = false;
  std::unordered_set<std::string> labels;
  std::string line;
  // Read one `/proc/stat` row at a time. Ignore system statistics that are not
  // aggregate or per-CPU time records.
  while (std::getline(input, line)) {
    std::istringstream line_input{line};
    std::vector<std::string> fields;
    for (std::string field; line_input >> field;) {
      fields.push_back(std::move(field));
    }
    if (fields.empty() || !fields.front().starts_with("cpu")) continue;

    const std::string &label = fields.front();
    // Accept only `cpu` or `cpu<N>`, and require each label to occur once.
    if (!is_cpu_label(label) || !labels.insert(label).second) {
      labels_valid = false;
      continue;
    }
    // The kernel aggregate `cpu` row must precede the per-CPU rows so it is
    // also the first record returned by each capability.
    if (first_label) {
      aggregate_first = label == "cpu";
      first_label = false;
    }

    // The first four counters form the portable `times` capability. A failure
    // here does not prevent collection of `extended_times`.
    Cpu_times portable;
    portable.cpu = label;
    if (fields.size() < 5 ||
        !parse_cpu_time(fields[1], ticks_per_second, portable.user) ||
        !parse_cpu_time(fields[2], ticks_per_second, portable.nice) ||
        !parse_cpu_time(fields[3], ticks_per_second, portable.system) ||
        !parse_cpu_time(fields[4], ticks_per_second, portable.idle)) {
      times_valid = false;
    } else {
      times.cpus.push_back(std::move(portable));
    }

    // The next six counters form the Linux-specific `extended_times`
    // capability. Older or truncated rows can fail it while `times` succeeds.
    Cpu_extended_times extended;
    extended.cpu = label;
    if (fields.size() < 11 ||
        !parse_cpu_time(fields[5], ticks_per_second, extended.io_wait) ||
        !parse_cpu_time(fields[6], ticks_per_second, extended.irq) ||
        !parse_cpu_time(fields[7], ticks_per_second, extended.soft_irq) ||
        !parse_cpu_time(fields[8], ticks_per_second, extended.steal) ||
        !parse_cpu_time(fields[9], ticks_per_second, extended.guest) ||
        !parse_cpu_time(fields[10], ticks_per_second, extended.guest_nice)) {
      extended_valid = false;
    } else {
      extended_times.cpus.push_back(std::move(extended));
    }
  }

  // Shared labels are valid only when at least one CPU row was found and the
  // aggregate `cpu` row came first.
  labels_valid = labels_valid && !labels.empty() && aggregate_first;
  // `times` is all-or-nothing across the aggregate and every per-CPU row.
  if (times_valid && labels_valid && !times.cpus.empty() &&
      times.cpus.front().cpu == "cpu") {
    result.times = System_result<Cpu_times_snapshot>::success(std::move(times));
  } else {
    result.times = System_result<Cpu_times_snapshot>::failure(invalid_input());
  }
  // `extended_times` has the same label requirements but keeps its own field
  // validity, so either capability can fail independently.
  if (extended_valid && labels_valid && !extended_times.cpus.empty() &&
      extended_times.cpus.front().cpu == "cpu") {
    result.extended_times = System_result<Cpu_extended_snapshot>::success(
        std::move(extended_times));
  } else {
    result.extended_times = System_result<Cpu_extended_snapshot>::failure(invalid_input());
  }
  return result;
}

/**
  Read process identity and memory from /proc/self/status.

  Process memory stats, subset. Fields are labeled. kB converted to bytes.
    Name   - Process name
    Pid    - Process ID
    VmRSS  - Resident set size, in kB
    VmData - Size of data segment, in kB
    VmSwap - Swapped out virtual memory, in kB
    ...
  See https://man7.org/linux/man-pages/man5/proc_pid_status.5.html

  - This file contains labeled `name: value` rows.
  - The reader uses Name, Pid, VmRSS, VmData and VmSwap. Other rows are ignored.
  - The three Vm values have a required kB suffix and are converted to bytes.
*/
Process_memory_snapshot read_linux_process_status(std::istream &input) {
  // Keep each selected status field separate so `identity`, `residency` and
  // `details` can be validated independently.
  Parsed_text name;
  Parsed_value pid;
  Parsed_value resident;
  Parsed_value data;
  Parsed_value swap;

  std::string line;
  // Read one labeled `/proc/self/status` row at a time until EOF or a read
  // error. Rows without a label separator cannot contain a selected field.
  while (std::getline(input, line)) {
    const auto separator = line.find(':');
    if (separator == std::string::npos) continue;
    const std::string_view field{line.data(), separator};
    const std::string_view value{line.data() + separator + 1,
                                 line.size() - separator - 1};
    // The assignment helpers trim the value and reject duplicates. Memory
    // fields must also contain the `kB` unit and are converted to bytes.
    if (field == "Name") assign_text(name, value);
    if (field == "Pid") assign_integer(pid, value);
    if (field == "VmRSS") assign_status_memory(resident, value);
    if (field == "VmData") assign_status_memory(data, value);
    if (field == "VmSwap") assign_status_memory(swap, value);
  }

  Process_memory_snapshot result;
  // `identity` requires one nonempty process name and a positive process ID.
  if (!name.invalid && !pid.invalid && name.value && pid.value && *pid.value > 0) {
    result.identity = System_result<Process_identity>::success({*pid.value, std::move(*name.value)});
  } else {
    result.identity = System_result<Process_identity>::failure(invalid_input());
  }

  // `residency` depends only on `VmRSS`, so other malformed memory fields do
  // not discard a valid resident memory value.
  if (!resident.invalid && resident.value) {
    result.residency = System_result<Process_memory_residency>::success({*resident.value});
  } else {
    result.residency = System_result<Process_memory_residency>::failure(invalid_input());
  }

  // `details` requires both `VmData` and `VmSwap`. If either is missing or
  // invalid then the `details` capability fails.
  if (!data.invalid && !swap.invalid && data.value && swap.value) {
    result.details = System_result<Process_memory_details>::success({*data.value, *swap.value});
  } else {
    result.details = System_result<Process_memory_details>::failure(invalid_input());
  }
  return result;
}

/**
  Read one `/proc/self/task/<tid>/stat` record.

  This file is not the same as /proc/stat despite sharing the name 'stat'.
    - /proc/stat contains system-wide statistics as labeled rows.
    - `/proc/self/task/<tid>/stat` contains one record for a single thread.

  The first fields are the thread ID, the command name in parentheses and a
  one-character state.
 
  The command name can contain spaces and `)` characters, so parsing resumes
  after the final `) ` instead of splitting on whitespace.
  
  States R, S, D, T or t and Z map to running, sleeping, waiting, stopped and
  zombie. Other kernel state characters map to unknown.

  The reader uses these kernel field numbers:
      14 utime                 15 stime
      16 cutime                17 cstime
      20 num_threads           23 vsize (bytes)
      24 rss (pages)           25 rsslim (bytes)
      39 processor             42 delayacct_blkio_ticks
      43 guest_time            44 cguest_time

  - Clock tick fields are converted to milliseconds with _SC_CLK_TCK.
  - RSS is converted to bytes with _SC_PAGESIZE.
  - Process-wide runtime fields are read only from the main thread record, whose
    TID equals the process PID.

  See https://man7.org/linux/man-pages/man5/proc_pid_stat.5.html
*/
Process_threads_snapshot read_linux_thread_stat(std::istream &input,
                                                uint64_t expected_tid,
                                                uint64_t process_pid,
                                                uint64_t ticks_per_second,
                                                uint64_t page_size) {
  Process_threads_snapshot result;
  std::string line;
  // A thread stat file contains one record. Read it as a complete line because
  // the command name can contain whitespace.
  std::getline(input, line);

  // The TID, command name and state determine where the numbered fields begin.
  // begin. If that structure is invalid, then the `threads`, `runtime`, `cpu`,
  // `extended_cpu` and `scheduler` capabilties fail.
  const auto fail_record = [&]() {
    result.threads = failed_result<Thread_inventory>();
    result.runtime = failed_result<Process_runtime_info>();
    result.cpu = failed_result<Thread_cpu_snapshot>();
    result.extended_cpu = failed_result<Thread_extended_cpu_snapshot>();
    result.scheduler = failed_result<Thread_scheduler_snapshot>();
  };

  // Find the command-name boundaries before splitting fields. Use the final
  // `) ` because the command name itself can contain `)` characters.
  const auto opening = line.find('(');
  const auto closing = line.rfind(") ");
  if (input.bad() || opening == std::string::npos ||
      closing == std::string::npos || opening >= closing ||
      closing + 3 >= line.size() || line[closing + 3] != ' ') {
    fail_record();
    return result;
  }

  // Confirm that field 1 matches the TID of the procfs file being read.
  uint64_t tid{0};
  const auto tid_text = trim(std::string_view{line}.substr(0, opening));
  if (!parse_unsigned(tid_text, tid) || tid == 0 || tid != expected_tid) {
    fail_record();
    return result;
  }

  const std::string name = line.substr(opening + 1, closing - opening - 1);
  const char state = line[closing + 2];
  // Accept any non-whitespace state character. Known kernel values are mapped
  // by `thread_state`. Others become `Thread_state::unknown`.
  if (std::isspace(static_cast<unsigned char>(state)) != 0) {
    fail_record();
    return result;
  }
  result.threads = System_result<Thread_inventory>::success(
      {{{tid, name, thread_state(state)}}});

  // The remaining tokens start at kernel field 4 (`ppid`). Their vector index
  // is therefore the documented kernel field number - 4.
  std::istringstream tail{line.substr(closing + 4)};
  std::vector<std::string> fields;
  for (std::string field; tail >> field;) fields.push_back(std::move(field));

  // `cpu` uses fields 14 and 15. Failure is local to this capability because
  // the common TID, name and state fields have already been validated.
  Thread_cpu_times cpu;
  cpu.tid = tid;
  if (fields.size() >= 12 &&
      parse_cpu_time(fields[10], ticks_per_second, cpu.user) &&
      parse_cpu_time(fields[11], ticks_per_second, cpu.system)) {
    result.cpu = System_result<Thread_cpu_snapshot>::success({{{std::move(cpu)}}});
  } else {
    result.cpu = failed_result<Thread_cpu_snapshot>();
  }

  // `extended_cpu` uses child CPU time from fields 16 and 17 and guest CPU
  // time from fields 43 and 44.
  Thread_extended_cpu_times extended;
  extended.tid = tid;
  if (fields.size() >= 41 &&
      parse_cpu_time(fields[12], ticks_per_second, extended.child_user) &&
      parse_cpu_time(fields[13], ticks_per_second, extended.child_system) &&
      parse_cpu_time(fields[39], ticks_per_second, extended.guest) &&
      parse_cpu_time(fields[40], ticks_per_second, extended.child_guest)) {
    result.extended_cpu = System_result<Thread_extended_cpu_snapshot>::success(
        {{{std::move(extended)}}});
  } else {
    result.extended_cpu = failed_result<Thread_extended_cpu_snapshot>();
  }

  // `scheduler` uses the last CPU from field 39 and cumulative block-I/O delay
  // from field 42.
  Thread_scheduler_info scheduler;
  scheduler.tid = tid;
  if (fields.size() >= 39 && parse_unsigned(fields[35], scheduler.last_cpu) &&
      parse_cpu_time(fields[38], ticks_per_second, scheduler.block_io_delay)) {
    result.scheduler = System_result<Thread_scheduler_snapshot>::success(
        {{{std::move(scheduler)}}});
  } else {
    result.scheduler = failed_result<Thread_scheduler_snapshot>();
  }

  // Process-wide fields are meaningful only in the main thread record, where
  // the TID equals the process PID. `runtime` remains unavailable for other
  // thread records.
  if (tid == process_pid) {
    Process_runtime_info runtime;
    uint64_t resident_pages{0};
    if (fields.size() >= 22 &&
        parse_unsigned(fields[16], runtime.thread_count) &&
        runtime.thread_count > 0 &&
        parse_unsigned(fields[19], runtime.virtual_bytes) && page_size > 0 &&
        parse_unsigned(fields[20], resident_pages) &&
        parse_unsigned(fields[21], runtime.resident_limit_bytes) &&
        resident_pages <= std::numeric_limits<uint64_t>::max() / page_size) {
      // Field 24 reports pages; the other runtime size fields are already in
      // bytes.
      runtime.resident_bytes = resident_pages * page_size;
      result.runtime = System_result<Process_runtime_info>::success(std::move(runtime));
    } else {
      result.runtime = failed_result<Process_runtime_info>();
    }
  }
  return result;
}

/**
  Read `/proc/self/task/<tid>/io`.

  I/O statistics for the current process, read/write bytes. Fields are labeled.
    read_bytes  - Bytes fetched from the storage layer
    write_bytes - Bytes written to the storage layer

  Both are cumulative. Unrelated counters such as `rchar` and `wchar` are ignored.

  See https://man7.org/linux/man-pages/man5/proc_pid_io.5.html
*/
System_result<Thread_io_counters> read_linux_thread_io(std::istream &input,
                                                       uint64_t tid) {
  Parsed_value read_bytes;
  Parsed_value write_bytes;
  std::string line;
  while (std::getline(input, line)) {
    const auto separator = line.find(':');
    if (separator == std::string::npos) continue;
    const std::string_view field{line.data(), separator};
    const std::string_view value{line.data() + separator + 1,
                                 line.size() - separator - 1};
    if (field == "read_bytes") assign_integer(read_bytes, value);
    if (field == "write_bytes") assign_integer(write_bytes, value);
  }
  if (input.bad() || tid == 0 || read_bytes.invalid || write_bytes.invalid ||
      !read_bytes.value || !write_bytes.value) {
    return failed_result<Thread_io_counters>();
  }
  return System_result<Thread_io_counters>::success(
      {tid, *read_bytes.value, *write_bytes.value});
}

// clang-format on

}  // namespace mysql::system_info::internal
