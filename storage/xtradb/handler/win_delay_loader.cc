/*****************************************************************************

Copyright (c) 2008, 2009, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/*******************************************************************//**
@file handler/win_delay_loader.cc
This file contains functions that implement the delay loader on Windows.

This is a customized version of delay loader with limited functionalities.
It does not support:

* (manual) unloading
* multiple delay loaded DLLs
* multiple loading of the same DLL

This delay loader is used only by the InnoDB plugin. Other components (DLLs)
can still use the default delay loader, provided by MSVC.

Several acronyms used by Microsoft:
 * IAT: import address table
 * INT: import name table
 * RVA: Relative Virtual Address

See http://msdn.microsoft.com/en-us/magazine/bb985992.aspx for details of
PE format.
***********************************************************************/
#if defined (__WIN__) && defined (MYSQL_DYNAMIC_PLUGIN)
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <delayimp.h>
# include <mysql_priv.h>

extern "C" {
# include "univ.i"
# include "hash0hash.h"
}

/*******************************************************************//**
This following contains a list of externals that can not be resolved by
delay loading. They have to be resolved indirectly via their addresses
in the .map file. All of them are external variables. */
CHARSET_INFO*		wdl_my_charset_bin;
CHARSET_INFO*		wdl_my_charset_latin1;
CHARSET_INFO*		wdl_my_charset_filename;
CHARSET_INFO**		wdl_system_charset_info;
CHARSET_INFO**		wdl_default_charset_info;
CHARSET_INFO**		wdl_all_charsets;
system_variables*	wdl_global_system_variables;
char*			wdl_mysql_real_data_home;
char**			wdl_mysql_data_home;
char**			wdl_tx_isolation_names;
char**			wdl_binlog_format_names;
char*			wdl_reg_ext;
pthread_mutex_t*	wdl_LOCK_thread_count;
key_map*		wdl_key_map_full;
MY_TMPDIR*		wdl_mysql_tmpdir_list;
bool*			wdl_mysqld_embedded;
uint*			wdl_lower_case_table_names;
ulong*			wdl_specialflag;
int*			wdl_my_umask;

/*******************************************************************//**
The preferred load-address defined in PE (portable executable format). */
#if defined(_M_IA64)
#pragma section(".base", long, read)
extern "C"
__declspec(allocate(".base"))
const IMAGE_DOS_HEADER __ImageBase;
#else
extern "C"
const IMAGE_DOS_HEADER __ImageBase;
#endif

/*******************************************************************//**
A template function for converting a relative address (RVA) to an
absolute address (VA). This is due to the pointers in the delay
descriptor (ImgDelayDescr in delayimp.h) have been changed from
VAs to RVAs to work on both 32- and 64-bit platforms.
@return	absolute virtual address */
template <class X>
X PFromRva(
/*=======*/
	RVA	rva)	/*!< in: relative virtual address */
{
	return X(PBYTE(&__ImageBase) + rva);
}

/*******************************************************************//**
Convert to the old format for convenience. The structure as well as its
element names follow the definition of ImgDelayDescr in delayimp.h. */
struct InternalImgDelayDescr
{
	DWORD		grAttrs;	/*!< attributes */
	LPCSTR		szName;		/*!< pointer to dll name */
	HMODULE*	phmod;		/*!< address of module handle */
	PImgThunkData	pIAT;		/*!< address of the IAT */
	PCImgThunkData	pINT;		/*!< address of the INT */
	PCImgThunkData	pBoundIAT;	/*!< address of the optional bound IAT */
	PCImgThunkData	pUnloadIAT;	/*!< address of optional copy of
					   original IAT */
	DWORD		dwTimeStamp;	/*!< 0 if not bound,
					   otherwise date/time stamp of DLL
					   bound to (Old BIND) */
};

typedef struct map_hash_chain_struct	map_hash_chain_t;

struct map_hash_chain_struct {
	char*			symbol;	/*!< pointer to a symbol */
	ulint			value;	/*!< address of the symbol */
	map_hash_chain_t*	next;	/*!< pointer to the next cell
					in the same folder. */
	map_hash_chain_t*	chain;	/*!< a linear chain used for
					cleanup. */
};

static HMODULE				my_hmod = 0;
static struct hash_table_struct*	m_htbl = NULL ;
static map_hash_chain_t*		chain_header = NULL;
static ibool				wdl_init = FALSE;
const ulint				MAP_HASH_CELLS_NUM = 10000;

#ifndef DBUG_OFF
/*******************************************************************//**
In the dynamic plugin, it is required to call the following dbug functions
in the server:
	_db_pargs_
	_db_doprnt_
	_db_enter_
	_db_return_
	_db_dump_

The plugin will get those function pointers during the initialization. */
typedef void (__cdecl* pfn_db_enter_)(
	const char*	_func_,
	const char*	_file_,
	uint		_line_,
	const char**	_sfunc_,
	const char**	_sfile_,
	uint*		_slevel_,
	char***);

typedef void (__cdecl* pfn_db_return_)(
	uint		_line_,
	const char**	_sfunc_,
	const char**	_sfile_,
	uint*		_slevel_);

typedef void (__cdecl* pfn_db_pargs_)(
	uint		_line_,
	const char*	keyword);

typedef void (__cdecl* pfn_db_doprnt_)(
	const char*	format,
	...);

typedef void (__cdecl* pfn_db_dump_)(
	uint			_line_,
	const char*		keyword,
	const unsigned char*	memory,
	size_t			length);

static pfn_db_enter_	wdl_db_enter_;
static pfn_db_return_	wdl_db_return_;
static pfn_db_pargs_	wdl_db_pargs_;
static pfn_db_doprnt_	wdl_db_doprnt_;
static pfn_db_dump_	wdl_db_dump_;
#endif /* !DBUG_OFF */

/*************************************************************//**
Creates a hash table with >= n array cells. The actual number of cells is
chosen to be a prime number slightly bigger than n.

This is the same function as hash_create in hash0hash.c, except the
memory allocation. This function is invoked before the engine is
initialized, and buffer pools are not ready yet.
@return	own: created hash table */
static
hash_table_t*
wdl_hash_create(
/*============*/
	ulint	n)	/*!< in: number of array cells */
{
	hash_cell_t*	array;
	ulint		prime;
	hash_table_t*	table;

	prime = ut_find_prime(n);

	table = (hash_table_t*) malloc(sizeof(hash_table_t));
	if (table == NULL) {
		return(NULL);
	}

	array = (hash_cell_t*) malloc(sizeof(hash_cell_t) * prime);
	if (array == NULL) {
		free(table);
		return(NULL);
	}

	table->array = array;
	table->n_cells = prime;
	table->n_mutexes = 0;
	table->mutexes = NULL;
	table->heaps = NULL;
	table->heap = NULL;
	table->magic_n = HASH_TABLE_MAGIC_N;

	/* Initialize the cell array */
	hash_table_clear(table);

	return(table);
}

/*************************************************************//**
Frees a hash table. */
static
void
wdl_hash_table_free(
/*================*/
	hash_table_t*	table)	/*!< in, own: hash table */
{
	ut_a(table != NULL);
	ut_a(table->mutexes == NULL);

	free(table->array);
	free(table);
}

/*******************************************************************//**
Function for calculating the count of imports given the base of the IAT.
@return	number of imports */
static
ulint
wdl_import_count(
/*=============*/
	PCImgThunkData	pitd_base)	/*!< in: base of the IAT */
{
	ulint		ret = 0;
	PCImgThunkData	pitd = pitd_base;

	while (pitd->u1.Function) {
		pitd++;
		ret++;
	}

	return(ret);
}

/*******************************************************************//**
Read Mapfile to a hashtable for faster access
@return	TRUE if the mapfile is loaded successfully. */
static
ibool
wdl_load_mapfile(
/*=============*/
	const char*	filename)	/*!< in: name of the mapfile. */
{
	FILE*		fp;
	const size_t	nSize = 256;
	char		tmp_buf[nSize];
	char*		func_name;
	char*		func_addr;
	ulint		load_addr = 0;
	ibool		valid_load_addr = FALSE;
#ifdef _WIN64
	const char*	tmp_string = " Preferred load address is %16llx";
#else
	const char*	tmp_string = " Preferred load address is %08x";
#endif

	fp = fopen(filename, "r");
	if (fp == NULL) {

		return(FALSE);
	}

	/* Check whether to create the hashtable */
	if (m_htbl == NULL) {

		m_htbl = wdl_hash_create(MAP_HASH_CELLS_NUM);

		if (m_htbl == NULL) {

			fclose(fp);
			return(FALSE);
		}
	}

	/* Search start of symbol list and get the preferred load address */
	while (fgets(tmp_buf, sizeof(tmp_buf), fp)) {

		if (sscanf(tmp_buf, tmp_string, &load_addr) == 1) {

			valid_load_addr = TRUE;
		}

		if (strstr(tmp_buf, "Rva+Base") != NULL) {

			break;
		}
	}

	if (valid_load_addr == FALSE) {

		/* No "Preferred load address", the map file is wrong. */
		fclose(fp);
		return(FALSE);
	}

	/* Read symbol list */
	while (fgets(tmp_buf, sizeof(tmp_buf), fp))
	{
		map_hash_chain_t*	map_cell;
		ulint			map_fold;

		if (*tmp_buf == 0) {

			continue;
		}

		func_name = strtok(tmp_buf, " ");
		func_name = strtok(NULL, " ");
		func_addr = strtok(NULL, " ");

		if (func_name && func_addr) {

			ut_snprintf(tmp_buf, nSize, "0x%s", func_addr);
			if (*func_name == '_') {

				func_name++;
			}

			map_cell = (map_hash_chain_t*)
				   malloc(sizeof(map_hash_chain_t));
			if (map_cell == NULL) {
				return(FALSE);
			}

			/* Chain all cells together */
			map_cell->chain = chain_header;
			chain_header = map_cell;

			map_cell->symbol = strdup(func_name);
			map_cell->value = (ulint) _strtoui64(tmp_buf, NULL, 0)
					  - load_addr;
			map_fold = ut_fold_string(map_cell->symbol);

			HASH_INSERT(map_hash_chain_t,
				    next,
				    m_htbl,
				    map_fold,
				    map_cell);
		}
	}

	fclose(fp);

	return(TRUE);
}

/*************************************************************//**
Cleanup.during DLL unload */
static
void
wdl_cleanup(void)
/*=============*/
{
	while (chain_header != NULL) {
		map_hash_chain_t*	tmp;

		tmp = chain_header->chain;
		free(chain_header->symbol);
		free(chain_header);
		chain_header = tmp;
	}

	if (m_htbl != NULL) {

		wdl_hash_table_free(m_htbl);
	}
}

/*******************************************************************//**
Load the mapfile mysqld.map.
@return	the module handle */
static
HMODULE
wdl_get_mysqld_mapfile(void)
/*========================*/
{
	char	file_name[MAX_PATH];
	char*	ext;
	ulint	err;

	if (my_hmod == 0) {

		size_t	nSize = MAX_PATH - strlen(".map") -1;

		/* First find out the name of current executable */
		my_hmod = GetModuleHandle(NULL);
		if (my_hmod == 0) {

			return(my_hmod);
		}

		err = GetModuleFileName(my_hmod, file_name, nSize);
		if (err == 0) {

			my_hmod = 0;
			return(my_hmod);
		}

		ext = strrchr(file_name, '.');
		if (ext != NULL) {

			*ext = 0;
			strcat(file_name, ".map");

			err = wdl_load_mapfile(file_name);
			if (err == 0) {

				my_hmod = 0;
			}
		} else {

			my_hmod = 0;
		}
	}

	return(my_hmod);
}

/*******************************************************************//**
Retrieves the address of an exported function. It follows the convention
of GetProcAddress().
@return	address of exported function. */
static
FARPROC
wdl_get_procaddr_from_map(
/*======================*/
	HANDLE		m_handle,	/*!< in: module handle */
	const char*	import_proc)	/*!< in: procedure name */
{
	map_hash_chain_t*	hash_chain;
	ulint			map_fold;

	map_fold = ut_fold_string(import_proc);
	HASH_SEARCH(
		next,
		m_htbl,
		map_fold,
		map_hash_chain_t*,
		hash_chain,
		,
		(ut_strcmp(hash_chain->symbol, import_proc) == 0));

	if (hash_chain == NULL) {

#ifdef _WIN64
		/* On Win64, the leading '_' may not be taken out. In this
		case, search again without the leading '_'. */
		if (*import_proc == '_') {

			import_proc++;
		}

		map_fold = ut_fold_string(import_proc);
		HASH_SEARCH(
			next,
			m_htbl,
			map_fold,
			map_hash_chain_t*,
			hash_chain,
			,
			(ut_strcmp(hash_chain->symbol, import_proc) == 0));

		if (hash_chain == NULL) {
#endif
			if (wdl_init == TRUE) {

				sql_print_error(
					"InnoDB: the procedure pointer of %s"
					" is not found.",
					import_proc);
			}

			return(0);
#ifdef _WIN64
		}
#endif
	}

	return((FARPROC) ((ulint) m_handle + hash_chain->value));
}

/*******************************************************************//**
Retrieves the address of an exported variable.
Note: It does not follow the Windows call convention FARPROC.
@return	address of exported variable. */
static
void*
wdl_get_varaddr_from_map(
/*=====================*/
	HANDLE		m_handle,		/*!< in: module handle */
	const char*	import_variable)	/*!< in: variable name */
{
	map_hash_chain_t*	hash_chain;
	ulint			map_fold;

	map_fold = ut_fold_string(import_variable);
	HASH_SEARCH(
		next,
		m_htbl,
		map_fold,
		map_hash_chain_t*,
		hash_chain,
		,
		(ut_strcmp(hash_chain->symbol, import_variable) == 0));

	if (hash_chain == NULL) {

#ifdef _WIN64
		/* On Win64, the leading '_' may not be taken out. In this
		case, search again without the leading '_'. */
		if (*import_variable == '_') {

			import_variable++;
		}

		map_fold = ut_fold_string(import_variable);
		HASH_SEARCH(
			next,
			m_htbl,
			map_fold,
			map_hash_chain_t*,
			hash_chain,
			,
			(ut_strcmp(hash_chain->symbol, import_variable) == 0));

		if (hash_chain == NULL) {
#endif
			if (wdl_init == TRUE) {

				sql_print_error(
					"InnoDB: the variable address of %s"
					" is not found.",
					import_variable);
			}

			return(0);
#ifdef _WIN64
		}
#endif
	}

	return((void*) ((ulint) m_handle + hash_chain->value));
}

/*******************************************************************//**
Bind all unresolved external variables from the MySQL executable.
@return	TRUE if successful */
static
bool
wdl_get_external_variables(void)
/*============================*/
{
	HMODULE	hmod = wdl_get_mysqld_mapfile();

	if (hmod == 0) {

		return(FALSE);
	}

#define GET_SYM(sym, var, type)					\
	var = (type*) wdl_get_varaddr_from_map(hmod, sym);	\
	if (var == NULL) return(FALSE)
#ifdef _WIN64
#define GET_SYM2(sym1, sym2, var, type)				\
	var = (type*) wdl_get_varaddr_from_map(hmod, sym1);	\
	if (var == NULL) return(FALSE)
#else
#define GET_SYM2(sym1, sym2, var, type)				\
	var = (type*) wdl_get_varaddr_from_map(hmod, sym2);	\
	if (var == NULL) return(FALSE)
#endif // (_WIN64)
#define GET_C_SYM(sym, type) GET_SYM(#sym, wdl_##sym, type)
#define GET_PROC_ADDR(sym)					\
	wdl##sym = (pfn##sym) wdl_get_procaddr_from_map(hmod, #sym)

	GET_C_SYM(my_charset_bin, CHARSET_INFO);
	GET_C_SYM(my_charset_latin1, CHARSET_INFO);
	GET_C_SYM(my_charset_filename, CHARSET_INFO);
	GET_C_SYM(default_charset_info, CHARSET_INFO*);
	GET_C_SYM(all_charsets, CHARSET_INFO*);
	GET_C_SYM(my_umask, int);

	GET_SYM("?global_system_variables@@3Usystem_variables@@A",
		wdl_global_system_variables, struct system_variables);
	GET_SYM("?mysql_real_data_home@@3PADA",
		wdl_mysql_real_data_home, char);
	GET_SYM("?reg_ext@@3PADA", wdl_reg_ext, char);
	GET_SYM("?LOCK_thread_count@@3U_RTL_CRITICAL_SECTION@@A",
		wdl_LOCK_thread_count, pthread_mutex_t);
	GET_SYM("?key_map_full@@3V?$Bitmap@$0EA@@@A",
		wdl_key_map_full, key_map);
	GET_SYM("?mysql_tmpdir_list@@3Ust_my_tmpdir@@A",
		wdl_mysql_tmpdir_list, MY_TMPDIR);
	GET_SYM("?mysqld_embedded@@3_NA",
		wdl_mysqld_embedded, bool);
	GET_SYM("?lower_case_table_names@@3IA",
		wdl_lower_case_table_names, uint);
	GET_SYM("?specialflag@@3KA", wdl_specialflag, ulong);

	GET_SYM2("?system_charset_info@@3PEAUcharset_info_st@@EA",
		 "?system_charset_info@@3PAUcharset_info_st@@A",
		 wdl_system_charset_info, CHARSET_INFO*);
	GET_SYM2("?mysql_data_home@@3PEADEA",
		 "?mysql_data_home@@3PADA",
		 wdl_mysql_data_home, char*);
	GET_SYM2("?tx_isolation_names@@3PAPEBDA",
		 "?tx_isolation_names@@3PAPBDA",
		 wdl_tx_isolation_names, char*);
	GET_SYM2("?binlog_format_names@@3PAPEBDA",
		 "?binlog_format_names@@3PAPBDA",
		 wdl_binlog_format_names, char*);

#ifndef DBUG_OFF
	GET_PROC_ADDR(_db_enter_);
	GET_PROC_ADDR(_db_return_);
	GET_PROC_ADDR(_db_pargs_);
	GET_PROC_ADDR(_db_doprnt_);
	GET_PROC_ADDR(_db_dump_);

	/* If any of the dbug functions is not available, just make them
	all invalid. This is the case when working with a non-debug
	version of the server. */
	if (wdl_db_enter_ == NULL || wdl_db_return_ == NULL
	    || wdl_db_pargs_ == NULL || wdl_db_doprnt_ == NULL
	    || wdl_db_dump_ == NULL) {

		wdl_db_enter_ = NULL;
		wdl_db_return_ = NULL;
		wdl_db_pargs_ = NULL;
		wdl_db_doprnt_ = NULL;
		wdl_db_dump_ = NULL;
	}
#endif /* !DBUG_OFF */

	wdl_init = TRUE;
	return(TRUE);

#undef GET_SYM
#undef GET_SYM2
#undef GET_C_SYM
#undef GET_PROC_ADDR
}

/*******************************************************************//**
The DLL Delayed Loading Helper Function for resolving externals.

The function may fail due to one of the three reasons:

* Invalid parameter, which happens if the attributes in pidd aren't
  specified correctly.
* Failed to load the map file mysqld.map.
* Failed to find an external name in the map file mysqld.map.

Note: this function is called by run-time as well as __HrLoadAllImportsForDll.
So, it has to follow Windows call convention.
@return	the address of the imported function */
extern "C"
FARPROC WINAPI
__delayLoadHelper2(
/*===============*/
	PCImgDelayDescr	pidd,		/*!< in: a const pointer to a
					ImgDelayDescr, see delayimp.h. */
	FARPROC*	iat_entry)	/*!< in/out: A pointer to the slot in
					the delay load import address table
					to be updated with the address of the
					imported function. */
{
	ulint		iIAT, iINT;
	HMODULE		hmod;
	PCImgThunkData	pitd;
	FARPROC		fun = NULL;

	/* Set up data used for the hook procs  */
	InternalImgDelayDescr	idd = {
				pidd->grAttrs,
				PFromRva<LPCSTR>(pidd->rvaDLLName),
				PFromRva<HMODULE*>(pidd->rvaHmod),
				PFromRva<PImgThunkData>(pidd->rvaIAT),
				PFromRva<PCImgThunkData>(pidd->rvaINT),
				PFromRva<PCImgThunkData>(pidd->rvaBoundIAT),
				PFromRva<PCImgThunkData>(pidd->rvaUnloadIAT),
				pidd->dwTimeStamp
	};

	DelayLoadInfo		dli = {
				sizeof(DelayLoadInfo),
				pidd,
				iat_entry,
				idd.szName,
				{0},
				0,
				0,
				0
	};

	/* Check the Delay Load Attributes, log an error of invalid
	parameter, which happens if the attributes in pidd are not
	specified correctly. */
	if ((idd.grAttrs & dlattrRva) == 0) {

		sql_print_error("InnoDB: invalid parameter for delay loader.");
		return(0);
	}

	hmod = *idd.phmod;

	/* Calculate the index for the IAT entry in the import address table.
	The INT entries are ordered the same as the IAT entries so the
	calculation can be done on the IAT side. */
	iIAT = (PCImgThunkData) iat_entry - idd.pIAT;
	iINT = iIAT;

	pitd = &(idd.pINT[iINT]);

	dli.dlp.fImportByName = !IMAGE_SNAP_BY_ORDINAL(pitd->u1.Ordinal);

	if (dli.dlp.fImportByName) {

		dli.dlp.szProcName = (LPCSTR) (PFromRva<PIMAGE_IMPORT_BY_NAME>
			((RVA) ((UINT_PTR) pitd->u1.AddressOfData))->Name);
	} else {

		dli.dlp.dwOrdinal = (ulint) IMAGE_ORDINAL(pitd->u1.Ordinal);
	}

	/* Now, load the mapfile, if it has not been done yet */
	if (hmod == 0) {

		hmod = wdl_get_mysqld_mapfile();
	}

	if (hmod == 0) {
		/* LoadLibrary failed. */
		PDelayLoadInfo	rgpdli[1] = {&dli};

		dli.dwLastError = ::GetLastError();

		sql_print_error(
			"InnoDB: failed to load mysqld.map with error %d.",
			dli.dwLastError);

		return(0);
	}

	/* Store the library handle. */
	idd.phmod = &hmod;

	/* Go for the procedure now. */
	dli.hmodCur = hmod;

	if (pidd->rvaBoundIAT && pidd->dwTimeStamp) {

		/* Bound imports exist, check the timestamp from the target
		image */
		PIMAGE_NT_HEADERS	pinh;

		pinh = (PIMAGE_NT_HEADERS) ((byte*) hmod
				+ ((PIMAGE_DOS_HEADER) hmod)->e_lfanew);

		if (pinh->Signature == IMAGE_NT_SIGNATURE
		    && pinh->FileHeader.TimeDateStamp == idd.dwTimeStamp
		    && (DWORD) hmod == pinh->OptionalHeader.ImageBase) {

			/* We have a decent address in the bound IAT. */
			fun = (FARPROC) (UINT_PTR)
					idd.pBoundIAT[iIAT].u1.Function;

			if (fun) {

				*iat_entry = fun;
				return(fun);
			}
		}
	}

	fun = wdl_get_procaddr_from_map(hmod, dli.dlp.szProcName);

	if (fun == 0) {

		return(0);
	}

	*iat_entry = fun;
	return(fun);
}

/*******************************************************************//**
Unload a DLL that was delay loaded. This function is called by run-time.
@return TRUE is returned if the DLL is found and the IAT matches the
original one. */
extern "C"
BOOL WINAPI
__FUnloadDelayLoadedDLL2(
/*=====================*/
	LPCSTR	module_name)	/*!< in: DLL name */
{
	return(TRUE);
}

/**************************************************************//**
Load all imports from a DLL that was specified with the /delayload linker
option.
Note: this function is called by run-time. So, it has to follow Windows call
convention.
@return	S_OK if the DLL matches, otherwise ERROR_MOD_NOT_FOUND is returned. */
extern "C"
HRESULT WINAPI
__HrLoadAllImportsForDll(
/*=====================*/
	LPCSTR	module_name)	/*!< in: DLL name */
{
	PIMAGE_NT_HEADERS	img;
	PCImgDelayDescr		pidd;
	IMAGE_DATA_DIRECTORY*	image_data;
	LPCSTR			current_module;
	HRESULT			ret = ERROR_MOD_NOT_FOUND;
	HMODULE			hmod = (HMODULE) &__ImageBase;

	img = (PIMAGE_NT_HEADERS) ((byte*) hmod
				   + ((PIMAGE_DOS_HEADER) hmod)->e_lfanew);
	image_data =
	 &img->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT];

	/* Scan the delay load IAT/INT for the DLL */
	if (image_data->Size) {

		pidd = PFromRva<PCImgDelayDescr>(image_data->VirtualAddress);

		/* Check all of the listed DLLs we want to load. */
		while (pidd->rvaDLLName) {

			current_module = PFromRva<LPCSTR>(pidd->rvaDLLName);

			if (stricmp(module_name, current_module) == 0) {

				/* Found it, break out with pidd and
				current_module set appropriately */
				break;
			}

			/* To the next delay import descriptor */
			pidd++;
		}

		if (pidd->rvaDLLName) {

			/* Found a matching DLL, now process it. */
			FARPROC*	iat_entry;
			size_t		count;

			iat_entry = PFromRva<FARPROC*>(pidd->rvaIAT);
			count = wdl_import_count((PCImgThunkData) iat_entry);

			/* now load all the imports from the DLL */
			while (count > 0) {

				/* No need to check the return value */
				__delayLoadHelper2(pidd, iat_entry);
				iat_entry++;
				count--;
			}

			ret = S_OK;
		}
	}

	return ret;
}

/**************************************************************//**
The main function of a DLL
@return	TRUE if the call succeeds */
BOOL
WINAPI
DllMain(
/*====*/
	HINSTANCE	hinstDLL,	/*!< in: handle to the DLL module */
	DWORD		fdwReason,	/*!< Reason code that indicates why the
					DLL entry-point function is being
					called.*/
	LPVOID		lpvReserved)	/*!< in: additional parameter based on
					fdwReason */
{
	BOOL	success = TRUE;

	switch (fdwReason) {

	case DLL_PROCESS_ATTACH:
		success = wdl_get_external_variables();
		break;

	case DLL_PROCESS_DETACH:
		wdl_cleanup();
		break;
	}

	return(success);
}

#ifndef DBUG_OFF
/**************************************************************//**
Process entry point to user function. It makes the call to _db_enter_
in mysqld.exe. The DBUG functions are defined in my_dbug.h. */
extern "C" UNIV_INTERN
void
_db_enter_(
	const char*	_func_,		/*!< in: current function name */
	const char*	_file_,		/*!< in: current file name */
	uint		_line_,		/*!< in: current source line number */
	const char**	_sfunc_,	/*!< out: previous _func_ */
	const char**	_sfile_,	/*!< out: previous _file_ */
	uint*		_slevel_,	/*!< out: previous nesting level */
	char***		_sframep_)	/*!< out: previous frame pointer */
{
	if (wdl_db_enter_ != NULL) {

		wdl_db_enter_(_func_, _file_, _line_, _sfunc_, _sfile_,
			      _slevel_, _sframep_);
	}
}

/**************************************************************//**
Process exit from user function. It makes the call to _db_return_()
in the server. */
extern "C" UNIV_INTERN
void
_db_return_(
	uint		_line_,		/*!< in: current source line number */
	const char**	_sfunc_,	/*!< out: previous _func_ */
	const char**	_sfile_,	/*!< out: previous _file_ */
	uint*		_slevel_)	/*!< out: previous level */
{
	if (wdl_db_return_ != NULL) {

		wdl_db_return_(_line_, _sfunc_, _sfile_, _slevel_);
	}
}

/**************************************************************//**
Log arguments for subsequent use. It makes the call to _db_pargs_()
in the server. */
extern "C" UNIV_INTERN
void
_db_pargs_(
	uint		_line_,		/*!< in: current source line number */
	const char*	keyword)	/*!< in: keyword for current macro */
{
	if (wdl_db_pargs_ != NULL) {

		wdl_db_pargs_(_line_, keyword);
	}
}

/**************************************************************//**
Handle print of debug lines. It saves the text into a buffer first,
then makes the call to _db_doprnt_() in the server. The text is
truncated to the size of buffer. */
extern "C" UNIV_INTERN
void
_db_doprnt_(
	const char*	format,		/*!< in: the format string */
	...)				/*!< in: list of arguments */
{
	va_list		argp;
	char		buffer[512];

	if (wdl_db_doprnt_ != NULL) {

		va_start(argp, format);
		/* it is ok to ignore the trunction. */
		_vsnprintf(buffer, sizeof(buffer), format, argp);
		wdl_db_doprnt_(buffer);
		va_end(argp);
	}
}

/**************************************************************//**
Dump a string in hex. It makes the call to _db_dump_() in the server. */
extern "C" UNIV_INTERN
void
_db_dump_(
	uint			_line_,		/*!< in: current source line
						number */
	const char*		keyword,	/*!< in: keyword list */
	const unsigned char*	memory,		/*!< in: memory to dump */
	size_t			length)		/*!< in: bytes to dump */
{
	if (wdl_db_dump_ != NULL) {

		wdl_db_dump_(_line_, keyword, memory, length);
	}
}

#endif /* !DBUG_OFF */
#endif /* defined (__WIN__) && defined (MYSQL_DYNAMIC_PLUGIN) */
