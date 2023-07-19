/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 * SPDX-License-Identifier: curl
 *
 ***************************************************************************/

#include "curl_setup.h"

#ifndef CURL_DISABLE_HTTP

#include "urldata.h"
#include <curl/curl.h>
#include "http.h"
#include "http1.h"
#include "urlapi-int.h"

/* The last 3 #include files should be in this order */
#include "curl_printf.h"
#include "curl_memory.h"
#include "memdebug.h"


#define MAX_URL_LEN   (4*1024)

void Curl_h1_req_parse_init(struct h1_req_parser *parser, size_t max_line_len)
{
  memset(parser, 0, sizeof(*parser));
  parser->max_line_len = max_line_len;
  Curl_bufq_init(&parser->scratch, max_line_len, 1);
}

void Curl_h1_req_parse_free(struct h1_req_parser *parser)
{
  if(parser) {
    Curl_http_req_free(parser->req);
    Curl_bufq_free(&parser->scratch);
    parser->req = NULL;
    parser->done = FALSE;
  }
}

static ssize_t detect_line(struct h1_req_parser *parser,
                           const char *buf, const size_t buflen, int options,
                           CURLcode *err)
{
  const char  *line_end;
  size_t len;

  DEBUGASSERT(!parser->line);
  line_end = memchr(buf, '\n', buflen);
  if(!line_end) {
    *err = (buflen > parser->max_line_len)? CURLE_URL_MALFORMAT : CURLE_AGAIN;
    return -1;
  }
  len = line_end - buf + 1;
  if(len > parser->max_line_len) {
    *err = CURLE_URL_MALFORMAT;
    return -1;
  }

  if(options & H1_PARSE_OPT_STRICT) {
    if((len == 1) || (buf[len - 2] != '\r')) {
      *err = CURLE_URL_MALFORMAT;
      return -1;
    }
    parser->line = buf;
    parser->line_len = len - 2;
  }
  else {
    parser->line = buf;
    parser->line_len = len - (((len == 1) || (buf[len - 2] != '\r'))? 1 : 2);
  }
  *err = CURLE_OK;
  return (ssize_t)len;
}

static ssize_t next_line(struct h1_req_parser *parser,
                         const char *buf, const size_t buflen, int options,
                         CURLcode *err)
{
  ssize_t nread = 0, n;

  if(parser->line) {
    if(parser->scratch_skip) {
      /* last line was from scratch. Remove it now, since we are done
       * with it and look for the next one. */
      Curl_bufq_skip_and_shift(&parser->scratch, parser->scratch_skip);
      parser->scratch_skip = 0;
    }
    parser->line = NULL;
    parser->line_len = 0;
  }

  if(Curl_bufq_is_empty(&parser->scratch)) {
    nread = detect_line(parser, buf, buflen, options, err);
    if(nread < 0) {
      if(*err != CURLE_AGAIN)
        return -1;
      /* not a complete line, add to scratch for later revisit */
      nread = Curl_bufq_write(&parser->scratch,
                              (const unsigned char *)buf, buflen, err);
      return nread;
    }
    /* found one */
  }
  else {
    const char *sbuf;
    size_t sbuflen;

    /* scratch contains bytes from last attempt, add more to it */
    if(buflen) {
      const char *line_end;
      size_t add_len;
      ssize_t pos;

      line_end = memchr(buf, '\n', buflen);
      pos = line_end? (line_end - buf + 1) : -1;
      add_len = (pos >= 0)? (size_t)pos : buflen;
      nread = Curl_bufq_write(&parser->scratch,
                              (const unsigned char *)buf, add_len, err);
      if(nread < 0) {
        /* Unable to add anything to scratch is an error, since we should
         * have seen a line there then before. */
        if(*err == CURLE_AGAIN)
          *err = CURLE_URL_MALFORMAT;
        return -1;
      }
    }

    if(Curl_bufq_peek(&parser->scratch,
                      (const unsigned char **)&sbuf, &sbuflen)) {
      n = detect_line(parser, sbuf, sbuflen, options, err);
      if(n < 0 && *err != CURLE_AGAIN)
        return -1;  /* real error */
      parser->scratch_skip = (size_t)n;
    }
    else {
      /* we SHOULD be able to peek at scratch data */
      DEBUGASSERT(0);
    }
  }
  return nread;
}

static CURLcode start_req(struct h1_req_parser *parser,
                          const char *scheme_default, int options)
{
  const char  *p, *m, *target, *hv, *scheme, *authority, *path;
  size_t m_len, target_len, hv_len, scheme_len, authority_len, path_len;
  size_t i;
  CURLU *url = NULL;
  CURLcode result = CURLE_URL_MALFORMAT; /* Use this as default fail */

  DEBUGASSERT(!parser->req);
  /* line must match: "METHOD TARGET HTTP_VERSION" */
  p = memchr(parser->line, ' ', parser->line_len);
  if(!p || p == parser->line)
    goto out;

  m = parser->line;
  m_len = p - parser->line;
  target = p + 1;
  target_len = hv_len = 0;
  hv = NULL;

  /* URL may contain spaces so scan backwards */
  for(i = parser->line_len; i > m_len; --i) {
    if(parser->line[i] == ' ') {
      hv = &parser->line[i + 1];
      hv_len = parser->line_len - i;
      target_len = (hv - target) - 1;
      break;
    }
  }
  /* no SPACE found or empty TARGET or empy HTTP_VERSION */
  if(!target_len || !hv_len)
    goto out;

  /* TODO: we do not check HTTP_VERSION for conformity, should
   + do that when STRICT option is supplied. */
  (void)hv;

  /* The TARGET can be (rfc 9112, ch. 3.2):
   * origin-form:     path + optional query
   * absolute-form:   absolute URI
   * authority-form:  host+port for CONNECT
   * asterisk-form:   '*' for OPTIONS
   *
   * from TARGET, we derive `scheme` `authority` `path`
   * origin-form            --        --          TARGET
   * absolute-form          URL*      URL*        URL*
   * authority-form         --        TARGET      --
   * asterisk-form          --        --          TARGET
   */
  scheme = authority = path = NULL;
  scheme_len = authority_len = path_len = 0;

  if(target_len == 1 && target[0] == '*') {
    /* asterisk-form */
    path = target;
    path_len = target_len;
  }
  else if(!strncmp("CONNECT", m, m_len)) {
    /* authority-form */
    authority = target;
    authority_len = target_len;
  }
  else if(target[0] == '/') {
    /* origin-form */
    path = target;
    path_len = target_len;
  }
  else {
    /* origin-form OR absolute-form */
    CURLUcode uc;
    char tmp[MAX_URL_LEN];

    /* default, unless we see an absolute URL */
    path = target;
    path_len = target_len;

    /* URL parser wants 0-termination */
    if(target_len >= sizeof(tmp))
      goto out;
    memcpy(tmp, target, target_len);
    tmp[target_len] = '\0';
    /* See if treating TARGET as an absolute URL makes sense */
    if(Curl_is_absolute_url(tmp, NULL, 0, FALSE)) {
      int url_options;

      url = curl_url();
      if(!url) {
        result = CURLE_OUT_OF_MEMORY;
        goto out;
      }
      url_options = (CURLU_NON_SUPPORT_SCHEME|
                     CURLU_PATH_AS_IS|
                     CURLU_NO_DEFAULT_PORT);
      if(!(options & H1_PARSE_OPT_STRICT))
        url_options |= CURLU_ALLOW_SPACE;
      uc = curl_url_set(url, CURLUPART_URL, tmp, url_options);
      if(uc) {
        goto out;
      }
    }

    if(!url && (options & H1_PARSE_OPT_STRICT)) {
      /* we should have an absolute URL or have seen `/` earlier */
      goto out;
    }
  }

  if(url) {
    result = Curl_http_req_make2(&parser->req, m, m_len, url, scheme_default);
  }
  else {
    if(!scheme && scheme_default) {
      scheme = scheme_default;
      scheme_len = strlen(scheme_default);
    }
    result = Curl_http_req_make(&parser->req, m, m_len, scheme, scheme_len,
                                authority, authority_len, path, path_len);
  }

out:
  curl_url_cleanup(url);
  return result;
}

ssize_t Curl_h1_req_parse_read(struct h1_req_parser *parser,
                               const char *buf, size_t buflen,
                               const char *scheme_default, int options,
                               CURLcode *err)
{
  ssize_t nread = 0, n;

  *err = CURLE_OK;
  while(!parser->done) {
    n = next_line(parser, buf, buflen, options, err);
    if(n < 0) {
      if(*err != CURLE_AGAIN) {
        nread = -1;
      }
      *err = CURLE_OK;
      goto out;
    }

    /* Consume this line */
    nread += (size_t)n;
    buf += (size_t)n;
    buflen -= (size_t)n;

    if(!parser->line) {
      /* consumed bytes, but line not complete */
      if(!buflen)
        goto out;
    }
    else if(!parser->req) {
      *err = start_req(parser, scheme_default, options);
      if(*err) {
        nread = -1;
        goto out;
      }
    }
    else if(parser->line_len == 0) {
      /* last, empty line, we are finished */
      if(!parser->req) {
        *err = CURLE_URL_MALFORMAT;
        nread = -1;
        goto out;
      }
      parser->done = TRUE;
      Curl_bufq_free(&parser->scratch);
      /* last chance adjustments */
    }
    else {
      *err = Curl_dynhds_h1_add_line(&parser->req->headers,
                                     parser->line, parser->line_len);
      if(*err) {
        nread = -1;
        goto out;
      }
    }
  }

out:
  return nread;
}


#endif /* !CURL_DISABLE_HTTP */
