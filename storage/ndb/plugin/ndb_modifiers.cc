/*
   Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

#include "ndb_modifiers.h"

#include <stdio.h>

#include "m_string.h"
#include "my_dbug.h"  // DBUG_PRINT

static bool end_of_token(const char *str) {
  return str[0] == 0 || str[0] == ' ' || str[0] == ',';
}

NDB_Modifiers::NDB_Modifiers(const char *prefix,
                             const NDB_Modifier modifiers[]) {
  m_prefix = prefix;
  m_prefixLen = strlen(prefix);
  for (m_len = 0; modifiers[m_len].m_name != nullptr; m_len++) {
  }
  m_modifiers = new NDB_Modifier[m_len + 1];
  memcpy(m_modifiers, modifiers, (m_len + 1) * sizeof(NDB_Modifier));
  m_comment_buf = nullptr;
  m_comment_len = 0;
  m_mod_start_offset = 0;
  m_mod_len = 0;
  m_errmsg[0] = 0;
}

NDB_Modifiers::~NDB_Modifiers() {
  for (uint32 i = 0; i < m_len; i++) {
    if (m_modifiers[i].m_type == NDB_Modifier::M_STRING &&
        m_modifiers[i].m_val_str.str != nullptr) {
      delete[] m_modifiers[i].m_val_str.str;
      m_modifiers[i].m_val_str.str = nullptr;
    }
  }
  delete[] m_modifiers;

  delete[] m_comment_buf;
}

int NDB_Modifiers::parse_modifier(struct NDB_Modifier *m, const char *str) {
  if (m->m_found) {
    snprintf(m_errmsg, sizeof(m_errmsg), "%s : modifier %s specified twice",
             m_prefix, m->m_name);
    return -1;
  }

  switch (m->m_type) {
    case NDB_Modifier::M_BOOL:
      if (end_of_token(str)) {
        m->m_val_bool = true;
        goto found;
      }
      if (str[0] != '=') break;

      str++;
      if (str[0] == '1' && end_of_token(str + 1)) {
        m->m_val_bool = true;
        goto found;
      }

      if (str[0] == '0' && end_of_token(str + 1)) {
        m->m_val_bool = false;
        goto found;
      }
      break;
    case NDB_Modifier::M_STRING: {
      if (end_of_token(str)) {
        m->m_val_str.str = "";
        m->m_val_str.len = 0;
        goto found;
      }

      if (str[0] != '=') break;

      str++;
      const char *start_str = str;
      while (!end_of_token(str)) str++;

      uint32 len = str - start_str;
      char *tmp = new (std::nothrow) char[len + 1];
      if (tmp == nullptr) {
        snprintf(m_errmsg, sizeof(m_errmsg), "Memory allocation error");
        return -1;
      }
      memcpy(tmp, start_str, len);
      tmp[len] = 0;  // Null terminate for safe printing
      m->m_val_str.len = len;
      m->m_val_str.str = tmp;
      goto found;
    }
  }

  {
    const char *end = strpbrk(str, " ,");
    if (end) {
      snprintf(m_errmsg, sizeof(m_errmsg), "%s : invalid value '%.*s' for %s",
               m_prefix, (int)(end - str), str, m->m_name);
    } else {
      snprintf(m_errmsg, sizeof(m_errmsg), "%s : invalid value '%s' for %s",
               m_prefix, str, m->m_name);
    }
  }
  return -1;
found:
  m->m_found = true;
  return 0;
}

int NDB_Modifiers::parseModifierListString(const char *string) {
  const char *pos = string;

  /* Attempt to extract modifiers */
  while (pos && pos[0] != 0 && pos[0] != ' ') {
    const char *end = strpbrk(pos, " ,");  // end of current modifier
    bool valid = false;

    /* Attempt to match modifier name */
    for (uint i = 0; i < m_len; i++) {
      size_t l = m_modifiers[i].m_name_len;
      if (native_strncasecmp(pos, m_modifiers[i].m_name, l) == 0) {
        /**
         * Found modifier...
         */

        if ((end_of_token(pos + l) || pos[l] == '=')) {
          pos += l;
          if (parse_modifier(m_modifiers + i, pos) != 0) {
            DBUG_PRINT("info", ("Parse modifier failed"));
            return -1;
          }

          valid = true;

          pos = end;
          if (pos && pos[0] == ',') pos++;

          break;
        } else {
          break;
        }
      }
    }  // for (modifier_name)

    if (!valid) {
      if (end) {
        snprintf(m_errmsg, sizeof(m_errmsg), "%s : unknown modifier: %.*s",
                 m_prefix, (int)(end - pos), pos);
      } else {
        snprintf(m_errmsg, sizeof(m_errmsg), "%s : unknown modifier: %s",
                 m_prefix, pos);
      }
      DBUG_PRINT("info", ("Error : %s", m_errmsg));
      return -1;
    }
  }  // while pos

  if (pos) {
    return pos - string;
  } else {
    return strlen(string);
  }
}

int NDB_Modifiers::loadComment(const char *str, size_t len) {
  if (m_comment_buf != nullptr) return -1;

  if (len == 0) {
    return 0;
  }

  /* Load into internal string buffer */
  m_comment_buf = new (std::nothrow) char[len + 1];
  if (m_comment_buf == nullptr) {
    snprintf(m_errmsg, sizeof(m_errmsg), "Memory allocation failed");
    return -1;
  }
  memcpy(m_comment_buf, str, len);
  m_comment_buf[len] = 0;
  uint32 inputLen = strlen(m_comment_buf);
  m_comment_len = inputLen;

  const char *pos = m_comment_buf;

  /* Check for comment prefix */
  if ((pos = strstr(pos, m_prefix)) == nullptr) {
    /* No prefix - nothing to do */
    return 0;
  }

  /* Record offset of prefix start */
  m_mod_start_offset = pos - m_comment_buf;
  pos += m_prefixLen;

  int mod_len = parseModifierListString(pos);
  if (mod_len > 0) {
    m_mod_len = mod_len + m_prefixLen;
    return 0;
  } else {
    DBUG_PRINT("info",
               ("parseModifierListString (%s) returned %d", pos, mod_len));
    /* Parse error */
    return -1;
  }
}

NDB_Modifier *NDB_Modifiers::find(const char *name) const {
  for (uint i = 0; i < m_len; i++) {
    if (native_strncasecmp(name, m_modifiers[i].m_name,
                           m_modifiers[i].m_name_len) == 0) {
      return m_modifiers + i;
    }
  }
  return nullptr;
}

const NDB_Modifier *NDB_Modifiers::get(const char *name) const {
  return find(name);
}

const NDB_Modifier *NDB_Modifiers::notfound() const {
  const NDB_Modifier *last = m_modifiers + m_len;
  assert(last->m_found == false);
  return last;  // last has m_found == false
}

bool NDB_Modifiers::set(const char *name, bool value) {
  NDB_Modifier *mod = find(name);
  if (mod != nullptr) {
    if (mod->m_type == NDB_Modifier::M_BOOL) {
      mod->m_val_bool = value;
      mod->m_found = true;
      return true;
    }
  }
  return false;
}

bool NDB_Modifiers::set(const char *name, const char *string) {
  NDB_Modifier *mod = find(name);
  if (mod != nullptr) {
    if (mod->m_type == NDB_Modifier::M_STRING) {
      uint32 len = strlen(string);
      char *tmp = new (std::nothrow) char[len + 1];
      if (tmp == nullptr) {
        return false;
      }
      memcpy(tmp, string, len);
      tmp[len] = 0;

      if (mod->m_found) {
        /* Delete old */
        delete[] mod->m_val_str.str;
      }

      mod->m_val_str.str = tmp;
      mod->m_val_str.len = len;
      mod->m_found = true;

      return true;
    }
  }
  return false;
}

uint32 NDB_Modifiers::generateModifierListString(char *buf,
                                                 size_t buflen) const {
  size_t length = 0;
  bool first = true;

  /* if buf == NULL, we just calculate the length */

  for (uint i = 0; i < m_len; i++) {
    const NDB_Modifier &mod = m_modifiers[i];
    if (mod.m_found) {
      if (!first) {
        if (buf) {
          snprintf(buf + length, buflen - length, ",");
        }

        length++;
      }
      first = false;

      if (buf) {
        snprintf(buf + length, buflen - length, "%s", mod.m_name);
      }

      length += mod.m_name_len;

      switch (mod.m_type) {
        case NDB_Modifier::M_BOOL: {
          if (buf) {
            snprintf(buf + length, buflen - length, "=%u",
                     mod.m_val_bool ? 1 : 0);
          }
          length += 2;
          break;
        }
        case NDB_Modifier::M_STRING: {
          if (buf) {
            snprintf(buf + length, buflen - length, "=%s", mod.m_val_str.str);
          }
          length += (mod.m_val_str.len + 1);
          break;
        }
      }
    }
  }

  return length;
}

const char *NDB_Modifiers::generateCommentString() {
  assert(m_comment_len >= m_mod_start_offset + m_mod_len);
  const uint32 postCommentLen =
      m_comment_len - (m_mod_start_offset + m_mod_len);

  /* Calculate new comment length */

  /* Determine new modifier string length, could be zero */
  const uint32 newModListStringLen = generateModifierListString(nullptr, 0);
  uint32 newModLen = 0;
  bool extraSpace = false;
  if (newModListStringLen > 0) {
    newModLen += m_prefixLen;
    newModLen += newModListStringLen;

    if ((m_mod_len == 0) && (postCommentLen > 0)) {
      /* Extra space to separate post comment */
      extraSpace = true;
      newModLen++;
    }
  }

  const uint32 newCommentLen = (m_comment_len - m_mod_len) + newModLen;

  DBUG_PRINT("info", ("getCommentString : old comment %s len %d "
                      "start %d len %d postLen %d",
                      m_comment_buf, m_comment_len, m_mod_start_offset,
                      m_mod_len, postCommentLen));
  DBUG_PRINT("info",
             ("getCommentString : newModListStringLen : %u newModLen : %u "
              "newCommentLen : %u",
              newModListStringLen, newModLen, newCommentLen));

  char *newBuf = new (std::nothrow) char[newCommentLen + 1];
  if (newBuf == nullptr) {
    snprintf(m_errmsg, sizeof(m_errmsg), "Memory allocation failed");
    return nullptr;
  }

  char *insertPos = newBuf;
  uint32 remain = newCommentLen + 1;

  /* Copy pre-comment if any */
  memcpy(insertPos, m_comment_buf, m_mod_start_offset);
  insertPos += m_mod_start_offset;
  remain -= m_mod_start_offset;

  const uint32 newStartOffset = insertPos - m_comment_buf;

  if (newModListStringLen > 0) {
    /* Add prefix */
    memcpy(insertPos, m_prefix, m_prefixLen);
    insertPos += m_prefixLen;
    remain -= m_prefixLen;

    /* Add modifier list */
    generateModifierListString(insertPos, remain);
    insertPos += newModListStringLen;
    remain -= newModListStringLen;
  }

  if (postCommentLen) {
    if (extraSpace) {
      /* No modifiers before, but some comment content.  Add a space */
      *insertPos = ' ';
      insertPos++;
      remain--;
    }

    memcpy(insertPos, m_comment_buf + m_mod_start_offset + m_mod_len,
           postCommentLen);
    insertPos += postCommentLen;
    remain -= postCommentLen;
  }

  /* Add trailing 0 */
  *insertPos = 0;

  assert(strlen(newBuf) == newCommentLen);

  DBUG_PRINT("info", ("comment = %s", newBuf));

  delete[] m_comment_buf;

  /* Update stored state */
  m_comment_buf = newBuf;
  m_comment_len = newCommentLen;
  m_mod_start_offset = newStartOffset;
  m_mod_len = newModLen;

  return m_comment_buf;
}
