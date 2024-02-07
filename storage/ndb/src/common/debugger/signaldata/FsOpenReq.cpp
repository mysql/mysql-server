/* Copyright (c) 2003, 2024, Oracle and/or its affiliates.

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

#include <signaldata/FsOpenReq.hpp>

bool printFSOPENREQ(FILE *output, const Uint32 *theData, Uint32 len,
                    Uint16 /*receiverBlockNo*/) {
  if (len < FsOpenReq::SignalLength) {
    assert(false);
    return false;
  }

  const FsOpenReq *const sig = (const FsOpenReq *)theData;

  fprintf(output, " UserReference: H\'%.8x, userPointer: H\'%.8x\n",
          sig->userReference, sig->userPointer);
  fprintf(output, " FileNumber[1-4]: H\'%.8x H\'%.8x H\'%.8x H\'%.8x\n",
          sig->fileNumber[0], sig->fileNumber[1], sig->fileNumber[2],
          sig->fileNumber[3]);
  fprintf(output, " FileFlags: H\'%.8x ", sig->fileFlags);

  // File open mode must be one of ReadOnly, WriteOnly or ReadWrite
  const Uint32 flags = sig->fileFlags;
  switch (flags & 3) {
    case FsOpenReq::OM_READONLY:
      fprintf(output, "Open read only");
      break;
    case FsOpenReq::OM_WRITEONLY:
      fprintf(output, "Open write only");
      break;
    case FsOpenReq::OM_READWRITE:
      fprintf(output, "Open read and write");
      break;
    default:
      fprintf(output, "Open mode unknown!");
  }

  if (flags & FsOpenReq::OM_APPEND) fprintf(output, ", Append");
  if (flags & FsOpenReq::OM_SYNC) fprintf(output, ", Sync");
  if (flags & FsOpenReq::OM_CREATE) fprintf(output, ", Create new file");
  if (flags & FsOpenReq::OM_TRUNCATE)
    fprintf(output, ", Truncate existing file");
  if (flags & FsOpenReq::OM_AUTOSYNC) fprintf(output, ", Auto Sync");

  if (flags & FsOpenReq::OM_CREATE_IF_NONE) fprintf(output, ", Create if None");
  if (flags & FsOpenReq::OM_INIT) fprintf(output, ", Initialise");
  if (flags & FsOpenReq::OM_CHECK_SIZE) fprintf(output, ", Check Size");
  if (flags & FsOpenReq::OM_DIRECT) fprintf(output, ", O_DIRECT");
  if (flags & FsOpenReq::OM_GZ) fprintf(output, ", gz compressed");
  if (flags & FsOpenReq::OM_THREAD_POOL) fprintf(output, ", threadpool");
  if (flags & FsOpenReq::OM_WRITE_BUFFER) fprintf(output, ", write buffer");
  if (flags & FsOpenReq::OM_READ_SIZE) fprintf(output, ", read size");
  if (flags & FsOpenReq::OM_DIRECT_SYNC) fprintf(output, ", O_DIRECT_SYNC");
  if (flags & FsOpenReq::OM_ENCRYPT_CIPHER_MASK) fprintf(output, ", encrypted");
  if ((flags & FsOpenReq::OM_ENCRYPT_CIPHER_MASK) == FsOpenReq::OM_ENCRYPT_CBC)
    fprintf(output, ", with cbc");
  if ((flags & FsOpenReq::OM_ENCRYPT_CIPHER_MASK) == FsOpenReq::OM_ENCRYPT_XTS)
    fprintf(output, ", with xts");
  if ((flags & FsOpenReq::OM_ENCRYPT_KEY_MATERIAL_MASK) ==
      FsOpenReq::OM_ENCRYPT_PASSWORD)
    fprintf(output, ", with password");
  if ((flags & FsOpenReq::OM_ENCRYPT_KEY_MATERIAL_MASK) ==
      FsOpenReq::OM_ENCRYPT_KEY)
    fprintf(output, ", with key");
  if (flags & FsOpenReq::OM_READ_FORWARD) fprintf(output, ", read forward");

  fprintf(output, "\n");
  return true;
}
