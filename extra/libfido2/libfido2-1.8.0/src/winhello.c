/*
 * Copyright (c) 2021 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <sys/types.h>

#include <stdlib.h>
#include <windows.h>
#include <webauthn.h>

#include "fido.h"

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

static char *
to_utf8(const wchar_t *utf16)
{
	int nch;
	char *utf8;

	if (utf16 == NULL) {
		fido_log_debug("%s: NULL", __func__);
		return NULL;
	}
	if ((nch = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, utf16,
	    -1, NULL, 0, NULL, NULL)) < 1 || (size_t)nch > MAXCHARS) {
		fido_log_debug("%s: WideCharToMultiByte %d", __func__);
		return NULL;
	}
	if ((utf8 = calloc((size_t)nch, sizeof(*utf8))) == NULL) {
		fido_log_debug("%s: calloc", __func__);
		return NULL;
	}
	if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, utf16, -1,
	    utf8, nch, NULL, NULL) != nch) {
		fido_log_debug("%s: WideCharToMultiByte", __func__);
		free(utf8);
		return NULL;
	}

	return utf8;
}

static int
to_fido_str_array(fido_str_array_t *sa, const char **v, size_t n)
{
	if ((sa->ptr = calloc(n, sizeof(char *))) == NULL) {
		fido_log_debug("%s: calloc", __func__);
		return -1;
	}
	for (size_t i = 0; i < n; i++) {
		if ((sa->ptr[i] = strdup(v[i])) == NULL) {
			fido_log_debug("%s: strdup", __func__);
			return -1;
		}
		sa->len++;
	}

	return 0;
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
		fido_log_debug("%s: hr=0x%x", __func__, hr);
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
set_uv(DWORD *out, fido_opt_t uv, const char *pin)
{
	if (pin) {
		*out = WEBAUTHN_USER_VERIFICATION_REQUIREMENT_REQUIRED;
		return 0;
	}

	switch (uv) {
	case FIDO_OPT_OMIT:
		*out = WEBAUTHN_USER_VERIFICATION_REQUIREMENT_ANY;
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
    fido_rp_t *in)
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
    WEBAUTHN_USER_ENTITY_INFORMATION *out, fido_user_t *in)
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
pack_cred_ext(WEBAUTHN_EXTENSIONS *out, fido_cred_ext_t *in)
{
	WEBAUTHN_EXTENSION *e;
	WEBAUTHN_CRED_PROTECT_EXTENSION_IN *p;
	BOOL *b;
	size_t n = 0, i = 0;

	if (in->mask == 0) {
		return 0; /* nothing to do */
	}
	if (in->mask & ~(FIDO_EXT_HMAC_SECRET | FIDO_EXT_CRED_PROTECT)) {
		fido_log_debug("%s: mask 0x%x", in->mask);
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
unpack_fmt(fido_cred_t *cred, WEBAUTHN_CREDENTIAL_ATTESTATION *att)
{
	char *fmt;
	int r;

	if ((fmt = to_utf8(att->pwszFormatType)) == NULL) {
		fido_log_debug("%s: fmt", __func__);
		return -1;
	}
	r = fido_cred_set_fmt(cred, fmt);
	free(fmt);
	fmt = NULL;
	if (r != FIDO_OK) {
		fido_log_debug("%s: fido_cred_set_fmt: %s", __func__,
		    fido_strerr(r));
		return -1;
	}

	return 0;
}

static int
unpack_cred_authdata(fido_cred_t *cred, WEBAUTHN_CREDENTIAL_ATTESTATION *att)
{
	int r;

	if (att->cbAuthenticatorData > SIZE_MAX) {
		fido_log_debug("%s: cbAuthenticatorData", __func__);
		return -1;
	}
	if ((r = fido_cred_set_authdata_raw(cred, att->pbAuthenticatorData,
	    (size_t)att->cbAuthenticatorData)) != FIDO_OK) {
		fido_log_debug("%s: fido_cred_set_authdata_raw: %s", __func__,
		    fido_strerr(r));
		return -1;
	}

	return 0;
}

static int
unpack_cred_sig(fido_cred_t *cred, WEBAUTHN_COMMON_ATTESTATION *attr)
{
	int r;

	if (attr->cbSignature > SIZE_MAX) {
		fido_log_debug("%s: cbSignature", __func__);
		return -1;
	}
	if ((r = fido_cred_set_sig(cred, attr->pbSignature,
	    (size_t)attr->cbSignature)) != FIDO_OK) {
		fido_log_debug("%s: fido_cred_set_sig: %s", __func__,
		    fido_strerr(r));
		return -1;
	}

	return 0;
}

static int
unpack_x5c(fido_cred_t *cred, WEBAUTHN_COMMON_ATTESTATION *attr)
{
	int r;

	fido_log_debug("%s: %u cert(s)", __func__, attr->cX5c);

	if (attr->cX5c == 0)
		return 0; /* self-attestation */
	if (attr->lAlg != WEBAUTHN_COSE_ALGORITHM_ECDSA_P256_WITH_SHA256) {
		fido_log_debug("%s: lAlg %d", __func__, attr->lAlg);
		return -1;
	}
	if (attr->pX5c[0].cbData > SIZE_MAX) {
		fido_log_debug("%s: cbData", __func__);
		return -1;
	}
	if ((r = fido_cred_set_x509(cred, attr->pX5c[0].pbData,
	    (size_t)attr->pX5c[0].cbData)) != FIDO_OK) {
		fido_log_debug("%s: fido_cred_set_x509: %s", __func__,
		    fido_strerr(r));
		return -1;
	}

	return 0;
}

static int
unpack_assert_authdata(fido_assert_t *assert, WEBAUTHN_ASSERTION *wa)
{
	int r;

	if (wa->cbAuthenticatorData > SIZE_MAX) {
		fido_log_debug("%s: cbAuthenticatorData", __func__);
		return -1;
	}
	if ((r = fido_assert_set_authdata_raw(assert, 0, wa->pbAuthenticatorData,
	    (size_t)wa->cbAuthenticatorData)) != FIDO_OK) {
		fido_log_debug("%s: fido_assert_set_authdata_raw: %s", __func__,
		    fido_strerr(r));
		return -1;
	}

	return 0;
}

static int
unpack_assert_sig(fido_assert_t *assert, WEBAUTHN_ASSERTION *wa)
{
	int r;

	if (wa->cbSignature > SIZE_MAX) {
		fido_log_debug("%s: cbSignature", __func__);
		return -1;
	}
	if ((r = fido_assert_set_sig(assert, 0, wa->pbSignature,
	    (size_t)wa->cbSignature)) != FIDO_OK) {
		fido_log_debug("%s: fido_assert_set_sig: %s", __func__,
		    fido_strerr(r));
		return -1;
	}

	return 0;
}

static int
unpack_cred_id(fido_assert_t *assert, WEBAUTHN_ASSERTION *wa)
{
	if (wa->Credential.cbId > SIZE_MAX) {
		fido_log_debug("%s: Credential.cbId", __func__);
		return -1;
	}
	if (fido_blob_set(&assert->stmt[0].id, wa->Credential.pbId,
	    (size_t)wa->Credential.cbId) < 0) {
		fido_log_debug("%s: fido_blob_set", __func__);
		return -1;
	}

	return 0;
}

static int
unpack_user_id(fido_assert_t *assert, WEBAUTHN_ASSERTION *wa)
{
	if (wa->cbUserId == 0)
		return 0; /* user id absent */
	if (wa->cbUserId > SIZE_MAX) {
		fido_log_debug("%s: cbUserId", __func__);
		return -1;
	}
	if (fido_blob_set(&assert->stmt[0].user.id, wa->pbUserId,
	    (size_t)wa->cbUserId) < 0) {
		fido_log_debug("%s: fido_blob_set", __func__);
		return -1;
	}

	return 0;
}

static int
translate_fido_assert(struct winhello_assert *ctx, fido_assert_t *assert,
    const char *pin)
{
	WEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS *opt;

	/* not supported by webauthn.h */
	if (assert->up == FIDO_OPT_FALSE) {
		fido_log_debug("%s: up %d", __func__, assert->up);
		return FIDO_ERR_UNSUPPORTED_OPTION;
	}
	/* not implemented */
	if (assert->ext.mask) {
		fido_log_debug("%s: ext 0x%x", __func__, assert->ext.mask);
		return FIDO_ERR_UNSUPPORTED_EXTENSION;
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
	opt->dwTimeoutMilliseconds = MAXMSEC;
	if (pack_credlist(&opt->CredentialList, &assert->allow_list) < 0) {
		fido_log_debug("%s: pack_credlist", __func__);
		return FIDO_ERR_INTERNAL;
	}
	if (set_uv(&opt->dwUserVerificationRequirement, assert->uv, pin) < 0) {
		fido_log_debug("%s: set_uv", __func__);
		return FIDO_ERR_INTERNAL;
	}

	return FIDO_OK;
}

static int
translate_winhello_assert(fido_assert_t *assert, WEBAUTHN_ASSERTION *wa)
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

	return FIDO_OK;
}

static int
translate_fido_cred(struct winhello_cred *ctx, fido_cred_t *cred,
    const char *pin)
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
	opt->dwTimeoutMilliseconds = MAXMSEC;
	if (pack_credlist(&opt->CredentialList, &cred->excl) < 0) {
		fido_log_debug("%s: pack_credlist", __func__);
		return FIDO_ERR_INTERNAL;
	}
	if (pack_cred_ext(&opt->Extensions, &cred->ext) < 0) {
		fido_log_debug("%s: pack_cred_ext", __func__);
		return FIDO_ERR_UNSUPPORTED_EXTENSION;
	}
	if (set_uv(&opt->dwUserVerificationRequirement, cred->uv, pin) < 0) {
		fido_log_debug("%s: set_uv", __func__);
		return FIDO_ERR_INTERNAL;
	}
	if (cred->rk == FIDO_OPT_TRUE) {
		opt->bRequireResidentKey = true;
	}

	return FIDO_OK;
}

static int
translate_winhello_cred(fido_cred_t *cred, WEBAUTHN_CREDENTIAL_ATTESTATION *att)
{
	if (unpack_fmt(cred, att) < 0) {
		fido_log_debug("%s: unpack_fmt", __func__);
		return FIDO_ERR_INTERNAL;
	}
	if (unpack_cred_authdata(cred, att) < 0) {
		fido_log_debug("%s: unpack_cred_authdata", __func__);
		return FIDO_ERR_INTERNAL;
	}

	switch (att->dwAttestationDecodeType) {
	case WEBAUTHN_ATTESTATION_DECODE_NONE:
		if (att->pvAttestationDecode != NULL) {
			fido_log_debug("%s: pvAttestationDecode", __func__);
			return FIDO_ERR_INTERNAL;
		}
		break;
	case WEBAUTHN_ATTESTATION_DECODE_COMMON:
		if (att->pvAttestationDecode == NULL) {
			fido_log_debug("%s: pvAttestationDecode", __func__);
			return FIDO_ERR_INTERNAL;
		}
		if (unpack_cred_sig(cred, att->pvAttestationDecode) < 0) {
			fido_log_debug("%s: unpack_cred_sig", __func__);
			return FIDO_ERR_INTERNAL;
		}
		if (unpack_x5c(cred, att->pvAttestationDecode) < 0) {
			fido_log_debug("%s: unpack_x5c", __func__);
			return FIDO_ERR_INTERNAL;
		}
		break;
	default:
		fido_log_debug("%s: dwAttestationDecodeType: %u", __func__,
		    att->dwAttestationDecodeType);
		return FIDO_ERR_INTERNAL;
	}

	return FIDO_OK;
}

static int
winhello_manifest(BOOL *present)
{
	DWORD n;
	HRESULT hr;
	int r = FIDO_OK;

	if ((n = WebAuthNGetApiVersionNumber()) < 1) {
		fido_log_debug("%s: unsupported api %u", __func__, n);
		return FIDO_ERR_INTERNAL;
	}
	fido_log_debug("%s: api version %u", __func__, n);
	hr = WebAuthNIsUserVerifyingPlatformAuthenticatorAvailable(present);
	if (hr != S_OK)  {
		r = to_fido(hr);
		fido_log_debug("%s: %ls -> %s", __func__,
		    WebAuthNGetErrorName(hr), fido_strerr(r));
	}

	return r;
}

static int
winhello_get_assert(HWND w, struct winhello_assert *ctx)
{
	HRESULT hr;
	int r = FIDO_OK;

	hr = WebAuthNAuthenticatorGetAssertion(w, ctx->rp_id, &ctx->cd,
	    &ctx->opt, &ctx->assert);
	if (hr != S_OK) {
		r = to_fido(hr);
		fido_log_debug("%s: %ls -> %s", __func__,
		    WebAuthNGetErrorName(hr), fido_strerr(r));
	}

	return r;
}

static int
winhello_make_cred(HWND w, struct winhello_cred *ctx)
{
	HRESULT hr;
	int r = FIDO_OK;

	hr = WebAuthNAuthenticatorMakeCredential(w, &ctx->rp, &ctx->user,
	    &ctx->cose, &ctx->cd, &ctx->opt, &ctx->att);
	if (hr != S_OK) {
		r = to_fido(hr);
		fido_log_debug("%s: %ls -> %s", __func__,
		    WebAuthNGetErrorName(hr), fido_strerr(r));
	}

	return r;
}

static void
winhello_assert_free(struct winhello_assert *ctx)
{
	if (ctx == NULL)
		return;
	if (ctx->assert != NULL)
		WebAuthNFreeAssertion(ctx->assert);

	free(ctx->rp_id);
	free(ctx->opt.CredentialList.pCredentials);
	free(ctx);
}

static void
winhello_cred_free(struct winhello_cred *ctx)
{
	if (ctx == NULL)
		return;
	if (ctx->att != NULL)
		WebAuthNFreeCredentialAttestation(ctx->att);

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
	int r;
	BOOL present;
	fido_dev_info_t *di;

	if (ilen == 0) {
		return FIDO_OK;
	}
	if (devlist == NULL) {
		return FIDO_ERR_INVALID_ARGUMENT;
	}
	if ((r = winhello_manifest(&present)) != FIDO_OK) {
		fido_log_debug("%s: winhello_manifest", __func__);
		return r;
	}
	if (present == false) {
		fido_log_debug("%s: not present", __func__);
		return FIDO_OK;
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
    const char *pin)
{
	HWND			 w;
	struct winhello_assert	*ctx;
	int			 r = FIDO_ERR_INTERNAL;

	(void)dev;

	if ((ctx = calloc(1, sizeof(*ctx))) == NULL) {
		fido_log_debug("%s: calloc", __func__);
		goto fail;
	}
	if ((w = GetForegroundWindow()) == NULL) {
		fido_log_debug("%s: GetForegroundWindow", __func__);
		goto fail;
	}
	if ((r = translate_fido_assert(ctx, assert, pin)) != FIDO_OK) {
		fido_log_debug("%s: translate_fido_assert", __func__);
		goto fail;
	}
	if ((r = winhello_get_assert(w, ctx)) != S_OK) {
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
	const char *o[4] = { "rk", "up", "plat", "clientPin" };

	(void)dev;

	fido_cbor_info_reset(ci);

	if (to_fido_str_array(&ci->versions, v, nitems(v)) < 0 ||
	    to_fido_str_array(&ci->extensions, e, nitems(e)) < 0 ||
	    to_fido_str_array(&ci->transports, t, nitems(t)) < 0) {
		fido_log_debug("%s: to_fido_str_array", __func__);
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
fido_winhello_make_cred(fido_dev_t *dev, fido_cred_t *cred, const char *pin)
{
	HWND			 w;
	struct winhello_cred	*ctx;
	int			 r = FIDO_ERR_INTERNAL;

	(void)dev;

	if ((ctx = calloc(1, sizeof(*ctx))) == NULL) {
		fido_log_debug("%s: calloc", __func__);
		goto fail;
	}
	if ((w = GetForegroundWindow()) == NULL) {
		fido_log_debug("%s: GetForegroundWindow", __func__);
		goto fail;
	}
	if ((r = translate_fido_cred(ctx, cred, pin)) != FIDO_OK) {
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
