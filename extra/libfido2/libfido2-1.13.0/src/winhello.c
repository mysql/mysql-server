/*
 * Copyright (c) 2021-2022 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>

#include <stdlib.h>
#include <windows.h>

#include "fido.h"
#include "webauthn.h"

#ifndef NTE_INVALID_PARAMETER
#define NTE_INVALID_PARAMETER	_HRESULT_TYPEDEF_(0x80090027)
#endif
#ifndef NTE_NOT_SUPPORTED
#define NTE_NOT_SUPPORTED	_HRESULT_TYPEDEF_(0x80090029)
#endif
#ifndef NTE_DEVICE_NOT_FOUND
#define NTE_DEVICE_NOT_FOUND	_HRESULT_TYPEDEF_(0x80090035)
#endif

#define MAXCHARS	128
#define MAXCREDS	128
#define MAXMSEC		6000 * 1000
#define VENDORID	0x045e
#define PRODID		0x0001

struct winhello_assert {
	WEBAUTHN_CLIENT_DATA				 cd;
	WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS	 opt;
	WEBAUTHN_ASSERTION				*assert;
	wchar_t						*rp_id;
};

struct winhello_cred {
	WEBAUTHN_RP_ENTITY_INFORMATION			 rp;
	WEBAUTHN_USER_ENTITY_INFORMATION		 user;
	WEBAUTHN_COSE_CREDENTIAL_PARAMETER		 alg;
	WEBAUTHN_COSE_CREDENTIAL_PARAMETERS		 cose;
	WEBAUTHN_CLIENT_DATA				 cd;
	WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS	 opt;
	WEBAUTHN_CREDENTIAL_ATTESTATION			*att;
	wchar_t						*rp_id;
	wchar_t						*rp_name;
	wchar_t						*user_name;
	wchar_t						*user_icon;
	wchar_t						*display_name;
};

typedef DWORD	WINAPI	webauthn_get_api_version_t(void);
typedef PCWSTR	WINAPI	webauthn_strerr_t(HRESULT);
typedef HRESULT	WINAPI	webauthn_get_assert_t(HWND, LPCWSTR,
			    PCWEBAUTHN_CLIENT_DATA,
			    PCWEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS,
			    PWEBAUTHN_ASSERTION *);
typedef HRESULT	WINAPI	webauthn_make_cred_t(HWND,
			    PCWEBAUTHN_RP_ENTITY_INFORMATION,
			    PCWEBAUTHN_USER_ENTITY_INFORMATION,
			    PCWEBAUTHN_COSE_CREDENTIAL_PARAMETERS,
			    PCWEBAUTHN_CLIENT_DATA,
			    PCWEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS,
			    PWEBAUTHN_CREDENTIAL_ATTESTATION *);
typedef void	WINAPI	webauthn_free_assert_t(PWEBAUTHN_ASSERTION);
typedef void	WINAPI	webauthn_free_attest_t(PWEBAUTHN_CREDENTIAL_ATTESTATION);

static TLS BOOL				 webauthn_loaded;
static TLS HMODULE			 webauthn_handle;
static TLS webauthn_get_api_version_t	*webauthn_get_api_version;
static TLS webauthn_strerr_t		*webauthn_strerr;
static TLS webauthn_get_assert_t	*webauthn_get_assert;
static TLS webauthn_make_cred_t		*webauthn_make_cred;
static TLS webauthn_free_assert_t	*webauthn_free_assert;
static TLS webauthn_free_attest_t	*webauthn_free_attest;

static int
webauthn_load(void)
{
	DWORD n = 1;

	if (webauthn_loaded || webauthn_handle != NULL) {
		fido_log_debug("%s: already loaded", __func__);
		return -1;
	}
	if ((webauthn_handle = LoadLibrary(TEXT("webauthn.dll"))) == NULL) {
		fido_log_debug("%s: LoadLibrary", __func__);
		return -1;
	}

	if ((webauthn_get_api_version =
	    (webauthn_get_api_version_t *)GetProcAddress(webauthn_handle,
	    "WebAuthNGetApiVersionNumber")) == NULL) {
		fido_log_debug("%s: WebAuthNGetApiVersionNumber", __func__);
		/* WebAuthNGetApiVersionNumber might not exist */
	}
	if (webauthn_get_api_version != NULL &&
	    (n = webauthn_get_api_version()) < 1) {
		fido_log_debug("%s: unsupported api %lu", __func__, (u_long)n);
		goto fail;
	}
	fido_log_debug("%s: api version %lu", __func__, (u_long)n);
	if ((webauthn_strerr =
	    (webauthn_strerr_t *)GetProcAddress(webauthn_handle,
	    "WebAuthNGetErrorName")) == NULL) {
		fido_log_debug("%s: WebAuthNGetErrorName", __func__);
		goto fail;
	}
	if ((webauthn_get_assert =
	    (webauthn_get_assert_t *)GetProcAddress(webauthn_handle,
	    "WebAuthNAuthenticatorGetAssertion")) == NULL) {
		fido_log_debug("%s: WebAuthNAuthenticatorGetAssertion",
		    __func__);
		goto fail;
	}
	if ((webauthn_make_cred =
	    (webauthn_make_cred_t *)GetProcAddress(webauthn_handle,
	    "WebAuthNAuthenticatorMakeCredential")) == NULL) {
		fido_log_debug("%s: WebAuthNAuthenticatorMakeCredential",
		    __func__);
		goto fail;
	}
	if ((webauthn_free_assert =
	    (webauthn_free_assert_t *)GetProcAddress(webauthn_handle,
	    "WebAuthNFreeAssertion")) == NULL) {
		fido_log_debug("%s: WebAuthNFreeAssertion", __func__);
		goto fail;
	}
	if ((webauthn_free_attest =
	    (webauthn_free_attest_t *)GetProcAddress(webauthn_handle,
	    "WebAuthNFreeCredentialAttestation")) == NULL) {
		fido_log_debug("%s: WebAuthNFreeCredentialAttestation",
		    __func__);
		goto fail;
	}

	webauthn_loaded = true;

	return 0;
fail:
	fido_log_debug("%s: GetProcAddress", __func__);
	webauthn_get_api_version = NULL;
	webauthn_strerr = NULL;
	webauthn_get_assert = NULL;
	webauthn_make_cred = NULL;
	webauthn_free_assert = NULL;
	webauthn_free_attest = NULL;
	FreeLibrary(webauthn_handle);
	webauthn_handle = NULL;

	return -1;
}

static wchar_t *
to_utf16(const char *utf8)
{
	int nch;
	wchar_t *utf16;

	if (utf8 == NULL) {
		fido_log_debug("%s: NULL", __func__);
		return NULL;
	}
	if ((nch = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0)) < 1 ||
	    (size_t)nch > MAXCHARS) {
		fido_log_debug("%s: MultiByteToWideChar %d", __func__, nch);
		return NULL;
	}
	if ((utf16 = calloc((size_t)nch, sizeof(*utf16))) == NULL) {
		fido_log_debug("%s: calloc", __func__);
		return NULL;
	}
	if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, utf16, nch) != nch) {
		fido_log_debug("%s: MultiByteToWideChar", __func__);
		free(utf16);
		return NULL;
	}

	return utf16;
}

static int
to_fido(HRESULT hr)
{
	switch (hr) {
	case NTE_NOT_SUPPORTED:
		return FIDO_ERR_UNSUPPORTED_OPTION;
	case NTE_INVALID_PARAMETER:
		return FIDO_ERR_INVALID_PARAMETER;
	case NTE_TOKEN_KEYSET_STORAGE_FULL:
		return FIDO_ERR_KEY_STORE_FULL;
	case NTE_DEVICE_NOT_FOUND:
	case NTE_NOT_FOUND:
		return FIDO_ERR_NOT_ALLOWED;
	default:
		fido_log_debug("%s: hr=0x%lx", __func__, (u_long)hr);
		return FIDO_ERR_INTERNAL;
	}
}

static int
pack_cd(WEBAUTHN_CLIENT_DATA *out, const fido_blob_t *in)
{
	if (in->ptr == NULL) {
		fido_log_debug("%s: NULL", __func__);
		return -1;
	}
	if (in->len > ULONG_MAX) {
		fido_log_debug("%s: in->len=%zu", __func__, in->len);
		return -1;
	}
	out->dwVersion = WEBAUTHN_CLIENT_DATA_CURRENT_VERSION;
	out->cbClientDataJSON = (DWORD)in->len;
	out->pbClientDataJSON = in->ptr;
	out->pwszHashAlgId = WEBAUTHN_HASH_ALGORITHM_SHA_256;

	return 0;
}

static int
pack_credlist(WEBAUTHN_CREDENTIALS *out, const fido_blob_array_t *in)
{
	WEBAUTHN_CREDENTIAL *c;

	if (in->len == 0) {
		return 0; /* nothing to do */
	}
	if (in->len > MAXCREDS) {
		fido_log_debug("%s: in->len=%zu", __func__, in->len);
		return -1;
	}
	if ((out->pCredentials = calloc(in->len, sizeof(*c))) == NULL) {
		fido_log_debug("%s: calloc", __func__);
		return -1;
	}
	out->cCredentials = (DWORD)in->len;
	for (size_t i = 0; i < in->len; i++) {
		if (in->ptr[i].len > ULONG_MAX) {
			fido_log_debug("%s: %zu", __func__, in->ptr[i].len);
			return -1;
		}
		c = &out->pCredentials[i];
		c->dwVersion = WEBAUTHN_CREDENTIAL_CURRENT_VERSION;
		c->cbId = (DWORD)in->ptr[i].len;
		c->pbId = in->ptr[i].ptr;
		c->pwszCredentialType = WEBAUTHN_CREDENTIAL_TYPE_PUBLIC_KEY;
	}

	return 0;
}

static int
set_cred_uv(DWORD *out, fido_opt_t uv, const char *pin)
{
	if (pin) {
		*out = WEBAUTHN_USER_VERIFICATION_REQUIREMENT_REQUIRED;
		return 0;
	}

	switch (uv) {
	case FIDO_OPT_OMIT:
	case FIDO_OPT_FALSE:
		*out = WEBAUTHN_USER_VERIFICATION_REQUIREMENT_DISCOURAGED;
		break;
	case FIDO_OPT_TRUE:
		*out = WEBAUTHN_USER_VERIFICATION_REQUIREMENT_REQUIRED;
		break;
	}

	return 0;
}

static int
set_assert_uv(DWORD *out, fido_opt_t uv, const char *pin)
{
	if (pin) {
		*out = WEBAUTHN_USER_VERIFICATION_REQUIREMENT_REQUIRED;
		return 0;
	}

	switch (uv) {
	case FIDO_OPT_OMIT:
		*out = WEBAUTHN_USER_VERIFICATION_REQUIREMENT_PREFERRED;
		break;
	case FIDO_OPT_FALSE:
		*out = WEBAUTHN_USER_VERIFICATION_REQUIREMENT_DISCOURAGED;
		break;
	case FIDO_OPT_TRUE:
		*out = WEBAUTHN_USER_VERIFICATION_REQUIREMENT_REQUIRED;
		break;
	}

	return 0;
}

static int
pack_rp(wchar_t **id, wchar_t **name, WEBAUTHN_RP_ENTITY_INFORMATION *out,
    const fido_rp_t *in)
{
	/* keep non-const copies of pwsz* for free() */
	out->dwVersion = WEBAUTHN_RP_ENTITY_INFORMATION_CURRENT_VERSION;
	if ((out->pwszId = *id = to_utf16(in->id)) == NULL) {
		fido_log_debug("%s: id", __func__);
		return -1;
	}
	if (in->name && (out->pwszName = *name = to_utf16(in->name)) == NULL) {
		fido_log_debug("%s: name", __func__);
		return -1;
	}
	return 0;
}

static int
pack_user(wchar_t **name, wchar_t **icon, wchar_t **display_name,
    WEBAUTHN_USER_ENTITY_INFORMATION *out, const fido_user_t *in)
{
	if (in->id.ptr == NULL || in->id.len > ULONG_MAX) {
		fido_log_debug("%s: id", __func__);
		return -1;
	}
	out->dwVersion = WEBAUTHN_USER_ENTITY_INFORMATION_CURRENT_VERSION;
	out->cbId = (DWORD)in->id.len;
	out->pbId = in->id.ptr;
	/* keep non-const copies of pwsz* for free() */
	if (in->name != NULL) {
		if ((out->pwszName = *name = to_utf16(in->name)) == NULL) {
			fido_log_debug("%s: name", __func__);
			return -1;
		}
	}
	if (in->icon != NULL) {
		if ((out->pwszIcon = *icon = to_utf16(in->icon)) == NULL) {
			fido_log_debug("%s: icon", __func__);
			return -1;
		}
	}
	if (in->display_name != NULL) {
		if ((out->pwszDisplayName = *display_name =
		    to_utf16(in->display_name)) == NULL) {
			fido_log_debug("%s: display_name", __func__);
			return -1;
		}
	}

	return 0;
}

static int
pack_cose(WEBAUTHN_COSE_CREDENTIAL_PARAMETER *alg,
    WEBAUTHN_COSE_CREDENTIAL_PARAMETERS *cose, int type)
{
	switch (type) {
	case COSE_ES256:
		alg->lAlg = WEBAUTHN_COSE_ALGORITHM_ECDSA_P256_WITH_SHA256;
		break;
	case COSE_ES384:
		alg->lAlg = WEBAUTHN_COSE_ALGORITHM_ECDSA_P384_WITH_SHA384;
		break;
	case COSE_EDDSA:
		alg->lAlg = -8; /* XXX */;
		break;
	case COSE_RS256:
		alg->lAlg = WEBAUTHN_COSE_ALGORITHM_RSASSA_PKCS1_V1_5_WITH_SHA256;
		break;
	default:
		fido_log_debug("%s: type %d", __func__, type);
		return -1;
	}
	alg->dwVersion = WEBAUTHN_COSE_CREDENTIAL_PARAMETER_CURRENT_VERSION;
	alg->pwszCredentialType = WEBAUTHN_CREDENTIAL_TYPE_PUBLIC_KEY;
	cose->cCredentialParameters = 1;
	cose->pCredentialParameters = alg;

	return 0;
}

static int
pack_cred_ext(WEBAUTHN_EXTENSIONS *out, const fido_cred_ext_t *in)
{
	WEBAUTHN_EXTENSION *e;
	WEBAUTHN_CRED_PROTECT_EXTENSION_IN *p;
	BOOL *b;
	size_t n = 0, i = 0;

	if (in->mask == 0) {
		return 0; /* nothing to do */
	}
	if (in->mask & ~(FIDO_EXT_HMAC_SECRET | FIDO_EXT_CRED_PROTECT)) {
		fido_log_debug("%s: mask 0x%x", __func__, in->mask);
		return -1;
	}
	if (in->mask & FIDO_EXT_HMAC_SECRET)
		n++;
	if (in->mask & FIDO_EXT_CRED_PROTECT)
		n++;
	if ((out->pExtensions = calloc(n, sizeof(*e))) == NULL) {
		fido_log_debug("%s: calloc", __func__);
		return -1;
	}
	out->cExtensions = (DWORD)n;
	if (in->mask & FIDO_EXT_HMAC_SECRET) {
		if ((b = calloc(1, sizeof(*b))) == NULL) {
			fido_log_debug("%s: calloc", __func__);
			return -1;
		}
		*b = true;
		e = &out->pExtensions[i];
		e->pwszExtensionIdentifier =
		    WEBAUTHN_EXTENSIONS_IDENTIFIER_HMAC_SECRET;
		e->pvExtension = b;
		e->cbExtension = sizeof(*b);
		i++;
	}
	if (in->mask & FIDO_EXT_CRED_PROTECT) {
		if ((p = calloc(1, sizeof(*p))) == NULL) {
			fido_log_debug("%s: calloc", __func__);
			return -1;
		}
		p->dwCredProtect = (DWORD)in->prot;
		p->bRequireCredProtect = true;
		e = &out->pExtensions[i];
		e->pwszExtensionIdentifier =
		    WEBAUTHN_EXTENSIONS_IDENTIFIER_CRED_PROTECT;
		e->pvExtension = p;
		e->cbExtension = sizeof(*p);
		i++;
	}

	return 0;
}

static int
pack_assert_ext(WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS *out,
    const fido_assert_ext_t *in)
{
	WEBAUTHN_HMAC_SECRET_SALT_VALUES *v;
	WEBAUTHN_HMAC_SECRET_SALT *s;

	if (in->mask == 0) {
		return 0; /* nothing to do */
	}
	if (in->mask != FIDO_EXT_HMAC_SECRET) {
		fido_log_debug("%s: mask 0x%x", __func__, in->mask);
		return -1;
	}
	if (in->hmac_salt.ptr == NULL ||
	    in->hmac_salt.len != WEBAUTHN_CTAP_ONE_HMAC_SECRET_LENGTH) {
		fido_log_debug("%s: salt %p/%zu", __func__,
		    (const void *)in->hmac_salt.ptr, in->hmac_salt.len);
		return -1;
	}
	if ((v = calloc(1, sizeof(*v))) == NULL ||
	    (s = calloc(1, sizeof(*s))) == NULL) {
		free(v);
		fido_log_debug("%s: calloc", __func__);
		return -1;
	}
	s->cbFirst = (DWORD)in->hmac_salt.len;
	s->pbFirst = in->hmac_salt.ptr;
	v->pGlobalHmacSalt = s;
	out->pHmacSecretSaltValues = v;
	out->dwFlags |= WEBAUTHN_AUTHENTICATOR_HMAC_SECRET_VALUES_FLAG;
	out->dwVersion = WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS_VERSION_6;

	return 0;
}

static int
unpack_assert_authdata(fido_assert_t *assert, const WEBAUTHN_ASSERTION *wa)
{
	int r;

	if ((r = fido_assert_set_authdata_raw(assert, 0, wa->pbAuthenticatorData,
	    wa->cbAuthenticatorData)) != FIDO_OK) {
		fido_log_debug("%s: fido_assert_set_authdata_raw: %s", __func__,
		    fido_strerr(r));
		return -1;
	}

	return 0;
}

static int
unpack_assert_sig(fido_assert_t *assert, const WEBAUTHN_ASSERTION *wa)
{
	int r;

	if ((r = fido_assert_set_sig(assert, 0, wa->pbSignature,
	    wa->cbSignature)) != FIDO_OK) {
		fido_log_debug("%s: fido_assert_set_sig: %s", __func__,
		    fido_strerr(r));
		return -1;
	}

	return 0;
}

static int
unpack_cred_id(fido_assert_t *assert, const WEBAUTHN_ASSERTION *wa)
{
	if (fido_blob_set(&assert->stmt[0].id, wa->Credential.pbId,
	    wa->Credential.cbId) < 0) {
		fido_log_debug("%s: fido_blob_set", __func__);
		return -1;
	}

	return 0;
}

static int
unpack_user_id(fido_assert_t *assert, const WEBAUTHN_ASSERTION *wa)
{
	if (wa->cbUserId == 0)
		return 0; /* user id absent */
	if (fido_blob_set(&assert->stmt[0].user.id, wa->pbUserId,
	    wa->cbUserId) < 0) {
		fido_log_debug("%s: fido_blob_set", __func__);
		return -1;
	}

	return 0;
}

static int
unpack_hmac_secret(fido_assert_t *assert, const WEBAUTHN_ASSERTION *wa)
{
	if (wa->dwVersion != WEBAUTHN_ASSERTION_VERSION_3) {
		fido_log_debug("%s: dwVersion %u", __func__,
		    (unsigned)wa->dwVersion);
		return 0; /* proceed without hmac-secret */
	}
	if (wa->pHmacSecret == NULL ||
	    wa->pHmacSecret->cbFirst == 0 ||
	    wa->pHmacSecret->pbFirst == NULL) {
		fido_log_debug("%s: hmac-secret absent", __func__);
		return 0; /* proceed without hmac-secret */
	}
	if (wa->pHmacSecret->cbSecond != 0 ||
	    wa->pHmacSecret->pbSecond != NULL) {
		fido_log_debug("%s: 64-byte hmac-secret", __func__);
		return 0; /* proceed without hmac-secret */
	}
	if (!fido_blob_is_empty(&assert->stmt[0].hmac_secret)) {
		fido_log_debug("%s: fido_blob_is_empty", __func__);
		return -1;
	}
	if (fido_blob_set(&assert->stmt[0].hmac_secret,
	    wa->pHmacSecret->pbFirst, wa->pHmacSecret->cbFirst) < 0) {
		fido_log_debug("%s: fido_blob_set", __func__);
		return -1;
	}

	return 0;
}

static int
translate_fido_assert(struct winhello_assert *ctx, const fido_assert_t *assert,
    const char *pin, int ms)
{
	WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS *opt;

	/* not supported by webauthn.h */
	if (assert->up == FIDO_OPT_FALSE) {
		fido_log_debug("%s: up %d", __func__, assert->up);
		return FIDO_ERR_UNSUPPORTED_OPTION;
	}
	if ((ctx->rp_id = to_utf16(assert->rp_id)) == NULL) {
		fido_log_debug("%s: rp_id", __func__);
		return FIDO_ERR_INTERNAL;
	}
	if (pack_cd(&ctx->cd, &assert->cd) < 0) {
		fido_log_debug("%s: pack_cd", __func__);
		return FIDO_ERR_INTERNAL;
	}
	/* options */
	opt = &ctx->opt;
	opt->dwVersion = WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS_VERSION_1;
	opt->dwTimeoutMilliseconds = ms < 0 ? MAXMSEC : (DWORD)ms;
	if (pack_credlist(&opt->CredentialList, &assert->allow_list) < 0) {
		fido_log_debug("%s: pack_credlist", __func__);
		return FIDO_ERR_INTERNAL;
	}
	if (pack_assert_ext(opt, &assert->ext) < 0) {
		fido_log_debug("%s: pack_assert_ext", __func__);
		return FIDO_ERR_UNSUPPORTED_EXTENSION;
	}
	if (set_assert_uv(&opt->dwUserVerificationRequirement, assert->uv,
	    pin) < 0) {
		fido_log_debug("%s: set_assert_uv", __func__);
		return FIDO_ERR_INTERNAL;
	}

	return FIDO_OK;
}

static int
translate_winhello_assert(fido_assert_t *assert, const WEBAUTHN_ASSERTION *wa)
{
	int r;

	if (assert->stmt_len > 0) {
		fido_log_debug("%s: stmt_len=%zu", __func__, assert->stmt_len);
		return FIDO_ERR_INTERNAL;
	}
	if ((r = fido_assert_set_count(assert, 1)) != FIDO_OK) {
		fido_log_debug("%s: fido_assert_set_count: %s", __func__,
		    fido_strerr(r));
		return FIDO_ERR_INTERNAL;
	}
	if (unpack_assert_authdata(assert, wa) < 0) {
		fido_log_debug("%s: unpack_assert_authdata", __func__);
		return FIDO_ERR_INTERNAL;
	}
	if (unpack_assert_sig(assert, wa) < 0) {
		fido_log_debug("%s: unpack_assert_sig", __func__);
		return FIDO_ERR_INTERNAL;
	}
	if (unpack_cred_id(assert, wa) < 0) {
		fido_log_debug("%s: unpack_cred_id", __func__);
		return FIDO_ERR_INTERNAL;
	}
	if (unpack_user_id(assert, wa) < 0) {
		fido_log_debug("%s: unpack_user_id", __func__);
		return FIDO_ERR_INTERNAL;
	}
	if (assert->ext.mask & FIDO_EXT_HMAC_SECRET &&
	    unpack_hmac_secret(assert, wa) < 0) {
		fido_log_debug("%s: unpack_hmac_secret", __func__);
		return FIDO_ERR_INTERNAL;
	}

	return FIDO_OK;
}

static int
translate_fido_cred(struct winhello_cred *ctx, const fido_cred_t *cred,
    const char *pin, int ms)
{
	WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS *opt;

	if (pack_rp(&ctx->rp_id, &ctx->rp_name, &ctx->rp, &cred->rp) < 0) {
		fido_log_debug("%s: pack_rp", __func__);
		return FIDO_ERR_INTERNAL;
	}
	if (pack_user(&ctx->user_name, &ctx->user_icon, &ctx->display_name,
	    &ctx->user, &cred->user) < 0) {
		fido_log_debug("%s: pack_user", __func__);
		return FIDO_ERR_INTERNAL;
	}
	if (pack_cose(&ctx->alg, &ctx->cose, cred->type) < 0) {
		fido_log_debug("%s: pack_cose", __func__);
		return FIDO_ERR_INTERNAL;
	}
	if (pack_cd(&ctx->cd, &cred->cd) < 0) {
		fido_log_debug("%s: pack_cd", __func__);
		return FIDO_ERR_INTERNAL;
	}
	/* options */
	opt = &ctx->opt;
	opt->dwVersion = WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS_VERSION_1;
	opt->dwTimeoutMilliseconds = ms < 0 ? MAXMSEC : (DWORD)ms;
	opt->dwAttestationConveyancePreference =
	    WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_DIRECT;
	if (pack_credlist(&opt->CredentialList, &cred->excl) < 0) {
		fido_log_debug("%s: pack_credlist", __func__);
		return FIDO_ERR_INTERNAL;
	}
	if (pack_cred_ext(&opt->Extensions, &cred->ext) < 0) {
		fido_log_debug("%s: pack_cred_ext", __func__);
		return FIDO_ERR_UNSUPPORTED_EXTENSION;
	}
	if (set_cred_uv(&opt->dwUserVerificationRequirement, (cred->ext.mask &
	    FIDO_EXT_CRED_PROTECT) ? FIDO_OPT_TRUE : cred->uv, pin) < 0) {
		fido_log_debug("%s: set_cred_uv", __func__);
		return FIDO_ERR_INTERNAL;
	}
	if (cred->rk == FIDO_OPT_TRUE) {
		opt->bRequireResidentKey = true;
	}

	return FIDO_OK;
}

static int
decode_attobj(const cbor_item_t *key, const cbor_item_t *val, void *arg)
{
	fido_cred_t *cred = arg;
	char *name = NULL;
	int ok = -1;

	if (cbor_string_copy(key, &name) < 0) {
		fido_log_debug("%s: cbor type", __func__);
		ok = 0; /* ignore */
		goto fail;
	}

	if (!strcmp(name, "fmt")) {
		if (cbor_decode_fmt(val, &cred->fmt) < 0) {
			fido_log_debug("%s: cbor_decode_fmt", __func__);
			goto fail;
		}
	} else if (!strcmp(name, "attStmt")) {
		if (cbor_decode_attstmt(val, &cred->attstmt) < 0) {
			fido_log_debug("%s: cbor_decode_attstmt", __func__);
			goto fail;
		}
	} else if (!strcmp(name, "authData")) {
		if (fido_blob_decode(val, &cred->authdata_raw) < 0) {
			fido_log_debug("%s: fido_blob_decode", __func__);
			goto fail;
		}
		if (cbor_decode_cred_authdata(val, cred->type,
		    &cred->authdata_cbor, &cred->authdata, &cred->attcred,
		    &cred->authdata_ext) < 0) {
			fido_log_debug("%s: cbor_decode_cred_authdata",
			    __func__);
			goto fail;
		}
	}

	ok = 0;
fail:
	free(name);

	return (ok);
}

static int
translate_winhello_cred(fido_cred_t *cred,
    const WEBAUTHN_CREDENTIAL_ATTESTATION *att)
{
	cbor_item_t *item = NULL;
	struct cbor_load_result cbor;
	int r = FIDO_ERR_INTERNAL;

	if (att->pbAttestationObject == NULL) {
		fido_log_debug("%s: pbAttestationObject", __func__);
		goto fail;
	}
	if ((item = cbor_load(att->pbAttestationObject,
	    att->cbAttestationObject, &cbor)) == NULL) {
		fido_log_debug("%s: cbor_load", __func__);
		goto fail;
	}
	if (cbor_isa_map(item) == false ||
	    cbor_map_is_definite(item) == false ||
	    cbor_map_iter(item, cred, decode_attobj) < 0) {
		fido_log_debug("%s: cbor type", __func__);
		goto fail;
	}

	r = FIDO_OK;
fail:
	if (item != NULL)
		cbor_decref(&item);

	return r;
}

static int
winhello_get_assert(HWND w, struct winhello_assert *ctx)
{
	HRESULT hr;
	int r = FIDO_OK;

	if ((hr = webauthn_get_assert(w, ctx->rp_id, &ctx->cd, &ctx->opt,
	    &ctx->assert)) != S_OK) {
		r = to_fido(hr);
		fido_log_debug("%s: %ls -> %s", __func__, webauthn_strerr(hr),
		    fido_strerr(r));
	}

	return r;
}

static int
winhello_make_cred(HWND w, struct winhello_cred *ctx)
{
	HRESULT hr;
	int r = FIDO_OK;

	if ((hr = webauthn_make_cred(w, &ctx->rp, &ctx->user, &ctx->cose,
	    &ctx->cd, &ctx->opt, &ctx->att)) != S_OK) {
		r = to_fido(hr);
		fido_log_debug("%s: %ls -> %s", __func__, webauthn_strerr(hr),
		    fido_strerr(r));
	}

	return r;
}

static void
winhello_assert_free(struct winhello_assert *ctx)
{
	if (ctx == NULL)
		return;
	if (ctx->assert != NULL)
		webauthn_free_assert(ctx->assert);

	free(ctx->rp_id);
	free(ctx->opt.CredentialList.pCredentials);
	if (ctx->opt.pHmacSecretSaltValues != NULL)
		free(ctx->opt.pHmacSecretSaltValues->pGlobalHmacSalt);
	free(ctx->opt.pHmacSecretSaltValues);
	free(ctx);
}

static void
winhello_cred_free(struct winhello_cred *ctx)
{
	if (ctx == NULL)
		return;
	if (ctx->att != NULL)
		webauthn_free_attest(ctx->att);

	free(ctx->rp_id);
	free(ctx->rp_name);
	free(ctx->user_name);
	free(ctx->user_icon);
	free(ctx->display_name);
	free(ctx->opt.CredentialList.pCredentials);
	for (size_t i = 0; i < ctx->opt.Extensions.cExtensions; i++) {
		WEBAUTHN_EXTENSION *e;
		e = &ctx->opt.Extensions.pExtensions[i];
		free(e->pvExtension);
	}
	free(ctx->opt.Extensions.pExtensions);
	free(ctx);
}

int
fido_winhello_manifest(fido_dev_info_t *devlist, size_t ilen, size_t *olen)
{
	fido_dev_info_t *di;

	if (ilen == 0) {
		return FIDO_OK;
	}
	if (devlist == NULL) {
		return FIDO_ERR_INVALID_ARGUMENT;
	}
	if (!webauthn_loaded && webauthn_load() < 0) {
		fido_log_debug("%s: webauthn_load", __func__);
		return FIDO_OK; /* not an error */
	}

	di = &devlist[*olen];
	memset(di, 0, sizeof(*di));
	di->path = strdup(FIDO_WINHELLO_PATH);
	di->manufacturer = strdup("Microsoft Corporation");
	di->product = strdup("Windows Hello");
	di->vendor_id = VENDORID;
	di->product_id = PRODID;
	if (di->path == NULL || di->manufacturer == NULL ||
	    di->product == NULL) {
		free(di->path);
		free(di->manufacturer);
		free(di->product);
		explicit_bzero(di, sizeof(*di));
		return FIDO_ERR_INTERNAL;
	}
	++(*olen);

	return FIDO_OK;
}

int
fido_winhello_open(fido_dev_t *dev)
{
	if (!webauthn_loaded && webauthn_load() < 0) {
		fido_log_debug("%s: webauthn_load", __func__);
		return FIDO_ERR_INTERNAL;
	}
	if (dev->flags != 0)
		return FIDO_ERR_INVALID_ARGUMENT;
	dev->attr.flags = FIDO_CAP_CBOR | FIDO_CAP_WINK;
	dev->flags = FIDO_DEV_WINHELLO | FIDO_DEV_CRED_PROT | FIDO_DEV_PIN_SET;

	return FIDO_OK;
}

int
fido_winhello_close(fido_dev_t *dev)
{
	memset(dev, 0, sizeof(*dev));

	return FIDO_OK;
}

int
fido_winhello_cancel(fido_dev_t *dev)
{
	(void)dev;

	return FIDO_ERR_INTERNAL;
}

int
fido_winhello_get_assert(fido_dev_t *dev, fido_assert_t *assert,
    const char *pin, int ms)
{
	HWND			 w;
	struct winhello_assert	*ctx;
	int			 r = FIDO_ERR_INTERNAL;

	(void)dev;

	fido_assert_reset_rx(assert);

	if ((ctx = calloc(1, sizeof(*ctx))) == NULL) {
		fido_log_debug("%s: calloc", __func__);
		goto fail;
	}
	if ((w = GetForegroundWindow()) == NULL) {
		fido_log_debug("%s: GetForegroundWindow", __func__);
		if ((w = GetTopWindow(NULL)) == NULL) {
			fido_log_debug("%s: GetTopWindow", __func__);
			goto fail;
		}
	}
	if ((r = translate_fido_assert(ctx, assert, pin, ms)) != FIDO_OK) {
		fido_log_debug("%s: translate_fido_assert", __func__);
		goto fail;
	}
	if ((r = winhello_get_assert(w, ctx)) != FIDO_OK) {
		fido_log_debug("%s: winhello_get_assert", __func__);
		goto fail;
	}
	if ((r = translate_winhello_assert(assert, ctx->assert)) != FIDO_OK) {
		fido_log_debug("%s: translate_winhello_assert", __func__);
		goto fail;
	}

fail:
	winhello_assert_free(ctx);

	return r;
}

int
fido_winhello_get_cbor_info(fido_dev_t *dev, fido_cbor_info_t *ci)
{
	const char *v[3] = { "U2F_V2", "FIDO_2_0", "FIDO_2_1_PRE" };
	const char *e[2] = { "credProtect", "hmac-secret" };
	const char *t[2] = { "nfc", "usb" };
	const char *o[4] = { "rk", "up", "uv", "plat" };

	(void)dev;

	fido_cbor_info_reset(ci);

	if (fido_str_array_pack(&ci->versions, v, nitems(v)) < 0 ||
	    fido_str_array_pack(&ci->extensions, e, nitems(e)) < 0 ||
	    fido_str_array_pack(&ci->transports, t, nitems(t)) < 0) {
		fido_log_debug("%s: fido_str_array_pack", __func__);
		return FIDO_ERR_INTERNAL;
	}
	if ((ci->options.name = calloc(nitems(o), sizeof(char *))) == NULL ||
	    (ci->options.value = calloc(nitems(o), sizeof(bool))) == NULL) {
		fido_log_debug("%s: calloc", __func__);
		return FIDO_ERR_INTERNAL;
	}
	for (size_t i = 0; i < nitems(o); i++) {
		if ((ci->options.name[i] = strdup(o[i])) == NULL) {
			fido_log_debug("%s: strdup", __func__);
			return FIDO_ERR_INTERNAL;
		}
		ci->options.value[i] = true;
		ci->options.len++;
	}

	return FIDO_OK;
}

int
fido_winhello_make_cred(fido_dev_t *dev, fido_cred_t *cred, const char *pin,
    int ms)
{
	HWND			 w;
	struct winhello_cred	*ctx;
	int			 r = FIDO_ERR_INTERNAL;

	(void)dev;

	fido_cred_reset_rx(cred);

	if ((ctx = calloc(1, sizeof(*ctx))) == NULL) {
		fido_log_debug("%s: calloc", __func__);
		goto fail;
	}
	if ((w = GetForegroundWindow()) == NULL) {
		fido_log_debug("%s: GetForegroundWindow", __func__);
		if ((w = GetTopWindow(NULL)) == NULL) {
			fido_log_debug("%s: GetTopWindow", __func__);
			goto fail;
		}
	}
	if ((r = translate_fido_cred(ctx, cred, pin, ms)) != FIDO_OK) {
		fido_log_debug("%s: translate_fido_cred", __func__);
		goto fail;
	}
	if ((r = winhello_make_cred(w, ctx)) != FIDO_OK) {
		fido_log_debug("%s: winhello_make_cred", __func__);
		goto fail;
	}
	if ((r = translate_winhello_cred(cred, ctx->att)) != FIDO_OK) {
		fido_log_debug("%s: translate_winhello_cred", __func__);
		goto fail;
	}

	r = FIDO_OK;
fail:
	winhello_cred_free(ctx);

	return r;
}
