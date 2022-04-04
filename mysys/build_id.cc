/* Copyright (c) 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <assert.h>
#include <elf.h>
#include <link.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/*
  Assuming we link with: -Wl,--build-id=sha1
  we will get a build-id which is:
     "a 160-bit SHA1 hash on the normative parts of the output contents."
  This can be used to uniquely identify the executable.
 */

using elf_word = ElfW(Word);

// Align to 4 bytes:
constexpr elf_word NOTE_ALIGN(elf_word sz) { return (((sz) + 3) & ~3); }

struct callback_data {
  Elf64_Word size{0};
  const uint8_t *build_id{nullptr};
};

/*
  typedef struct
  {
    Elf32_Word n_namesz;  Length of the note's name.
    Elf32_Word n_descsz;  Length of the note's descriptor.
    Elf32_Word n_type;    Type of the note.
  } Elf32_Nhdr;

  NT_GNU_BUILD_ID == 3

  So we are looking for:
  {{4, 20, 3}, "GNU", <20 bytes build-id>}
*/

using elf_note_struct = ElfW(Nhdr);

struct elf_note {
  elf_note_struct nhdr;
  char name[4];
};

// Callback for dl_iterate_phdr().
// A nonzero return here will terminate iteration.
static int build_id_callback(dl_phdr_info *info, size_t, void *data_) {
  /*
    The first object visited by the callback is the main program.
    For the main program, the dlpi_name field will be an empty string.
  */
  if (info->dlpi_name == nullptr || strcmp(info->dlpi_name, "") != 0) {
    assert(false);
    return 0;
  }

  callback_data *data = reinterpret_cast<callback_data *>(data_);

  for (unsigned i = 0; i < info->dlpi_phnum; i++) {
    if (info->dlpi_phdr[i].p_type != PT_NOTE) continue;

    elf_note *note = reinterpret_cast<elf_note *>(
        reinterpret_cast<void *>(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr));
    ptrdiff_t segment_size = info->dlpi_phdr[i].p_filesz;

    while (segment_size > 0 &&
           segment_size >= static_cast<ptrdiff_t>(sizeof(elf_note))) {
      if (note->nhdr.n_type == NT_GNU_BUILD_ID && note->nhdr.n_descsz != 0 &&
          note->nhdr.n_namesz == 4 && memcmp(note->name, "GNU", 4) == 0) {
        // build_id is right after the name.
        data->build_id =
            reinterpret_cast<unsigned char *>(note) + sizeof(elf_note);
        data->size = note->nhdr.n_descsz;
        return 1;
      }
      // Skip to the next note:
      size_t offset = sizeof(elf_note_struct) +
                      NOTE_ALIGN(note->nhdr.n_namesz) +
                      NOTE_ALIGN(note->nhdr.n_descsz);
      note = reinterpret_cast<elf_note *>(
          reinterpret_cast<unsigned char *>(note) + offset);
      segment_size -= offset;
    }
  }

  return 0;
}

bool my_find_build_id(char *dst) {
  callback_data data;
  if (!dl_iterate_phdr(build_id_callback, &data)) return true;

  assert(data.size == 20);
  for (ElfW(Word) i = 0; i < data.size; i++) {
    sprintf(&dst[2 * i], "%02x", data.build_id[i]);
  }
  return false;
}
