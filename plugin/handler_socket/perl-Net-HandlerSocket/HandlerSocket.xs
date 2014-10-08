
// vim:ai:sw=2:ts=8

/*
 * Copyright (C) 2010 DeNA Co.,Ltd.. All rights reserved.
 * See COPYRIGHT.txt for details.
 */

#undef VERSION
#include <config.h>

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"
#include "hstcpcli.hpp"

#define DBG(x)

static SV *
arr_get_entry(AV *av, I32 avmax, I32 idx)
{
  if (idx > avmax) {
    DBG(fprintf(stderr, "arr_get_entry1 %d %d\n", avmax, idx));
    return 0;
  }
  SV **const ev = av_fetch(av, idx, 0);
  if (ev == 0) {
    DBG(fprintf(stderr, "arr_get_entry2 %d %d\n", avmax, idx));
    return 0;
  }
  return *ev;
}

static int
arr_get_intval(AV *av, I32 avmax, I32 idx, int default_val = 0)
{
  SV *const e = arr_get_entry(av, avmax, idx);
  if (e == 0) {
    return default_val;
  }
  return SvIV(e);
}

static const char *
sv_get_strval(SV *sv)
{
  if (sv == 0 || !SvPOK(sv)) {
    DBG(fprintf(stderr, "sv_get_strval\n"));
    return 0;
  }
  return SvPV_nolen(sv);
}

static const char *
arr_get_strval(AV *av, I32 avmax, I32 idx)
{
  SV *const e = arr_get_entry(av, avmax, idx);
  return sv_get_strval(e);
}

static AV *
sv_get_arrval(SV *sv)
{
  if (sv == 0 || !SvROK(sv)) {
    DBG(fprintf(stderr, "sv_get_arrval1\n"));
    return 0;
  }
  SV *const svtarget = SvRV(sv);
  if (svtarget == 0 || SvTYPE(svtarget) != SVt_PVAV) {
    DBG(fprintf(stderr, "sv_get_arrval2\n"));
    return 0;
  }
  return (AV *)svtarget;
}

static AV *
arr_get_arrval(AV *av, I32 avmax, I32 idx)
{
  SV *const e = arr_get_entry(av, avmax, idx);
  if (e == 0) {
    DBG(fprintf(stderr, "arr_get_arrval1\n"));
    return 0;
  }
  return sv_get_arrval(e);
}

static void
hv_to_strmap(HV *hv, std::map<std::string, std::string>& m_r)
{
  if (hv == 0) {
    return;
  }
  hv_iterinit(hv);
  HE *hent = 0;
  while ((hent = hv_iternext(hv)) != 0) {
    I32 klen = 0;
    char *const k = hv_iterkey(hent, &klen);
    DBG(fprintf(stderr, "k=%s\n", k));
    const std::string key(k, klen);
    SV *const vsv = hv_iterval(hv, hent);
    STRLEN vlen = 0;
    char *const v = SvPV(vsv, vlen);
    DBG(fprintf(stderr, "v=%s\n", v));
    const std::string val(v, vlen);
    m_r[key] = val;
  }
}

static void
strrefarr_push_back(std::vector<dena::string_ref>& a_r, SV *sv)
{
  if (sv == 0 || SvTYPE(sv) == SVt_NULL) { /* !SvPOK()? */
    DBG(fprintf(stderr, "strrefarr_push_back: null\n"));
    return a_r.push_back(dena::string_ref());
  }
  STRLEN vlen = 0;
  char *const v = SvPV(sv, vlen);
  DBG(fprintf(stderr, "strrefarr_push_back: %s\n", v));
  a_r.push_back(dena::string_ref(v, vlen));
}

static void
av_to_strrefarr(AV *av, std::vector<dena::string_ref>& a_r)
{
  if (av == 0) {
    return;
  }
  const I32 len = av_len(av) + 1;
  for (I32 i = 0; i < len; ++i) {
    SV **const ev = av_fetch(av, i, 0);
    strrefarr_push_back(a_r, ev ? *ev : 0);
  }
}

static dena::string_ref
sv_get_string_ref(SV *sv)
{
  if (sv == 0 || SvTYPE(sv) == SVt_NULL) { /* !SvPOK()? */
    return dena::string_ref();
  }
  STRLEN vlen = 0;
  char *const v = SvPV(sv, vlen);
  return dena::string_ref(v, vlen);
}

static IV
sv_get_iv(SV *sv)
{
  if (sv == 0 || !SvIOK(sv)) {
    return 0;
  }
  return SvIV(sv);
}

static void
av_to_filters(AV *av, std::vector<dena::hstcpcli_filter>& f_r)
{
  DBG(fprintf(stderr, "av_to_filters: %p\n", av));
  if (av == 0) {
    return;
  }
  const I32 len = av_len(av) + 1;
  DBG(fprintf(stderr, "av_to_filters: len=%d\n", (int)len));
  for (I32 i = 0; i < len; ++i) {
    AV *const earr = arr_get_arrval(av, len, i);
    if (earr == 0) {
      continue;
    }
    const I32 earrlen = av_len(earr) + 1;
    dena::hstcpcli_filter fe;
    fe.filter_type = sv_get_string_ref(arr_get_entry(earr, earrlen, 0));
    fe.op = sv_get_string_ref(arr_get_entry(earr, earrlen, 1));
    fe.ff_offset = sv_get_iv(arr_get_entry(earr, earrlen, 2));
    fe.val = sv_get_string_ref(arr_get_entry(earr, earrlen, 3));
    f_r.push_back(fe);
    DBG(fprintf(stderr, "av_to_filters: %s %s %d %s\n",
      fe.filter_action.begin(), fe.filter_op.begin(), (int)fe.ff_offset,
      fe.value.begin()));
  }
}

static void
set_process_verbose_level(const std::map<std::string, std::string>& m)
{
  std::map<std::string, std::string>::const_iterator iter = m.find("verbose");
  if (iter != m.end()) {
    dena::verbose_level = atoi(iter->second.c_str());
  }
}

static AV *
execute_internal(SV *obj, int id, const char *op, AV *keys, int limit,
  int skip, const char *modop, AV *modvals, AV *filters, int invalues_keypart,
  AV *invalues)
{
  AV *retval = (AV *)&PL_sv_undef;
  dena::hstcpcli_i *const ptr =
    reinterpret_cast<dena::hstcpcli_i *>(SvIV(SvRV(obj)));
  do {
    std::vector<dena::string_ref> keyarr, mvarr;
    std::vector<dena::hstcpcli_filter> farr;
    std::vector<dena::string_ref> ivs;
    av_to_strrefarr(keys, keyarr);
    dena::string_ref modop_ref;
    if (modop != 0) {
      modop_ref = dena::string_ref(modop, strlen(modop));
      av_to_strrefarr(modvals, mvarr);
    }
    if (filters != 0) {
      av_to_filters(filters, farr);
    }
    if (invalues_keypart >= 0 && invalues != 0) {
      av_to_strrefarr(invalues, ivs);
    }
    ptr->request_buf_exec_generic(id, dena::string_ref(op, strlen(op)),
      &keyarr[0], keyarr.size(), limit, skip, modop_ref, &mvarr[0],
      mvarr.size(), &farr[0], farr.size(), invalues_keypart, &ivs[0],
      ivs.size());
    AV *const av = newAV();
    retval = av;
    if (ptr->request_send() != 0) {
      break;
    }
    size_t nflds = 0;
    ptr->response_recv(nflds);
    const int e = ptr->get_error_code();
    DBG(fprintf(stderr, "e=%d nflds=%zu\n", e, nflds));
    av_push(av, newSViv(e));
    if (e != 0) {
      const std::string s = ptr->get_error();
      av_push(av, newSVpvn(s.data(), s.size()));
    } else {
      const dena::string_ref *row = 0;
      while ((row = ptr->get_next_row()) != 0) {
	DBG(fprintf(stderr, "row=%p\n", row));
	for (size_t i = 0; i < nflds; ++i) {
	  const dena::string_ref& v = row[i];
	  DBG(fprintf(stderr, "FLD %zu v=%s vbegin=%p\n", i,
	    std::string(v.begin(), v.size())
	      .c_str(), v.begin()));
	  if (v.begin() != 0) {
	    SV *const e = newSVpvn(
	      v.begin(), v.size());
	    av_push(av, e);
	  } else {
	    av_push(av, &PL_sv_undef);
	  }
	}
      }
    }
    if (e >= 0) {
      ptr->response_buf_remove();
    }
  } while (0);
  return retval;
}

struct execute_arg {
  int id;
  const char *op;
  AV *keys;
  int limit;
  int skip;
  const char *modop;
  AV *modvals;
  AV *filters;
  int invalues_keypart;
  AV *invalues;
  execute_arg() : id(0), op(0), keys(0), limit(0), skip(0), modop(0),
    modvals(0), filters(0), invalues_keypart(-1), invalues(0) { }
};

static AV *
execute_multi_internal(SV *obj, const execute_arg *args, size_t num_args)
{
  dena::hstcpcli_i *const ptr =
    reinterpret_cast<dena::hstcpcli_i *>(SvIV(SvRV(obj)));
  /* appends multiple requests to the send buffer */
  for (size_t i = 0; i < num_args; ++i) {
    std::vector<dena::string_ref> keyarr, mvarr;
    std::vector<dena::hstcpcli_filter> farr;
    std::vector<dena::string_ref> ivs;
    const execute_arg& arg = args[i];
    av_to_strrefarr(arg.keys, keyarr);
    dena::string_ref modop_ref;
    if (arg.modop != 0) {
      modop_ref = dena::string_ref(arg.modop, strlen(arg.modop));
      av_to_strrefarr(arg.modvals, mvarr);
    }
    if (arg.filters != 0) {
      av_to_filters(arg.filters, farr);
    }
    if (arg.invalues_keypart >= 0 && arg.invalues != 0) {
      av_to_strrefarr(arg.invalues, ivs);
    }
    ptr->request_buf_exec_generic(arg.id,
      dena::string_ref(arg.op, strlen(arg.op)), &keyarr[0], keyarr.size(),
      arg.limit, arg.skip, modop_ref, &mvarr[0], mvarr.size(), &farr[0],
      farr.size(), arg.invalues_keypart, &ivs[0], ivs.size());
  }
  AV *const retval = newAV();
  /* sends the requests */
  if (ptr->request_send() < 0) {
    /* IO error */
    AV *const av_respent = newAV();
    av_push(retval, newRV_noinc((SV *)av_respent));
    av_push(av_respent, newSViv(ptr->get_error_code()));
    const std::string& s = ptr->get_error();
    av_push(av_respent, newSVpvn(s.data(), s.size()));
    return retval; /* retval : [ [ err_code, err_message ] ] */
  }
  /* receives responses */
  for (size_t i = 0; i < num_args; ++i) {
    AV *const av_respent = newAV();
    av_push(retval, newRV_noinc((SV *)av_respent));
    size_t nflds = 0;
    const int e = ptr->response_recv(nflds);
    av_push(av_respent, newSViv(e));
    if (e != 0) {
      const std::string& s = ptr->get_error();
      av_push(av_respent, newSVpvn(s.data(), s.size()));
    } else {
      const dena::string_ref *row = 0;
      while ((row = ptr->get_next_row()) != 0) {
	for (size_t i = 0; i < nflds; ++i) {
	  const dena::string_ref& v = row[i];
	  DBG(fprintf(stderr, "%zu %s\n", i,
	    std::string(v.begin(), v.size()).c_str()));
	  if (v.begin() != 0) {
	    av_push(av_respent, newSVpvn(v.begin(), v.size()));
	  } else {
	    /* null */
	    av_push(av_respent, &PL_sv_undef);
	  }
	}
      }
    }
    if (e >= 0) {
      ptr->response_buf_remove();
    }
    if (e < 0) {
      return retval;
    }
  }
  return retval;
}

MODULE = Net::HandlerSocket    PACKAGE = Net::HandlerSocket

SV *
new(klass, args)
  char *klass
  HV *args
CODE:
  RETVAL = &PL_sv_undef;
  dena::config conf;
  hv_to_strmap(args, conf);
  set_process_verbose_level(conf);
  dena::socket_args sargs;
  sargs.set(conf);
  dena::hstcpcli_ptr p = dena::hstcpcli_i::create(sargs);
  SV *const objref = newSViv(0);
  SV *const obj = newSVrv(objref, klass);
  dena::hstcpcli_i *const ptr = p.get();
  sv_setiv(obj, reinterpret_cast<IV>(ptr));
  p.release();
  SvREADONLY_on(obj);
  RETVAL = objref;
OUTPUT:
  RETVAL

void
DESTROY(obj)
  SV *obj
CODE:
  dena::hstcpcli_i *const ptr =
    reinterpret_cast<dena::hstcpcli_i *>(SvIV(SvRV(obj)));
  delete ptr;

void
close(obj)
  SV *obj
CODE:
  dena::hstcpcli_i *const ptr =
    reinterpret_cast<dena::hstcpcli_i *>(SvIV(SvRV(obj)));
  ptr->close();

int
reconnect(obj)
  SV *obj
CODE:
  RETVAL = 0;
  dena::hstcpcli_i *const ptr =
    reinterpret_cast<dena::hstcpcli_i *>(SvIV(SvRV(obj)));
  RETVAL = ptr->reconnect();
OUTPUT:
  RETVAL

int
stable_point(obj)
  SV *obj
CODE:
  RETVAL = 0;
  dena::hstcpcli_i *const ptr =
    reinterpret_cast<dena::hstcpcli_i *>(SvIV(SvRV(obj)));
  const bool rv = ptr->stable_point();
  RETVAL = static_cast<int>(rv);
OUTPUT:
  RETVAL

int
get_error_code(obj)
  SV *obj
CODE:
  RETVAL = 0;
  dena::hstcpcli_i *const ptr =
    reinterpret_cast<dena::hstcpcli_i *>(SvIV(SvRV(obj)));
  RETVAL = ptr->get_error_code();
OUTPUT:
  RETVAL

SV *
get_error(obj)
  SV *obj
CODE:
  RETVAL = &PL_sv_undef;
  dena::hstcpcli_i *const ptr =
    reinterpret_cast<dena::hstcpcli_i *>(SvIV(SvRV(obj)));
  const std::string s = ptr->get_error();
  RETVAL = newSVpvn(s.data(), s.size());
OUTPUT:
  RETVAL

int
auth(obj, key, typ = 0)
  SV *obj
  const char *key
  const char *typ
CODE:
  RETVAL = 0;
  dena::hstcpcli_i *const ptr =
    reinterpret_cast<dena::hstcpcli_i *>(SvIV(SvRV(obj)));
  do {
    ptr->request_buf_auth(key, typ);
    if (ptr->request_send() != 0) {
      break;
    }
    size_t nflds = 0;
    ptr->response_recv(nflds);
    const int e = ptr->get_error_code();
    DBG(fprintf(stderr, "errcode=%d\n", ptr->get_error_code()));
    if (e >= 0) {
      ptr->response_buf_remove();
    }
    DBG(fprintf(stderr, "errcode=%d\n", ptr->get_error_code()));
  } while (0);
  RETVAL = ptr->get_error_code();
OUTPUT:
  RETVAL


int
open_index(obj, id, db, table, index, fields, ffields = 0)
  SV *obj
  int id
  const char *db
  const char *table
  const char *index
  const char *fields
  SV *ffields
CODE:
  const char *const ffields_str = sv_get_strval(ffields);
  RETVAL = 0;
  dena::hstcpcli_i *const ptr =
    reinterpret_cast<dena::hstcpcli_i *>(SvIV(SvRV(obj)));
  do {
    ptr->request_buf_open_index(id, db, table, index, fields, ffields_str);
    if (ptr->request_send() != 0) {
      break;
    }
    size_t nflds = 0;
    ptr->response_recv(nflds);
    const int e = ptr->get_error_code();
    DBG(fprintf(stderr, "errcode=%d\n", ptr->get_error_code()));
    if (e >= 0) {
      ptr->response_buf_remove();
    }
    DBG(fprintf(stderr, "errcode=%d\n", ptr->get_error_code()));
  } while (0);
  RETVAL = ptr->get_error_code();
OUTPUT:
  RETVAL

AV *
execute_single(obj, id, op, keys, limit, skip, mop = 0, mvs = 0, fils = 0, ivkeypart = -1, ivs = 0)
  SV *obj
  int id
  const char *op
  AV *keys
  int limit
  int skip
  SV *mop
  SV *mvs
  SV *fils
  int ivkeypart
  SV *ivs
CODE:
  const char *const mop_str = sv_get_strval(mop);
  AV *const mvs_av = sv_get_arrval(mvs);
  AV *const fils_av = sv_get_arrval(fils);
  AV *const ivs_av = sv_get_arrval(ivs);
  RETVAL = execute_internal(obj, id, op, keys, limit, skip, mop_str, mvs_av,
    fils_av, ivkeypart, ivs_av);
  sv_2mortal((SV *)RETVAL);
OUTPUT:
  RETVAL

AV *
execute_multi(obj, cmds)
  SV *obj
  AV *cmds
CODE:
  DBG(fprintf(stderr, "execute_multi0\n"));
  const I32 cmdsmax = av_len(cmds);
  execute_arg args[cmdsmax + 1]; /* GNU */
  for (I32 i = 0; i <= cmdsmax; ++i) {
    AV *const avtarget = arr_get_arrval(cmds, cmdsmax, i);
    if (avtarget == 0) {
      DBG(fprintf(stderr, "execute_multi1 %d\n", i));
      continue;
    }
    const I32 argmax = av_len(avtarget);
    if (argmax < 2) {
      DBG(fprintf(stderr, "execute_multi2 %d\n", i));
      continue;
    }
    execute_arg& ag = args[i];
    ag.id = arr_get_intval(avtarget, argmax, 0);
    ag.op = arr_get_strval(avtarget, argmax, 1);
    ag.keys = arr_get_arrval(avtarget, argmax, 2);
    ag.limit = arr_get_intval(avtarget, argmax, 3);
    ag.skip = arr_get_intval(avtarget, argmax, 4);
    ag.modop = arr_get_strval(avtarget, argmax, 5);
    ag.modvals = arr_get_arrval(avtarget, argmax, 6);
    ag.filters = arr_get_arrval(avtarget, argmax, 7);
    ag.invalues_keypart = arr_get_intval(avtarget, argmax, 8, -1);
    ag.invalues = arr_get_arrval(avtarget, argmax, 9);
    DBG(fprintf(stderr, "execute_multi3 %d: %d %s %p %d %d %s %p %p %d %p\n",
      i, ag.id, ag.op, ag.keys, ag.limit, ag.skip, ag.modop, ag.modvals,
      ag.filters, ag.invalues_keypart, ag.invalues));
  }
  RETVAL = execute_multi_internal(obj, args, cmdsmax + 1);
  sv_2mortal((SV *)RETVAL);
OUTPUT:
  RETVAL

AV *
execute_find(obj, id, op, keys, limit, skip, mop = 0, mvs = 0, fils = 0, ivkeypart = -1, ivs = 0)
  SV *obj
  int id
  const char *op
  AV *keys
  int limit
  int skip
  SV *mop
  SV *mvs
  SV *fils
  int ivkeypart
  SV *ivs
CODE:
  const char *const mop_str = sv_get_strval(mop);
  AV *const mvs_av = sv_get_arrval(mvs);
  AV *const fils_av = sv_get_arrval(fils);
  AV *const ivs_av = sv_get_arrval(ivs);
  RETVAL = execute_internal(obj, id, op, keys, limit, skip, mop_str, mvs_av,
    fils_av, ivkeypart, ivs_av);
  sv_2mortal((SV *)RETVAL);
OUTPUT:
  RETVAL

AV *
execute_update(obj, id, op, keys, limit, skip, modvals, fils = 0, ivkeypart = -1, ivs = 0)
  SV *obj
  int id
  const char *op
  AV *keys
  int limit
  int skip
  AV *modvals
  SV *fils
  int ivkeypart
  SV *ivs
CODE:
  AV *const fils_av = sv_get_arrval(fils);
  AV *const ivs_av = sv_get_arrval(ivs);
  RETVAL = execute_internal(obj, id, op, keys, limit, skip, "U",
    modvals, fils_av, ivkeypart, ivs_av);
  sv_2mortal((SV *)RETVAL);
OUTPUT:
  RETVAL

AV *
execute_delete(obj, id, op, keys, limit, skip, fils = 0, ivkeypart = -1, ivs = 0)
  SV *obj
  int id
  const char *op
  AV *keys
  int limit
  int skip
  SV *fils
  int ivkeypart
  SV *ivs
CODE:
  AV *const fils_av = sv_get_arrval(fils);
  AV *const ivs_av = sv_get_arrval(ivs);
  RETVAL = execute_internal(obj, id, op, keys, limit, skip, "D", 0, fils_av,
    ivkeypart, ivs_av);
  sv_2mortal((SV *)RETVAL);
OUTPUT:
  RETVAL

AV *
execute_insert(obj, id, fvals)
  SV *obj
  int id
  AV *fvals
CODE:
  RETVAL = execute_internal(obj, id, "+", fvals, 0, 0, 0, 0, 0, -1, 0);
  sv_2mortal((SV *)RETVAL);
OUTPUT:
  RETVAL

