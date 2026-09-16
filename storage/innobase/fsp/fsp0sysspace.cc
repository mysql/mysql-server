/*****************************************************************************

Copyright (c) 2013, 2026, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file fsp/fsp0sysspace.cc
 Multi file, shared, system tablespace implementation.

 Created 2012-11-16 by Sunny Bains as srv/srv0space.cc
 Refactored 2013-7-26 by Kevin Lewis
 *******************************************************/

#include <stdlib.h>
#include <sys/types.h>
#include <optional>

#include "dict0load.h"
#include "fil0tablespace_node_handle_interface.h"
#include "fil0tablespaces_nodes_interface.h"
#include "fsp0sysspace.h"
#ifndef UNIV_HOTBACKUP
#include "ha_prototypes.h"
#include "mem0mem.h"

/** The server header file is included to access opt_initialize global variable.
If server passes the option for create/open DB to SE, we should remove such
direct reference to server header and global variable */
#include "mysqld.h"
#endif /* !UNIV_HOTBACKUP */
#include "os0file.h"
#ifndef UNIV_HOTBACKUP
#include "row0mysql.h"
#endif /* !UNIV_HOTBACKUP */
#include "page0page.h"
#include "srv0start.h"
#include "trx0sys.h"
#include "ut0new.h"

/** The control info of the system tablespace. */
ib::fsp::SysTablespace srv_sys_space(TRX_SYS_SPACE, FIL_TYPE_TABLESPACE);

/** The control info of a temporary table shared tablespace. */
ib::fsp::SysTablespace srv_tmp_space(dict_sys_t::s_temp_space_id,
                                     FIL_TYPE_TEMPORARY);

ulong sys_tablespace_auto_extend_increment;

#ifdef UNIV_DEBUG
bool srv_skip_temp_table_checks_debug = true;
#endif /* UNIV_DEBUG */

namespace ib::fsp {

char *SysTablespace::parse_file_name(char *ptr) {
  const char *start = ptr;

  while ((*ptr != ':' && *ptr != '\0') ||
         (ptr != start && *ptr == ':' &&
          (*(ptr + 1) == '\\' || *(ptr + 1) == '/' || *(ptr + 1) == ':'))) {
    ptr++;
  }

  return (ptr);
}

page_no_t SysTablespace::parse_suffixed_page_count(char *&ptr) {
  char *endp;
  ulint num = strtoul(ptr, &endp, 10);
  ulint megs;

  ptr = endp;

  switch (*ptr) {
    case 'G':
    case 'g':
      megs = num * 1024;
      ++ptr;
      break;

    case 'M':
    case 'm':
      megs = num;
      ++ptr;
      break;
    case 'K':
    case 'k':
      megs = num / 1024;
      ++ptr;
      break;
    default:
      megs = num / (1024 * 1024);
      break;
  }

  return (static_cast<page_no_t>(megs * (1024 * 1024 / UNIV_PAGE_SIZE)));
}

bool SysTablespace::parse_params(const char *filepath_spec) {
  char *filepath;
  page_no_t size;
  ulint n_files = 0;

  ut_ad(m_last_file_size_in_pages_max == 0);
  ut_ad(!m_auto_extend_last_file);

  char *input_str = mem_strdup(filepath_spec);
  char *ptr = input_str;

  /*---------------------- PASS 1 ---------------------------*/
  /* First calculate the number of data files and check syntax. */
  while (*ptr != '\0') {
    filepath = ptr;

    ptr = parse_file_name(ptr);

    if (ptr == filepath) {
      ib::error(ER_IB_MSG_431) << "File Path Specification '" << filepath_spec
                               << "' is missing a file name.";

      ut::free(input_str);
      return (false);
    }

    if (*ptr == '\0') {
      ib::error(ER_IB_MSG_432) << "File Path Specification '" << filepath_spec
                               << "' is missing a file size.";

      ut::free(input_str);
      return (false);
    }

    ptr++;

    size = parse_suffixed_page_count(ptr);

    if (size == 0) {
    invalid_size:
      ib::error(ER_IB_MSG_433)
          << "Invalid File Path Specification: '" << filepath_spec
          << "'. An invalid file size was specified.";

      ut::free(input_str);
      return (false);
    }

    if (0 == strncmp(ptr, ":autoextend", (sizeof ":autoextend") - 1)) {
      ptr += (sizeof ":autoextend") - 1;

      if (0 == strncmp(ptr, ":max:", (sizeof ":max:") - 1)) {
        ptr += (sizeof ":max:") - 1;

        page_no_t max = parse_suffixed_page_count(ptr);

        if (max < size) {
          goto invalid_size;
        }
      }

      if (*ptr == ';') {
        ib::error(ER_IB_MSG_434)
            << "Invalid File Path Specification: '" << filepath_spec
            << "'. Only the last"
               " file defined can be 'autoextend'.";

        ut::free(input_str);
        return (false);
      }
    }

    if (0 == strncmp(ptr, "new", (sizeof "new") - 1)) {
      ptr += (sizeof "new") - 1;
    }

    if (0 == strncmp(ptr, "raw", (sizeof "raw") - 1)) {
      ptr += (sizeof "raw") - 1;
    }

    ++n_files;

    if (*ptr == ';') {
      ptr++;
    } else if (*ptr != '\0') {
      ptr[0] = '\0';
      ib::error(ER_IB_MSG_436)
          << "File Path Specification: '" << filepath_spec
          << "' has unrecognized characters after '" << input_str << "'";

      ut::free(input_str);
      return (false);
    }
  }

  if (n_files == 0) {
    ib::error(ER_IB_MSG_437) << "File Path Specification: '" << filepath_spec
                             << "' must contain"
                                " at least one data file definition";

    ut::free(input_str);
    return (false);
  }

  /*---------------------- PASS 2 ---------------------------*/
  /* Then store the actual values to our arrays */
  ptr = input_str;
  ulint order = 0;

  while (*ptr != '\0') {
    filepath = ptr;

    ptr = parse_file_name(ptr);

    if (*ptr == ':') {
      /* Make filepath a null-terminated string */
      *ptr = '\0';
      ptr++;
    }

    size = parse_suffixed_page_count(ptr);
    ut_ad(size > 0);

    if (0 == strncmp(ptr, ":autoextend", (sizeof ":autoextend") - 1)) {
      m_auto_extend_last_file = true;

      ptr += (sizeof ":autoextend") - 1;

      if (0 == strncmp(ptr, ":max:", (sizeof ":max:") - 1)) {
        ptr += (sizeof ":max:") - 1;

        m_last_file_size_in_pages_max = parse_suffixed_page_count(ptr);
      }
    }

    if (0 == strncmp(ptr, "new", (sizeof "new") - 1)) {
      ptr += (sizeof "new") - 1;
    }

    node_device_type_t device_type{node_device_type_t::REGULAR_FILE};
    if (0 == strncmp(ptr, "raw", (sizeof "raw") - 1)) {
      ptr += (sizeof "raw") - 1;

      /* Initialize new raw device only during initialize */
      device_type =
#ifndef UNIV_HOTBACKUP
          opt_initialize ? node_device_type_t::NEW_RAW
                         : node_device_type_t::OLD_RAW;
#else  /* !UNIV_HOTBACKUP */
          node_device_type_t::OLD_RAW;
#endif /* !UNIV_HOTBACKUP */
    }

    if (*ptr == ';') {
      ++ptr;
    }

    m_nodes.emplace_back(std::string{filepath}, size, order, device_type);

    order++;
  }

  ut_ad(n_files == ulint(m_nodes.size()));

  ut::free(input_str);

  return (true);
}

dberr_t SysTablespace::check_size(
    SysTablespace_node &node,
    const ib::fil::Tablespaces_nodes_interface::Node_info &node_info) {
  ut_a(node_info.size != std::numeric_limits<page_no_t>::max());

  /* If last file */
  if (node.order() == m_nodes.size() - 1 && m_auto_extend_last_file) {
    if (node.expected_size_in_pages() > node_info.size ||
        (m_last_file_size_in_pages_max > 0 &&
         m_last_file_size_in_pages_max < node_info.size)) {
      ib::error(ER_IB_AUTO_EXTENDING_SPACE_NODE_HAS_UNEXPECTED_SIZE, name(),
                get_node_full_path(node).c_str(), ulong{node_info.size},
                ulong{node.expected_size_in_pages()},
                ulong{m_last_file_size_in_pages_max});
      return DB_ERROR;
    }
  } else if (node_info.size != node.expected_size_in_pages()) {
    ib::error(ER_IB_SPACE_NODE_HAS_UNEXPECTED_SIZE, name(),
              get_node_full_path(node).c_str(), ulong{node_info.size},
              ulong{node.expected_size_in_pages()});
    return DB_ERROR;
  }

  return DB_SUCCESS;
}

ut::Expected<SysTablespace::Opened_storage_node> SysTablespace::create_node(
    SysTablespace_node &node) {
  using Status = ib::fil::Tablespace_node_handle_interface::Status;
  using Create_error = ib::fil::Tablespaces_nodes_interface::Create_error;

  ut_a(!is_read_only());
  ut_a((!node.node_storage_exists() &&
        node.device_type() == node_device_type_t::REGULAR_FILE) ||
       (node.node_storage_exists() &&
        node.device_type() == node_device_type_t::NEW_RAW));

  ut_ad(page_size_t{flags()}.physical() == UNIV_PAGE_SIZE);
  const auto node_path = get_node_full_path(node);
  auto created_node = tablespaces_nodes->create(
      space_id(), node.order(),
      {.m_base_hints = {.m_path = node_path,
                        .m_is_raw_disk = node.device_type() !=
                                         node_device_type_t::REGULAR_FILE}},
      flags(), 0);
  if (!created_node) {
    dberr_t err;
    switch (created_node.error()) {
      case Create_error::NODE_EXISTS:
        ib::error(ER_IB_MSG_UNEXPECTED_FILE_EXISTS, node_path.c_str(),
                  node_path.c_str());
        err = DB_TABLESPACE_EXISTS;
        break;

      case Create_error::FILE_NAME_TOO_LONG:
        ib::error(ER_IB_MSG_TOO_LONG_PATH, node_path.c_str());
        err = DB_TOO_LONG_PATH;
        break;

      case Create_error::OUT_OF_DISK_SPACE:
        err = DB_OUT_OF_DISK_SPACE;
        break;

      case Create_error::IO_ERROR:
        err = DB_ERROR;
        break;
      case Create_error::UNSUPPORTED:
        err = DB_UNSUPPORTED;
        break;

      default:
        ut_error;
    }
    ib::error(ER_IB_NODE_CREATION_FAILED, node_path.c_str(), ut_strerr(err));
    return ut::Unexpected(err);
  }

  ib::info(
      ER_IB_NODE_IS_BEING_PREPARED, node_path.c_str(),
      ulonglong{node.expected_size_in_pages()} >> (20 - UNIV_PAGE_SIZE_SHIFT));

  const auto zeroing_status =
      (*created_node)
          ->fill_range_with_zeros(0, node.expected_size_in_pages(),
                                  !tbsp_extend_and_initialize);
  if (zeroing_status != Status::SUCCESS) {
    ut_ad(zeroing_status == Status::IO_ERROR);
    ib::error(ER_IB_NODE_FILLING_FAILED, node_path.c_str(),
              ut_strerr(DB_IO_ERROR));
    return ut::Unexpected(DB_IO_ERROR);
  }

  ib::info(
      ER_IB_NODE_CREATION_FINISHED, node_path.c_str(),
      ulonglong{node.expected_size_in_pages()} >> (20 - UNIV_PAGE_SIZE_SHIFT));

  node.set_node_storage_exists();
  m_sum_of_new_sizes_in_pages += node.expected_size_in_pages();

  return Opened_storage_node(node, std::move(*created_node));
}

ut::Expected<SysTablespace::Opened_storage_node> SysTablespace::open_node(
    const SysTablespace_node &node) {
  using Open_error = ib::fil::Tablespaces_nodes_interface::Open_error;

  ut_a(node.node_storage_exists());

  switch (node.device_type()) {
    case node_device_type_t::OLD_RAW:
      srv_start_raw_disk_in_use = true;

      if (is_read_only()) {
        ib::error(ER_IB_OPEN_RAW_DEVICE_IN_READONLY_MODE, node.name().c_str());
        return ut::Unexpected(DB_ERROR);
      }
      [[fallthrough]];
    case node_device_type_t::REGULAR_FILE: {
      auto opened_node = tablespaces_nodes->open(
          space_id(), node.order(),
          {.m_path = get_node_full_path(node),
           .m_is_raw_disk =
               node.device_type() != node_device_type_t::REGULAR_FILE},
          UNIV_PAGE_SIZE, is_read_only());
      if (!opened_node) {
        switch (opened_node.error()) {
          case Open_error::NODE_DOES_NOT_EXIST:
          case Open_error::IO_ERROR:
            return ut::Unexpected(DB_CANNOT_OPEN_FILE);

          default:
            ut_error;
        }
      }
      return Opened_storage_node(node, std::move(*opened_node));
    } break;

    default:
      ut_error;
  }
}

#ifndef UNIV_HOTBACKUP

ut::Expected<lsn_t> SysTablespace::read_lsn_and_check_flags() {
  /* Methods like page_get_page_no() are checking for UNIV_PAGE_SIZE alignment
  where for IO the UNIV_SECTOR_SIZE would be sufficient. */
  const auto page =
      ut::make_unique_aligned<byte[]>(UNIV_PAGE_SIZE, UNIV_PAGE_SIZE_MAX);

  /* Only relevant for the system tablespace. */
  ut_ad(space_id() == TRX_SYS_SPACE);

  /* We want to open all files in this tablespace and keep them open as they are
  not part of the opened nodes LRU. */
  if (const auto open_res = fil_space_open(TRX_SYS_SPACE); !open_res) {
    return open_res.error();
  } else {
    fil_space_release(*open_res);
  }

  {
    /* We use NO_COMPRESSION to not transform the page content in any way. We
    want first validate this page is valid, we don't want the fil system to
    even attempt checking the fields on compression, even if currently the
    fil_io does not run decompression or decryption for page 0. */
    const auto err =
        fil_io(IORequest::Type::READ | IORequest::Type::NO_COMPRESSION, true,
               page_id_t{TRX_SYS_SPACE, 0}, univ_page_size, UNIV_PAGE_SIZE,
               page.get(), nullptr, false);
    if (err != DB_SUCCESS) {
      ib::error(ER_IB_MSG_393) << "Cannot read first page of '"
                               << m_nodes[0].name() << "' " << ut_strerr(err);

      return err;
    }
  }
  uint32_t space_flags_on_disk;
  /** TODO: Enable following after WL#11063-related comment in
  fsp_header_validate() is fixed.
  const auto server_version = fsp_header_get_server_version(page.get());
  const auto space_version = fsp_header_get_space_version(page.get());
  */

  Encryption_metadata encryption_metadata{};
  Encryption_key encryption_key{encryption_metadata.m_key,
                                encryption_metadata.m_iv};
  const auto err =
      fsp_header_validate(page.get(), space_id(), space_flags_on_disk,
                          get_node_full_path(0), false, encryption_key);
  if (err != DB_SUCCESS) {
    return err;
  }
  /* The System Tablespaces are never encrypted. Match against empty key to
  check no non-zero key was extracted from FSP header. */
  ut_a(Encryption_metadata{}.match(encryption_metadata));
  /* The flags of srv_sys_space do not have SDI Flag set.
  Update the flags of system tablespace to indicate the presence of SDI */
  set_flags(space_flags_on_disk);
  return mach_read_from_8(page.get() + FIL_PAGE_FILE_FLUSH_LSN);
}

dberr_t SysTablespace::check_file_spec(bool create_new_tablespace,
                                       uint64_t min_expected_size,
                                       bool supports_raw_devices) {
  ut_a(min_expected_size % UNIV_PAGE_SIZE == 0);
  if (get_sum_of_expected_sizes_in_pages() <
      min_expected_size / UNIV_PAGE_SIZE) {
    ib::error(ER_IB_MSG_452) << "Tablespace size must be at least "
                             << min_expected_size / (1024 * 1024) << " MB";

    return DB_ERROR;
  }

  /* Files specifications should have been already parsed. */
  ut_a(!m_nodes.empty());
  const auto node_description_for_error = [this](const auto &node) {
    return ib::logger::msg(ER_IB_SYS_SPACE_NODE_ERROR_INFO, node.name().c_str(),
                           get_node_full_path(node).c_str(), this->name());
  };

  bool seen_nonexisting_node = false;
  for (auto &node : m_nodes) {
    if (node.device_type() != node_device_type_t::REGULAR_FILE &&
        !supports_raw_devices) {
      ib::error(ER_IB_MSG_NO_RAW_DEVICES_SUPPORT_FOR_TABLESPACE,
                node_description_for_error(node).c_str());
      return DB_ERROR;
    }

    const auto node_info = tablespaces_nodes->get_node_info(
        space_id(), node.order(),
        {.m_path = get_node_full_path(node),
         .m_is_raw_disk =
             node.device_type() != node_device_type_t::REGULAR_FILE,
         .m_check_permissions = true},
        page_size_t{flags()}.physical());
    if (!node_info) {
      switch (node_info.error()) {
        case ib::fil::Tablespaces_nodes_interface::Node_error::IO_ERROR:
          ib::error(ER_IB_NODE_STAT_FAILED,
                    node_description_for_error(node).c_str());
          return DB_ERROR;

        case ib::fil::Tablespaces_nodes_interface::Node_error::NOT_A_NODE:
          ib::error(ER_IB_NODE_NOT_A_REGULAR_FILE,
                    node_description_for_error(node).c_str());
          return DB_ERROR;

        case ib::fil::Tablespaces_nodes_interface::Node_error::
            NODE_DOES_NOT_EXIST:
          if (is_read_only()) {
            ib::error(ER_IB_NODE_CREATION_IN_READONLY_MODE,
                      node_description_for_error(node).c_str());
            return DB_ERROR;
          } else if (node.device_type() != node_device_type_t::REGULAR_FILE) {
            ib::error(ER_IB_CANT_CREATE_RAW_DEVICE,
                      node_description_for_error(node).c_str());
            return DB_ERROR;
          } else if (node.order() == 0) {
            if (create_new_tablespace) {
              if (fsp_is_system_tablespace(space_id())) {
                ib::info(ER_IB_FIRST_NODE_OF_TABLESPACE_WILL_BE_CREATED,
                         node_description_for_error(node).c_str());
              }
            } else {
              ib::error(ER_IB_TABLESPACE_FILE_MISSING,
                        node_description_for_error(node).c_str());
              return DB_ERROR;
            }
          } else if (!create_new_tablespace) {
            /* We allow the user to restart server with a path specification
            that has:
            - last node size changed from the `autoextend` to the actual size,
            - new nodes specified at the end of the previous specification.
            This way, the user can request InnoDB to create new nodes which will
            extend the System Tablespace (perhaps even onto another drives). The
            way this situation would manifest here is that a non-empty prefix of
            the list of paths from the specification would already exist, and
            the rest of it (a non-empty suffix starts where the before mentioned
            prefix ended) would be all missing. We don't know what the previous
            specification really was used. Currently we can assume that it had
            to have at least one, first, node, with correct tablespaces header,
            but all other nodes could differ.
            Later in the `srv_start()` we will:
            - add the sum of sizes of nodes created in the
            `SysTablespace::prepare_nodes()` called after this method finishes,
            to the size stored in the tablespace header.
            - validate the updated size stored in header is not larger than the
            sum of actual file sizes, and in case last file is not `autoextend`,
            if they are equal.
            To be done: this method should check space ID and page numbers
            stored in subsequent nodes from the specification are almost
            consistent (excluding corrupted pages). Currently we don't validate
            the any pages but first.*/
            seen_nonexisting_node = true;
          }
          ib::info(ER_IB_INFO_TABLESPACE_NODE_WILL_BE_CREATED,
                   node_description_for_error(node).c_str());
          break;

        default:
          ut_error;
      }
    } else {
      /* Node exists. */

      /* If we see an existing node, but we are to create all the tablespace
      nodes, or we have seen non-existing node already, we emit error and stop,
      as there is evidently some problem with the path specification provided by
      the user. However it is ok for the RAW device to exist when @p
      create_new_tablespace is specified. */
      if ((node.device_type() == node_device_type_t::REGULAR_FILE &&
           create_new_tablespace) ||
          seen_nonexisting_node) {
        ib::error(ER_IB_TABLESPACE_NODE_SHOULD_NOT_EXIST,
                  node_description_for_error(node).c_str());
        return DB_ERROR;
      }

      if (is_read_only() ? !node_info->access_permissions.has_read_access
                         : !node_info->access_permissions.has_write_access) {
        ib::error(ER_IB_NODE_MUST_BE_ACCESSIBLE,
                  node_description_for_error(node).c_str(),
                  is_read_only() ? "readable" : "writable");

        return DB_ERROR;
      }
      node.set_node_storage_exists();
      const auto err = check_size(node, *node_info);
      if (err != DB_SUCCESS) {
        return err;
      }
    }
  }

  /* We assume doublewrite blocks in the first data file. */
  if (space_id() == TRX_SYS_SPACE && m_nodes[0].expected_size_in_pages() <
                                         TRX_SYS_DOUBLEWRITE_BLOCK_SIZE * 3) {
    ib::error(ER_IB_TABLESPACE_TOO_SMALL,
              node_description_for_error(m_nodes[0]).c_str(),
              ulong{TRX_SYS_DOUBLEWRITE_BLOCK_SIZE * 3 * UNIV_PAGE_SIZE /
                    (1024 * 1024)});

    return DB_ERROR;
  }

  return DB_SUCCESS;
}

dberr_t SysTablespace::prepare_nodes() {
  ut_ad(!m_nodes.empty());

  for (auto &node : m_nodes) {
    auto res = (node.node_storage_exists() &&
                node.device_type() != node_device_type_t::NEW_RAW)
                   ? open_node(node)
                   : create_node(node);

    if (!res) {
      return res.error();
    }
  }
  /* Create the tablespace entry for the multi-file
  tablespace in the tablespace manager. */
  const auto space =
      fil_space_create(name(), space_id(), flags(), space_type());
  ut_ad(fil_validate());
  ut_a(space != nullptr);

  for (auto &node : m_nodes) {
    ut_a(node.node_storage_exists());

    page_no_t max_size =
        (&node == &m_nodes.back() ? (m_last_file_size_in_pages_max == 0
                                         ? PAGE_NO_MAX
                                         : m_last_file_size_in_pages_max)
                                  : node.expected_size_in_pages());

    /* Add the datafile to the fil_system cache. */
    fil_node_create(get_node_full_path(node).data(), space,
                    node.device_type() != node_device_type_t::REGULAR_FILE,
                    max_size);
    ut_ad(fil_validate());
  }
  return DB_SUCCESS;
}
#endif /* !UNIV_HOTBACKUP */

}  // namespace ib::fsp
