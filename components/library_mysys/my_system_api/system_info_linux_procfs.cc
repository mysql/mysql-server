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

namespace mysql::system_info::internal {
namespace {

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
  const auto conversion =
      std::from_chars(text.data(), text.data() + text.size(), value);
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
  const auto conversion =
      std::from_chars(text.data(), text.data() + text.size(), ticks);
  return conversion.ec == std::errc{} &&
         conversion.ptr == text.data() + text.size() &&
         !cpu_ticks_to_milliseconds(ticks, ticks_per_second, time);
}

[[nodiscard]] bool parse_unsigned(std::string_view text, uint64_t &value) {
  const auto conversion =
      std::from_chars(text.data(), text.data() + text.size(), value);
  return conversion.ec == std::errc{} &&
         conversion.ptr == text.data() + text.size();
}

[[nodiscard]] bool parse_unsigned(std::string_view text, uint32_t &value) {
  const auto conversion =
      std::from_chars(text.data(), text.data() + text.size(), value);
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
  std::filesystem::path class_path;
  std::filesystem::path canonical_path;
  bool partition{false};
};

}  // namespace

bool valid_linux_storage_device_name(std::string_view name) {
  return !name.empty() && name != "." && name != ".." &&
         name.find('/') == std::string_view::npos &&
         name.find('\0') == std::string_view::npos;
}

System_result<std::vector<Linux_block_device>> read_linux_partitions(
    std::istream &input) {
  std::vector<Linux_block_device> devices;
  std::unordered_set<std::string> names;
  std::unordered_set<std::string> numbers;
  bool invalid = false;
  std::string line;
  while (std::getline(input, line)) {
    const auto content = trim(line);
    if (content.empty()) continue;

    std::istringstream line_input{std::string{content}};
    std::vector<std::string> fields;
    for (std::string field; line_input >> field;) fields.push_back(field);
    if (fields.size() == 4 && fields[0] == "major" && fields[1] == "minor" &&
        fields[2] == "#blocks" && fields[3] == "name") {
      continue;
    }
    uint64_t major{0};
    uint64_t minor{0};
    uint64_t blocks{0};
    if (fields.size() != 4 || !parse_unsigned(fields[0], major) ||
        !parse_unsigned(fields[1], minor) ||
        !parse_unsigned(fields[2], blocks) ||
        !valid_linux_storage_device_name(fields[3])) {
      invalid = true;
      continue;
    }
    const std::string number =
        std::to_string(major) + ":" + std::to_string(minor);
    if (!names.insert(fields[3]).second || !numbers.insert(number).second) {
      invalid = true;
      continue;
    }
    devices.push_back({major, minor, std::move(fields[3])});
  }
  if (invalid || devices.empty()) {
    return failed_result<std::vector<Linux_block_device>>();
  }
  std::sort(devices.begin(), devices.end(),
            [](const auto &left, const auto &right) {
              return left.name < right.name;
            });
  return System_result<std::vector<Linux_block_device>>::success(
      std::move(devices));
}

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

Linux_diskstats_snapshot read_linux_diskstats(
    std::istream &input, const std::unordered_set<std::string> &whole_devices) {
  Linux_diskstats_snapshot result;
  Storage_read_write_snapshot read_write;
  Storage_flush_snapshot flush;
  std::unordered_set<std::string> names;
  std::unordered_set<std::string> numbers;
  bool identity_valid = true;
  bool read_write_valid = true;
  bool flush_valid = true;
  bool saw_row = false;
  bool saw_flush = false;
  std::string line;
  while (std::getline(input, line)) {
    const auto content = trim(line);
    if (content.empty()) continue;
    saw_row = true;

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
    const std::string number =
        std::to_string(major) + ":" + std::to_string(minor);
    if (!names.insert(fields[2]).second || !numbers.insert(number).second) {
      identity_valid = false;
      continue;
    }

    std::vector<uint64_t> statistics;
    statistics.reserve(std::min<size_t>(fields.size() - 3, 17));
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
      const auto maximum_milliseconds =
          static_cast<uint64_t>(std::chrono::milliseconds::max().count());
      required_valid = statistics[2] <= std::numeric_limits<uint64_t>::max() /
                                            k_bytes_per_diskstats_sector &&
                       statistics[6] <= std::numeric_limits<uint64_t>::max() /
                                            k_bytes_per_diskstats_sector &&
                       statistics[3] <= maximum_milliseconds &&
                       statistics[7] <= maximum_milliseconds;
    }
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
      saw_flush = true;
      flush_valid = false;
    }
  }

  const auto by_device = [](const auto &left, const auto &right) {
    return left.device < right.device;
  };
  std::sort(read_write.devices.begin(), read_write.devices.end(), by_device);
  std::sort(flush.devices.begin(), flush.devices.end(), by_device);
  if (!saw_row || !identity_valid || !read_write_valid ||
      read_write.devices.empty()) {
    result.read_write = failed_result<Storage_read_write_snapshot>();
  } else {
    result.read_write = System_result<Storage_read_write_snapshot>::success(
        std::move(read_write));
  }
  if (!identity_valid || (saw_flush && !flush_valid)) {
    result.flush = failed_result<Storage_flush_snapshot>();
  } else if (!saw_flush || flush.devices.empty()) {
    result.flush = System_result<Storage_flush_snapshot>::unavailable();
  } else {
    result.flush =
        System_result<Storage_flush_snapshot>::success(std::move(flush));
  }
  return result;
}

Storage_snapshot query_linux_storage_devices(
    const Linux_storage_paths &paths,
    const std::function<void()> &after_metadata) {
  Storage_snapshot result;
  const auto invalid = invalid_input();

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
    std::error_code canonical_error;
    auto canonical_path =
        std::filesystem::canonical(entries->path(), canonical_error);
    if (canonical_error == std::errc::no_such_file_or_directory) continue;
    if (canonical_error) {
      metadata_error = canonical_error;
      break;
    }
    std::error_code partition_error;
    const bool partition =
        std::filesystem::exists(entries->path() / "partition", partition_error);
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
  if (after_metadata) after_metadata();

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
      result.inventory =
          System_result<Storage_inventory>::failure(metadata_error);
      result.capacities =
          System_result<Storage_capacities>::failure(metadata_error);
      result.hierarchy =
          System_result<Storage_hierarchy>::failure(metadata_error);
    } else {
      Storage_inventory inventory;
      Storage_capacities capacities;
      Storage_hierarchy hierarchy;
      bool capacities_valid = true;
      bool hierarchy_valid = true;
      std::error_code inventory_error;
      std::error_code capacity_error;
      std::unordered_set<std::string> retained_devices;

      for (const auto &device : parsed_partitions.value()) {
        const auto found = metadata.find(device.name);
        if (found == metadata.end()) continue;
        const auto &device_metadata = found->second;
        std::error_code exists_error;
        if (!std::filesystem::exists(device_metadata.class_path,
                                     exists_error)) {
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
        auto sector_path = linux_storage_sector_size_path(
            device_metadata.canonical_path.generic_string(),
            device_metadata.partition);

        errno = 0;
        std::ifstream size{device_metadata.class_path / "size"};
        const auto size_error =
            size.is_open() ? std::error_code{} : storage_io_error();
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
          capacity =
              read_linux_storage_capacity(size, sector_size, device.name);
          if (size.bad() || sector_size.bad()) {
            capacity = System_result<Storage_device_capacity>::failure(
                storage_io_error());
          } else if (!capacity.has_value()) {
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
        }

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

        if (device_metadata.partition) {
          if (!relationship.has_value()) {
            hierarchy_valid = false;
          } else {
            hierarchy.relationships.push_back(std::move(relationship).value());
          }
        }
      }

      if (inventory_error) {
        result.inventory =
            System_result<Storage_inventory>::failure(inventory_error);
        result.capacities =
            System_result<Storage_capacities>::failure(inventory_error);
        result.hierarchy =
            System_result<Storage_hierarchy>::failure(inventory_error);
      } else if (inventory.devices.empty()) {
        const auto error = std::make_error_code(std::errc::no_such_device);
        result.inventory = System_result<Storage_inventory>::failure(error);
        result.capacities = System_result<Storage_capacities>::failure(error);
        result.hierarchy = System_result<Storage_hierarchy>::failure(error);
      } else {
        for (const auto &relationship : hierarchy.relationships) {
          if (!retained_devices.contains(relationship.parent_device)) {
            hierarchy_valid = false;
            break;
          }
        }
        result.inventory =
            System_result<Storage_inventory>::success(std::move(inventory));
        result.capacities =
            capacities_valid &&
                    capacities.devices.size() == retained_devices.size()
                ? System_result<Storage_capacities>::success(
                      std::move(capacities))
                : System_result<Storage_capacities>::failure(
                      capacity_error ? capacity_error : invalid);
        result.hierarchy =
            hierarchy_valid
                ? System_result<Storage_hierarchy>::success(
                      std::move(hierarchy))
                : System_result<Storage_hierarchy>::failure(invalid);
      }
    }
  }

  errno = 0;
  std::ifstream diskstats{paths.proc_diskstats};
  if (!diskstats.is_open()) {
    const auto error = storage_io_error();
    result.read_write =
        System_result<Storage_read_write_snapshot>::failure(error);
    result.flush = System_result<Storage_flush_snapshot>::failure(error);
  } else {
    auto statistics = read_linux_diskstats(diskstats, whole_devices);
    if (diskstats.bad()) {
      const auto error = storage_io_error();
      result.read_write =
          System_result<Storage_read_write_snapshot>::failure(error);
      result.flush = System_result<Storage_flush_snapshot>::failure(error);
    } else {
      result.read_write = std::move(statistics.read_write);
      result.flush =
          metadata_error
              ? System_result<Storage_flush_snapshot>::failure(metadata_error)
              : std::move(statistics.flush);
    }
  }
  return result;
}

System_result<Filesystem_identity> read_linux_mount_info(
    std::istream &input, std::string_view device_id,
    std::string_view target_path) {
  std::optional<Filesystem_identity> best;
  size_t best_length = 0;
  bool malformed = false;
  std::string line;
  while (std::getline(input, line)) {
    std::istringstream line_input{line};
    std::vector<std::string> fields;
    for (std::string field; line_input >> field;) fields.push_back(field);

    if (fields.size() < 6) {
      malformed = true;
      continue;
    }
    if (fields[2] != device_id) continue;

    const auto separator = std::find(fields.begin(), fields.end(), "-");
    if (separator == fields.end() || separator - fields.begin() < 6 ||
        fields.end() - separator < 4 || !is_decimal(fields[0])) {
      malformed = true;
      continue;
    }

    auto mount_point = decode_mount_field(fields[4]);
    auto mount_source = decode_mount_field(*(separator + 2));
    if (!mount_point || !mount_source) {
      malformed = true;
      continue;
    }
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
  return System_result<Filesystem_identity>::failure(std::make_error_code(
      malformed ? std::errc::invalid_argument : std::errc::no_such_device));
}

Host_memory_snapshot read_linux_meminfo(std::istream &input) {
  Parsed_value total;
  Parsed_value free;
  Parsed_value available;
  Parsed_value buffers;
  Parsed_value cached;
  Parsed_value slab;
  Parsed_value swap_total;
  Parsed_value swap_free;

  std::string line;
  while (std::getline(input, line)) {
    std::istringstream fields{line};
    std::string name;
    std::string value;
    std::string unit;
    std::string extra;
    fields >> name;
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
    if (!(fields >> value >> unit) || unit != "kB" || fields >> extra) {
      target->value.reset();
      target->invalid = true;
      continue;
    }
    assign_value(*target, value);
  }

  Host_memory_snapshot result;
  if (!total.invalid && !free.invalid && !available.invalid && total.value &&
      free.value && available.value) {
    result.memory = System_result<Host_memory_info>::success(
        {*total.value, *free.value, *available.value});
  } else {
    result.memory = System_result<Host_memory_info>::failure(invalid_input());
  }
  if (!buffers.invalid && !cached.invalid && !slab.invalid && buffers.value &&
      cached.value && slab.value) {
    result.breakdown = System_result<Host_memory_breakdown>::success(
        {*buffers.value, *cached.value, *slab.value});
  } else {
    result.breakdown =
        System_result<Host_memory_breakdown>::failure(invalid_input());
  }
  if (!swap_total.invalid && !swap_free.invalid && swap_total.value &&
      swap_free.value) {
    result.swap_capacity = System_result<Swap_capacity_info>::success(
        {*swap_total.value, *swap_free.value});
  } else {
    result.swap_capacity =
        System_result<Swap_capacity_info>::failure(invalid_input());
  }
  return result;
}

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
  if (bytes_in.invalid || bytes_out.invalid || !bytes_in.value ||
      !bytes_out.value) {
    return System_result<Swap_activity_info>::failure(invalid_input());
  }
  return System_result<Swap_activity_info>::success(
      {*bytes_in.value, *bytes_out.value});
}

Host_cpu_snapshot read_linux_cpu_stat(std::istream &input,
                                      uint64_t ticks_per_second) {
  Host_cpu_snapshot result;
  if (ticks_per_second == 0 ||
      ticks_per_second > std::numeric_limits<uint64_t>::max() / 1000U) {
    const auto error = invalid_input();
    result.times = System_result<Cpu_times_snapshot>::failure(error);
    result.extended_times =
        System_result<Cpu_extended_snapshot>::failure(error);
    return result;
  }

  Cpu_times_snapshot times;
  Cpu_extended_snapshot extended_times;
  bool times_valid = true;
  bool extended_valid = true;
  bool labels_valid = true;
  bool first_label = true;
  bool aggregate_first = false;
  std::unordered_set<std::string> labels;
  std::string line;
  while (std::getline(input, line)) {
    std::istringstream line_input{line};
    std::vector<std::string> fields;
    for (std::string field; line_input >> field;) {
      fields.push_back(std::move(field));
    }
    if (fields.empty() || !fields.front().starts_with("cpu")) continue;

    const std::string &label = fields.front();
    if (!is_cpu_label(label) || !labels.insert(label).second) {
      labels_valid = false;
      continue;
    }
    if (first_label) {
      aggregate_first = label == "cpu";
      first_label = false;
    }

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

  labels_valid = labels_valid && !labels.empty() && aggregate_first;
  if (times_valid && labels_valid && !times.cpus.empty() &&
      times.cpus.front().cpu == "cpu") {
    result.times = System_result<Cpu_times_snapshot>::success(std::move(times));
  } else {
    result.times = System_result<Cpu_times_snapshot>::failure(invalid_input());
  }
  if (extended_valid && labels_valid && !extended_times.cpus.empty() &&
      extended_times.cpus.front().cpu == "cpu") {
    result.extended_times = System_result<Cpu_extended_snapshot>::success(
        std::move(extended_times));
  } else {
    result.extended_times =
        System_result<Cpu_extended_snapshot>::failure(invalid_input());
  }
  return result;
}

Process_memory_snapshot read_linux_process_status(std::istream &input) {
  Parsed_text name;
  Parsed_value pid;
  Parsed_value resident;
  Parsed_value data;
  Parsed_value swap;

  std::string line;
  while (std::getline(input, line)) {
    const auto separator = line.find(':');
    if (separator == std::string::npos) continue;
    const std::string_view field{line.data(), separator};
    const std::string_view value{line.data() + separator + 1,
                                 line.size() - separator - 1};
    if (field == "Name") assign_text(name, value);
    if (field == "Pid") assign_integer(pid, value);
    if (field == "VmRSS") assign_status_memory(resident, value);
    if (field == "VmData") assign_status_memory(data, value);
    if (field == "VmSwap") assign_status_memory(swap, value);
  }

  Process_memory_snapshot result;
  if (!name.invalid && !pid.invalid && name.value && pid.value &&
      *pid.value > 0) {
    result.identity = System_result<Process_identity>::success(
        {*pid.value, std::move(*name.value)});
  } else {
    result.identity = System_result<Process_identity>::failure(invalid_input());
  }

  if (!resident.invalid && resident.value) {
    result.residency =
        System_result<Process_memory_residency>::success({*resident.value});
  } else {
    result.residency =
        System_result<Process_memory_residency>::failure(invalid_input());
  }

  if (!data.invalid && !swap.invalid && data.value && swap.value) {
    result.details = System_result<Process_memory_details>::success(
        {*data.value, *swap.value});
  } else {
    result.details =
        System_result<Process_memory_details>::failure(invalid_input());
  }
  return result;
}

Process_threads_snapshot read_linux_thread_stat(std::istream &input,
                                                uint64_t expected_tid,
                                                uint64_t process_pid,
                                                uint64_t ticks_per_second,
                                                uint64_t page_size) {
  Process_threads_snapshot result;
  std::string line;
  std::getline(input, line);

  const auto fail_record = [&]() {
    result.threads = failed_result<Thread_inventory>();
    result.runtime = failed_result<Process_runtime_info>();
    result.cpu = failed_result<Thread_cpu_snapshot>();
    result.extended_cpu = failed_result<Thread_extended_cpu_snapshot>();
    result.scheduler = failed_result<Thread_scheduler_snapshot>();
  };

  const auto opening = line.find('(');
  const auto closing = line.rfind(") ");
  if (input.bad() || opening == std::string::npos ||
      closing == std::string::npos || opening >= closing ||
      closing + 3 >= line.size() || line[closing + 3] != ' ') {
    fail_record();
    return result;
  }

  uint64_t tid{0};
  const auto tid_text = trim(std::string_view{line}.substr(0, opening));
  if (!parse_unsigned(tid_text, tid) || tid == 0 || tid != expected_tid) {
    fail_record();
    return result;
  }

  const std::string name = line.substr(opening + 1, closing - opening - 1);
  const char state = line[closing + 2];
  if (std::isspace(static_cast<unsigned char>(state)) != 0) {
    fail_record();
    return result;
  }
  result.threads = System_result<Thread_inventory>::success(
      {{{tid, name, thread_state(state)}}});

  std::istringstream tail{line.substr(closing + 4)};
  std::vector<std::string> fields;
  for (std::string field; tail >> field;) fields.push_back(std::move(field));

  Thread_cpu_times cpu;
  cpu.tid = tid;
  if (fields.size() >= 12 &&
      parse_cpu_time(fields[10], ticks_per_second, cpu.user) &&
      parse_cpu_time(fields[11], ticks_per_second, cpu.system)) {
    result.cpu =
        System_result<Thread_cpu_snapshot>::success({{{std::move(cpu)}}});
  } else {
    result.cpu = failed_result<Thread_cpu_snapshot>();
  }

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

  Thread_scheduler_info scheduler;
  scheduler.tid = tid;
  if (fields.size() >= 39 && parse_unsigned(fields[35], scheduler.last_cpu) &&
      parse_cpu_time(fields[38], ticks_per_second, scheduler.block_io_delay)) {
    result.scheduler = System_result<Thread_scheduler_snapshot>::success(
        {{{std::move(scheduler)}}});
  } else {
    result.scheduler = failed_result<Thread_scheduler_snapshot>();
  }

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
      runtime.resident_bytes = resident_pages * page_size;
      result.runtime =
          System_result<Process_runtime_info>::success(std::move(runtime));
    } else {
      result.runtime = failed_result<Process_runtime_info>();
    }
  }
  return result;
}

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

}  // namespace mysql::system_info::internal
