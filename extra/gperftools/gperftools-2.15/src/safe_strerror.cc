/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*-
 * Copyright (c) 2023, gperftools Contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "safe_strerror.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

namespace tcmalloc {

namespace {

const char* TryMapErrno(int errnum) {

#define C(v) if (errnum == v) return #v
#ifdef E2BIG
C(E2BIG);
#endif
#ifdef EACCES
C(EACCES);
#endif
#ifdef EADDRINUSE
C(EADDRINUSE);
#endif
#ifdef EADDRNOTAVAIL
C(EADDRNOTAVAIL);
#endif
#ifdef EAFNOSUPPORT
C(EAFNOSUPPORT);
#endif
#ifdef EAGAIN
C(EAGAIN);
#endif
#ifdef EALREADY
C(EALREADY);
#endif
#ifdef EBADF
C(EBADF);
#endif
#ifdef EBADMSG
C(EBADMSG);
#endif
#ifdef EBUSY
C(EBUSY);
#endif
#ifdef ECANCELED
C(ECANCELED);
#endif
#ifdef ECHILD
C(ECHILD);
#endif
#ifdef ECONNABORTED
C(ECONNABORTED);
#endif
#ifdef ECONNREFUSED
C(ECONNREFUSED);
#endif
#ifdef ECONNRESET
C(ECONNRESET);
#endif
#ifdef EDEADLK
C(EDEADLK);
#endif
#ifdef EDESTADDRREQ
C(EDESTADDRREQ);
#endif
#ifdef EDOM
C(EDOM);
#endif
#ifdef EDQUOT
C(EDQUOT);
#endif
#ifdef EEXIST
C(EEXIST);
#endif
#ifdef EFAULT
C(EFAULT);
#endif
#ifdef EFBIG
C(EFBIG);
#endif
#ifdef EHOSTUNREACH
C(EHOSTUNREACH);
#endif
#ifdef EIDRM
C(EIDRM);
#endif
#ifdef EILSEQ
C(EILSEQ);
#endif
#ifdef EINPROGRESS
C(EINPROGRESS);
#endif
#ifdef EINTR
C(EINTR);
#endif
#ifdef EINVAL
C(EINVAL);
#endif
#ifdef EIO
C(EIO);
#endif
#ifdef EISCONN
C(EISCONN);
#endif
#ifdef EISDIR
C(EISDIR);
#endif
#ifdef ELOOP
C(ELOOP);
#endif
#ifdef EMFILE
C(EMFILE);
#endif
#ifdef EMLINK
C(EMLINK);
#endif
#ifdef EMSGSIZE
C(EMSGSIZE);
#endif
#ifdef EMULTIHOP
C(EMULTIHOP);
#endif
#ifdef ENAMETOOLONG
C(ENAMETOOLONG);
#endif
#ifdef ENETDOWN
C(ENETDOWN);
#endif
#ifdef ENETRESET
C(ENETRESET);
#endif
#ifdef ENETUNREACH
C(ENETUNREACH);
#endif
#ifdef ENFILE
C(ENFILE);
#endif
#ifdef ENOBUFS
C(ENOBUFS);
#endif
#ifdef ENODATA
C(ENODATA);
#endif
#ifdef ENODEV
C(ENODEV);
#endif
#ifdef ENOENT
C(ENOENT);
#endif
#ifdef ENOEXEC
C(ENOEXEC);
#endif
#ifdef ENOLCK
C(ENOLCK);
#endif
#ifdef ENOLINK
C(ENOLINK);
#endif
#ifdef ENOMEM
C(ENOMEM);
#endif
#ifdef ENOMSG
C(ENOMSG);
#endif
#ifdef ENOPROTOOPT
C(ENOPROTOOPT);
#endif
#ifdef ENOSPC
C(ENOSPC);
#endif
#ifdef ENOSR
C(ENOSR);
#endif
#ifdef ENOSTR
C(ENOSTR);
#endif
#ifdef ENOSYS
C(ENOSYS);
#endif
#ifdef ENOTCONN
C(ENOTCONN);
#endif
#ifdef ENOTDIR
C(ENOTDIR);
#endif
#ifdef ENOTEMPTY
C(ENOTEMPTY);
#endif
#ifdef ENOTRECOVERABLE
C(ENOTRECOVERABLE);
#endif
#ifdef ENOTSOCK
C(ENOTSOCK);
#endif
#ifdef ENOTSUP
C(ENOTSUP);
#endif
#ifdef ENOTTY
C(ENOTTY);
#endif
#ifdef ENXIO
C(ENXIO);
#endif
#ifdef EOPNOTSUPP
C(EOPNOTSUPP);
#endif
#ifdef EOVERFLOW
C(EOVERFLOW);
#endif
#ifdef EOWNERDEAD
C(EOWNERDEAD);
#endif
#ifdef EPERM
C(EPERM);
#endif
#ifdef EPIPE
C(EPIPE);
#endif
#ifdef EPROTO
C(EPROTO);
#endif
#ifdef EPROTONOSUPPORT
C(EPROTONOSUPPORT);
#endif
#ifdef EPROTOTYPE
C(EPROTOTYPE);
#endif
#ifdef ERANGE
C(ERANGE);
#endif
#ifdef EROFS
C(EROFS);
#endif
#ifdef ESPIPE
C(ESPIPE);
#endif
#ifdef ESRCH
C(ESRCH);
#endif
#ifdef ESTALE
C(ESTALE);
#endif
#ifdef ETIME
C(ETIME);
#endif
#ifdef ETIMEDOUT
C(ETIMEDOUT);
#endif
#ifdef ETXTBSY
C(ETXTBSY);
#endif
#ifdef EWOULDBLOCK
C(EWOULDBLOCK);
#endif
#ifdef EXDEV
C(EXDEV);
#endif
#undef C

  return nullptr;
}

}  // namespace

SafeStrError::SafeStrError(int errnum) {
  result_ = TryMapErrno(errnum);
  if (result_ == nullptr) {
    snprintf(buf_, sizeof(buf_), "errno %d", errnum);
    result_ = buf_;
  }
}

}  // namespace tcmalloc
