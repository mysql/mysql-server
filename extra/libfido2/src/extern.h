/*
 * Copyright (c) 2018 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#ifndef _EXTERN_H
#define _EXTERN_H

#include <stdint.h>

#include "fido/types.h"
#include "blob.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* aes256 */
int aes256_cbc_dec(const fido_blob_t *, const fido_blob_t *, fido_blob_t *);
int aes256_cbc_enc(const fido_blob_t *, const fido_blob_t *, fido_blob_t *);

/* cbor encoding functions */
cbor_item_t *cbor_flatten_vector(cbor_item_t **, size_t);
cbor_item_t *cbor_encode_assert_options(fido_opt_t, fido_opt_t);
cbor_item_t *cbor_encode_change_pin_auth(const fido_blob_t *,
    const fido_blob_t *, const fido_blob_t *);
cbor_item_t *cbor_encode_extensions(const fido_cred_ext_t *);
cbor_item_t *cbor_encode_hmac_secret_param(const fido_blob_t *,
    const es256_pk_t *, const fido_blob_t *);
cbor_item_t *cbor_encode_options(fido_opt_t, fido_opt_t);
cbor_item_t *cbor_encode_pin_auth(const fido_blob_t *, const fido_blob_t *);
cbor_item_t *cbor_encode_pin_enc(const fido_blob_t *, const fido_blob_t *);
cbor_item_t *cbor_encode_pin_hash_enc(const fido_blob_t *, const fido_blob_t *);
cbor_item_t *cbor_encode_pin_opt(void);
cbor_item_t *cbor_encode_pubkey(const fido_blob_t *);
cbor_item_t *cbor_encode_pubkey_list(const fido_blob_array_t *);
cbor_item_t *cbor_encode_pubkey_param(int);
cbor_item_t *cbor_encode_rp_entity(const fido_rp_t *);
cbor_item_t *cbor_encode_set_pin_auth(const fido_blob_t *, const fido_blob_t *);
cbor_item_t *cbor_encode_user_entity(const fido_user_t *);
cbor_item_t *es256_pk_encode(const es256_pk_t *, int);

/* cbor decoding functions */
int cbor_decode_attstmt(const cbor_item_t *, fido_attstmt_t *);
int cbor_decode_cred_authdata(const cbor_item_t *, int, fido_blob_t *,
    fido_authdata_t *, fido_attcred_t *, fido_cred_ext_t *);
int cbor_decode_assert_authdata(const cbor_item_t *, fido_blob_t *,
    fido_authdata_t *, int *, fido_blob_t *);
int cbor_decode_cred_id(const cbor_item_t *, fido_blob_t *);
int cbor_decode_fmt(const cbor_item_t *, char **);
int cbor_decode_pubkey(const cbor_item_t *, int *, void *);
int cbor_decode_rp_entity(const cbor_item_t *, fido_rp_t *);
int cbor_decode_uint64(const cbor_item_t *, uint64_t *);
int cbor_decode_user(const cbor_item_t *, fido_user_t *);
int es256_pk_decode(const cbor_item_t *, es256_pk_t *);
int rs256_pk_decode(const cbor_item_t *, rs256_pk_t *);
int eddsa_pk_decode(const cbor_item_t *, eddsa_pk_t *);

/* auxiliary cbor routines */
int cbor_add_bool(cbor_item_t *, const char *, fido_opt_t);
int cbor_add_bytestring(cbor_item_t *, const char *, const unsigned char *,
    size_t);
int cbor_add_string(cbor_item_t *, const char *, const char *);
int cbor_array_iter(const cbor_item_t *, void *, int(*)(const cbor_item_t *,
    void *));
int cbor_build_frame(uint8_t, cbor_item_t *[], size_t, fido_blob_t *);
int cbor_bytestring_copy(const cbor_item_t *, unsigned char **, size_t *);
int cbor_map_iter(const cbor_item_t *, void *, int(*)(const cbor_item_t *,
    const cbor_item_t *, void *));
int cbor_string_copy(const cbor_item_t *, char **);
int cbor_parse_reply(const unsigned char *, size_t, void *,
    int(*)(const cbor_item_t *, const cbor_item_t *, void *));
int cbor_add_pin_params(fido_dev_t *, const fido_blob_t *, const es256_pk_t *,
    const fido_blob_t *,const char *, cbor_item_t **, cbor_item_t **);
void cbor_vector_free(cbor_item_t **, size_t);

#ifndef nitems
#define nitems(_a)	(sizeof((_a)) / sizeof((_a)[0]))
#endif

/* buf */
int fido_buf_read(const unsigned char **, size_t *, void *, size_t);
int fido_buf_write(unsigned char **, size_t *, const void *, size_t);

/* hid i/o */
void *fido_hid_open(const char *);
void  fido_hid_close(void *);
int fido_hid_read(void *, unsigned char *, size_t, int);
int fido_hid_write(void *, const unsigned char *, size_t);
size_t fido_hid_report_in_len(void *);
size_t fido_hid_report_out_len(void *);

/* generic i/o */
int fido_rx_cbor_status(fido_dev_t *, int);
int fido_rx(fido_dev_t *, uint8_t, void *, size_t, int);
int fido_tx(fido_dev_t *, uint8_t, const void *, size_t);

/* log */
#ifdef FIDO_NO_DIAGNOSTIC
#define fido_log_init(...)	do { /* nothing */ } while (0)
#define fido_log_debug(...)	do { /* nothing */ } while (0)
#define fido_log_xxd(...)	do { /* nothing */ } while (0)
#else
#ifdef __GNUC__
void fido_log_init(void);
void fido_log_debug(const char *, ...)
    __attribute__((__format__ (printf, 1, 2)));
void fido_log_xxd(const void *, size_t);
#else
void fido_log_init(void);
void fido_log_debug(const char *, ...);
void fido_log_xxd(const void *, size_t);
#endif /* __GNUC__ */
#endif /* FIDO_NO_DIAGNOSTIC */

/* u2f */
int u2f_register(fido_dev_t *, fido_cred_t *, int);
int u2f_authenticate(fido_dev_t *, fido_assert_t *, int);
int u2f_get_touch_begin(fido_dev_t *);
int u2f_get_touch_status(fido_dev_t *, int *, int);

/* unexposed fido ops */
int fido_dev_authkey(fido_dev_t *, es256_pk_t *);
int fido_dev_get_cbor_info_wait(fido_dev_t *, fido_cbor_info_t *, int);
int fido_dev_get_pin_token(fido_dev_t *, const char *, const fido_blob_t *,
    const es256_pk_t *, fido_blob_t *);
int fido_do_ecdh(fido_dev_t *, es256_pk_t **, fido_blob_t **);

/* misc */
void fido_assert_reset_rx(fido_assert_t *);
void fido_assert_reset_tx(fido_assert_t *);
void fido_cred_reset_rx(fido_cred_t *);
void fido_cred_reset_tx(fido_cred_t *);
int fido_check_rp_id(const char *, const unsigned char *);
int fido_check_flags(uint8_t, fido_opt_t, fido_opt_t);

/* crypto */
int fido_verify_sig_es256(const fido_blob_t *, const es256_pk_t *,
    const fido_blob_t *);
int fido_verify_sig_rs256(const fido_blob_t *, const rs256_pk_t *,
    const fido_blob_t *);
int fido_verify_sig_eddsa(const fido_blob_t *, const eddsa_pk_t *,
    const fido_blob_t *);
int fido_get_signed_hash(int, fido_blob_t *, const fido_blob_t *,
    const fido_blob_t *);

/* hid device manifest */
int fido_hid_manifest(fido_dev_info_t *, size_t, size_t *);

/* device manifest registration */
typedef int (*dev_manifest_func_t)(fido_dev_info_t *, size_t, size_t *);
int fido_dev_register_manifest_func(const dev_manifest_func_t);
void fido_dev_unregister_manifest_func(const dev_manifest_func_t);

/* fuzzing instrumentation */
#ifdef FIDO_FUZZ
uint32_t uniform_random(uint32_t);
#endif

/* internal device capability flags */
#define FIDO_DEV_PIN_SET	0x01
#define FIDO_DEV_PIN_UNSET	0x02
#define FIDO_DEV_CRED_PROT	0x04

/* miscellanea */
#define FIDO_DUMMY_CLIENTDATA	""
#define FIDO_DUMMY_RP_ID	"localhost"
#define FIDO_DUMMY_USER_NAME	"dummy"
#define FIDO_DUMMY_USER_ID	1

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* !_EXTERN_H */
