/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "converter.h"
#include <string.h>
#include <iostream>

namespace keyring {

const size_t WORD_32 = 4;        //<! number of bytes in 32 bit word
const size_t WORD_64 = 8;        //<! number of bytes in 64 bit word
const size_t LENGTHS_COUNT = 5;  //<! number of length values in serialization

// determine native architecture at startup
const Converter::Arch Converter::native_arch{Converter::detect_native_arch()};

/**
  fetches native machine architecture of server binary

  @return - native machine architecture
 */
Converter::Arch Converter::get_native_arch() { return native_arch; }

/**
  (static) determines width of architecture word in bytes

  @param arch     - architecture to return word width of

  @return
    @retval > 0   - word width of the architecture
    @retval = 0   - architecture has no known word width
 */
size_t Converter::get_width(Arch arch) {
  if (arch == Arch::LE_64 || arch == Arch::BE_64) return WORD_64;
  if (arch == Arch::LE_32 || arch == Arch::BE_32) return WORD_32;
  return 0;
}

/**
  (static) calculates native unsigned integer (size_t) value of binary buffer
  - buffer has to provide for at least sizeof(size_t) bytes

  @param length  - pointer to native binary representation of unsigned value

  @return        - unsigned integer (size_t) value of binary buffer value
*/
size_t Converter::native_value(char const *binary_value) {
  // overwrite variable with bytes from binary buffer
  size_t real_value = 0;
  memcpy(&real_value, binary_value, sizeof(size_t));
  return real_value;
}

/**
  (static) converts serialized key data from one architecture to another
  - key lengths word widths are expanded/contracted, key data just copied

  @param (I) data       - buffer holding key data to be converted
  @param (I) data_size  - size of the key data within buffer
  @param (I) src        - machine architecture of the input key data
  @param (I) dst        - machine architecture of the output key data
  @param (O) out        - output key data

  @return
    @retval false - data was successfully converted
    @retval true  - error occurred during conversion
 */
bool Converter::convert_data(char const *data, size_t data_size, Arch src,
                             Arch dst, std::string &out) {
  // at least source or destination has to be native
  if (src != native_arch && dst != native_arch) return true;

  // no data - we can convert that :)
  if (data_size == 0) {
    out = std::string();
    return false;
  }

  // no need to convert between identical architectures
  if (src == dst) {
    out = std::string(data, data_size);
    return false;
  }

  // get word width of source and destination architecture
  auto src_width = get_width(src);
  auto dst_width = get_width(dst);

  size_t loc = 0;
  std::string output;
  char number[WORD_64] = {0};
  size_t lengths[LENGTHS_COUNT] = {0};
  std::string key_content;

  // load remaining keys and convert them
  while (data_size >= loc + LENGTHS_COUNT * src_width) {
    key_content.clear();

    // load file length values, convert them and append
    for (size_t i = 0; i < LENGTHS_COUNT; i++) {
      // convert value to different architecture
      size_t converted_width = convert(data + loc, number, src, dst);
      if (i > 0) key_content.append(number, converted_width);

      // determine length integer value
      if (src == get_native_arch())
        lengths[i] = native_value(data + loc);
      else
        lengths[i] = native_value(number);

      // move to next length
      loc += src_width;
    }

    // real size without padding has to be smaller that total size
    size_t real_size = lengths[1] + lengths[2] + lengths[3] + lengths[4];
    if (lengths[0] < real_size) return true;

    // we also have to have at least remaining key data
    if (loc + lengths[0] - LENGTHS_COUNT * src_width > data_size) return true;

    // append only key data without padding
    key_content.append(data + loc, real_size);
    loc += lengths[0] - LENGTHS_COUNT * src_width;

    // append required padding to destination
    auto total = LENGTHS_COUNT * dst_width + real_size;
    size_t padding = (dst_width - total % dst_width) % dst_width;
    key_content.append(padding, '\0');

    // new key package size is waranted
    lengths[0] = total + padding;
    char tmp_buffer[WORD_64];
    memcpy(tmp_buffer, lengths, sizeof(size_t));

    // conversion may be necessary
    if (dst != get_native_arch()) {
      auto converted_width = convert(tmp_buffer, number, src, dst);
      output += std::string(number, converted_width);
      output += key_content;
    } else {
      output += std::string(tmp_buffer, sizeof(size_t));
      output += key_content;
    }
  }

  // we must've taken all keys
  if (loc != data_size) return true;

  // return converted data buffer
  out = output;
  return false;
}

/**
  (static) converts binary length representation between architecture types

  @param (I) src        - input binary representation
  @param (O) dst        - output binary representation
  @param (I) src_t      - architecture type of input representation
  @param (I) dst_t      - architecture type of output representation

  @return
    @retval > 0         - size of destination representation
    @retval = 0         - conversion was not possible

*/
size_t Converter::convert(char const *src, char *dst, Arch src_t, Arch dst_t) {
  // source and destination type should be known
  if (src_t == Arch::UNKNOWN || dst_t == Arch::UNKNOWN) return 0;

  // find out source and destination characteristics
  const size_t src_width = get_width(src_t);
  const size_t dst_width = get_width(dst_t);
  const bool src_is_le = get_endian(src_t) == Endian::LITTLE ? 1 : 0;
  const bool dst_is_le = get_endian(dst_t) == Endian::LITTLE ? 1 : 0;

  // determine required operations
  const bool swap = (src_is_le != dst_is_le);
  const bool grow = (src_width < dst_width);
  const bool crop = (src_width > dst_width);

  // crop security - we mustn't lose precision
  if (crop) {
    if (src_is_le)  // for little endian ending bytes must be zero
    {
      if (src[4] | src[5] | src[6] | src[7]) return 0;
    } else  // for big endian leading bytes must be zero
    {
      if (src[0] | src[1] | src[2] | src[3]) return 0;
    }
  }

  // if needed, prepare reversed number
  char swapped_src[WORD_64] = {0};
  if (swap) {
    // copy reversed byte order
    for (size_t i = 0; i < src_width; i++)
      swapped_src[i] = src[src_width - i - 1];

    // use reversed representation instead of original
    src = swapped_src;
  }

  // ================== CASE 1 - no change in bit width ================== //
  if (!grow && !crop) {
    memcpy(dst, src, dst_width);
  }
  // ================== CASE 2 - grow bit width ========================== //
  else if (grow) {
    if (dst_is_le) {
      // copy to starting bytes, clear upper bytes
      memcpy(dst, src, src_width);
      memset(dst + src_width, 0, dst_width - src_width);
    } else {
      // clear starting bytes, copy to upper bytes
      memset(dst, 0, dst_width - src_width);
      memcpy(dst + dst_width - src_width, src, src_width);
    }
  }
  // ================== CASE 3 - crop bit width ========================== //
  else if (crop) {
    if (dst_is_le)
      memcpy(dst, src, dst_width);
    else
      memcpy(dst, src + 4, dst_width);
  }

  // we're returning size of output binary representation
  return dst_width;
}

// =========================== PRIVATE UTILITIES ============================ //

/**
  (static) detects native architecture type of machine server is running on
  - to be called only at server startup

  @return - native architecture of machine
 */
Converter::Arch Converter::detect_native_arch() {
  auto type = Arch::UNKNOWN;

  // determine bit width
  const size_t bit_width = 8 * sizeof(size_t);

  // determine endianess
  size_t number = 1;
  bool isLittleEndian = *(char *)(&number);

  // assign architecture type based on findings
  switch (bit_width) {
    case 32:
      if (isLittleEndian)
        type = Arch::LE_32;
      else
        type = Arch::BE_32;
      break;

    case 64:
      if (isLittleEndian)
        type = Arch::LE_64;
      else
        type = Arch::BE_64;
      break;
  }

  return type;
}

/**
  (static) determines endianess of architecture

  @param arch   - architecture to return endianess of

  @return       - endianess of the architecture
*/
Converter::Endian Converter::get_endian(Arch arch) {
  if (arch == Arch::LE_64 || arch == Arch::LE_32) return Endian::LITTLE;
  if (arch == Arch::BE_64 || arch == Arch::BE_32) return Endian::BIG;
  return Endian::UNKNOWN;
}

} /* namespace keyring */
