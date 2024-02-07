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

#ifndef NDB_UTIL_NODE_CERTIFICATE_HPP
#define NDB_UTIL_NODE_CERTIFICATE_HPP

#include <array>

#include "util/BaseString.hpp"
#include "util/Vector.hpp"
#include "util/cstrbuf.h"

struct PkiFile {
  using FileName = cstrbuf<32>;
  using PathName = cstrbuf<PATH_MAX>;

  enum class Type {
    PendingKey,
    ActiveKey,
    RetiredKey,
    CertReq,
    PendingCert,
    ActiveCert,
    RetiredCert
  };

  static bool remove(const PathName &);
  static int assign(PathName &path, const char *dir, const char *file);
};

namespace Node {
enum class Type { MGMD = 0x01, DB = 0x02, Client = 0x04, ANY = 0x07 };
inline bool And(Type a, int b) { return ((int)a) & b; }
inline bool And(Type a, Type b) { return ((int)a) & ((int)b); }
inline Type Mask(int f) { return static_cast<Type>(f & (int)Type::ANY); }
}  // namespace Node

class TlsSearchPath {
 public:
#ifdef _WIN32
  static constexpr const char *Separator = ";";
#else
  static constexpr const char *Separator = ":";
#endif

  TlsSearchPath(const char *path_string);
  TlsSearchPath() = default;
  ~TlsSearchPath() = default;

  /* Number of directories in search path */
  unsigned int size() const { return m_path.size(); }

  /* Return a single directory */
  const char *dir(unsigned int i) const;

  /* Push current working directory to search path if not already present */
  void push_cwd();

  /* Return the delimited search path including resolved environment variables.
     The caller should free() the returned string. */
  char *expanded_path_string() const;

  /* Return the first writable directory in the search path */
  const char *first_writable() const;

  /* Returns true if directory component i is writable */
  bool writable(unsigned int i) const;

  /* Find file name in search path and write whole path into buffer */
  bool find(const char *name, PkiFile::PathName &buffer) const;

  /* Find file name in search path.
     Returns the index of the directory where file exists, or -1 if not found.
  */
  int find(const char *name) const;

 private:
  Vector<BaseString> m_path;
};

class PrivateKey {
 public:
  static struct evp_pkey_st *create(const char *curve);
  static struct evp_pkey_st *open(const char *path, char *pass);
  static struct evp_pkey_st *open(const PkiFile::PathName &p,
                                  char *pass = nullptr) {
    return open(p.c_str(), pass);
  }
  static bool store(struct evp_pkey_st *, const PkiFile::PathName &path,
                    char *pass, bool encrypted);
  static bool store(struct evp_pkey_st *, const char *dir, const char *filename,
                    char *pass);
  static void free(struct evp_pkey_st *key);
};

class PendingPrivateKey {
 public:
  // find() on Pending and Active object classes returns a small integer
  // preference score if found, or 0 if not found. Higher scores indicate
  // more specific file names.
  static short find(const TlsSearchPath *, int node_id, Node::Type,
                    PkiFile::PathName &path_buffer);
  static bool store(struct evp_pkey_st *, const char *dir,
                    const class CertSubject &);
  /* Retires active file; promotes pending file to active.
     Returns true on success. User should check errno on failure. */
  static bool promote(const PkiFile::PathName &);
};

class ActivePrivateKey {
 public:
  static short find(const TlsSearchPath *, int node_id, Node::Type,
                    PkiFile::PathName &path_buffer);
};

class PendingCertificate {
 public:
  static short find(const TlsSearchPath *, int node_id, Node::Type,
                    PkiFile::PathName &path_buffer);
  static bool store(const class NodeCertificate *, const char *dir);
  static bool remove(const class NodeCertificate *, const char *dir);
  /* Retires active file; promotes pending file to active.
     Returns true on success. User should check errno on failure. */
  static bool promote(const PkiFile::PathName &);
};

class ActiveCertificate {
 public:
  static short find(const TlsSearchPath *, int node_id, Node::Type,
                    PkiFile::PathName &path_buffer);
};

class Certificate {
 public:
  /* Create an X509 certificate for a key */
  static struct x509_st *create(struct evp_pkey_st *key);

  /* Set "not after" date to exp_days days from now */
  static void set_expire_time(struct x509_st *, int exp_days);

  /* Set the CN in the X509 subject name */
  static int set_common_name(struct x509_st *, const char *);

  /* Get the CN from the X509 subject name */
  static size_t get_common_name(struct x509_st *, char *buf, size_t len);

  /* Returns the first three bytes of the signature, as for cluster ID */
  static int get_signature_prefix(struct x509_st *);

  /* Store in filesystem */
  static bool write(struct stack_st_X509 *, FILE *);
  static bool store(struct stack_st_X509 *, const char *dir, const char *fil);
  static bool store(struct stack_st_X509 *, const PkiFile::PathName &path);
  static bool store(struct x509_st *, const char *dir, const char *file);
  static bool remove(const char *dir, const char *file);

  /* Read from file */
  static bool read(struct stack_st_X509 *, FILE *);
  static struct stack_st_X509 *open(const char *path);
  static struct stack_st_X509 *open(const PkiFile::PathName &);
  static struct x509_st *open_one(const char *path);  // first cert in file
  static struct x509_st *open_one(const PkiFile::PathName &);

  /* Free */
  static void free(struct x509_st *);
  static void free(struct stack_st_X509 *);
};

inline struct stack_st_X509 *Certificate::open(const PkiFile::PathName &p) {
  return Certificate::open(p.c_str());
}

inline struct x509_st *Certificate::open_one(const PkiFile::PathName &p) {
  return Certificate::open_one(p.c_str());
}

class CertSubject {
 public:
  CertSubject(Node::Type, int node_id);
  CertSubject() = default;
  CertSubject(const CertSubject &);
  ~CertSubject() = default;

  /* Class Methods */
  static int set_common_name(struct X509_name_st *, const char *);

  /* Public Instance Methods */
  bool bind_hostname(const char *);
  struct X509_extension_st *build_extension();

  /* Const Public Instance Methods */
  Node::Type node_type() const { return m_type; }
  int filename(PkiFile::Type, PkiFile::FileName &buffer) const;
  int pathname(PkiFile::Type, const char *dir, PkiFile::PathName &buffer) const;

  bool bound_localhost() const;  // True if the bound hostname is "localhost"
  int bound_hostnames() const;   // Returns number of bound hostnames
  BaseString bound_hostname(int n) const;  // Returns nth bound hostname

 protected:
  int bound_hostname(int n, char *, int len) const;  // Returns length written
  size_t timestamp(time_t, char *m, size_t) const;
  size_t timestamp(char *, size_t) const;
  size_t print_name(char *, size_t) const;
  bool parse_name(struct X509_name_st *);
  bool parse_name(const struct asn1_string_st *);

  /* Member variables */
  /* The derived classes must initialize and free m_bound_hostnames */
  struct stack_st_GENERAL_NAME *m_bound_hostnames{nullptr};
  Node::Type m_type{Node::Type::ANY};
  int m_cluster_id{0};
  bool m_names_owner{true};
};

/*  Manage a Certificate's or request's notValidBefore and notValidAfter times.

    It is possible to add a small random amount of time to a certificate
    lifetime so that the expiration dates of related certificates are staggered.
*/
class CertLifetime {
 public:
  static constexpr const int DefaultDays = 90;
  static constexpr long SecondsPerHour = 60 * 60;
  static constexpr long SecondsPerDay = 24 * SecondsPerHour;

  CertLifetime() { set_lifetime(DefaultDays, 0); }
  CertLifetime(struct x509_st *cert) { set_lifetime(cert); }
  CertLifetime(const CertLifetime &) = default;

  /* set_lifetime()

     Set the X.509 certificate to expire in expire_days plus some portion of
     rnd_extra_days.
  */
  bool set_lifetime(int expire_days, int rand_extra_days);

  /* Set the exact certificate lifetime in seconds. */
  bool set_exact_duration(time_t);

  /* Set lifetime based on notValidBefore and notValidAfter in X509 */
  bool set_lifetime(struct x509_st *);

  /* Set the lifetime in the X509 certificate */
  void set_cert_lifetime(struct x509_st *) const;

  /* Returns the expiration time, and sets tp (if non-null) to point to it */
  time_t expire_time(struct tm **tp) const;

  /* Returns total lifetime in seconds */
  time_t duration() const { return m_duration; }

  /* Return a certificate replacement date given a replacement rule.
     Positive d represents days after the notBefore date;
     negative d represents days before the notAfter date. */
  time_t replace_time(int d) const;

  /* Return a certificate replacement date given a replacement rule.
     pct represents a percentage of the certificate's total lifetime. */
  time_t replace_time(float pct) const;

 protected:
  CertLifetime(int n) : m_duration(n) {}

  mutable struct tm m_notBefore;  // mutable because of timegm()
  mutable struct tm m_notAfter;
  time_t m_duration{0};
};

class SigningRequest : public CertSubject, public CertLifetime {
 public:
  /* Named constructors */
  static SigningRequest *create(struct evp_pkey_st *, Node::Type);
  static SigningRequest *open(const char *full_path_name);
  static SigningRequest *open(const PkiFile::PathName &);
  static SigningRequest *read(FILE *);

  /* Destructor */
  ~SigningRequest();

  /* Class Methods */
  static bool find(const TlsSearchPath *, int node_id, Node::Type t,
                   PkiFile::PathName &path_buffer);

  /* finalise(): Set all fields and sign the CSR. Returns 0 on success. */
  int finalise(struct evp_pkey_st *);

  /* Const Public Instance Methods */
  struct x509_st *create_unsigned_certificate() const;
  struct X509_req_st *req() const {
    return m_req;
  }
  struct evp_pkey_st *key() const {
    return m_key;
  }
  bool store(const char *dir) const;
  bool write(FILE *) const;
  bool verify() const;

 private:
  SigningRequest(struct X509_req_st *);
  SigningRequest(struct X509_req_st *, Node::Type, int);
  bool parse_name();

  /* Member variables */
  struct X509_req_st *m_req;
  struct evp_pkey_st *m_key{nullptr};
};

inline SigningRequest *SigningRequest::open(const PkiFile::PathName &p) {
  return open(p.c_str());
}

class SerialNumber {
 public:
  static constexpr size_t MaxLengthInBytes = 20;
  static struct asn1_string_st *random(size_t length = 10);
  static int print(char *buf, int len, const struct asn1_string_st *);
  static std::string_view print_0x(const asn1_string_st *serial);
  static void free(struct asn1_string_st *);

  class HexString {
   public:
    HexString(const struct asn1_string_st *);
    const char *c_str() { return buf.c_str(); }

   private:
    static constexpr size_t Length = 2 + (MaxLengthInBytes * 2) + 1;
    cstrbuf<Length> buf;
  };
};

class ClusterCertAuthority {
 public:
  static struct x509_st *create(struct evp_pkey_st *key,
                                const char *ordinal = "First",
                                bool self_sign = true);
  static int sign(struct x509_st *ca, struct evp_pkey_st *, struct x509_st *);
  static constexpr const char *Subject = "MySQL NDB Cluster %s Certificate";
  static constexpr const char *CertFile = "NDB-Cluster-cert";
  static constexpr const char *KeyFile = "NDB-Cluster-private-key";
};

class NodeCertificate : public CertSubject, public CertLifetime {
 public:
  /* Public Constructors */
  NodeCertificate(Node::Type type, int node_id);
  NodeCertificate(const SigningRequest &, struct evp_pkey_st *key);

  /* Named constructors (const) */
  static const NodeCertificate *from_credentials(struct stack_st_X509 *,
                                                 struct evp_pkey_st *);
  static const NodeCertificate *for_peer(struct x509_st *);

  /* Destructor */
  ~NodeCertificate();

  /* Const Public Instance Methods */
  struct evp_pkey_st *key() const {
    return m_key;
  }
  struct x509_st *cert() const {
    return m_x509;
  }
  struct stack_st_X509 *all_certs() const {
    return m_all_certs;
  }

  BaseString serial_number() const;

  bool name_is_conforming() const { return m_name_conforming; }
  bool is_signed() const { return m_signed; }
  bool is_final() const { return m_final; }

  /* Public Instance Methods */
  bool create_keys(const char *curve);
  // For set_own_keys(), NodeCertificate does not increment the reference count.
  // It becomes the owner of the pointers, and the caller must not free them.
  bool set_own_keys(struct evp_pkey_st *, struct x509_st *);
  // For set_key() and set_cert(), the NodeCertificate claims a reference,
  // so the caller should use Certificate::free() and PrivateKey::free() to
  // free the supplied pointers.
  bool set_key(struct evp_pkey_st *key);
  bool set_cert(struct x509_st *cert);
  bool set_signed_cert(struct x509_st *cert);  // Replace X509 with signed one
  bool parse_name(const char *);

  /* push_extra_ca_cert()
   *
   * When a new has CA replaced an old CA, and the NodeCertificate is signed
   * with the new CA, push the old CA onto its authority chain. This may
   * only be done *after* finalise().
   *
   * Returns true on success.
   */
  bool push_extra_ca_cert(struct x509_st *old_ca);

  /* finalise() sets the serial number, issuer name, lifetime, and extensions
     in the X.509 Node Certificate. If CA_key is non-null, it also signs the
     certificate. A finalised but unsigned certificate may be sent to a CA host
     for remote signing. A signed and finalised NodeCertificate should be
     considered immutable, except for adding CAs to its authority chain.

     Returns 0 on success.
  */
  int finalise(struct x509_st *CA_cert, struct evp_pkey_st *CA_key);

  /* Self-sign the node certificate
   */
  int self_sign();

  /* Verify the signature; writes diagnostic output on stderr */
  bool verify_signature(struct evp_pkey_st *CA_key) const;

  /* Verify trust from CA to cert; writes diagnostic output on failure */
  bool verify_chain() const;

 protected:
  friend class TlsKeyManager;
  NodeCertificate() : CertSubject(), CertLifetime(0) {}
  void init_from_x509(struct x509_st *);
  bool parse_name();
  static int stderr_callback(int result, struct x509_store_ctx_st *);
  void init_from_credentials(struct stack_st_X509 *, struct evp_pkey_st *,
                             bool up_ref_count = false);

 private:
  struct evp_pkey_st *m_key{nullptr};
  struct x509_st *m_x509{nullptr};
  struct stack_st_X509 *m_all_certs{nullptr};

  bool m_x509_names_set{false};
  bool m_name_conforming{false};
  bool m_final{false};
  bool m_signed{false};
  bool m_self_signed{false};
};

#endif
