/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#ifndef MYSQL_JSON_ENCODE_H
#define MYSQL_JSON_ENCODE_H

#include <mysql/components/service.h>
#include <mysql/components/services/mysql_string.h>

/**
  @ingroup group_components_services_inventory

  A specialized service that transcodes input text from specified encoding into
  UTF-8 (MB4) and escapes JSON characters. Regular encoders require that
  a source is placed in single buffer and the size of the output buffer cannot
  be determined (it must be counted). This service allows to transcode input
  buffer in chunks.
*/
BEGIN_SERVICE_DEFINITION(mysql_json_encode)

/**
 Transcode input buffer (a chunk of data) into destination buffer.

 @param [in]  src          Input buffer pointer.
 @param [in]  src_end      When dividing input stream into smaller chunks, this
                           pointer must be set to src_data_end -
                           max_encoded_char - 1, where max_encoded_char is the
                           maximum lenght of the character in the specified
                           encoding (charset argument). This ensures that a
                           partial character is not consumed. The last chunk
                           should be called with src_end == src_data_end and the
                           returned pointer should point to src_data_end.
 @param [in]  src_data_end Input buffer end pointer.
 @param [in]  dst          Destination buffer.
 @param [in]  dst_end      Destination buffer end. The buffer should be at least
                           6 buffer long (JSON encodes single character as
                           \\uXXXX), which can be a case in a single encode
                           method call.
 @param [in]  charset      Input data encoding.
 @param [out] dst_out      Pointer ending the transcoded data buffer. The
                           pointer is not part of the transcoded data.

 @return Pointer to the next data withing the input buffer to be transcoded.
*/
DECLARE_METHOD(const unsigned char *, encode,
               (const unsigned char *src, const unsigned char *src_end,
                const unsigned char *src_data_end, unsigned char *dst,
                unsigned char *dst_end, const CHARSET_INFO_h charset,
                unsigned char **dst_out));

END_SERVICE_DEFINITION(mysql_json_encode)

#endif /* MYSQL_JSON_ENCODE_H */
