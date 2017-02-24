/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "gcs_internal_message.h"
#include "gcs_message_stage_lz4.h"
#include <lz4.h>
#include <mysql/gcs/xplatform/byteorder.h>
#include "gcs_logging.h"
#include <string.h>
#include <map>
#include <limits>
#include <cassert>

const unsigned short Gcs_message_stage_lz4::WIRE_HD_UNCOMPRESSED_OFFSET=
  static_cast<unsigned short>(Gcs_message_stage::WIRE_HD_LEN_SIZE +
                              Gcs_message_stage::WIRE_HD_TYPE_SIZE);

const unsigned short Gcs_message_stage_lz4::WIRE_HD_UNCOMPRESSED_SIZE= 8;

const unsigned long long Gcs_message_stage_lz4::DEFAULT_THRESHOLD= 1024;

bool
Gcs_message_stage_lz4::apply(Gcs_packet &packet)
{
  if (packet.get_payload_length() > m_threshold)
  {
    unsigned short hd_len=
      static_cast<unsigned short>(WIRE_HD_UNCOMPRESSED_SIZE +
                                  WIRE_HD_UNCOMPRESSED_OFFSET);

    unsigned char *old_buffer= NULL;
    Gcs_internal_message_header hd;

    unsigned long long fixed_header_len= packet.get_header_length();
    unsigned long long old_payload_len= packet.get_payload_length();

    // we are compressing the payload, but not the header
    int compress_bound= LZ4_compressBound(static_cast<int>(old_payload_len));
    /*
      Currently, this function can just compress packets smaller than
      LZ4_MAX_INPUT_SIZE. old_payload_len > max() must be tested, because high
      32bits of old_payload_len bigger than max() will be truncated when
      passing to LZ4_compressBound. The truncated value may be small enough
      and the function will think it can be compressed and return an valid
      value. E.g 0x100000001 will became 0x1 after high 32bits are truncated.
    */
    if (old_payload_len > std::numeric_limits<unsigned int>::max() ||
        compress_bound <= 0)
    {
      MYSQL_GCS_LOG_ERROR("Gcs_packet's payload is too big. Only the packets "
                          "smaller than 2113929216 bytes can be compressed.");
      return true;
    }


    unsigned long long new_packet_len= fixed_header_len + hd_len + compress_bound;
    int compressed_len= 0;
    // align to Gcs_packet::BLOCK_SIZE
    unsigned long long new_capacity= ((new_packet_len / Gcs_packet::BLOCK_SIZE) + 1) *
                                        Gcs_packet::BLOCK_SIZE;
    unsigned char *new_buffer= (unsigned char*) malloc(new_capacity);
    unsigned char *new_payload_ptr= new_buffer + fixed_header_len + hd_len;

    // compress payload
    compressed_len= LZ4_compress_default((const char*)packet.get_payload(),
                                         (char*)new_payload_ptr,
                                         static_cast<int>(old_payload_len),
                                         compress_bound);

    new_packet_len= fixed_header_len + hd_len + compressed_len;

    // swap buffers
    old_buffer= packet.swap_buffer(new_buffer, new_capacity);

    // copy the header and fix a couple of fields in it
    hd.decode(old_buffer);  // decode old information
    hd.set_msg_length(new_packet_len);
    hd.set_dynamic_headers_length(hd.get_dynamic_headers_length() + hd_len);
    hd.encode(packet.get_buffer());  // encode to the new buffer

    // reload the header details into the packet
    packet.reload_header(hd);

    // encode the new dynamic header into the buffer
    encode(packet.get_payload(),
           hd_len, Gcs_message_stage::ST_LZ4, old_payload_len);

    // delete the temp buffer
    free(old_buffer);
  }

  return false;
}

bool
Gcs_message_stage_lz4::revert(Gcs_packet &packet)
{
  // if there are valid headers in the packet
  if (packet.get_dyn_headers_length() > 0)
  {
    Gcs_message_stage::enum_type_code type_code;
    unsigned short hd_len;

    unsigned char *old_buffer= NULL;
    Gcs_internal_message_header hd;

    unsigned long long fixed_header_size= packet.get_header_length();
    unsigned long long old_payload_len= packet.get_payload_length();
    unsigned long long new_length= 0;
    unsigned long long uncompressed_size= 0;

    // align to Gcs_packet::BLOCK_SIZE
    unsigned long long new_capacity= 0;

    // decode the header
    decode(packet.get_payload(), &hd_len, &type_code, &uncompressed_size);

    /* assert(type_code==this->type_code()); */

    new_capacity= (((uncompressed_size + fixed_header_size) /
                     Gcs_packet::BLOCK_SIZE) + 1) * Gcs_packet::BLOCK_SIZE;

    unsigned char *new_buffer= (unsigned char*) malloc(new_capacity);
    if (!new_buffer)
      return true;
    unsigned char *compressed_payload_ptr= packet.get_payload() + hd_len;
    unsigned char *new_payload_ptr= new_buffer + fixed_header_size;

    // This is the deserialization part, so assertions are fine.
    assert(old_payload_len < std::numeric_limits<unsigned int>::max());
    assert(uncompressed_size < std::numeric_limits<unsigned int>::max());
    // decompress to the new buffer
    int src_len= static_cast<int>(old_payload_len - hd_len);
    int dest_len= static_cast<int>(uncompressed_size);
    int uncompressed_len=
      LZ4_decompress_safe((const char *)compressed_payload_ptr,
                          (char*)new_payload_ptr, src_len, dest_len);

    if(uncompressed_len < 0)
    {
      free(new_buffer);
      return true;
    }

    // effective length of the packet
    new_length= fixed_header_size + uncompressed_len;

    // swap buffers
    old_buffer= packet.swap_buffer(new_buffer, new_capacity);

    // copy the old headers and fix a couple of fields in it
    hd.decode(old_buffer);  // decode old information
    hd.set_dynamic_headers_length(hd.get_dynamic_headers_length() - hd_len);
    hd.set_msg_length(new_length);
    hd.encode(packet.get_buffer());  // encode to the new buffer

    // reload the header into the packet
    packet.reload_header(hd);

    // delete the temp buffer
    free(old_buffer);
  }

  return false;
}

void
Gcs_message_stage_lz4::encode(unsigned char *hd,
       unsigned short hd_len,
       Gcs_message_stage::enum_type_code type_code,
       unsigned long long uncompressed)
{
  unsigned int type_code_enc= (unsigned int) type_code;

  unsigned short hd_len_enc= htole16(hd_len);
  memcpy(hd + WIRE_HD_LEN_OFFSET,
         &hd_len_enc, WIRE_HD_LEN_SIZE);

  // encode filter header - enums may have different storage
  // sizes, force to 32 bits
  type_code_enc= htole32(type_code_enc);
  memcpy(hd + WIRE_HD_TYPE_OFFSET,
         &type_code_enc, WIRE_HD_TYPE_SIZE);

  unsigned long long uncompressed_enc= htole64(uncompressed);
  memcpy(hd + WIRE_HD_UNCOMPRESSED_OFFSET,
         &uncompressed_enc, WIRE_HD_UNCOMPRESSED_SIZE);
}

void
Gcs_message_stage_lz4::decode(const unsigned char *hd,
                              unsigned short *hd_len,
                              Gcs_message_stage::enum_type_code *type,
                              unsigned long long *uncompressed)
{
  const unsigned char *slider= hd;
  unsigned int type_code_enc;

  memcpy(hd_len, slider, WIRE_HD_LEN_SIZE);
  *hd_len= le16toh(*hd_len);
  slider += WIRE_HD_LEN_SIZE;

  // enums may require more than four bytes. We force this to 4 bytes.
  memcpy(&type_code_enc, slider, WIRE_HD_TYPE_SIZE);
  type_code_enc= le32toh(type_code_enc);
  *type= (Gcs_message_stage::enum_type_code) type_code_enc;
  slider += WIRE_HD_TYPE_SIZE;

  memcpy(uncompressed, slider, WIRE_HD_UNCOMPRESSED_SIZE);
  *uncompressed= le64toh(*uncompressed);
}
