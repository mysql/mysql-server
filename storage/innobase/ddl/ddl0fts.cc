/****************************************************************************

Copyright (c) 2010, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file ddl/ddl0fts.cc
Create Full Text Index with (parallel) merge sort.
Created 10/13/2010 Jimmy Yang */

#include <sys/types.h>

#include "univ.i"

#include "ddl0ddl.h"
#include "ddl0fts.h"
#include "ddl0impl-builder.h"
#include "ddl0impl-merge.h"
#include "dict0dd.h"
#include "fts0plugin.h"
#include "lob0lob.h"
#include "os0thread-create.h"
#include "sql/sql_class.h"

#include <current_thd.h>

/** Parallel sort degree, must be a power of 2. */
ulong ddl::fts_parser_threads = 2;

/** Maximum pending doc memory limit in bytes for a fts tokenization thread */
static constexpr size_t PENDING_DOC_MEMORY_LIMIT = 1000000;

/** Must be a power of 2. */
static constexpr size_t DOC_ITEM_QUEUE_SIZE = 64;

namespace ddl {

/** status bit used for communication between parent and child thread */
enum class Thread_state : uint8_t { UNKNOWN, COMPLETE, EXITING, ABORT };

/** Row fts token for plugin parser */
struct Token {
  /** Token */
  fts_string_t *m_text{};

  /** Token position in the document */
  size_t m_position{};

  /** Next token link */
  UT_LIST_NODE_T(Token) m_token_list;
};

/** Structure stores information needed for the insertion phase of FTS
parallel sort. */
struct Insert {
  /** Charset info */
  CHARSET_INFO *m_charset{};

  /** Heap */
  mem_heap_t *m_heap{};

  /** Whether to use smaller (4 bytes) integer for Doc ID */
  bool m_doc_id_32_bit{};

  /** Bulk load instance */
  Btree_load *m_btr_bulk{};

  /** Tuple to insert */
  dtuple_t *m_tuple{};

#ifdef UNIV_DEBUG
  /** Auxiliary index id */
  size_t m_handler_id{};
#endif /* UNIV_DEBUG */
};

/** Structure stores information from string tokenization operation */
struct Tokenize_ctx {
  using Token_list = UT_LIST_BASE_NODE_T(Token, m_token_list);

  /** Processed string length */
  size_t m_processed_len{};

  /** Doc start position */
  size_t m_init_pos{};

  /** The handler that triggered a buffer full event. */
  size_t m_handler_id{};

  /** Stopword list */
  ib_rbt_t *m_cached_stopword{};

  /** Token list. */
  Token_list m_token_list{};
};

/** For parsing and sorting the documents. */
struct FTS::Parser {
  /** Constructor.
  @param[in] id               Parser ID.
  @param[in,out] ctx          DDL context.
  @param[in,out] dup          Descriptor of FTS index being created.
  @param[in] doc_id_32_bit      Size of the doc ID column to use for sort. */
  Parser(size_t id, Context &ctx, Dup *dup, bool doc_id_32_bit) noexcept;

  /** Destructor. */
  ~Parser() noexcept;

  /** @return the parser ID. */
  size_t id() const noexcept { return m_id; }

  /** Initialize the data structures.
  @param[in] n_threads        Number of parsing threads.
  @return DB_SUCCESS or error code. */
  dberr_t init(size_t n_threads) noexcept;

  /** Releases ownership of the i'th file used.
  @return the i'th file. */
  file_t release_file(size_t id) noexcept {
    return std::move(m_handlers[id]->m_file);
  }

  /** Data structures for building an index. */
  struct Handler {
    /** Constructor.
    @param[in,out] index      Index to create.
    @param[in] size           IO buffer size. */
    explicit Handler(dict_index_t *index, size_t size) noexcept;

    /** Destructor. */
    ~Handler() noexcept;

    /** Aux index id. */
    size_t m_id{};

    /** Sort file */
    file_t m_file{};

    /** Sort buffer */
    Key_sort_buffer m_key_buffer;

    /** Buffer to use for temporary file writes. */
    ut::unique_ptr_aligned<byte[]> m_aligned_buffer;

    /** Buffer for IO to use for temporary file writes. */
    IO_buffer m_io_buffer;

    /** Record list start offsets. */
    Merge_offsets m_offsets{};
  };

  /** @return the parser error status. */
  dberr_t get_error() const noexcept { return m_ctx.get_error(); }

  /** Set the error code.
  @param[in] err                Error code to set. */
  void set_error(dberr_t err) noexcept { m_ctx.set_error(err); }

  /** Enqueue a document to parse.
  @param[in,out] doc_item       Document to parse.
  @return DB_SUCCESS or error code. */
  dberr_t enqueue(FTS::Doc_item *doc_item) noexcept;

  /** Function performs parallel tokenization of the incoming doc strings.
  @param[in,out] builder        Index builder instance. */
  void parse(Builder *builder) noexcept;

  /** Set the parent thread state.
  @param[in] state              The parent state. */
  void set_parent_state(Thread_state state) noexcept { m_parent_state = state; }

  Diagnostics_area da{false};

 private:
  /** Tokenize incoming text data and add to the sort buffer.
  @param[in] doc_id             Doc ID.
  @param[in] doc                Doc to be tokenized.
  @param[in] word_dtype         Data structure for word col.
  @param[in,out] t_ctx          Tokenize context.
  @return true if the record passed, false if out of space */
  bool doc_tokenize(doc_id_t doc_id, fts_doc_t *doc, dtype_t *word_dtype,
                    Tokenize_ctx *t_ctx) noexcept;

  /** Get next doc item from fts_doc_lis.
  @param[in,out] doc_item         Doc item. */
  void get_next_doc_item(FTS::Doc_item *&doc_item) noexcept;

  /** Tokenize by fts plugin parser.
  @param[in] doc                To tokenize
  @param[in] parser             Plugin parser instance.
  @param[in,out] t_ctx          Tokenize ctx instance. */
  void tokenize(fts_doc_t *doc, st_mysql_ftparser *parser,
                Tokenize_ctx *t_ctx) noexcept;

  /** FTS plugin parser 'myql_add_word' callback function for row merge.
  Refer to 'MYSQL_FTPARSER_PARAM' for more detail.
  @param[in] param              Parser parameter.
  @param[in] word               Token word.
  @param[in] word_len           Word len.
  @param[in] boolean_info       Boolean info.
  @return always 0 - plugin requirement. */
  static int add_word(MYSQL_FTPARSER_PARAM *param, char *word, int word_len,
                      MYSQL_FTPARSER_BOOLEAN_INFO *boolean_info) noexcept;

 private:
  using Docq = mpmc_bq<FTS::Doc_item *>;
  using Docq_ptr = std::unique_ptr<Docq, std::function<void(Docq *)>>;
  using Handler_ptr = std::unique_ptr<Handler, std::function<void(Handler *)>>;
  using Handlers = std::array<Handler_ptr, FTS_NUM_AUX_INDEX>;

  /** Parallel sort ID */
  size_t m_id{};

  /** Descriptor of FTS index. */
  Dup *m_dup{};

  /** DDL context. */
  Context &m_ctx;

  /** Buffers etc. */
  Handlers m_handlers{};

  /** Whether to use 4 bytes instead of 8 bytes integer to store Doc ID during
  sort, if Doc ID will not be big enough to use 8 bytes value. */
  bool m_doc_id_32_bit{};

  /** Doc queue to process */
  Docq_ptr m_docq{};

  /** Memory used by fts_doc_list */
  std::atomic_size_t m_memory_used{};

  /** Parent thread state */
  Thread_state m_parent_state{Thread_state::UNKNOWN};
};

struct FTS::Inserter {
  /** Data structures for building an index. */
  struct Handler {
    /** Constructor. */
    Handler() = default;

    /** Destructor. */
    ~Handler() = default;

    using Buffer = ut::unique_ptr_aligned<byte[]>;
    using Files = std::vector<file_t, ut::allocator<file_t>>;

    /** Aux index id. */
    size_t m_id{};

    /** Sort file */
    Files m_files{};
  };

  /** Constructor.
  @param[in,out] ctx            DDL context.
  @param[in,out] dup            Descriptor of FTS index being created.
  @param[in] doc_id_32_bit      Size of the doc ID column to use for sort. */
  Inserter(Context &ctx, Dup *dup, bool doc_id_32_bit) noexcept;

  /** Destructor. */
  ~Inserter() noexcept {}

  /** Read sorted file(s) containing index data tuples and insert these data
  tuples to the index
  @param[in,out] builder        Index builder.
  @param[in,out] handler        Insert handler.
  @return DB_SUCCESS or error number */
  [[nodiscard]] dberr_t insert(Builder *builder, Handler *handler) noexcept;

  /** Add a file to the handler for merging and inserting.
  @param[in] id                 Aux index ID.
  @param[in] file               File to merge and insert.
  @return DB_SUCCESS or error code. */
  dberr_t add_file(size_t id, file_t file) noexcept {
    auto &handler = m_handlers[id];

    handler.m_files.push_back(std::move(file));

    return DB_SUCCESS;
  }

  /** Write out a single word's data as new entry/entries in the INDEX table.
  @param[in] ins_ctx                Insert context.
  @param[in] word                       Word string.
  @param[in] node                       Node columns.
  @return       DB_SUCCUESS if insertion runs fine, otherwise error code */
  dberr_t write_node(const Insert *ins_ctx, const fts_string_t *word,
                     const fts_node_t *node) noexcept;

  /** Insert processed FTS data to auxiliary index tables.
  @param[in] ins_ctx              Insert context.
  @param[in] word                 Sorted and tokenized word.
  @return DB_SUCCESS if insertion runs fine */
  dberr_t write_word(Insert *ins_ctx, fts_tokenizer_word_t *word) noexcept;

  /** Read sorted FTS data files and insert data tuples to auxiliary tables.
  @param[in] ins_ctx              Insert context.
  @param[in] word                 Last processed tokenized word.
  @param[in] positions            Word position.
  @param[in] in_doc_id            Last item doc id.
  @param[in] dtuple               Entry to insert or nullptr on end. */
  void insert_tuple(Insert *ins_ctx, fts_tokenizer_word_t *word,
                    ib_vector_t *positions, doc_id_t *in_doc_id,
                    const dtuple_t *dtuple) noexcept;

  using Handlers = std::array<Handler, FTS_NUM_AUX_INDEX>;

  /** For duplicate reporting. */
  Dup *m_dup{};

  /** DDL context. */
  Context &m_ctx;

  /** 32 or 64 bit doc id. */
  bool m_doc_id_32_bit{};

  /** There is one handler per parser. */
  Handlers m_handlers{};
};

FTS::Parser::Handler::Handler(dict_index_t *index, size_t size) noexcept
    : m_file(), m_key_buffer(index, size), m_aligned_buffer() {}

FTS::Parser::Handler::~Handler() noexcept {}

FTS::Parser::Parser(size_t id, Context &ctx, Dup *dup,
                    bool doc_id_32_bit) noexcept
    : m_id(id), m_dup(dup), m_ctx(ctx), m_doc_id_32_bit(doc_id_32_bit) {}

dberr_t FTS::Parser::init(size_t n_threads) noexcept {
  m_docq = Docq_ptr(ut::new_withkey<Docq>(ut::make_psi_memory_key(mem_key_ddl),
                                          DOC_ITEM_QUEUE_SIZE),
                    [](Docq *docq) { ut::delete_(docq); });

  if (m_docq == nullptr) {
    return DB_OUT_OF_MEMORY;
  }

  const auto path = thd_innodb_tmpdir(m_ctx.thd());
  const auto buffer_size = m_ctx.scan_buffer_size(n_threads);

  for (size_t i = 0; i < FTS_NUM_AUX_INDEX; ++i) {
    m_handlers[i] = Handler_ptr(
        ut::new_withkey<Handler>(ut::make_psi_memory_key(mem_key_ddl),
                                 m_dup->m_index, buffer_size.first),
        [](Handler *handler) { ut::delete_(handler); });

    auto &handler = m_handlers[i];

    if (handler == nullptr) {
      return DB_OUT_OF_MEMORY;
    }

    handler->m_aligned_buffer =
        ut::make_unique_aligned<byte[]>(ut::make_psi_memory_key(mem_key_ddl),
                                        UNIV_SECTOR_SIZE, buffer_size.first);

    if (!handler->m_aligned_buffer) {
      return DB_OUT_OF_MEMORY;
    }

    handler->m_io_buffer = {handler->m_aligned_buffer.get(), buffer_size.first};

    if (!file_create(&handler->m_file, path)) {
      return DB_OUT_OF_MEMORY;
    }
  }

  return DB_SUCCESS;
}

FTS::Parser::~Parser() noexcept {}

dberr_t FTS::Parser::enqueue(FTS::Doc_item *doc_item) noexcept {
  auto err = get_error();

  if (err != DB_SUCCESS) {
    ut::free(doc_item);
    return err;
  }

  const auto sz = sizeof(*doc_item) + doc_item->m_field->len;

  m_memory_used.fetch_add(sz, std::memory_order_relaxed);

  while (!m_docq->enqueue(doc_item)) {
    auto err = get_error();

    if (err != DB_SUCCESS) {
      ut::delete_(doc_item);
      m_memory_used.fetch_sub(sz, std::memory_order_relaxed);
      return err;
    }

    std::this_thread::sleep_for(std::chrono::microseconds(1000));
  }

  size_t retries{};
  constexpr size_t MAX_RETRIES{10000};
  constexpr auto LIMIT = PENDING_DOC_MEMORY_LIMIT;

  /* Sleep when memory used exceeds limit. */
  while (m_memory_used.load(std::memory_order_relaxed) > LIMIT &&
         retries < MAX_RETRIES) {
    ++retries;
    std::this_thread::sleep_for(std::chrono::microseconds(1000));
  }

  return DB_SUCCESS;
}

FTS::Inserter::Inserter(Context &ctx, Dup *dup, bool doc_id_32_bit) noexcept
    : m_dup(dup), m_ctx(ctx), m_doc_id_32_bit(doc_id_32_bit) {
  size_t i{};
  for (auto &handler : m_handlers) {
    handler.m_id = i++;
  }
}

dict_index_t *FTS::create_index(dict_index_t *index, dict_table_t *table,
                                bool *doc_id_32_bit) noexcept {
  // FIXME: This name shouldn't be hard coded here.
  auto new_index = dict_mem_index_create(index->table->name.m_name,
                                         "tmp_fts_idx", 0, DICT_FTS, 3);

  new_index->id = index->id;
  new_index->table = table;
  new_index->n_uniq = FTS_NUM_FIELDS_SORT;
  new_index->n_def = FTS_NUM_FIELDS_SORT;
  new_index->cached = true;
  new_index->parser = index->parser;
  new_index->is_ngram = index->is_ngram;

  auto idx_field = index->get_field(0);
  auto charset = fts_index_get_charset(index);

  /* The first field is on the Tokenized Word */
  auto field = new_index->get_field(0);

  field->name = nullptr;
  field->prefix_len = 0;
  field->is_ascending = true;

  field->col = static_cast<dict_col_t *>(
      mem_heap_alloc(new_index->heap, sizeof(dict_col_t)));

  field->col->len = FTS_MAX_WORD_LEN;

  field->col->mtype =
      (charset == &my_charset_latin1) ? DATA_VARCHAR : DATA_VARMYSQL;

  field->col->prtype = idx_field->col->prtype | DATA_NOT_NULL;
  field->col->mbminmaxlen = idx_field->col->mbminmaxlen;
  field->fixed_len = 0;
  field->col->set_version_added(UINT8_UNDEFINED);
  field->col->set_version_dropped(UINT8_UNDEFINED);
  field->col->set_phy_pos(UINT32_UNDEFINED);

  /* Doc ID */
  field = new_index->get_field(1);
  field->name = nullptr;
  field->prefix_len = 0;
  field->is_ascending = true;

  field->col = static_cast<dict_col_t *>(
      mem_heap_alloc(new_index->heap, sizeof(dict_col_t)));

  field->col->mtype = DATA_INT;
  *doc_id_32_bit = false;

  /* Check whether we can use 4 bytes instead of 8 bytes integer
  field to hold the Doc ID, thus reduce the overall sort size */
  if (DICT_TF2_FLAG_IS_SET(table, DICT_TF2_FTS_ADD_DOC_ID)) {
    /* If Doc ID column is being added by this create
    index, then just check the number of rows in the table */
    if (dict_table_get_n_rows(table) < MAX_DOC_ID_OPT_VAL) {
      *doc_id_32_bit = true;
    }
  } else {
    doc_id_t max_doc_id;

    /* If the Doc ID column is supplied by user, then
    check the maximum Doc ID in the table */
    max_doc_id = fts_get_max_doc_id((dict_table_t *)table);

    if (max_doc_id && max_doc_id < MAX_DOC_ID_OPT_VAL) {
      *doc_id_32_bit = true;
    }
  }

  if (*doc_id_32_bit) {
    field->col->len = sizeof(uint32_t);
    field->fixed_len = sizeof(uint32_t);
  } else {
    field->col->len = FTS_DOC_ID_LEN;
    field->fixed_len = FTS_DOC_ID_LEN;
  }

  field->col->prtype = DATA_NOT_NULL | DATA_BINARY_TYPE;

  field->col->mbminmaxlen = 0;
  field->col->set_version_added(UINT8_UNDEFINED);
  field->col->set_version_dropped(UINT8_UNDEFINED);
  field->col->set_phy_pos(UINT32_UNDEFINED);

  /* The third field is on the word's position in the original doc */
  field = new_index->get_field(2);
  field->name = nullptr;
  field->prefix_len = 0;
  field->is_ascending = true;

  field->col = static_cast<dict_col_t *>(
      mem_heap_alloc(new_index->heap, sizeof(dict_col_t)));

  field->col->mtype = DATA_INT;
  field->col->len = 4;
  field->fixed_len = 4;
  field->col->prtype = DATA_NOT_NULL;
  field->col->mbminmaxlen = 0;
  field->col->set_version_added(UINT8_UNDEFINED);
  field->col->set_version_dropped(UINT8_UNDEFINED);
  field->col->set_phy_pos(UINT32_UNDEFINED);

  return new_index;
}

int FTS::Parser::add_word(MYSQL_FTPARSER_PARAM *param, char *word, int word_len,
                          MYSQL_FTPARSER_BOOLEAN_INFO *boolean_info) noexcept {
  auto t_ctx = static_cast<Tokenize_ctx *>(param->mysql_ftparam);
  ut_a(t_ctx != nullptr);

  fts_string_t str;

  str.f_len = word_len;
  str.f_str = reinterpret_cast<byte *>(word);
  str.f_n_char = fts_get_token_size((CHARSET_INFO *)param->cs, word, word_len);

  ut_ad(boolean_info->position >= 0);

  auto ptr = static_cast<byte *>(
      ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY,
                         sizeof(Token) + sizeof(fts_string_t) + str.f_len));

  auto fts_token = reinterpret_cast<Token *>(ptr);

  fts_token->m_text = reinterpret_cast<fts_string_t *>(ptr + sizeof(Token));

  fts_token->m_text->f_str =
      static_cast<byte *>(ptr + sizeof(Token) + sizeof(fts_string_t));

  fts_token->m_text->f_len = str.f_len;

  fts_token->m_text->f_n_char = str.f_n_char;

  memcpy(fts_token->m_text->f_str, str.f_str, str.f_len);

  fts_token->m_position = boolean_info->position;

  /* Add token to list */
  UT_LIST_ADD_LAST(t_ctx->m_token_list, fts_token);

  return 0;
}

void FTS::Parser::tokenize(fts_doc_t *doc, st_mysql_ftparser *parser,
                           Tokenize_ctx *t_ctx) noexcept {
  MYSQL_FTPARSER_PARAM param;

  param.cs = doc->charset;
  param.mysql_ftparam = t_ctx;
  param.mysql_add_word = add_word;
  param.mode = MYSQL_FTPARSER_SIMPLE_MODE;
  param.length = static_cast<int>(doc->text.f_len);
  param.mysql_parse = fts_tokenize_document_internal;
  param.doc = reinterpret_cast<char *>(doc->text.f_str);

  int ret{};

  if (parser->init != nullptr) {
    ret = parser->init(&param);
  }

  if (ret == 0) {
    ret = parser->parse(&param);
    if (ret != 0) {
      set_error(DB_UNSUPPORTED);
    }
  } else {
    set_error(DB_ERROR);
  }

  if (parser->deinit != nullptr) {
    ut_a(parser->init != nullptr);
    ret = parser->deinit(&param);
    if (ret != 0) {
      set_error(DB_ERROR);
    }
  }
}

bool FTS::Parser::doc_tokenize(doc_id_t doc_id, fts_doc_t *doc,
                               dtype_t *word_dtype,
                               Tokenize_ctx *t_ctx) noexcept {
  size_t inc = 0;
  bool buf_full{};
  fts_string_t str;
  fts_string_t t_str;
  std::array<byte, FTS_MAX_WORD_LEN + 1> str_buf;

  t_str.f_n_char = 0;
  t_ctx->m_handler_id = 0;

  auto parser = m_dup->m_index->parser;
  auto is_ngram = m_dup->m_index->is_ngram;

  /* When using a plug-in parser, the whole document is tokenized first
  by the plugin and written to t_ctx->m_token_list. The list is not
  empty at this point iff the buffer was filled without processing all
  tokens (function returned false on same document). In this case the
  list contains the remaining tokens to be processed. */
  if (parser != nullptr) {
    ut_ad(t_ctx->m_processed_len == 0);

    if (UT_LIST_GET_LEN(t_ctx->m_token_list) == 0) {
      /* Parse the whole doc and cache tokens. */
      tokenize(doc, parser, t_ctx);
    }
  }

  /* Iterate over each word string and add it with its corresponding
  doc id and position to sort buffer. In non-plugin mode
  t_ctx->m_processed_len indicates the position of the next unprocessed
  token. With a plugin parser it is only updated once all remaining tokens
  produced by the plugin are processed. */
  while (t_ctx->m_processed_len < doc->text.f_len) {
    Token *fts_token{};

    /* Get the next unprocessed token */
    if (parser != nullptr) {
      fts_token = UT_LIST_GET_FIRST(t_ctx->m_token_list);

      if (fts_token != nullptr) {
        str.f_len = fts_token->m_text->f_len;
        str.f_n_char = fts_token->m_text->f_n_char;
        str.f_str = fts_token->m_text->f_str;
      } else {
        ut_a(UT_LIST_GET_LEN(t_ctx->m_token_list) == 0);
        /* Reach the end of the list */
        t_ctx->m_processed_len = doc->text.f_len;
        break;
      }
    } else {
      inc = innobase_mysql_fts_get_token(
          doc->charset, doc->text.f_str + t_ctx->m_processed_len,
          doc->text.f_str + doc->text.f_len, &str);

      ut_a(inc > 0);
    }
    /* str now contains the token */

    /* Ignore string whose character number is less than
    "fts_min_token_size" or more than "fts_max_token_size" */
    if (!fts_check_token(&str, nullptr, is_ngram, nullptr)) {
      if (parser != nullptr) {
        UT_LIST_REMOVE(t_ctx->m_token_list, fts_token);
        ut::free(fts_token);
      } else {
        t_ctx->m_processed_len += inc;
      }
      continue;
    }

    t_str.f_len =
        innobase_fts_casedn_str(doc->charset, (char *)str.f_str, str.f_len,
                                (char *)&str_buf, FTS_MAX_WORD_LEN + 1);

    t_str.f_str = (byte *)&str_buf;

    /* If "cached_stopword" is defined, ignore words in the stopword list */
    if (!fts_check_token(&str, t_ctx->m_cached_stopword, is_ngram,
                         doc->charset)) {
      if (parser != nullptr) {
        UT_LIST_REMOVE(t_ctx->m_token_list, fts_token);
        ut::free(fts_token);
      } else {
        t_ctx->m_processed_len += inc;
      }
      continue;
    }

    /* There are FTS_NUM_AUX_INDEX auxiliary tables, find
    out which sort buffer to put this word record in */
    t_ctx->m_handler_id =
        fts_select_index(doc->charset, t_str.f_str, t_str.f_len);

    auto key_buffer = &m_handlers[t_ctx->m_handler_id]->m_key_buffer;

    ut_a(t_ctx->m_handler_id < FTS_NUM_AUX_INDEX);

    auto &fields = key_buffer->m_dtuples[key_buffer->m_n_tuples];
    auto field = fields = key_buffer->alloc(FTS_NUM_FIELDS_SORT);

    /* The first field is the tokenized word */
    dfield_set_data(field, t_str.f_str, t_str.f_len);
    auto len = dfield_get_len(field);

    field->type.mtype = word_dtype->mtype;
    field->type.prtype = word_dtype->prtype | DATA_NOT_NULL;

    /* Variable length field, set to max size. */
    field->type.len = FTS_MAX_WORD_LEN;
    field->type.mbminmaxlen = word_dtype->mbminmaxlen;

    auto cur_len = len;

    ++field;

    /* The second field is the Doc ID */

    doc_id_t write_doc_id;
    uint32_t doc_id_32_bit;

    if (!m_doc_id_32_bit) {
      fts_write_doc_id((byte *)&write_doc_id, doc_id);
      dfield_set_data(field, &write_doc_id, sizeof(write_doc_id));
    } else {
      mach_write_to_4((byte *)&doc_id_32_bit, (uint32_t)doc_id);
      dfield_set_data(field, &doc_id_32_bit, sizeof(doc_id_32_bit));
    }

    len = field->len;
    ut_a(len == FTS_DOC_ID_LEN || len == sizeof(uint32_t));

    field->type.len = len;
    field->type.mbminmaxlen = 0;
    field->type.mtype = DATA_INT;
    field->type.prtype = DATA_NOT_NULL | DATA_BINARY_TYPE;

    cur_len += len;

    ++field;

    uint32_t position;

    {
      auto p = reinterpret_cast<byte *>(&position);
      /* The third field is the position. */
      if (parser != nullptr) {
        mach_write_to_4(p, fts_token->m_position + t_ctx->m_init_pos);
      } else {
        const auto n =
            t_ctx->m_processed_len + inc - str.f_len + t_ctx->m_init_pos;
        mach_write_to_4(p, n);
      }
    }

    dfield_set_data(field, &position, sizeof(position));
    len = dfield_get_len(field);
    ut_a(len == sizeof(uint32_t));

    field->type.len = len;
    field->type.mbminmaxlen = 0;
    field->type.mtype = DATA_INT;
    field->type.prtype = DATA_NOT_NULL;
    cur_len += len;

    /* One variable length column, word with its length less than
    fts_max_token_size, add one extra size and one extra byte.

    Since the max length for FTS token now is larger than 255, so we will
    need to signify length byte itself, so only 1 to 128 bytes can be used
    for 1 bytes, larger than that 2 bytes. */
    cur_len += t_str.f_len < 128 ? 2 : 3;

    /* Reserve one byte for the end marker of Aligned_buffer. */
    if (key_buffer->m_total_size + cur_len >= key_buffer->m_buffer_size - 1) {
      buf_full = true;
      break;
    }

    key_buffer->deep_copy(FTS_NUM_FIELDS_SORT, cur_len);

    if (parser != nullptr) {
      UT_LIST_REMOVE(t_ctx->m_token_list, fts_token);
      ut::free(fts_token);
    } else {
      t_ctx->m_processed_len += inc;
    }
  }

  if (!buf_full) {
    /* we pad one byte between text across two fields */
    t_ctx->m_init_pos += doc->text.f_len + 1;
  }

  return !buf_full;
}

void FTS::Parser::get_next_doc_item(FTS::Doc_item *&doc_item) noexcept {
  if (doc_item != nullptr) {
    ut::free(doc_item);
    doc_item = nullptr;
  }

  if (!m_docq->dequeue(doc_item)) {
    return;
  }

  if (doc_item != nullptr) {
    const auto sz = sizeof(FTS::Doc_item) + doc_item->m_field->len;
    ut_a(m_memory_used >= sz);

    m_memory_used.fetch_sub(sz, std::memory_order_relaxed);
  }
}

void FTS::Parser::parse(Builder *builder) noexcept {
  fts_doc_t doc;
  size_t retried{};
  dtype_t word_dtype;
  uint64_t total_rec{};
  dberr_t err{DB_SUCCESS};
  Tokenize_ctx t_ctx{};
  size_t n_doc_processed{};
  FTS::Doc_item *doc_item{};

  auto table = m_ctx.new_table();
  auto old_table = m_ctx.old_table();
  auto blob_heap = mem_heap_create(512, UT_LOCATION_HERE);

  memset(&doc, 0, sizeof(doc));

  doc.charset = fts_index_get_charset(m_dup->m_index);

  auto idx_field = m_dup->m_index->get_field(0);

  word_dtype.prtype = idx_field->col->prtype;
  word_dtype.mbminmaxlen = idx_field->col->mbminmaxlen;

  word_dtype.mtype =
      (doc.charset == &my_charset_latin1) ? DATA_VARCHAR : DATA_VARMYSQL;

  const page_size_t &page_size = dict_table_page_size(table);

  get_next_doc_item(doc_item);

  t_ctx.m_cached_stopword = table->fts->cache->stopword_info.cached_stopword;

  auto processed{true};

  auto clean_up = [&](dberr_t err) {
    mem_heap_free(blob_heap);

#ifdef UNIV_DEBUG
    if (Sync_point::enabled(m_ctx.thd(), "ddl_fts_write_failure")) {
      err = DB_TEMP_FILE_WRITE_FAIL;
    };
#endif

    if (err != DB_SUCCESS) {
      builder->set_error(err);
      set_error(err);
    }

    if (!m_docq->empty()) {
      /* Child can exit either with error or told by parent. */
      ut_a(err != DB_SUCCESS || m_parent_state == Thread_state::ABORT);
    }

    /* Free fts doc list in case of err. */
    do {
      get_next_doc_item(doc_item);
    } while (doc_item != nullptr);
  };

  auto handle_tail_end = [&]() {
    /* Do a final sort of the last (or latest) batch of records
    in block memory. Flush them to temp file if records cannot
    be held in one block of memory. */
    for (size_t i = 0; i < FTS_NUM_AUX_INDEX; ++i) {
      auto &handler = m_handlers[i];

      if (!handler->m_key_buffer.empty()) {
        auto key_buffer = &handler->m_key_buffer;
        auto io_buffer = handler->m_io_buffer;

        const auto n_tuples = key_buffer->size();

        key_buffer->sort(nullptr);

        auto &file = handler->m_file;
        handler->m_offsets.push_back(file.m_size);

        auto persistor = [&](IO_buffer io_buffer) -> dberr_t {
          return builder->append(file, io_buffer);
        };

        err = key_buffer->serialize(io_buffer, persistor);

        if (err != DB_SUCCESS) {
          clean_up(DB_TEMP_FILE_WRITE_FAIL);
          return;
        }

        key_buffer->clear();

        file.m_n_recs += n_tuples;
      }
    }

    for (size_t i = 0; i < FTS_NUM_AUX_INDEX; i++) {
      auto &handler = m_handlers[i];

      if (handler->m_offsets.size() <= 1) {
        continue;
      }

      Merge_file_sort::Context merge_ctx;

      merge_ctx.m_dup = m_dup;
      merge_ctx.m_file = &handler->m_file;

      merge_ctx.m_n_threads =
          m_ctx.m_fts.m_ptr->get_n_parsers() * FTS_NUM_AUX_INDEX;

      Merge_file_sort merge_file_sort{&merge_ctx};

      err = merge_file_sort.sort(builder, handler->m_offsets);

      if (err != DB_SUCCESS) {
        clean_up(err);
        return;
      }

      total_rec += handler->m_file.m_n_recs;
    }

    clean_up(DB_SUCCESS);
  };

  /* Items provided by get_next_doc_item are individual fields
  of a potentially multi-field document. Subsequent fields in
  multi-field document must arrive consecutively, not
  interleaved by fields from other documents; last_doc_id
  is used to determine whether a new item is part of the same
  document as the previous one. */
  doc_id_t last_doc_id{};

  /* get_next_doc_item() reads items from a non-blocking queue.
  It may therefore yield a nullptr result even when there are more
  documents to be read. The inner loop reads doc items from the
  queue as long as they are available and there is space to store
  the item on the buffer. When either of these conditions is not
  met, control will break out to the outer loop, which handles
  buffer flushing and polling for more data. */
  for (;;) {
    while (doc_item != nullptr) {
      auto dfield = doc_item->m_field;

      last_doc_id = doc_item->m_doc_id;

      ut_a(dfield->data != nullptr && dfield_get_len(dfield) != UNIV_SQL_NULL);

      /* If finish processing the last item, update "doc" with strings in the
      doc_item, otherwise continue processing last item. */
      if (processed) {
        dfield = doc_item->m_field;
        auto data = static_cast<byte *>(dfield_get_data(dfield));
        auto data_len = dfield_get_len(dfield);

        if (dfield_is_ext(dfield)) {
          auto clust_index = old_table->first_index();

          doc.text.f_str = lob::btr_copy_externally_stored_field(
              nullptr, clust_index, &doc.text.f_len, nullptr, data, page_size,
              data_len, false, blob_heap);
        } else {
          doc.text.f_str = data;
          doc.text.f_len = data_len;
        }

        doc.tokens = nullptr;
        t_ctx.m_processed_len = 0;
      } else {
        /* Finish processing the current "doc", continue processing it. */
        ut_a(doc.text.f_str != nullptr);
        ut_a(t_ctx.m_processed_len < doc.text.f_len);
      }

      processed = doc_tokenize(doc_item->m_doc_id, &doc, &word_dtype, &t_ctx);

      /* Current sort buffer full, need to recycle */
      if (!processed) {
        ut_a(t_ctx.m_processed_len < doc.text.f_len);
        break;
      }

      ++n_doc_processed;

      mem_heap_empty(blob_heap);

      get_next_doc_item(doc_item);

      if (doc_item != nullptr && last_doc_id != doc_item->m_doc_id) {
        t_ctx.m_init_pos = 0;
      }
    }

    auto &handler = m_handlers[t_ctx.m_handler_id];

    /* If we run out of current sort buffer, need to sort and flush the
    sort buffer to disk. */
    if (handler->m_key_buffer.size() > 0 && !processed) {
      auto &file = handler->m_file;
      auto key_buffer = &handler->m_key_buffer;
      auto io_buffer = handler->m_io_buffer;
      const auto n_tuples = key_buffer->size();

      key_buffer->sort(nullptr);

      handler->m_offsets.push_back(file.m_size);

      auto persistor = [&](IO_buffer io_buffer) -> dberr_t {
        return builder->append(file, io_buffer);
      };

      err = key_buffer->serialize(io_buffer, persistor);

      if (err != DB_SUCCESS) {
        clean_up(DB_TEMP_FILE_WRITE_FAIL);
        return;
      }

      key_buffer->clear();

      file.m_n_recs += n_tuples;

      ut_a(doc_item != nullptr);
      continue;
    }

    /* Parent done scanning, and if finish processing all the docs, exit. */
    if (m_parent_state == Thread_state::COMPLETE) {
      if (m_docq->empty()) {
        handle_tail_end();
        break;
      }

      if (retried > 10000) {
        ut_a(doc_item == nullptr);
        /* retied too many times and cannot get new record */
        ib::error(ER_IB_MSG_930)
            << "FTS parallel sort processed " << n_doc_processed
            << " records, the sort queue is not empty but tokenizer"
            << " cannot dequeue records.";
        handle_tail_end();
        break;
      }
    } else if (m_parent_state == Thread_state::ABORT) {
      /* Parent abort. */
      clean_up(err);
      break;
    }

    if (doc_item == nullptr) {
      std::this_thread::yield();
    }

    get_next_doc_item(doc_item);

    if (doc_item != nullptr) {
      if (last_doc_id != doc_item->m_doc_id) {
        t_ctx.m_init_pos = 0;
      }

      retried = 0;
    } else if (m_parent_state == Thread_state::COMPLETE) {
      ++retried;
    }
  }
}

dberr_t FTS::Inserter::write_node(const Insert *ins_ctx,
                                  const fts_string_t *word,
                                  const fts_node_t *node) noexcept {
  auto tuple = ins_ctx->m_tuple;

  /* We don't do a deep copy. Be careful moving these around. */
  uint32_t doc_count{};
  doc_id_t last_doc_id{};
  doc_id_t first_doc_id{};

  {
    /* The first field is the tokenized word */
    auto field = dtuple_get_nth_field(tuple, 0);
    dfield_set_data(field, word->f_str, word->f_len);
  }

  {
    /* The second field is first_doc_id */
    auto field = dtuple_get_nth_field(tuple, 1);
    fts_write_doc_id((byte *)&first_doc_id, node->first_doc_id);
    dfield_set_data(field, &first_doc_id, sizeof(first_doc_id));
  }

  {
    /* The third and fourth fields(TRX_ID, ROLL_PTR) are filled already.*/
    /* The fifth field is last_doc_id */
    auto field = dtuple_get_nth_field(tuple, 4);
    fts_write_doc_id((byte *)&last_doc_id, node->last_doc_id);
    dfield_set_data(field, &last_doc_id, sizeof(last_doc_id));
  }

  {
    /* The sixth field is doc_count */
    auto field = dtuple_get_nth_field(tuple, 5);
    mach_write_to_4((byte *)&doc_count, (uint32_t)node->doc_count);
    dfield_set_data(field, &doc_count, sizeof(doc_count));
  }

  {
    /* The seventh field is ilist */
    auto field = dtuple_get_nth_field(tuple, 6);
    dfield_set_data(field, node->ilist, node->ilist_size);
  }

  return ins_ctx->m_btr_bulk->insert(tuple, 0);
}

dberr_t FTS::Inserter::write_word(Insert *ins_ctx,
                                  fts_tokenizer_word_t *word) noexcept {
  dberr_t ret{DB_SUCCESS};

  ut_ad(ins_ctx->m_handler_id == fts_select_index(ins_ctx->m_charset,
                                                  word->text.f_str,
                                                  word->text.f_len));

  /* Pop out each fts_node in word->nodes write them to auxiliary table */
  for (size_t i = 0; i < ib_vector_size(word->nodes); i++) {
    auto fts_node = static_cast<fts_node_t *>(ib_vector_get(word->nodes, i));

    auto err = write_node(ins_ctx, &word->text, fts_node);

    if (err != DB_SUCCESS) {
      ib::error(ER_IB_MSG_931) << "Failed to write word " << word->text.f_str
                               << " to FTS auxiliary"
                                  " index table, error ("
                               << ut_strerr(err) << ")";
      ret = err;
    } else {
      ut_ad(ins_ctx->m_btr_bulk->get_n_recs() > 0);
    }

    ut::free(fts_node->ilist);
    fts_node->ilist = nullptr;
  }

  ib_vector_reset(word->nodes);

  return ret;
}

void FTS::Inserter::insert_tuple(Insert *ins_ctx, fts_tokenizer_word_t *word,
                                 ib_vector_t *positions, doc_id_t *in_doc_id,
                                 const dtuple_t *dtuple) noexcept {
  fts_node_t *fts_node;

  /* Get fts_node for the FTS auxiliary INDEX table */
  if (ib_vector_size(word->nodes) > 0) {
    fts_node = static_cast<fts_node_t *>(ib_vector_last(word->nodes));
  } else {
    fts_node = nullptr;
  }

  if (fts_node == nullptr || fts_node->ilist_size > FTS_ILIST_MAX_SIZE) {
    fts_node = static_cast<fts_node_t *>(ib_vector_push(word->nodes, nullptr));

    memset(fts_node, 0x0, sizeof(*fts_node));
  }

  /* If dtuple == nullptr, this is the last word to be processed */
  if (dtuple == nullptr) {
    if (fts_node != nullptr && ib_vector_size(positions) > 0) {
      fts_cache_node_add_positions(nullptr, fts_node, *in_doc_id, positions);

      /* Write out the current word */
      write_word(ins_ctx, word);
    }

    return;
  }

  /* Get the first field for the tokenized word */
  auto dfield = dtuple_get_nth_field(dtuple, 0);

  fts_string_t token_word;

  token_word.f_n_char = 0;
  token_word.f_len = dfield->len;
  token_word.f_str = static_cast<byte *>(dfield_get_data(dfield));

  if (word->text.f_str == nullptr) {
    fts_string_dup(&word->text, &token_word, ins_ctx->m_heap);
  }

  /* Compare to the last word, to see if they are the same word */
  if (innobase_fts_text_cmp(ins_ctx->m_charset, &word->text, &token_word) !=
      0) {
    /* Getting a new word, flush the last position info
    for the current word in fts_node */
    if (ib_vector_size(positions) > 0) {
      fts_cache_node_add_positions(nullptr, fts_node, *in_doc_id, positions);
    }

    /* Write out the current word */
    write_word(ins_ctx, word);

    /* Copy the new word */
    fts_string_dup(&word->text, &token_word, ins_ctx->m_heap);

    const auto n_item = ib_vector_size(positions);

    /* Clean up position queue */
    for (size_t i = 0; i < n_item; i++) {
      ib_vector_pop(positions);
    }

    /* Reset Doc ID */
    *in_doc_id = 0;
    memset(fts_node, 0x0, sizeof(*fts_node));
  }

  /* Get the word's Doc ID */
  dfield = dtuple_get_nth_field(dtuple, 1);

  doc_id_t doc_id;

  {
    const auto ptr = static_cast<byte *>(dfield_get_data(dfield));

    if (ins_ctx->m_doc_id_32_bit == 0) {
      doc_id = fts_read_doc_id(ptr);
    } else {
      doc_id = mach_read_from_4(ptr);
    }
  }

  /* Get the word's position info. */
  dfield = dtuple_get_nth_field(dtuple, 2);

  const auto ptr = static_cast<byte *>(dfield_get_data(dfield));
  const doc_id_t position = mach_read_from_4(ptr);

  /* If this is the same word as the last word, and they
  have the same Doc ID, we just need to add its position
  info. Otherwise, we will flush position info to the
  fts_node and initiate a new position vector  */
  if (*in_doc_id == 0 || *in_doc_id == doc_id) {
    ib_vector_push(positions, &position);
  } else {
    const auto n_pos = ib_vector_size(positions);

    fts_cache_node_add_positions(nullptr, fts_node, *in_doc_id, positions);

    for (size_t i = 0; i < n_pos; i++) {
      ib_vector_pop(positions);
    }

    ib_vector_push(positions, &position);
  }

  /* Record the current Doc ID */
  *in_doc_id = doc_id;
}

dberr_t FTS::Inserter::insert(Builder *builder,
                              Inserter::Handler *handler) noexcept {
  ut_a(!handler->m_files.empty());

  /* We use the insert query graph as the dummy graph needed in the
  row module call */

  auto trx = trx_allocate_for_background();

  trx_start_if_not_started(trx, true, UT_LOCATION_HERE);

  trx->op_info = "inserting index entries";

  Insert ins_ctx;

  ins_ctx.m_doc_id_32_bit = m_doc_id_32_bit;

  auto tuple_heap = mem_heap_create(512, UT_LOCATION_HERE);

  auto index = m_dup->m_index;

  ins_ctx.m_heap = tuple_heap;
  ins_ctx.m_charset = fts_index_get_charset(index);

  /* Initialize related variables if creating FTS indexes */
  auto heap_alloc = ib_heap_allocator_create(tuple_heap);

  fts_tokenizer_word_t new_word;

  memset(&new_word, 0, sizeof(new_word));

  new_word.nodes = ib_vector_create(heap_alloc, sizeof(fts_node_t), 4);

  dict_table_t *aux_table{};
  {
    fts_table_t fts_table;

    fts_table.index_id = index->id;
    fts_table.table_id = m_ctx.new_table()->id;
    fts_table.table = index->table;
    fts_table.type = FTS_INDEX_TABLE;
    fts_table.suffix = fts_get_suffix(handler->m_id);
    fts_table.parent = index->table->name.m_name;

    std::array<char, MAX_FULL_NAME_LEN> aux_table_name;

    /* Get aux index */
    fts_get_table_name(&fts_table, aux_table_name.data());

    aux_table = dd_table_open_on_name(nullptr, nullptr, aux_table_name.data(),
                                      false, DICT_ERR_IGNORE_NONE);
  }

  ut_ad(aux_table != nullptr);
  dd_table_close(aux_table, nullptr, nullptr, false);

  auto observer = m_ctx.flush_observer();
  auto aux_index = aux_table->first_index();

  auto func_exit = [&](dberr_t err) {
    fts_sql_commit(trx);

    trx->op_info = "";

    if (ins_ctx.m_btr_bulk != nullptr) {
      err = ins_ctx.m_btr_bulk->finish(err);
      ut::delete_(ins_ctx.m_btr_bulk);
    }

    trx_free_for_background(trx);

    mem_heap_free(tuple_heap);

    return err;
  };

  /* Create bulk load instance */
  ins_ctx.m_btr_bulk = ut::new_withkey<Btree_load>(
      ut::make_psi_memory_key(mem_key_ddl), aux_index, trx->id, observer);

  /* Create tuple for insert. */
  ins_ctx.m_tuple =
      dtuple_create(tuple_heap, dict_index_get_n_fields(aux_index));

  const auto n_fields = dict_index_get_n_fields(aux_index);

  dict_index_copy_types(ins_ctx.m_tuple, aux_index, n_fields);

  /* Set TRX_ID and ROLL_PTR */
  roll_ptr_t roll_ptr{};
  byte trx_id_buf[DATA_TRX_ID_LEN];

  {
    auto field = dtuple_get_nth_field(ins_ctx.m_tuple, 2);

    trx_write_trx_id(trx_id_buf, trx->id);
    dfield_set_data(field, &trx_id_buf, DATA_TRX_ID_LEN);
  }

  {
    auto field = dtuple_get_nth_field(ins_ctx.m_tuple, 3);

    dfield_set_data(field, &roll_ptr, 7);
  }

  ut_d(ins_ctx.m_handler_id = handler->m_id);

  size_t total_rows{};
  Merge_cursor cursor(builder, nullptr, nullptr);

  {
    const auto n_buffers = handler->m_files.size();
    const auto io_buffer_size = m_ctx.merge_io_buffer_size(n_buffers);

    for (auto &file : handler->m_files) {
      ut_a(file.m_n_recs > 0);

      auto err = cursor.add_file(file, io_buffer_size);

      if (err != DB_SUCCESS) {
        return err;
      }
      total_rows += file.m_n_recs;
    }
  }

  if (total_rows == 0) {
    return func_exit(DB_SUCCESS);
  }

  auto err = cursor.open();

  if (err != DB_SUCCESS) {
    return func_exit(err);
  }

  /* Fetch sorted records from the run files and insert them into
  corresponding FTS index auxiliary tables. */

  doc_id_t doc_id{};
  dtuple_t *dtuple{};
  auto heap = mem_heap_create(1000, UT_LOCATION_HERE);
  auto positions = ib_vector_create(heap_alloc, sizeof(doc_id_t), 32);

  while ((err = cursor.fetch(dtuple)) == DB_SUCCESS) {
    mem_heap_empty(heap);

    insert_tuple(&ins_ctx, &new_word, positions, &doc_id, dtuple);

    --total_rows;

    err = cursor.next();

    if (err != DB_SUCCESS) {
      break;
    }
  }

  if (err == DB_SUCCESS || err == DB_END_OF_INDEX) {
    ut_a(total_rows == 0);
    insert_tuple(&ins_ctx, &new_word, positions, &doc_id, nullptr);
  }

  mem_heap_free(heap);

  return func_exit(err == DB_END_OF_INDEX ? DB_SUCCESS : err);
}

FTS::FTS(Context &ctx, dict_index_t *index, dict_table_t *table) noexcept
    : m_ctx(ctx), m_index(index), m_table(table) {
  m_dup.m_n_dup = 0;
  m_dup.m_index = nullptr;
  m_dup.m_table = ctx.m_table;
  m_dup.m_col_map = m_ctx.m_col_map;
}

FTS::~FTS() noexcept {
  destroy();

  if (m_dup.m_index != nullptr) {
    dict_mem_index_free(m_dup.m_index);
  }
}

dberr_t FTS::create(size_t n_threads) noexcept {
  ut_a(m_parsers.empty());

  for (size_t i = 0; i < n_threads; ++i) {
    auto parser = ut::new_withkey<Parser>(ut::make_psi_memory_key(mem_key_ddl),
                                          i, m_ctx, &m_dup, m_doc_id_32_bit);

    if (parser == nullptr) {
      destroy();
      return DB_OUT_OF_MEMORY;
    }

    m_parsers.push_back(parser);

    auto err = parser->init(n_threads);

    if (err != DB_SUCCESS) {
      destroy();
      return err;
    }
  }

  m_inserter = ut::new_withkey<Inserter>(ut::make_psi_memory_key(mem_key_ddl),
                                         m_ctx, &m_dup, m_doc_id_32_bit);

  if (m_inserter == nullptr) {
    destroy();
    return DB_OUT_OF_MEMORY;
  }

  return DB_SUCCESS;
}

void FTS::destroy() noexcept {
  for (auto parser : m_parsers) {
    ut::delete_(parser);
  }

  m_parsers.clear();

  if (m_inserter != nullptr) {
    ut::delete_(m_inserter);
    m_inserter = nullptr;
  }
}

dberr_t FTS::init(size_t n_threads) noexcept {
  ut_a(m_dup.m_index == nullptr);

  m_dup.m_index = create_index(m_index, m_table, &m_doc_id_32_bit);

  m_sort_index = m_dup.m_index;

  return create(n_threads);
}

dberr_t FTS::start_parse_threads(Builder *builder) noexcept {
  auto fn = [&](PSI_thread_seqnum seqnum, Parser *parser, Builder *builder) {
    ut_a(seqnum > 0);
#ifdef UNIV_PFS_THREAD
    Runnable runnable{fts_parallel_tokenization_thread_key, seqnum};
#else
    Runnable runnable{PSI_NOT_INSTRUMENTED, seqnum};
#endif /* UNIV_PFS_THREAD */
    runnable([&]() {
      auto thd = create_internal_thd();
      ut_ad(current_thd == thd);

      thd->push_diagnostics_area(&parser->da, false);
      parser->parse(builder);
      thd->pop_diagnostics_area();

      destroy_internal_thd(current_thd);
      /* Return value ignored but required for Runnable::operator() */
      return DB_SUCCESS;
    });
  };

  size_t seqnum{1};

  for (auto parser : m_parsers) {
    try {
      m_threads.push_back(std::thread(fn, seqnum++, parser, builder));
    } catch (...) {
      parser->set_error(DB_OUT_OF_RESOURCES);
      return DB_OUT_OF_RESOURCES;
    }
  }

  return DB_SUCCESS;
}

dberr_t FTS::enqueue(FTS::Doc_item *doc_item) noexcept {
  auto parser = m_parsers[doc_item->m_doc_id % m_parsers.size()];

  return parser->enqueue(doc_item);
}

dberr_t FTS::check_for_errors() noexcept {
  for (auto parser : m_parsers) {
    auto da = &parser->da;
    if (da->is_error() && !m_ctx.thd()->is_error()) {
      m_ctx.thd()->get_stmt_da()->set_error_status(
          da->mysql_errno(), da->message_text(), da->returned_sqlstate());
    }
    m_ctx.thd()->get_stmt_da()->copy_sql_conditions_from_da(m_ctx.thd(),
                                                            &parser->da);
  }
  for (auto parser : m_parsers) {
    auto err = parser->get_error();

    if (err != DB_SUCCESS) {
      m_ctx.m_trx->error_key_num = parser->id();
      return err;
    }
  }

  return DB_SUCCESS;
}

dberr_t FTS::insert(Builder *builder) noexcept {
  Threads threads{};
  std::vector<dberr_t, ut::allocator<dberr_t>> errs{};

  errs.assign(FTS_NUM_AUX_INDEX, DB_SUCCESS);

  auto fn = [&](PSI_thread_seqnum seqnum, FTS::Inserter::Handler *handler,
                dberr_t &err) {
    ut_a(seqnum > 0);
#ifdef UNIV_PFS_THREAD
    Runnable runnable{fts_parallel_merge_thread_key, seqnum};
#else
    Runnable runnable{PSI_NOT_INSTRUMENTED, seqnum};
#endif /* UNIV_PFS_THREAD */

    if (!handler->m_files.empty()) {
      err = runnable([&]() { return m_inserter->insert(builder, handler); });
    }
  };

  dberr_t err{DB_SUCCESS};
  const auto last = FTS_NUM_AUX_INDEX - 1;

  for (size_t i = 0; i < last; ++i) {
    auto handler = &m_inserter->m_handlers[i];
    try {
      threads.push_back(std::thread{fn, i + 1, handler, std::ref(errs[i])});
    } catch (...) {
      err = errs[i] = DB_OUT_OF_RESOURCES;
      break;
    }
  }

  if (err == DB_SUCCESS) {
    auto handler = &m_inserter->m_handlers[last];
    if (!handler->m_files.empty()) {
      errs[last] = m_inserter->insert(builder, handler);
    }
    if (errs[last] != DB_SUCCESS) {
      builder->set_error(errs[last]);
    }
  }

  {
    size_t i{};

    for (auto &thread : threads) {
      thread.join();
      if (errs[i] != DB_SUCCESS) {
        builder->set_error(errs[i]);
      }
      ++i;
    }
  }

  return builder->get_error();
}

dberr_t FTS::setup_insert_phase() noexcept {
  for (auto parser : m_parsers) {
    for (size_t i = 0; i < FTS_NUM_AUX_INDEX; ++i) {
      auto file = parser->release_file(i);

      if (file.m_n_recs == 0) {
        /* Ignore empty files. */
        continue;
      }

      const auto err = m_inserter->add_file(i, std::move(file));

      if (err != DB_SUCCESS) {
        break;
      }
    }
  }

  return DB_SUCCESS;
}

dberr_t FTS::scan_finished(dberr_t err) noexcept {
  for (auto parser : m_parsers) {
    if (err == DB_SUCCESS) {
      parser->set_parent_state(Thread_state::COMPLETE);
    } else {
      parser->set_parent_state(Thread_state::ABORT);
    }
  }

  for (auto &thread : m_threads) {
    thread.join();
  }

  if (err == DB_SUCCESS) {
    err = check_for_errors();
  }

  if (err != DB_SUCCESS) {
    return err;
  }

  auto &fts = m_ctx.m_fts;

  /* Update the next Doc ID we used. Table should be locked, so
  no concurrent DML */
  if (fts.m_doc_id != nullptr && err == DB_SUCCESS) {
    const auto generated = fts.m_doc_id->is_generated();

    if ((generated && fts.m_doc_id->generated_count() > 0) ||
        (!generated && fts.m_doc_id->max_doc_id() > 0)) {
      /* Sync fts cache for other fts indexes to keep all fts indexes
      consistent in sync_doc_id. */
      auto table = const_cast<dict_table_t *>(m_ctx.m_new_table);

      err = fts_sync_table(table, false, true, false);

      if (err == DB_SUCCESS) {
        auto name = m_ctx.m_old_table->name.m_name;
        const auto max_doc_id{fts.m_doc_id->max_doc_id()};

        fts_update_next_doc_id(nullptr, m_ctx.m_new_table, name, max_doc_id);
      }
    }
  }

  if (err == DB_SUCCESS) {
    err = setup_insert_phase();
  }

  for (auto parser : m_parsers) {
    ut::delete_(parser);
  }

  m_parsers.clear();

  return err;
}

}  // namespace ddl
