/*
   Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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
#include <assert.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <memory>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "ndb_global.h"  // DIR_SEPARATOR, access()
#include "ndb_limits.h"

#include "debugger/EventLogger.hpp"
#include "portlib/ndb_localtime.h"
#include "util/File.hpp"
#include "util/ndb_openssl3_compat.h"
#include "util/require.h"

#include "util/NodeCertificate.hpp"

#ifdef _WIN32
#include <direct.h>
#define gmtime_r(A, B) (gmtime_s(B, A) == 0)
#define timegm(A) _mkgmtime(A)
#define getcwd _getcwd
#else
inline void _putenv(const char *a) { putenv(const_cast<char *>(a)); }
#endif

/* CN length is limited to 64 characters per RFC 5280:
 */
static constexpr size_t CN_max_length = 65;

static void handle_pem_error(const char fn_name[]) {
  int err = ERR_peek_last_error();
  if (err != 0) {
    char buffer[256];
    ERR_error_string_n(err, buffer, 256);
    g_eventLogger->error("NDB TLS %s: %s", fn_name, buffer);
  } else {
    // Expected some error
    g_eventLogger->error("NDB TLS %s: Expected error but found none.", fn_name);
  }
  /*
   * Check that this function are called for every failed function call that
   * have set an error.
   */
#if defined(VM_TRACE) || !defined(NDEBUG) || defined(ERROR_INSERT)
  require(ERR_get_error() != 0);  // At least one error
#endif
  while (ERR_get_error() != 0) /* clear SSL error stack without checks */
    ;
}

/*
 * Implementation of certificate and key file naming conventions
 */

bool PkiFile::remove(const PathName &path) {
  return File_class::remove(path.c_str());
}

int PkiFile::assign(PathName &path, const char *dir, const char *file) {
  path.clear();
  if (dir == nullptr || dir[0] == '\0')
    path.append(file);
  else {
    path.append(dir);
    path.append(DIR_SEPARATOR);
    path.append(file);
  }
  return path.is_truncated();
}

static void expand(BaseString &result, const BaseString &path, int envStart) {
  const char *item = path.c_str();
  size_t envEnd = envStart + 1;
  char c = item[envEnd];
  while (isalnum(c) || c == '_') {
    envEnd++;
    c = item[envEnd];
  }

  if (envStart > 0) result.assign(path.substr(0, envStart));
  if (envEnd - envStart > 1) {
    BaseString envVar = path.substr(envStart + 1, envEnd);
    result.append(getenv(envVar.c_str()));
  } else {
    result.append('$');
  }
  result.append(path.substr(envEnd));
}

TlsSearchPath::TlsSearchPath(const char *path_str) {
  /* Split into an array of directories stored in m_path.
     The empty string "" signifies zero directories.
     A string consisting of a single dot "." signifies just the cwd.
  */
  if (path_str && path_str[0]) {
    BaseString path(path_str);
    if (path == ".")
      m_path.push_back(BaseString(""));
    else
      path.split(m_path, Separator);
  }

  /* Expand environment variables */
  for (size_t i = 0; i < m_path.size(); i++) {
    ssize_t envStart = m_path[i].indexOf('$');
    if (envStart > -1) {
      BaseString expansion;
      expand(expansion, m_path[i], envStart);
      if (expansion.length())
        m_path[i] = expansion;
      else {
        m_path.erase(i);
        i--;
      }
    }
  }
}

void TlsSearchPath::push_cwd() {
  for (size_t i = 0; i < m_path.size(); i++)
    if (m_path[i].length() == 0) return;
  m_path.push_back(BaseString(""));
}

bool TlsSearchPath::find(const char *name, PkiFile::PathName &buffer) const {
  struct stat s;

  for (size_t i = 0; i < m_path.size(); i++) {
    buffer.clear();
    buffer.append(m_path[i].c_str());
    if (m_path[i].length()) buffer.append(DIR_SEPARATOR);
    buffer.append(name);
    if (!buffer.is_truncated())
      if (stat(buffer.c_str(), &s) == 0) return true;
  }
  return false;
}

int TlsSearchPath::find(const char *name) const {
  cstrbuf<PATH_MAX> file_buf;
  struct stat s;

  for (size_t i = 0; i < m_path.size(); i++) {
    file_buf.append(m_path[i].c_str());
    if (m_path[i].length()) file_buf.append(DIR_SEPARATOR);
    file_buf.append(name);

    if (!file_buf.is_truncated())
      if (stat(file_buf.c_str(), &s) == 0) return i;

    file_buf.clear();
  }
  return -1;
}

const char *TlsSearchPath::dir(unsigned int i) const {
  return (i > m_path.size()) ? nullptr : m_path[i].c_str();
}

const char *TlsSearchPath::first_writable() const {
  for (size_t i = 0; i < m_path.size(); i++)
    if (writable(i)) return m_path[i].c_str();
  return nullptr;
}

bool TlsSearchPath::writable(unsigned int i) const {
  char cwd[PATH_MAX];
  const char *dir = cwd;

  if (i >= m_path.size()) return false;

  if (m_path[i].length())
    dir = m_path[i].c_str();
  else if (!getcwd(cwd, PATH_MAX))
    return false;

  return (access(dir, W_OK) == 0);
}

char *TlsSearchPath::expanded_path_string() const {
  BaseString p;
  for (size_t i = 0; i < m_path.size(); i++) {
    if (i) p.append(Separator);
    p.append(m_path[i]);
  }
  return strdup(p.c_str());
}

class PkiFilenames {
 public:
  PkiFilenames(int node_id, Node::Type, PkiFile::Type);

  short find_file(const TlsSearchPath *path,
                  PkiFile::PathName &path_buffer) const;
  const char *first() { return m_list[0].c_str(); }

  static const char *suffix(PkiFile::Type t) { return type_names[(int)t]; }

  static int client_file(PkiFile::Type, PkiFile::FileName &);  // ndb-api-%
  static int data_node_file(PkiFile::Type,
                            PkiFile::FileName &);            // ndb-data-...
  static int mgmd_file(PkiFile::Type, PkiFile::FileName &);  // ndb-mgm-server-%

 private:
  static constexpr int Max_List = 3;
  static constexpr const char *type_names[7] = {
      "pending-key",  "private-key", "retired-key", "cert-request",
      "pending-cert", "cert",        "retired-cert"};

  PkiFile::FileName &current(int score) {
    m_score[m_size] = score;
    return m_list[m_size++];
  }

  short m_score[Max_List];
  PkiFile::FileName m_list[Max_List];
  int m_size{0};
};

int PkiFilenames::client_file(PkiFile::Type file_type, PkiFile::FileName &buf) {
  buf.clear();
  const char *file_suffix = type_names[(int)file_type];
  return buf.appendf("ndb-api-%s", file_suffix);
}

int PkiFilenames::data_node_file(PkiFile::Type file_type,
                                 PkiFile::FileName &buf) {
  buf.clear();
  const char *file_suffix = type_names[(int)file_type];
  return buf.appendf("ndb-data-node-%s", file_suffix);
}

int PkiFilenames::mgmd_file(PkiFile::Type file_type, PkiFile::FileName &buf) {
  buf.clear();
  const char *file_suffix = type_names[(int)file_type];
  return buf.appendf("ndb-mgm-server-%s", file_suffix);
}

PkiFilenames::PkiFilenames(int, Node::Type node_type, PkiFile::Type file_type) {
  if (Node::And(node_type, Node::Type::MGMD)) mgmd_file(file_type, current(3));

  if (Node::And(node_type, Node::Type::DB))
    data_node_file(file_type, current(2));

  if (Node::And(node_type, Node::Type::Client))
    client_file(file_type, current(1));
}

/*  Find a PKI file.
    Takes pointer to buffer for that will hold full pathname to file.
    Returns 0 if not found, or a preference score from 1 to 5 if found.
*/
short PkiFilenames::find_file(const TlsSearchPath *path,
                              PkiFile::PathName &path_buffer) const {
  for (int i = 0; i < m_size; i++)
    if (path->find(m_list[i].c_str(), path_buffer)) return m_score[i];
  return 0;
}

static bool promote_file(const char *pending, const char *active,
                         const char *retired) {
#ifdef _WIN32
  rename(active, retired);  // this may fail if active does not exist
#else
  File_class::remove(retired);  // this may fail if retired does not exist
  if (link(active, retired)) {
  }  // this may fail if active does not exist
#endif
  return (rename(pending, active) == 0);
}

/*
 *    PrivateKey class
 */
EVP_PKEY *PrivateKey::create(const char *curve) {
  return EVP_EC_generate(curve);
}

EVP_PKEY *PrivateKey::open(const char *path, char *passphrase) {
  EVP_PKEY *key = nullptr;
  FILE *fp = fopen(path, "r");
  if (fp != nullptr) {
    PEM_read_PrivateKey(fp, &key, nullptr, passphrase);
    if (!key) handle_pem_error("PEM_read_PrivateKey");
    fclose(fp);
  }
  return key;
}

bool PrivateKey::store(EVP_PKEY *key, const PkiFile::PathName &path,
                       char *passphrase, bool encrypted) {
  FILE *fp = nullptr;
  int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, S_IRUSR);
  if (fd > 0) fp = fdopen(fd, "w");
  if (!fp) return false;

  const EVP_CIPHER *enc = encrypted ? EVP_des_ede3_cbc() : nullptr;

  if (PEM_write_PKCS8PrivateKey(fp, key, enc, nullptr, 0, nullptr,
                                passphrase)) {
    fclose(fp);
    return true;
  } else {
    handle_pem_error("PEM_write_PKCS8PrivateKey");
    fclose(fp);
    PkiFile::remove(path);
    return false;
  }
}

bool PrivateKey::store(EVP_PKEY *key, const char *dir, const char *file,
                       char *passphrase) {
  PkiFile::PathName pathname;
  PkiFile::assign(pathname, dir, file);
  return PrivateKey::store(key, pathname, passphrase, true);
}

void PrivateKey::free(EVP_PKEY *key) { EVP_PKEY_free(key); }

/*
 *    PendingPrivateKey class
 */
short PendingPrivateKey::find(const TlsSearchPath *searchPath, int node_id,
                              Node::Type type, PkiFile::PathName &path_buffer) {
  PkiFilenames list(node_id, type, PkiFile::Type::PendingKey);
  return list.find_file(searchPath, path_buffer);
}

bool PendingPrivateKey::store(EVP_PKEY *key, const char *dir,
                              const CertSubject &cert) {
  PkiFile::PathName pathname;
  PkiFile::FileName file;
  cert.filename(PkiFile::Type::PendingKey, file);
  PkiFile::assign(pathname, dir, file.c_str());
  return PrivateKey::store(key, pathname, nullptr, false); /* Not encrypted */
}

bool PendingPrivateKey::promote(const PkiFile::PathName &pending_file) {
  PkiFile::PathName active;
  PkiFile::PathName retired;

  const char *suffix1 = PkiFilenames::suffix(PkiFile::Type::PendingKey),
             *suffix2 = PkiFilenames::suffix(PkiFile::Type::ActiveKey),
             *suffix3 = PkiFilenames::suffix(PkiFile::Type::RetiredKey);
  size_t len = strlen(suffix1);

  std::string_view base = pending_file;  // make a copy
  base.remove_suffix(len);

  active.append(base);
  active.append(suffix2);
  if (active.is_truncated()) return false;

  retired.append(base);
  retired.append(suffix3);
  if (retired.is_truncated()) return false;

  return promote_file(pending_file.c_str(), active.c_str(), retired.c_str());
}

/*
 *    ActivePrivateKey class
 */
short ActivePrivateKey::find(const TlsSearchPath *searchPath, int node_id,
                             Node::Type type, PkiFile::PathName &path_buffer) {
  PkiFilenames list(node_id, type, PkiFile::Type::ActiveKey);
  return list.find_file(searchPath, path_buffer);
}

/*
 *    SigningRequest class
 */
SigningRequest::SigningRequest(X509_REQ *req, Node::Type type, int node_id)
    : CertSubject(type, node_id), CertLifetime(DefaultDays), m_req(req) {
  m_bound_hostnames = sk_GENERAL_NAME_new_null();
}

SigningRequest::SigningRequest(X509_REQ *req)
    : CertSubject(), CertLifetime(DefaultDays), m_req(req) {
  parse_name();
  m_bound_hostnames = sk_GENERAL_NAME_new_null();
  int idx = -1;
  STACK_OF(X509_EXTENSION) *x = X509_REQ_get_extensions(m_req);
  if (x) {
    STACK_OF(GENERAL_NAME) *gn = static_cast<struct stack_st_GENERAL_NAME *>(
        X509V3_get_d2i(x, NID_subject_alt_name, nullptr, &idx));
    if (gn) {
      for (int i = 0; i < sk_GENERAL_NAME_num(gn); i++)
        sk_GENERAL_NAME_push(m_bound_hostnames,
                             GENERAL_NAME_dup(sk_GENERAL_NAME_value(gn, i)));
      sk_GENERAL_NAME_pop_free(gn, GENERAL_NAME_free);
    }
    sk_X509_EXTENSION_pop_free(x, X509_EXTENSION_free);
  }
}

SigningRequest::~SigningRequest() {
  X509_REQ_free(m_req);
  /* Sometimes we see "pointer beeing freed was not allocated" here */
  require(m_bound_hostnames);
  if (m_names_owner)
    sk_GENERAL_NAME_pop_free(m_bound_hostnames, GENERAL_NAME_free);
}

SigningRequest *SigningRequest::create(EVP_PKEY *key, Node::Type type) {
  X509_REQ *req = X509_REQ_new();
  if (!req) return nullptr;

  /* Set the key in the request */
  if (!X509_REQ_set_pubkey(req, key)) {
    X509_REQ_free(req);
    return nullptr;
  }

  return new SigningRequest(req, type, 0);
}

int SigningRequest::finalise(EVP_PKEY *key) {
  /* Set the subject common name */
  char cn[CN_max_length];
  print_name(cn, CN_max_length);
  set_common_name(X509_REQ_get_subject_name(m_req), cn);

  /* Set the subject alt names extension */
  if (bound_hostnames()) {
    STACK_OF(X509_EXTENSION) *x = sk_X509_EXTENSION_new_null();
    if (x == nullptr) return -10;
    int r = X509V3_add1_i2d(&x, NID_subject_alt_name, m_bound_hostnames, 1,
                            X509V3_ADD_DEFAULT);
    if (r == 0) return -20;
    if (!X509_REQ_add_extensions(m_req, x)) return -30;
    sk_X509_EXTENSION_pop_free(x, X509_EXTENSION_free);
  }

  /* Sign the CSR with the private key */
  if (X509_REQ_sign(m_req, key, EVP_sha256()) == 0) return -40;

  m_key = key;
  return 0;
}

bool SigningRequest::find(const TlsSearchPath *searchPath, int node_id,
                          Node::Type node_type,
                          PkiFile::PathName &path_buffer) {
  PkiFilenames list(node_id, node_type, PkiFile::Type::CertReq);
  return list.find_file(searchPath, path_buffer);
}

SigningRequest *SigningRequest::open(const char *file) {
  SigningRequest *signingRequest = nullptr;
  FILE *fp = fopen(file, "r");
  if (fp) {
    signingRequest = SigningRequest::read(fp);
    fclose(fp);
  }
  return signingRequest;
}

SigningRequest *SigningRequest::read(FILE *fp) {
  X509_REQ *req = nullptr;
  PEM_read_X509_REQ(fp, &req, nullptr, nullptr);
  if (req == nullptr) handle_pem_error("PEM_read_X509_REQ");
  return req ? new SigningRequest(req) : nullptr;
}

bool SigningRequest::store(const char *dir) const {
  PkiFilenames list(0, m_type, PkiFile::Type::CertReq);
  PkiFile::PathName pathname;
  PkiFile::assign(pathname, dir, list.first());
  const char *path = pathname.c_str();

  FILE *fp = nullptr;
  int fd = ::open(path, O_WRONLY | O_CREAT | O_EXCL,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (fd > 0) fp = fdopen(fd, "w");
  if (!fp) return false;

  if (write(fp)) {
    fclose(fp);
    return true;
  } else {
    fclose(fp);
    File_class::remove(path);
    return false;
  }
}

bool SigningRequest::write(FILE *fp) const {
  bool ok = PEM_write_X509_REQ(fp, m_req);
  if (!ok) handle_pem_error("PEM_write_X509_REQ");
  return ok;
}

bool SigningRequest::verify() const {
  return (X509_REQ_verify(m_req, X509_REQ_get0_pubkey(m_req)) == 1);
}

X509 *SigningRequest::create_unsigned_certificate() const {
  EVP_PKEY *key = X509_REQ_get0_pubkey(m_req);
  X509 *cert = Certificate::create(key);
  if (!cert) return nullptr;

  /* Copy name from csr to cert */
  X509_NAME *name = X509_REQ_get_subject_name(m_req);
  if (X509_set_subject_name(cert, name) != 1) return nullptr;

  /* Set serial number in x509 */
  ASN1_STRING *serial = SerialNumber::random();
  X509_set_serialNumber(cert, serial);
  SerialNumber::free(serial);

  /* Duplicate extensions */
  STACK_OF(X509_EXTENSION) *x = X509_REQ_get_extensions(m_req);
  for (int i = 0; i < sk_X509_EXTENSION_num(x); i++) {
    X509_EXTENSION *ext = sk_X509_EXTENSION_value(x, i);
    X509_add_ext(cert, ext, -1);
  }
  sk_X509_EXTENSION_pop_free(x, X509_EXTENSION_free);
  return cert;
}

bool SigningRequest::parse_name() {
  if (m_req == nullptr) return false;
  X509_NAME *name = X509_REQ_get_subject_name(m_req);
  return CertSubject::parse_name(name);
}

/*
 *    SerialNumber class
 */
ASN1_STRING *SerialNumber::random(size_t length) {
  unsigned char buff[MaxLengthInBytes];
  if (length > MaxLengthInBytes) length = MaxLengthInBytes;
  if (RAND_bytes(buff, length) != 1) return nullptr;
  /* The serial number must not be negative (RFC 5280 sec. 4.1.2.2) */
  if (buff[0] == 0)
    buff[0] = 1;
  else
    buff[0] = abs((char)buff[0]);
  ASN1_INTEGER *serial = ASN1_STRING_type_new(V_ASN1_INTEGER);
  ASN1_STRING_set(serial, buff, length);
  return serial;
}

int SerialNumber::print(char *buf, int len, const ASN1_STRING *serial) {
  int offset = 0;
  for (int i = 0; i < serial->length && offset < (len - 4); i++)
    offset += sprintf(buf + offset, "%02X:", serial->data[i]);
  if (offset) buf[offset - 1] = '\0';
  return offset;
}

void SerialNumber::free(ASN1_STRING *serial) { ASN1_STRING_free(serial); }

SerialNumber::HexString::HexString(const ASN1_STRING *serial) {
  buf.append("0x");
  int truncated [[maybe_unused]] = 0;
  for (int i = 0; i < serial->length; i++)
    truncated = buf.appendf("%02x", serial->data[i]);
  assert(!truncated);
}

/*
 *    Certificate class
 */
X509 *Certificate::create(EVP_PKEY *key) {
  X509 *cert = X509_new();
  if (cert) {
    X509_set_version(cert, 2);  // X509v3

    if (X509_set_pubkey(cert, key)) return cert;
    X509_free(cert);
  }
  return nullptr;
}

void Certificate::set_expire_time(X509 *cert, int days) {
  long expires = days * CertLifetime::SecondsPerDay;
  X509_gmtime_adj(X509_getm_notBefore(cert), 0);
  X509_gmtime_adj(X509_getm_notAfter(cert), expires);
}

int Certificate::set_common_name(X509 *cert, const char *CN) {
  X509_NAME *name = X509_get_subject_name(cert);
  return CertSubject::set_common_name(name, CN);
}

size_t Certificate::get_common_name(X509 *cert, char *buf, size_t len) {
  return X509_NAME_get_text_by_NID(X509_get_subject_name(cert), NID_commonName,
                                   buf, len);
}

int Certificate::get_signature_prefix(X509 *cert) {
  int prefix = 0;
  const ASN1_BIT_STRING *sig = nullptr;
  const X509_ALGOR *algorithm;
  X509_get0_signature(&sig, &algorithm, cert);
  if (sig && sig->data)
    prefix = (sig->data[0] << 16) | (sig->data[1] << 8) | sig->data[2];
  return prefix;
}

bool Certificate::write(STACK_OF(X509) * certs, FILE *fp) {
  int r = 1;
  for (int i = 0; i < sk_X509_num(certs) && r == 1; i++)
    if (r == 1) r = PEM_write_X509(fp, sk_X509_value(certs, i));
  if (r != 1) handle_pem_error("PEM_writeX509");
  return r;
}

bool Certificate::store(STACK_OF(X509) * certs, const PkiFile::PathName &path) {
  FILE *fp = nullptr;
  int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (fd > 0) fp = fdopen(fd, "w");
  if (!fp) return false;

  if (Certificate::write(certs, fp)) {
    fclose(fp);
    return true;
  } else {
    fclose(fp);
    PkiFile::remove(path);
    return false;
  }
}

bool Certificate::store(STACK_OF(X509) * certs, const char *dir,
                        const char *file) {
  PkiFile::PathName pathname;
  PkiFile::assign(pathname, dir, file);
  return Certificate::store(certs, pathname);
}

bool Certificate::store(X509 *cert, const char *dir, const char *path) {
  STACK_OF(X509) *stack = sk_X509_new_null();
  sk_X509_push(stack, cert);
  bool r = Certificate::store(stack, dir, path);
  sk_X509_free(stack);
  return r;
}

bool Certificate::remove(const char *dir, const char *file) {
  PkiFile::PathName pathname;
  PkiFile::assign(pathname, dir, file);
  return PkiFile::remove(pathname);
}

STACK_OF(X509) * Certificate::open(const char *path) {
  STACK_OF(X509) *certs = nullptr;
  FILE *fp = fopen(path, "r");

  if (fp) {
    certs = sk_X509_new_null();
    bool ok = Certificate::read(certs, fp);
    fclose(fp);

    if (!ok || sk_X509_num(certs) == 0) {
      sk_X509_pop_free(certs, X509_free);
      certs = nullptr;
    }
  }

  return certs;
}

bool Certificate::read(STACK_OF(X509) * certs, FILE *fp) {
  X509 *cert;
  while ((cert = PEM_read_X509(fp, nullptr, nullptr, nullptr)) != nullptr)
    sk_X509_push(certs, cert);
  // Expect PEM_R_NO_START_LINE error
  int err = ERR_peek_last_error();
  if (ERR_GET_REASON(err) == PEM_R_NO_START_LINE) {
    while (ERR_get_error() != 0) /* clear ssl errors */
      ;
    return true;
  }
  handle_pem_error("PEM_read_X509");
  return false;
}

X509 *Certificate::open_one(const char *path) {
  X509 *c = nullptr;
  STACK_OF(X509) *stack = Certificate::open(path);
  if (stack) {
    c = sk_X509_shift(stack);
    sk_X509_pop_free(stack, X509_free);
  }
  return c;
}

void Certificate::free(X509 *c) { X509_free(c); }

void Certificate::free(STACK_OF(X509) * s) { sk_X509_pop_free(s, X509_free); }

/*
 *    ClusterCertAuthorty class
 */
static bool initClusterCertAuthority(X509 *cert, const char *ordinal) {
  unsigned char subject[CN_max_length];
  int r1;
  snprintf((char *)subject, sizeof(subject), ClusterCertAuthority::Subject,
           ordinal);

  /* Set a random ten byte serial number */
  ASN1_STRING *serial = SerialNumber::random();
  r1 = X509_set_serialNumber(cert, serial);
  SerialNumber::free(serial);
  if (r1 == 0) return false;

  /* Set subject name */
  X509_NAME *name = X509_get_subject_name(cert);
  r1 = X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, subject, -1, -1, 0);
  if (r1 == 0) return false;

  /* Add extension */
  X509V3_CTX ctx;
  X509_EXTENSION *x;
  x = X509V3_EXT_conf_nid(nullptr, &ctx, NID_basic_constraints,
                          "critical,CA:TRUE");
  r1 = X509_add_ext(cert, x, -1);
  X509_EXTENSION_free(x);

  return (r1 == 1);
}

static X509 *create_unsigned_CA(EVP_PKEY *key, const char *ordinal,
                                const CertLifetime &certLifetime) {
  X509 *cert = Certificate::create(key);
  if (cert) {
    certLifetime.set_cert_lifetime(cert);
    if (initClusterCertAuthority(cert, ordinal)) return cert;
    Certificate::free(cert);
  }
  return nullptr;
}

X509 *ClusterCertAuthority::create(EVP_PKEY *key, const CertLifetime &lifetime,
                                   const char *ordinal, bool sign) {
  X509 *cert = create_unsigned_CA(key, ordinal, lifetime);
  if (cert) {
    if (!sign || ClusterCertAuthority::sign(cert, key, cert)) return cert;
    Certificate::free(cert);
  }
  return nullptr;
}

int ClusterCertAuthority::sign(X509 *issuer, EVP_PKEY *key, X509 *cert) {
  if (X509_set_issuer_name(cert, X509_get_subject_name(issuer)) == 0) return 0;
  return X509_sign(cert, key, EVP_sha256());
}

/*
 *    PendingCertificate class
 */
short PendingCertificate::find(const TlsSearchPath *searchPath, int node_id,
                               Node::Type type,
                               PkiFile::PathName &path_buffer) {
  PkiFilenames list(node_id, type, PkiFile::Type::PendingCert);
  return list.find_file(searchPath, path_buffer);
}

bool PendingCertificate::store(const NodeCertificate *nc, const char *dir) {
  PkiFile::FileName file;
  if (!nc->is_signed()) return false;
  nc->filename(PkiFile::Type::PendingCert, file);
  return Certificate::store(nc->all_certs(), dir, file.c_str());
}

bool PendingCertificate::promote(const PkiFile::PathName &pending_file) {
  PkiFile::PathName active;
  PkiFile::PathName retired;

  const char *suffix1 = PkiFilenames::suffix(PkiFile::Type::PendingCert),
             *suffix2 = PkiFilenames::suffix(PkiFile::Type::ActiveCert),
             *suffix3 = PkiFilenames::suffix(PkiFile::Type::RetiredCert);
  size_t len = strlen(suffix1);

  std::string_view base = pending_file;  // make a copy
  base.remove_suffix(len);

  active.append(base);
  active.append(suffix2);
  if (active.is_truncated()) return false;

  retired.append(base);
  retired.append(suffix3);
  if (retired.is_truncated()) return false;

  return promote_file(pending_file.c_str(), active.c_str(), retired.c_str());
}

bool PendingCertificate::remove(const NodeCertificate *cert, const char *dir) {
  PkiFile::FileName file;
  cert->filename(PkiFile::Type::PendingCert, file);
  return Certificate::remove(dir, file.c_str());
}

/*
 *    ActiveCertificate class
 */
short ActiveCertificate::find(const TlsSearchPath *searchPath, int node_id,
                              Node::Type type, PkiFile::PathName &path_buffer) {
  PkiFilenames list(node_id, type, PkiFile::Type::ActiveCert);
  return list.find_file(searchPath, path_buffer);
}

/*
 *    CertSubject class
 */
CertSubject::CertSubject(Node::Type type, int) : m_type(type) {}

CertSubject::CertSubject(const CertSubject &other)
    : m_bound_hostnames(other.m_bound_hostnames),
      m_type(other.m_type),
      m_cluster_id(other.m_cluster_id) {
  m_names_owner = false;
}

int CertSubject::set_common_name(X509_NAME *name, const char *text) {
  return X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                    (const unsigned char *)text, -1, -1, 0);
}

bool CertSubject::bind_hostname(const char *hostname) {
  assert(hostname != nullptr);
  assert(m_bound_hostnames != nullptr);
  if (strlen(hostname) == 0) return false;

  ASN1_IA5STRING *str = ASN1_IA5STRING_new();
  ASN1_STRING_set(str, hostname, strlen(hostname));

  GENERAL_NAME *name = GENERAL_NAME_new();
  GENERAL_NAME_set0_value(name, GEN_DNS, str);

  sk_GENERAL_NAME_push(m_bound_hostnames, name);
  return true;
}

int CertSubject::bound_hostname(int n, char *buffer, int size) const {
  int nwritten = 0;
  if (m_bound_hostnames) {
    int name_type;
    if (n < sk_GENERAL_NAME_num(m_bound_hostnames)) {
      GENERAL_NAME *name = sk_GENERAL_NAME_value(m_bound_hostnames, n);
      ASN1_STRING *str =
          static_cast<ASN1_STRING *>(GENERAL_NAME_get0_value(name, &name_type));
      if (name_type == GEN_DNS) {
        if (str->length < size) size = str->length;
        memcpy(buffer, str->data, size);
        buffer[size] = '\0';
        nwritten = size;
      }
    }
  }
  return nwritten;
}

int CertSubject::bound_hostnames() const {
  if (m_bound_hostnames) return sk_GENERAL_NAME_num(m_bound_hostnames);
  return 0;
}

bool CertSubject::bound_localhost() const {
  if (sk_GENERAL_NAME_num(m_bound_hostnames) == 1) {
    int name_type;
    GENERAL_NAME *name = sk_GENERAL_NAME_value(m_bound_hostnames, 0);
    ASN1_STRING *str =
        static_cast<ASN1_STRING *>(GENERAL_NAME_get0_value(name, &name_type));
    if (name_type == GEN_DNS) {
      if ((str->length == 9) &&
          (strncmp("localhost", (const char *)str->data, 9) == 0))
        return true;
    }
  }
  return false;
}

BaseString CertSubject::bound_hostname(int n) const {
  BaseString s;
  char buffer[256];  // Max DNS name len per RFC 1035
  int len = bound_hostname(n, buffer, 256);
  if (len > 0) s.assign(buffer, len);
  return s;
}

/* Supply a filename for use in saving */
int CertSubject::filename(PkiFile::Type file_type,
                          PkiFile::FileName &buffer) const {
  switch (m_type) {
    case Node::Type::MGMD:
      return PkiFilenames::mgmd_file(file_type, buffer);
    case Node::Type::DB:
      return PkiFilenames::data_node_file(file_type, buffer);
    default:
      return PkiFilenames::client_file(file_type, buffer);
  }
}

int CertSubject::pathname(PkiFile::Type type, const char *dir,
                          PkiFile::PathName &buffer) const {
  PkiFile::FileName name;
  filename(type, name);

  buffer.clear();
  if (dir) buffer.append(dir);
  if (buffer.length()) buffer.append(DIR_SEPARATOR);
  buffer.append(name.c_str());
  return buffer.is_truncated();
}

/* Write current month and year into buffer
 */
size_t CertSubject::timestamp(char *buf, size_t len) const {
  time_t raw_time;
  time(&raw_time);
  return timestamp(raw_time, buf, len);
}

size_t CertSubject::timestamp(time_t raw_time, char *out, size_t len) const {
  struct tm partials;
  gmtime_r(&raw_time, &partials);
  return strftime(out, len, "%b %Y", &partials);
}

size_t CertSubject::print_name(char *buffer, size_t sz) const {
  size_t len = 0;
  len = snprintf(buffer, sz, "NDB ");

  if (m_type == Node::Type::DB)
    len += snprintf(buffer + len, sz - len, "Data ");
  else if (m_type == Node::Type::MGMD)
    len += snprintf(buffer + len, sz - len, "Management ");

  len += snprintf(buffer + len, sz - len, "Node ");

  len += timestamp(buffer + len, sz - len);

  if (m_cluster_id)
    len += snprintf(buffer + len, sz - len, " Cluster %6X", m_cluster_id);

  assert(len < CN_max_length);
  return len;
}

bool CertSubject::parse_name(X509_NAME *name) {
  int idx = X509_NAME_get_index_by_NID(name, NID_commonName, -1);
  if (idx < 0) return false;
  X509_NAME_ENTRY *cn = X509_NAME_get_entry(name, idx);
  if (cn == nullptr) return false;
  ASN1_STRING *str = X509_NAME_ENTRY_get_data(cn);
  return parse_name(str);
}

bool CertSubject::parse_name(const ASN1_STRING *str) {
  if (str == nullptr) return false;
  if (str->length == 0) return false;

  int p = 0;  // cursor into name
  auto atEnd = [&]() { return (str->length == p); };
  auto data = [&]() { return (char *)(str->data) + p; };
  auto find = [&](const char *a, size_t l) {
    if (str->length <= p) return false;
    int r = strncmp(data(), a, l);
    if (r == 0) {
      p += l;
      return true;
    }
    return false;
  };

  m_type = Node::Type::ANY;

  if (!find("NDB ", 4)) return false;

  if (find("Data Node", 9))
    m_type = Node::Type::DB;
  else if (find("Management Node", 15))
    m_type = Node::Type::MGMD;
  else if (find("Node", 4))
    m_type = Node::Type::Client;
  else
    return false;

  if (!(atEnd() || find(" ", 1))) return false;  // non-whitespace after token

  return true;
}

/*
 *    CertLifetime class
 */
bool CertLifetime::set_lifetime(X509 *cert) {
  // ASM1_TIME_to_tm(): "The output time is GMT"
  if (ASN1_TIME_to_tm(X509_get0_notBefore(cert), &m_notBefore) != 1)
    return false;
  if (ASN1_TIME_to_tm(X509_get0_notAfter(cert), &m_notAfter) != 1) return false;

  time_t time1 = timegm(&m_notBefore);
  time_t time2 = timegm(&m_notAfter);

  m_duration = time2 - time1;
  return true;
}

bool CertLifetime::set_lifetime(int expire_days, int extra_days) {
  int extra_hours = extra_days * 24;
  if (extra_hours) {
    union {
      unsigned long rn;
      unsigned char c[sizeof(long)];
    } u;
    RAND_bytes(u.c, sizeof(long));
    extra_hours = u.rn % extra_hours;
  }

  time_t duration =
      (expire_days * SecondsPerDay) + (extra_hours * SecondsPerHour);
  return set_exact_duration(duration);
}

bool CertLifetime::set_exact_duration(time_t duration) {
  m_duration = duration;

  time_t now = time(nullptr);
  if (now == -1) return false;

  time_t expires = now + m_duration;
  gmtime_r(&now, &m_notBefore);
  return (bool)gmtime_r(&expires, &m_notAfter);
}

bool CertLifetime::set_cert_lifetime(X509 *cert) const {
  time_t time1 = timegm(&m_notBefore);
  time_t time2 = timegm(&m_notAfter);
  return (ASN1_TIME_set(X509_getm_notBefore(cert), time1) &&
          ASN1_TIME_set(X509_getm_notAfter(cert), time2));
}

time_t CertLifetime::expire_time(struct tm **tptr) const {
  if (tptr) *tptr = &m_notAfter;
  return timegm(&m_notAfter);
}

time_t CertLifetime::replace_time(int replace_days) const {
  time_t rtime;

  if (replace_days <= 0)
    rtime = timegm(&m_notAfter);
  else
    rtime = timegm(&m_notBefore);

  rtime += (replace_days * SecondsPerDay);  // add or subtract days
  return rtime;
}

time_t CertLifetime::replace_time(float pct) const {
  time_t t1 = timegm(&m_notBefore);
  time_t t2 = timegm(&m_notAfter);

  float portion = (t2 - t1) * pct;
  return t1 + (int)portion;
}

/*
 *    NodeCertificate class
 */
NodeCertificate::NodeCertificate(Node::Type type, int node_id)
    : CertSubject(type, node_id), CertLifetime(DefaultDays) {
  m_bound_hostnames = sk_GENERAL_NAME_new_null();
}

NodeCertificate::NodeCertificate(const SigningRequest &csr, EVP_PKEY *key)
    : CertSubject(csr), CertLifetime(csr) {
  set_own_keys(key, csr.create_unsigned_certificate());
  m_x509_names_set = true;
  m_signed = false;
}

void NodeCertificate::init_from_x509(X509 *cert) {
  set_cert(cert);
  set_lifetime(cert);
  m_name_conforming = parse_name();

  int idx = -1;
  STACK_OF(GENERAL_NAME) *gn = static_cast<struct stack_st_GENERAL_NAME *>(
      X509_get_ext_d2i(m_x509, NID_subject_alt_name, nullptr, &idx));
  if (gn) {
    m_bound_hostnames = sk_GENERAL_NAME_new_null();
    for (int i = 0; i < sk_GENERAL_NAME_num(gn); i++)
      sk_GENERAL_NAME_push(m_bound_hostnames,
                           GENERAL_NAME_dup(sk_GENERAL_NAME_value(gn, i)));
    sk_GENERAL_NAME_pop_free(gn, GENERAL_NAME_free);
  }
  m_x509_names_set = true;
  m_signed =
      (bool)X509_get_signature_info(cert, nullptr, nullptr, nullptr, nullptr);
  m_final = m_signed;
}

void NodeCertificate::init_from_credentials(STACK_OF(X509) * certs,
                                            EVP_PKEY *key, bool up_ref_count) {
  if (up_ref_count) {
    m_all_certs = X509_chain_up_ref(certs);
    if (key) set_key(key);
  } else {
    m_all_certs = certs;
    m_key = key;
  }
  init_from_x509(sk_X509_value(certs, 0));
}

const NodeCertificate *NodeCertificate::from_credentials(STACK_OF(X509) * certs,
                                                         EVP_PKEY *key) {
  NodeCertificate *nc = new NodeCertificate();
  nc->init_from_credentials(certs, key);
  return nc;
}

const NodeCertificate *NodeCertificate::for_peer(X509 *cert) {
  NodeCertificate *nc = new NodeCertificate();
  nc->init_from_x509(cert);
  return nc;
}

bool NodeCertificate::create_keys(const char *curve) {
  m_key = PrivateKey::create(curve);
  m_x509 = Certificate::create(m_key);
  return (bool)m_x509;
}

bool NodeCertificate::set_own_keys(EVP_PKEY *key, X509 *cert) {
  m_key = key;
  m_x509 = cert;
  return m_x509;
}

bool NodeCertificate::set_key(EVP_PKEY *key) {
  if (key) EVP_PKEY_up_ref(key);
  m_key = key;
  return key;
}

bool NodeCertificate::set_cert(X509 *cert) {
  if (cert) X509_up_ref(cert);
  m_x509 = cert;
  return cert;
}

bool NodeCertificate::set_signed_cert(X509 *signed_x509) {
  if (!m_final) return false;
  if (m_signed) return false;

  X509_free(m_x509);

  int r = sk_X509_unshift(m_all_certs, m_x509);
  assert(r == 2);
  if (r != 2) return false;

  m_signed = set_cert(signed_x509);
  return m_signed;
}

NodeCertificate::~NodeCertificate() {
  if (m_key) EVP_PKEY_free(m_key);
  if (m_all_certs) sk_X509_pop_free(m_all_certs, X509_free);
  if (m_x509) X509_free(m_x509);
  if (m_bound_hostnames && m_names_owner)
    sk_GENERAL_NAME_pop_free(m_bound_hostnames, GENERAL_NAME_free);
}

int NodeCertificate::self_sign() {
  m_self_signed = true;
  return finalise(m_x509, m_key);
}

int NodeCertificate::finalise(X509 *CA_cert, EVP_PKEY *CA_key) {
  assert(!m_final);
  if (CA_cert == nullptr) return -10;
  if (!m_cluster_id) m_cluster_id = Certificate::get_signature_prefix(CA_cert);

  if (!m_x509_names_set) {
    /* Set subject name */
    char cn[CN_max_length];
    print_name(cn, CN_max_length);
    Certificate::set_common_name(m_x509, cn);
    m_name_conforming = true;

    /* Add extension: Subject Alternative Name */
    if (bound_hostnames())
      if (!X509_add1_ext_i2d(m_x509, NID_subject_alt_name, m_bound_hostnames, 0,
                             X509V3_ADD_DEFAULT))
        return -20;

    m_x509_names_set = true;
  }

  /* Set serial number */
  ASN1_STRING *serial = SerialNumber::random();
  int r1 = X509_set_serialNumber(m_x509, serial);
  SerialNumber::free(serial);
  if (r1 == 0) return -50;

  /* Set issuer name */
  r1 = X509_set_issuer_name(m_x509, X509_get_subject_name(CA_cert));
  if (r1 == 0) return -60;

  /* Set lifetime */
  if (!set_cert_lifetime(m_x509)) return -70;

  /* Sign the certificate */
  if (CA_key) {
    if (!X509_sign(m_x509, CA_key, EVP_sha256())) return -40;
    m_signed = true;
  }

  /* Make a stack containing the signed subject cert and the CA cert */
  m_all_certs = sk_X509_new_null();
  if (m_signed) {
    sk_X509_push(m_all_certs, m_x509);
    X509_up_ref(m_x509);
  }
  if (m_signed && !m_self_signed) {
    sk_X509_push(m_all_certs, CA_cert);
    X509_up_ref(CA_cert);
  }

  m_final = true;
  return 0;
}

bool NodeCertificate::push_extra_ca_cert(X509 *extra) {
  if (m_final && m_all_certs) {
    X509_up_ref(extra);
    sk_X509_push(m_all_certs, extra);
    return true;
  }
  return false;
}

int NodeCertificate::stderr_callback(int result, X509_STORE_CTX *ctx) {
  if (!result) {
    int err = X509_STORE_CTX_get_error(ctx);
    fprintf(stderr, "Error %i: %s\n", err, X509_verify_cert_error_string(err));
    fprintf(stderr, "Depth: %d\n", X509_STORE_CTX_get_error_depth(ctx));
  }
  return result;
}

bool NodeCertificate::verify_signature(EVP_PKEY *CA_key) const {
  int r0 = X509_verify(m_x509, CA_key);
  if (r0 != 1) {
    handle_pem_error("X509_verify");
    return false;
  }

  /* Print signature info */
  int mdnid, pknid, secbits;
  uint32_t flags;
  int sig = X509_get_signature_info(m_x509, &mdnid, &pknid, &secbits, &flags);
  fprintf(stderr,
          "signed = %d, mdnid = %d, pknid = %d, secbits = %d, "
          "flags = %d\n",
          sig, mdnid, pknid, secbits, flags);
  return (sig == 1);
}

bool NodeCertificate::verify_chain() const {
  require(m_signed);
  require(m_final);

  /* Create a CA store for chain verification */
  X509_STORE *store = X509_STORE_new();
  if (!store) return false;
  X509_STORE_set_depth(store, 1);
  X509_STORE_set_verify_cb_func(store, stderr_callback);

  /* Add trusted certs */
  for (int i = 1; i < sk_X509_num(m_all_certs); i++)
    X509_STORE_add_cert(store, sk_X509_value(m_all_certs, i));

  /* Run X509_verify_cert */
  X509_STORE_CTX *csc = X509_STORE_CTX_new();
  if (!csc) return false;
  X509_STORE_CTX_init(csc, store, m_x509, nullptr);
  int r0 = X509_verify_cert(csc);

  X509_STORE_CTX_free(csc);
  X509_STORE_free(store);
  return (r0 == 1);
}

BaseString NodeCertificate::serial_number() const {
  BaseString s;
  char buffer[100];
  const ASN1_INTEGER *serial = X509_get0_serialNumber(m_x509);
  int len = SerialNumber::print(buffer, sizeof(buffer), serial);
  if (len > 0) s.assign(buffer, len);
  return s;
}

bool NodeCertificate::parse_name() {
  if (m_x509 == nullptr) return false;
  X509_NAME *name = X509_get_subject_name(m_x509);
  int idx = X509_NAME_get_index_by_NID(name, NID_commonName, -1);
  if (idx < 0) return false;
  X509_NAME_ENTRY *cn = X509_NAME_get_entry(name, idx);
  if (cn == nullptr) return false;
  ASN1_STRING *str = X509_NAME_ENTRY_get_data(cn);
  return CertSubject::parse_name(str);
}

bool NodeCertificate::parse_name(const char *name) {
  if (name == nullptr) return false;
  ASN1_STRING *str = ASN1_STRING_new();
  ASN1_STRING_set(str, name, strlen(name));
  bool r = CertSubject::parse_name(str);
  ASN1_STRING_free(str);
  return r;
}

#ifdef TEST_NODECERTIFICATE

#ifdef _WIN32
#include <openssl/applink.c>
static constexpr bool isWin32 = 1;
#else
static constexpr bool isWin32 = 0;
#endif

static constexpr bool openssl_version_ok =
    (OPENSSL_VERSION_NUMBER >= NDB_TLS_MINIMUM_OPENSSL);

/*
  Test name parsing
*/
static int parser_test() {
  NodeCertificate nc(Node::Type::DB, 1);
  require(!nc.parse_name((const char *)nullptr));
  require(!nc.parse_name(""));
  require(!nc.parse_name("0"));
  require(!nc.parse_name("Quatnum Entanglement"));
  require(!nc.parse_name("NDB"));
  require(!nc.parse_name("NDB "));
  require(!nc.parse_name("NDB Blooey"));
  require(!nc.parse_name("NDB Clients"));

  require(nc.parse_name("NDB Node"));
  require(nc.parse_name("NDB Node Certificate"));
  require(nc.parse_name("NDB Node Q1/20"));
  require(nc.parse_name("NDB Data Node Q1/20"));
  require(nc.parse_name("NDB Management Node Q1/20"));
  require(nc.parse_name("NDB Node Q1/20 Cluster AABBCC"));
  require(nc.parse_name("NDB Management Node Jan 2020 Cluster AABBCC"));
  // 123456789 123456789 123456789 123456789 123

  return 0;
}

/*
  Test CertLifetime
*/

static int cert_lifetime_test() {
  CertLifetime c1(CertLifetime::DefaultDays);
  time_t t1, t2;

  /* Compare replacement date */
  static constexpr time_t five_days = CertLifetime::SecondsPerDay * 5;
  static constexpr time_t ten_days = five_days * 2;
  static constexpr time_t five_pct =
      CertLifetime::SecondsPerDay / 20 * CertLifetime::DefaultDays;
  t1 = c1.expire_time(nullptr);
  t2 = c1.replace_time(-10);
  if (t1 - t2 != ten_days) return t1;
  t2 = c1.replace_time(.95F);
  if (t1 - t2 != five_pct) return 95;

  /* Setting an expiration date in the past is okay, and useful for testing */
  if (!c1.set_lifetime(-10, 0)) return 8;

  /* Cert expires 20 days from now. Create a replacement 5 days from now. */
  CertLifetime c2(20);
  t2 = c2.replace_time(5) - time(&t1);
  printf("t2: %ld days\n", (long)t2 / CertLifetime::SecondsPerDay);
  if (t2 != five_days) return 9;

  /* Write lifetime to certificate */
  EVP_PKEY *key = PrivateKey::create("P-256");
  X509 *cert = Certificate::create(key);
  if (!c2.set_cert_lifetime(cert)) return 13;

  /* Read lifetime from certificate and compare to original */
  CertLifetime c3(cert);
  if (c2.duration() != c3.duration()) return 10;
  if (c2.expire_time(nullptr) != c3.expire_time(nullptr)) return 11;
  if (c2.replace_time(5) != c3.replace_time(5)) return 12;

  PrivateKey::free(key);
  Certificate::free(cert);
  return 0;
}

/*
   Test Key Creation
*/
static int file_subtest_csr(bool output) {
  bool r;
  int r1;

  /* Create a private key */
  EVP_PKEY *key = PrivateKey::create("P-256");

  /* Create a signing request and save it */
  SigningRequest *csr = SigningRequest::create(key, Node::Type::DB);
  require(csr);

  /* Save the private key as pending */
  r = PendingPrivateKey::store(key, "", *csr);
  require(r);

  /* Bind two hostnames */
  csr->bind_hostname("edson.mysql.com");
  csr->bind_hostname("bly.mysql.com");

  r1 = csr->finalise(key);
  require(r1 == 0);
  require(csr->verify());
  r = csr->store("");
  require(r);

  if (output) {
    NodeCertificate *nc = new NodeCertificate(*csr, key);
    r1 = nc->self_sign();
    require(!r1);
    Certificate::write(nc->all_certs(), stdout);
    delete nc;
  }

  /* Free the key and the signing request */
  delete csr;
  PrivateKey::free(key);

  return 0;
}

static int file_test() {
  bool r;
  int r1;
  short r2;

  /* Test key creation */
  r1 = file_subtest_csr(false);
  if (r1 != 0) return r1;

  /*
     Test Key Signing
  */

  /* Create a CA */
  EVP_PKEY *CA_key = EVP_RSA_gen(2048);
  require(CA_key);
  CertLifetime lifetime(CertLifetime::CaDefaultDays);
  X509 *CA_cert = ClusterCertAuthority::create(CA_key, lifetime);
  require(CA_cert);

  /* Open the stored signing request */
  PkiFile::PathName cert_file, key_file, csr_file;
  TlsSearchPath tlsPath(".");
  const SigningRequest *csr = nullptr;
  EVP_PKEY *key = nullptr;
  STACK_OF(X509) *certs = nullptr;

  if (SigningRequest::find(&tlsPath, 1, Node::Type::DB, csr_file))
    csr = SigningRequest::open(csr_file);
  require(csr);
  require(csr->verify());
  require(csr->node_type() == Node::Type::DB);
  /* Create a NodeCertificate */
  {
    if (PendingPrivateKey::find(&tlsPath, 1, Node::Type::DB, key_file))
      key = PrivateKey::open(key_file, nullptr);
    require(key);
    NodeCertificate node_cert(*csr, key);

    /* Set lifetime */
    r = node_cert.set_lifetime(90, 4);
    require(r);

    /* Add extension, Set the final details and sign it */
    r1 = node_cert.finalise(CA_cert, CA_key);
    if (r1) return r1;

    /* Verify the signature */
    r = node_cert.verify_signature(CA_key);
    require(r);

    printf("Serial No  : %s\n", node_cert.serial_number().c_str());

    struct tm *expires = nullptr;
    (void)node_cert.expire_time(&expires);
    require(expires);
    printf("Expires    : %s", asctime(expires));

    /* Test for two certs in chain */
    require(sk_X509_num(node_cert.all_certs()) == 2);

    /* Save the pending node certifcate */
    r = PendingCertificate::store(&node_cert, "");
    require(r);
  }

  /* Remove the signing request */
  r = PkiFile::remove(csr_file);
  if (!r) perror("remove csr file");
  require(r);

  /* Free the signing request */
  delete csr;

  /* Free the CA */
  PrivateKey::free(CA_key);
  Certificate::free(CA_cert);

  /*
     Test promotion of the pending key and certificate to active
  */

  /* Read the pending certificate */
  if (PendingCertificate::find(&tlsPath, 1, Node::Type::DB, cert_file))
    certs = Certificate::open(cert_file);
  require(certs);
  require(sk_X509_num(certs) == 2);

  /* Read the pending private key */
  if (PendingPrivateKey::find(&tlsPath, 1, Node::Type::DB, key_file))
    key = PrivateKey::open(key_file, nullptr);
  assert(key);

  /* Verify that the two keys match. */
  r1 = EVP_PKEY_eq(key, X509_get0_pubkey(sk_X509_value(certs, 0)));
  require(r1 == 1);

  /* Create NodeCertificate c2 from saved X509 */
  const NodeCertificate *c2 = NodeCertificate::from_credentials(certs, key);
  require(c2->is_signed());

  require(c2->node_type() == Node::Type::DB);
  require(c2->bound_hostnames() == 2);
  require(c2->bound_hostname(0) == "edson.mysql.com");
  require(c2->bound_hostname(1) == "bly.mysql.com");

  /* Promote the pending files to active */
  r = PendingCertificate::promote(cert_file);
  require(r);
  r = PendingPrivateKey::promote(key_file);
  require(r);

  /* Find the active files */
  r2 = PendingPrivateKey::find(&tlsPath, 1, Node::Type::DB, key_file);
  require(r2 == 0);
  r2 = ActivePrivateKey::find(&tlsPath, 1, Node::Type::DB, key_file);
  require(r2 > 0);
  r2 = PendingCertificate::find(&tlsPath, 1, Node::Type::DB, cert_file);
  require(r2 == 0);
  r2 = ActiveCertificate::find(&tlsPath, 1, Node::Type::DB, cert_file);
  require(r2 > 0);

  /* Free up memory. */
  delete c2;

  return 0;
}

static int verify_test() {
  bool r;
  int r1;

  /* Create a CA */
  EVP_PKEY *CA_key = EVP_RSA_gen(2048);
  CertLifetime CA_lifetime(CertLifetime::CaDefaultDays);
  X509 *CA_cert = ClusterCertAuthority::create(CA_key, CA_lifetime);
  require(CA_cert);

  /* Create a private key and a NodeCertificate */
  auto nc = std::make_unique<NodeCertificate>(Node::Type::Client, 150);
  nc->create_keys("P-256");
  nc->set_lifetime(90, 10);
  r1 = nc->finalise(CA_cert, CA_key);
  if (r1 != 0) return r1;

  /* Verify the signature */
  r = nc->verify_signature(CA_key);
  require(r);

  /* Verify the trust chain */
  r = nc->verify_chain();
  require(r);

  Certificate::free(CA_cert);
  PrivateKey::free(CA_key);

  return 0;
}

inline bool test_expansion(const char *path, const char *expansion) {
  TlsSearchPath s(path);
  char *full = s.expanded_path_string();
  bool b = (strcmp(full, expansion) == 0);
  if (!b) printf(" ===> Got expansion: '%s'\n", full);
  free(full);
  return b;
}

static int search_path_test() {
  static char tmpdir_string[] = "TMPDIR=/tmp/foo";
  putenv(tmpdir_string);
  BaseString pathStr("$TMPDIR");
  pathStr.append(TlsSearchPath::Separator);
  pathStr.append(MYSQL_DATADIR);
  pathStr.append(TlsSearchPath::Separator);
  pathStr.append(isWin32 ? "/test/$USERNAME/foo" : "/test/$USER/foo");

  TlsSearchPath searchPath(pathStr.c_str());
  char *full = searchPath.expanded_path_string();
  printf("%s\n", full);
  free(full);

  if (searchPath.size() != 3) return 1;
  if (searchPath.dir(2)[0] != '/') return 2;
  require(searchPath.dir(101) == nullptr);
  require(searchPath.writable(102) == false);

  searchPath.push_cwd();
  if (!searchPath.first_writable()) return 3;
  if (searchPath.size() != 4) return 4;
  searchPath.push_cwd();
  if (searchPath.size() != 4) return 5;

  TlsSearchPath searchPath0(nullptr);
  full = searchPath0.expanded_path_string();
  free(full);
  if (searchPath0.first_writable()) return 6;
  if (searchPath0.size()) return 7;
  searchPath0.push_cwd();
  if (searchPath0.size() != 1) return 8;

  TlsSearchPath searchPath1("");
  full = searchPath1.expanded_path_string();
  free(full);
  if (searchPath1.first_writable()) return 9;
  if (searchPath1.size()) return 10;
  searchPath1.push_cwd();
  if (searchPath1.size() != 1) return 11;

  TlsSearchPath searchPath2(".");
  full = searchPath2.expanded_path_string();
  free(full);
  if (searchPath2.size() != 1) return 12;
  searchPath2.push_cwd();
  if (searchPath2.size() != 1) return 13;
  if (!searchPath2.first_writable()) return 14;

  /* If the character following $ is not alnum or _, do not expand
     $VAR expands correctly if VAR is set
     $VAR expands to nothing if VAR is not set
     a:my$SUFFIX expands to a:my if SUFFIX is not set
     a:$VAR:b expands to a:b if $VAR is not set
  */
  if (!test_expansion("$", "$")) return 15;
  if (!test_expansion("$$", "$$")) return 16;
  if (!test_expansion("$#", "$#")) return 17;
  if (isWin32) {
    if (!test_expansion("f;abc$", "f;abc$")) return 18;
    if (!test_expansion("a;$;b", "a;$;b")) return 19;
    if (!test_expansion("a;$", "a;$")) return 20;
    _putenv("ARMAGOGLYPOD=A");
    if (!test_expansion("$ARMAGOGLYPOD:/tls", "A:/tls")) return 21;
    _putenv("ARMAGOGLYPOD=");
    if (!test_expansion("$ARMAGOGLYPOD", "")) return 22;
    if (!test_expansion("a;$ARMAGOGLYPOD;b", "a;b")) return 23;
    if (!test_expansion("a;my$ARMAGOGLYPOD", "a;my")) return 24;
  } else {
    if (!test_expansion("f:abc$", "f:abc$")) return 18;
    if (!test_expansion("a:$:b", "a:$:b")) return 19;
    if (!test_expansion("a:$", "a:$")) return 20;
    if (!test_expansion("$ARMAGOGLYPOD", "")) return 22;
    if (!test_expansion("a:$ARMAGOGLYPOD:b", "a:b")) return 23;
    if (!test_expansion("a:my$ARMAGOGLYPOD", "a:my")) return 24;
  }

  return 0;
}

static int fail(const char *test_name, int code) {
  printf("Test '%s' Failed: %d \n", test_name, code);
  fflush(stdout);
  return code;
}

int main(int argc, char *argv[]) {
  int r1;

  /* Remove any leftover files that may be here */
  PkiFile::PathName file;

  TlsSearchPath tlsPath(".");
  if (PendingPrivateKey::find(&tlsPath, 1, Node::Type::DB, file))
    PkiFile::remove(file);
  if (SigningRequest::find(&tlsPath, 1, Node::Type::DB, file))
    PkiFile::remove(file);
  if (PendingCertificate::find(&tlsPath, 1, Node::Type::DB, file))
    PkiFile::remove(file);
  if (ActivePrivateKey::find(&tlsPath, 1, Node::Type::DB, file))
    PkiFile::remove(file);
  if (ActiveCertificate::find(&tlsPath, 1, Node::Type::DB, file))
    PkiFile::remove(file);

  // Create a private key and signing request for further testing, then exit:
  if (argc == 2 && (strcmp(argv[1], "--csr") == 0))
    return file_subtest_csr(true);

  r1 = search_path_test();
  if (r1 != 0) return fail("search path", r1);

  r1 = parser_test();
  if (r1 != 0) return fail("parser", r1);

  if (openssl_version_ok) {
    r1 = cert_lifetime_test();
    if (r1 != 0) return fail("lifetime", r1);

    r1 = file_test();
    if (r1 != 0) return fail("file", r1);

    r1 = verify_test();
    if (r1 != 0) return fail("verify", r1);
  }

  return 0;
}

#endif
