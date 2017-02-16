/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */

#ifndef HANDSHAKE_H
#define HANDSHAKE_H

#include "common.h"

/**
  Name of the SSP (Security Support Provider) to be used for authentication.

  We use "Negotiate" which will find the most secure SSP which can be used
  and redirect to that SSP.
*/
#define SSP_NAME  "Negotiate"

/**
  Maximal number of rounds in authentication handshake.

  Server will interrupt authentication handshake with error if client's
  identity can not be determined within this many rounds.
*/
#define MAX_HANDSHAKE_ROUNDS  50


/// Convenience wrapper around @c SecBufferDesc.

class Security_buffer: public SecBufferDesc
{
  SecBuffer m_buf;        ///< A @c SecBuffer instance.

  void init(byte *ptr, size_t len)
  {
    ulVersion= 0;
    cBuffers=  1;
    pBuffers=  &m_buf;

    m_buf.BufferType= SECBUFFER_TOKEN;
    m_buf.pvBuffer= ptr;
    m_buf.cbBuffer= len;
  }

  /// If @c false, no deallocation will be done in the destructor.
  bool m_allocated;

 public:

  Security_buffer(const Blob&);
  Security_buffer();

  ~Security_buffer()
  {
    free();
  }

  byte*  ptr() const
  {
    return (byte*)m_buf.pvBuffer;
  }

  size_t len() const
  {
    return m_buf.cbBuffer;
  }

  bool is_valid() const
  {
    return ptr() != NULL;
  }

  const Blob as_blob() const
  {
    return Blob(ptr(), len());
  }

  void free(void);
};


/// Common base for Handshake_{server,client}.

class Handshake
{
public:

  typedef enum {CLIENT, SERVER} side_t;

  Handshake(const char *ssp, side_t side);
  virtual ~Handshake();

  int Handshake::packet_processing_loop();

  bool virtual is_complete() const
  {
    return m_complete;
  }

  int error() const
  {
    return m_error;
  }

protected:

  /// Security context object created during the handshake.
  CtxtHandle  m_sctx;

  /// Credentials of the principal performing this handshake.
  CredHandle  m_cred;

  /// Stores expiry date of the created security context.
  TimeStamp  m_expire;

  /// Stores attributes of the created security context.
  ULONG  m_atts;

  /**
    Round of the handshake (starting from round 1). One round
    consist of reading packet from the other side, processing it and
    optionally sending a reply (see @c packet_processing_loop()).
  */
  unsigned int m_round;

  /// If non-zero, stores error code of the last failed operation.
  int  m_error;

  /// @c true when handshake is complete.
  bool  m_complete;

  /// @c true when the principal credentials has been determined.
  bool  m_have_credentials;

  /// @c true when the security context has been created.
  bool  m_have_sec_context;

  /// Buffer for data to be send to the other side.
  Security_buffer  m_output;

  bool process_result(int);

  /**
    This method is used inside @c packet_processing_loop to process
    data packets received from the other end.

    @param[IN]  data  data to be processed

    @return A blob with data to be sent to the other end or null blob if
    no more data needs to be exchanged.
  */
  virtual Blob process_data(const Blob &data) =0;

  /// Read packet from the other end.
  virtual Blob read_packet()  =0;

  /// Write packet to the other end.
  virtual int  write_packet(Blob &data) =0;

#ifndef DBUG_OFF

private:
  SecPkgInfo  *m_ssp_info;
public:
  const char* ssp_name();

#endif
};


#endif
