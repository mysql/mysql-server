/*
 * Copyright (c) 2018 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#ifndef _EXTERN_H_
#define _EXTERN_H_

#include <stddef.h>
#include <stdio.h>

#include <openssl/ec.h>
#include <fido.h>

struct blob {
	unsigned char *ptr;
	size_t len;
};

#define TOKEN_OPT	"CDILPRSVbcdei:k:n:r"

#define FLAG_DEBUG	0x01
#define FLAG_QUIET	0x02
#define FLAG_RK		0x04
#define FLAG_UV		0x08
#define FLAG_U2F	0x10
#define FLAG_HMAC	0x20
#define FLAG_UP		0x40

EC_KEY *read_ec_pubkey(const char *);
fido_dev_t *open_dev(const char *);
FILE *open_read(const char *);
FILE *open_write(const char *);
const char *cose_string(int);
const char *prot_string(int);
int assert_get(int, char **);
int assert_verify(int, char **);
int base64_decode(const char *, void **, size_t *);
int base64_encode(const void *, size_t, char **);
int base64_read(FILE *, struct blob *);
int bio_delete(fido_dev_t *, char *, char *);
int bio_enroll(char *);
void bio_info(fido_dev_t *);
int bio_list(char *);
int bio_set_name(char *, char *, char *);
int cose_type(const char *, int *);
int cred_make(int, char **);
int cred_verify(int, char **);
int credman_delete_rk(fido_dev_t *, const char *, char *);
int credman_get_metadata(fido_dev_t *, const char *);
int credman_list_rk(char *, const char *);
int credman_list_rp(char *);
int credman_print_rk(fido_dev_t *, const char *, char *, char *);
int pin_change(char *);
int pin_set(char *);
int string_read(FILE *, char **);
int token_delete(int, char **, char *);
int token_info(int, char **, char *);
int token_list(int, char **, char *);
int token_reset(char *);
int token_set(int, char **, char *);
int write_ec_pubkey(FILE *, const void *, size_t);
int write_rsa_pubkey(FILE *, const void *, size_t);
RSA *read_rsa_pubkey(const char *);
EVP_PKEY *read_eddsa_pubkey(const char *);
int write_eddsa_pubkey(FILE *, const void *, size_t);
void print_cred(FILE *, int, const fido_cred_t *);
void read_pin(const char *, char *, size_t);
void usage(void);
void xxd(const void *, size_t);
int base10(const char *);

#endif /* _EXTERN_H_ */
