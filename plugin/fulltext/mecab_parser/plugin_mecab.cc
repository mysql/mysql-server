/* Copyright (c) 2014, 2015,  Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "my_config.h"
#include "mysqld_error.h"
#include <string>
#include <log.h>
#include <mecab.h>
#include <fts0tokenize.h>

/* We are following InnoDB coding guidelines. */

/** Global mecab objects shared by all threads. */
static MeCab::Model*	mecab_model = NULL;
static MeCab::Tagger*	mecab_tagger = NULL;

/** Mecab charset. */
static char	mecab_charset[64];

/** Mecab rc file path. */
static char*	mecab_rc_file;

static const char*	mecab_min_supported_version = "0.993";
static const char*	mecab_max_supported_version = "0.996";

#if defined(BUNDLE_MECAB)
static const bool bundle_mecab= true;
#else
static const bool bundle_mecab= false;
#endif

/** Set MeCab parser charset.
@param[in]	charset charset string
@retval	true	on success
@retval	false	on failure */
static
bool
mecab_parser_check_and_set_charset(
	const char*	charset)
{
	/* Array used to map mecab charset to mysql charset. */
	static const int	mecab_charset_count = 4;
	static const char*	mecab_charset_values[mecab_charset_count][2] = {
		{"euc-jp",	"ujis"},
		{"sjis",	"sjis"},
		{"utf-8",	"utf8"},
		{"utf8",	"utf8"}
	};

	for (int i = 0; i < mecab_charset_count; i++) {
		if (native_strcasecmp(charset, mecab_charset_values[i][0])
		    == 0 ) {
			strcpy(mecab_charset, mecab_charset_values[i][1]);
			return(true);
		}
	}

	return(false);
}

/** MeCab parser plugin initialization.
@retval 0 on success
@retval 1 on failure. */
static
int
mecab_parser_plugin_init(void*)
{
	const MeCab::DictionaryInfo*	mecab_dict;

	/* Check mecab version. */
	if (strcmp(MeCab::Model::version(), mecab_min_supported_version) < 0) {
		sql_print_error("Mecab v%s is not supported,"
				" the lowest version supported is v%s.",
				MeCab::Model::version(),
				mecab_min_supported_version);
		return(1);
	}

	if (strcmp(MeCab::Model::version(), mecab_max_supported_version) > 0) {
		sql_print_warning("Mecab v%s is not verified,"
				  " the highest version supported is v%s.",
				  MeCab::Model::version(),
				  mecab_max_supported_version);
	}

	if (mecab_rc_file != NULL) {
		std::string	rcfile_arg;

		/* See src/tagger.cpp for available options.
		--rcfile=<mecabrc file>  "use FILE as resource file" */
		rcfile_arg += "--rcfile=";
		rcfile_arg += mecab_rc_file;

		/* It seems we *must* have some kind of mecabrc
		file available before calling createModel, see
		load_dictionary_resource() in  src/utils.cpp */
		sql_print_information("Mecab: Trying createModel(%s)",
				      rcfile_arg.c_str());

		mecab_model = MeCab::createModel(rcfile_arg.c_str());
	} else {
		sql_print_information("Mecab: Trying createModel()");
		mecab_model = MeCab::createModel("");
	}

	if (mecab_model == NULL) {
		sql_print_error("Mecab: createModel() failed: %s",
				MeCab::getLastError());
		return(1);
	}

	mecab_tagger = mecab_model->createTagger();
	if (mecab_tagger == NULL) {
		sql_print_error("Mecab: createTagger() failed: %s",
				MeCab::getLastError());
		delete mecab_model;
		mecab_model= NULL;
		return(1);
	}

	mecab_dict = mecab_model->dictionary_info();
	mecab_charset[0] = '\0';
	if (!mecab_parser_check_and_set_charset(mecab_dict->charset)) {
		delete mecab_tagger;
		mecab_tagger = NULL;

		delete mecab_model;
		mecab_model = NULL;

		sql_print_error("Mecab: Unsupported dictionary charset %s",
				mecab_dict->charset);
		return(1);
	} else {
		sql_print_information("Mecab: Loaded dictionary charset is %s",
				      mecab_dict->charset);
		return(0);
	}
}

/** MeCab parser plugin deinit
@retval	0 */
static
int
mecab_parser_plugin_deinit(void*)
{
	delete mecab_tagger;
	mecab_tagger = NULL;

	delete mecab_model;
	mecab_model = NULL;

	return(0);
}

/** Parse a document by MeCab.
@param[in]	mecab_lattice	mecab lattice
@param[in]	param		plugin parser param
@param[in]	doc		document to parse
@param[in]	len		document length
@param[in,out]	bool_info	boolean info
@retvat	0	on success
@retval	1	on failure. */
static
int
mecab_parse(
	MeCab::Lattice*		mecab_lattice,
	MYSQL_FTPARSER_PARAM*	param,
	char*			doc,
	int			len,
	MYSQL_FTPARSER_BOOLEAN_INFO*
				bool_info)
{
	static MYSQL_FTPARSER_BOOLEAN_INFO token_info =
		{ FT_TOKEN_WORD, 0, 0, 0, 0, 0, ' ', 0};
	int	position = 0;
	int	token_num = 0;
	int	ret = 0;
	bool	term_converted = false;

	try {
		mecab_lattice->set_sentence(doc, len);

		if(!mecab_tagger->parse(mecab_lattice)) {
			sql_print_error("Mecab: parse() failed: %s",
					mecab_lattice->what());
			return(1);
		}
	} catch (std::bad_alloc const &) {
		sql_print_error("Mecab: parse() failed: out of memory.");

		return(1);
	}

	if (param->mode == MYSQL_FTPARSER_FULL_BOOLEAN_INFO) {
		for (const MeCab::Node* node = mecab_lattice->bos_node();
		     node != NULL; node = node->next) {
			token_num += 1;
		}

		/* If the term has more than one token, convert it to a phrase.*/
		if (bool_info->quot == NULL && token_num > 1) {
			term_converted = true;

			bool_info->type = FT_TOKEN_LEFT_PAREN;
			bool_info->quot = reinterpret_cast<char*>(1);

			ret = param->mysql_add_word(param, NULL, 0, bool_info);
			if (ret != 0) {
				return(ret);
			}
		}
	}

	for (const MeCab::Node* node = mecab_lattice->bos_node();
	     node != NULL; node = node->next) {
		bool_info->position = position;
		position += node->rlength;

		param->mysql_add_word(param, const_cast<char*>(node->surface),
				      node->length,
				      term_converted ? &token_info : bool_info);
	}

	if (term_converted) {
		bool_info->type = FT_TOKEN_RIGHT_PAREN;
		ret = param->mysql_add_word(param, NULL, 0, bool_info);

		DBUG_ASSERT(bool_info->quot == NULL);
		bool_info->type = FT_TOKEN_WORD;
	}

	return(ret);
}

/** MeCab parser parse a document.
@param[in]	param	plugin parser param
@retval	0	on success
@retval	1	on failure. */
static
int
mecab_parser_parse(
	MYSQL_FTPARSER_PARAM*	param)
{
	MeCab::Lattice*			mecab_lattice = NULL;
	MYSQL_FTPARSER_BOOLEAN_INFO	bool_info =
		{ FT_TOKEN_WORD, 0, 0, 0, 0, 0, ' ', 0};
	int		ret = 0;
	const char*	csname = NULL;

	/* Mecab supports utf8mb4(utf8), eucjpms(ujis) and cp932(sjis). */
	if (strcmp(param->cs->csname, MY_UTF8MB4) == 0) {
		csname = "utf8";
	} else if (strcmp(param->cs->csname, "eucjpms") == 0) {
		csname = "ujis";
	} else if (strcmp(param->cs->csname, "cp932") == 0) {
		csname = "sjis";
	} else {
		csname = param->cs->csname;
	}

	/* Check charset */
	if (strcmp(mecab_charset, csname) != 0) {
		char	error_msg[128];

		my_snprintf(error_msg, 127, "Fulltext index charset '%s'"
			    " doesn't match mecab charset '%s'.",
			    param->cs->csname, mecab_charset);
		my_message(ER_ERROR_ON_WRITE, error_msg, MYF(0));

		return(1);
	}

	DBUG_ASSERT(param->cs->mbminlen == 1);

	/* Create mecab lattice for parsing */
	mecab_lattice = mecab_model->createLattice();
	if (mecab_lattice == NULL) {
		sql_print_error("Mecab: createLattice() failed: %s",
				MeCab::getLastError());
		return(1);
	}

	/* Allocate a new string with '\0' in the end to avoid
	valgrind error "Invalid read of size 1" in mecab. */
	DBUG_ASSERT(param->length >= 0);
	int	doc_length = param->length;
	char*	doc = reinterpret_cast<char*>(malloc(doc_length + 1));

	if (doc == NULL) {
		my_error(ER_OUTOFMEMORY, MYF(0), doc_length);
		return(1);
	}

	memcpy(doc, param->doc, doc_length);
	doc[doc_length]= '\0';

	switch(param->mode) {
	case MYSQL_FTPARSER_SIMPLE_MODE:
	case MYSQL_FTPARSER_WITH_STOPWORDS:
		ret = mecab_parse(mecab_lattice, param, doc,
				  doc_length, &bool_info);

		break;

	case MYSQL_FTPARSER_FULL_BOOLEAN_INFO:
		uchar*		start = reinterpret_cast<uchar*>(doc);
		uchar*		end = start + doc_length;
		FT_WORD		word = {NULL, 0, 0};

		while (fts_get_word(param->cs, &start, end, &word, &bool_info)) {
			/* Don't convert term with wildcard. */
			if (bool_info.type == FT_TOKEN_WORD
			    && !bool_info.trunc) {
				ret = mecab_parse(
					mecab_lattice,
					param,
					reinterpret_cast<char*>(word.pos),
					word.len,
					&bool_info);
			} else {
				ret = param->mysql_add_word(
					param,
					reinterpret_cast<char*>(word.pos),
					word.len,
					&bool_info);
			}

			if (ret != 0) {
				break;
			}
		}
	}

	free(doc);
	delete mecab_lattice;

	return(ret);
}

/** Fulltext MeCab Parser Descriptor*/
static struct st_mysql_ftparser mecab_parser_descriptor =
{
	MYSQL_FTPARSER_INTERFACE_VERSION,
	mecab_parser_parse,
	0,
	0
};

/* MeCab plugin status variables */
static struct st_mysql_show_var mecab_status[] =
{
	{"mecab_charset", mecab_charset, SHOW_CHAR, SHOW_SCOPE_GLOBAL},
	{0, 0, enum_mysql_show_type(0), SHOW_SCOPE_GLOBAL}
};

static MYSQL_SYSVAR_STR(rc_file, mecab_rc_file,
  PLUGIN_VAR_READONLY,
  "MECABRC file path",
  NULL, NULL, NULL);

/* MeCab plugin system variables */
static struct st_mysql_sys_var* mecab_system_variables[]= {
	MYSQL_SYSVAR(rc_file),
	NULL
};

/* MeCab plugin descriptor */
mysql_declare_plugin(mecab_parser)
{
	MYSQL_FTPARSER_PLUGIN,		/*!< type	*/
	&mecab_parser_descriptor,	/*!< descriptor	*/
	"mecab",			/*!< name	*/
	"Oracle Corp",			/*!< author	*/
	"Mecab Full-Text Parser for Japanese",	/*!< description*/
	PLUGIN_LICENSE_GPL,		/*!< license	*/
	mecab_parser_plugin_init,	/*!< init function (when loaded)*/
	mecab_parser_plugin_deinit,	/*!< deinit function (when unloaded)*/
	0x0001,				/*!< version	*/
	mecab_status,			/*!< status variables	*/
	mecab_system_variables,		/*!< system variables	*/
	NULL,
	0,
}
mysql_declare_plugin_end;
