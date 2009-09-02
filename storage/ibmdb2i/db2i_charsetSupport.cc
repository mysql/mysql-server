/*
Licensed Materials - Property of IBM
DB2 Storage Engine Enablement
Copyright IBM Corporation 2007,2008
All rights reserved

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met: 
 (a) Redistributions of source code must retain this list of conditions, the
     copyright notice in section {d} below, and the disclaimer following this
     list of conditions. 
 (b) Redistributions in binary form must reproduce this list of conditions, the
     copyright notice in section (d) below, and the disclaimer following this
     list of conditions, in the documentation and/or other materials provided
     with the distribution. 
 (c) The name of IBM may not be used to endorse or promote products derived from
     this software without specific prior written permission. 
 (d) The text of the required copyright notice is: 
       Licensed Materials - Property of IBM
       DB2 Storage Engine Enablement 
       Copyright IBM Corporation 2007,2008 
       All rights reserved

THIS SOFTWARE IS PROVIDED BY IBM CORPORATION "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
SHALL IBM CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/



#include "db2i_charsetSupport.h"
#include "as400_types.h"
#include "as400_protos.h"
#include "db2i_ileBridge.h"
#include "qlgusr.h"
#include "db2i_errors.h"


/*
  The following arrays define a mapping between IANA-style text descriptors and
  IBM i CCSID text descriptors. The mapping is a 1-to-1 correlation between
  corresponding array slots.
*/
#define MAX_IANASTRING 23
static const char ianaStringType[MAX_IANASTRING][10] = 
{
    {"ascii"},
    {"Big5"},    //big5
    {"cp1250"},
    {"cp1251"},
    {"cp1256"},
    {"cp850"},
    {"cp852"},
    {"cp866"},
    {"IBM943"},   //cp932
    {"EUC-KR"},   //euckr
    {"IBM1381"},  //gb2312
    {"IBM1386"},  //gbk
    {"greek"},
    {"hebrew"},
    {"latin1"},
    {"latin2"},
    {"latin5"},
    {"macce"},
    {"tis620"},
    {"Shift_JIS"}, //sjis
    {"ucs2"},
    {"EUC-JP"},    //ujis
    {"utf8"}
};
static const char ccsidType[MAX_IANASTRING][6] = 
{
    {"367"},  //ascii
    {"950"},  //big5
    {"1250"}, //cp1250
    {"1251"}, //cp1251
    {"1256"}, //cp1256
    {"850"},  //cp850
    {"852"},  //cp852
    {"866"},  //cp866
    {"943"},  //cp932
    {"970"},  //euckr
    {"1381"}, //gb2312
    {"1386"}, //gbk
    {"813"},  //greek
    {"916"},  //hebrew
    {"923"},  //latin1
    {"912"},  //latin2
    {"920"},  //latin5
    {"1282"}, //macce
    {"874"},  //tis620
    {"943"},  //sjis
    {"13488"},//ucs2
    {"5050"}, //ujis
    {"1208"}  //utf8
};

static _ILEpointer *QlgCvtTextDescToDesc_sym;

/* We keep a cache of the mapping for text descriptions obtained via
   QlgTextDescToDesc. The following structures implement this cache. */
static HASH textDescMapHash;
static MEM_ROOT textDescMapMemroot;
static pthread_mutex_t textDescMapHashMutex;
struct TextDescMap
{
  struct HashKey
  {
    int32 inType;
    int32 outType;
    char inDesc[Qlg_MaxDescSize];
  } hashKey;  
  char outDesc[Qlg_MaxDescSize];
};

/* We keep a cache of the mapping for open iconv descriptors. The following 
   structures implement this cache. */
static HASH iconvMapHash;
static MEM_ROOT iconvMapMemroot;
static pthread_mutex_t iconvMapHashMutex;
struct IconvMap
{
  struct HashKey
  {
    uint32 direction; // These are uint32s to avoid garbage data in the key from compiler padding
    uint32 db2CCSID;
    const CHARSET_INFO* myCharset;
  } hashKey;
  iconv_t iconvDesc;
};


/**
  Initialize the static structures used by this module.
  
  This must only be called once per plugin instantiation.
  
  @return  0 if successful. Failure otherwise
*/
int32 initCharsetSupport()
{
  DBUG_ENTER("initCharsetSupport");

  int actmark = _ILELOAD("QSYS/QLGUSR", ILELOAD_LIBOBJ);
  if ( actmark == -1 )
  {
    DBUG_PRINT("initCharsetSupport", ("conversion srvpgm activation failed"));
    DBUG_RETURN(1);
  }

  QlgCvtTextDescToDesc_sym = (ILEpointer*)malloc_aligned(sizeof(ILEpointer));
  if (_ILESYM(QlgCvtTextDescToDesc_sym, actmark, "QlgCvtTextDescToDesc") == -1)
  {
    DBUG_PRINT("initCharsetSupport", 
        ("resolve of QlgCvtTextDescToDesc failed"));
    DBUG_RETURN(errno);
  }

  VOID(pthread_mutex_init(&textDescMapHashMutex,MY_MUTEX_INIT_FAST));
  hash_init(&textDescMapHash, &my_charset_bin, 10, offsetof(TextDescMap, hashKey), sizeof(TextDescMap::hashKey), 0, 0, HASH_UNIQUE);

  VOID(pthread_mutex_init(&iconvMapHashMutex,MY_MUTEX_INIT_FAST));
  hash_init(&iconvMapHash, &my_charset_bin, 10, offsetof(IconvMap, hashKey), sizeof(IconvMap::hashKey), 0, 0, HASH_UNIQUE);

  init_alloc_root(&textDescMapMemroot, 2048, 0);
  init_alloc_root(&iconvMapMemroot, 256, 0);

  initMyconv();
  
  DBUG_RETURN(0);
}

/**
  Cleanup the static structures used by this module.
  
  This must only be called once per plugin instantiation and only if 
  initCharsetSupport() was successful.
*/
void doneCharsetSupport()
{
  cleanupMyconv();
    
  free_root(&textDescMapMemroot, 0);
  free_root(&iconvMapMemroot, 0);
  
  pthread_mutex_destroy(&textDescMapHashMutex);
  hash_free(&textDescMapHash);
  pthread_mutex_destroy(&iconvMapHashMutex);
  hash_free(&iconvMapHash);
  free_aligned(QlgCvtTextDescToDesc_sym);
}


/**
  Convert a text description from one type to another.
  
  This function is just a wrapper for the IBM i QlgTextDescToDesc function plus
  some overrides for conversions that the API does not handle correctly and 
  support for caching the computed conversion.
    
  @param inType  The type of descriptor pointed to by "in".
  @param outType  The type of descriptor requested for "out".
  @param in  The descriptor to be convereted.
  @param[out] out  The equivalent descriptor
  @param hashKey  The hash key to be used for caching the conversion result.
    
  @return  0 if successful. Failure otherwise
*/
static int32 getNewTextDesc(const int32 inType, 
                            const int32 outType, 
                            const char* in, 
                            char* out,
                            const TextDescMap::HashKey* hashKey)
{
  DBUG_ENTER("db2i_charsetSupport::getNewTextDesc");
  const arg_type_t signature[] = { ARG_INT32, ARG_INT32, ARG_MEMPTR, ARG_INT32, ARG_MEMPTR, ARG_INT32, ARG_INT32, ARG_END };
  struct ArgList
  {
    ILEarglist_base base;
    int32 CRDIInType;
    int32 CRDIOutType;
    ILEpointer CRDIDesc;
    int32 CRDIDescSize;
    ILEpointer CRDODesc;
    int32 CRDODescSize;
    int32 CTDCCSID;
  } *arguments;

  if ((inType == Qlg_TypeIANA) && (outType == Qlg_TypeAix41))
  {
    // Override non-standard charsets
    if (unlikely(strcmp("IBM1381", in) == 0))
    {
      strcpy(out, "IBM-1381");
      DBUG_RETURN(0);
    }
  }
  else if ((inType == Qlg_TypeAS400CCSID) && (outType == Qlg_TypeAix41))
  {
    // Override non-standard charsets
    if (strcmp("1148", in) == 0)
    {
      strcpy(out, "IBM-1148");
      DBUG_RETURN(0);
    }
    else if (unlikely(strcmp("1153", in) == 0))
    {
      strcpy(out, "IBM-1153");
      DBUG_RETURN(0);
    }
  }

  char argBuf[sizeof(ArgList)+15];
  arguments = (ArgList*)roundToQuadWordBdy(argBuf);

  arguments->CRDIInType = inType;
  arguments->CRDIOutType = outType;
  arguments->CRDIDesc.s.addr = (address64_t) in;
  arguments->CRDIDescSize = Qlg_MaxDescSize;
  arguments->CRDODesc.s.addr = (address64_t) out;
  arguments->CRDODescSize = Qlg_MaxDescSize;
  arguments->CTDCCSID = 819;
  _ILECALL(QlgCvtTextDescToDesc_sym, 
      &arguments->base,
      signature, 
      RESULT_INT32);
  if (unlikely(arguments->base.result.s_int32.r_int32 < 0))
  {
    if (arguments->base.result.s_int32.r_int32 == Qlg_InDescriptorNotFound)
    {
      DBUG_RETURN(DB2I_ERR_UNSUPP_CHARSET);
    }
    else
    {
      getErrTxt(DB2I_ERR_ILECALL,"QlgCvtTextDescToDesc",arguments->base.result.s_int32.r_int32);
      DBUG_RETURN(DB2I_ERR_ILECALL);
    }
  }
  
  // Store the conversion information into a cache entry
  TextDescMap* mapping = (TextDescMap*)alloc_root(&textDescMapMemroot, sizeof(TextDescMap));
  if (unlikely(!mapping))
    DBUG_RETURN(HA_ERR_OUT_OF_MEM);
  memcpy(&(mapping->hashKey), hashKey, sizeof(hashKey));
  strcpy(mapping->outDesc, out);
  pthread_mutex_lock(&textDescMapHashMutex);
  my_hash_insert(&textDescMapHash, (const uchar*)mapping);
  pthread_mutex_unlock(&textDescMapHashMutex);
  
  DBUG_RETURN(0);
}


/**
  Convert a text description from one type to another.
  
  This function takes a text description in one representation and converts 
  it into another representation. Although the OS provides some facilities for
  doing this, the support is not complete, nor does MySQL always use standard
  identifiers. Therefore, there are a lot of hardcoded overrides required.
  There is probably some room for optimization here, but this should not be
  called frequently under most circumstances.

  @param inType  The type of descriptor pointed to by "in".
  @param outType  The type of descriptor requested for "out".
  @param in  The descriptor to be convereted.
  @param[out] out  The equivalent descriptor
    
  @return  0 if successful. Failure otherwise
*/
static int32 convertTextDesc(const int32 inType, const int32 outType, const char* inDesc, char* outDesc)
{
  DBUG_ENTER("db2i_charsetSupport::convertTextDesc");
  const char* inDescOverride;
  
  if (inType == Qlg_TypeIANA) 
  {
    // Override non-standard charsets
    if (strcmp("big5", inDesc) == 0)
      inDescOverride = "Big5";
    else if (strcmp("cp932", inDesc) == 0)
      inDescOverride = "IBM943";
    else if (strcmp("euckr", inDesc) == 0)
      inDescOverride = "EUC-KR";
    else if (strcmp("gb2312", inDesc) == 0)
      inDescOverride = "IBM1381";
    else if (strcmp("gbk", inDesc) == 0)
      inDescOverride = "IBM1386";
    else if (strcmp("sjis", inDesc) == 0)
      inDescOverride = "Shift_JIS";
    else if (strcmp("ujis", inDesc) == 0)
      inDescOverride = "EUC-JP";
    else
      inDescOverride = inDesc;
     
    // Hardcode non-standard charsets
    if (outType == Qlg_TypeAix41) 
    {
      if (strcmp("Big5", inDescOverride) == 0)
      {
        strcpy(outDesc,"big5");
        DBUG_RETURN(0);
      }
      else if (strcmp("IBM1386", inDescOverride) == 0)
      {
        strcpy(outDesc,"GBK");
        DBUG_RETURN(0);
      }
      else if (strcmp("Shift_JIS", inDescOverride) == 0 ||
               strcmp("IBM943", inDescOverride) == 0)
      {
        strcpy(outDesc,"IBM-943");
        DBUG_RETURN(0);
      }
      else if (strcmp("tis620", inDescOverride) == 0)
      {
        strcpy(outDesc,"TIS-620");
        DBUG_RETURN(0);
      }
      else if (strcmp("ucs2", inDescOverride) == 0)
      {
        strcpy(outDesc,"UCS-2");
        DBUG_RETURN(0);
      }
      else if (strcmp("cp1250", inDescOverride) == 0)
      {
        strcpy(outDesc,"IBM-1250");
        DBUG_RETURN(0);
      }
      else if (strcmp("cp1251", inDescOverride) == 0)
      {
        strcpy(outDesc,"IBM-1251");
        DBUG_RETURN(0);
      }
      else if (strcmp("cp1256", inDescOverride) == 0)
      {
        strcpy(outDesc,"IBM-1256");
        DBUG_RETURN(0);
      }
      else if (strcmp("macce", inDescOverride) == 0)
      {
        strcpy(outDesc,"IBM-1282");
        DBUG_RETURN(0);
      }
    }
    else if (outType == Qlg_TypeAS400CCSID)
    {
      // See if we can fast path the convert
      for (int loopCnt = 0; loopCnt < MAX_IANASTRING; ++loopCnt)
      {
        if (strcmp((char*)ianaStringType[loopCnt],inDescOverride) == 0)
        {
          strcpy(outDesc,ccsidType[loopCnt]);
          DBUG_RETURN(0);
        }
      }
    }
  }
  else 
    inDescOverride = inDesc;

  // We call getNewTextDesc for all other conversions and cache the result.  
  TextDescMap *mapping;
  TextDescMap::HashKey hashKey;
  hashKey.inType= inType;
  hashKey.outType= outType;
  uint32 len = strlen(inDescOverride);
  memcpy(hashKey.inDesc, inDescOverride, len);
  memset(hashKey.inDesc+len, 0, sizeof(hashKey.inDesc) - len);
  
  if (!(mapping=(TextDescMap *) hash_search(&textDescMapHash,
                                           (const uchar*)&hashKey,
                                           sizeof(hashKey))))
  {
    DBUG_RETURN(getNewTextDesc(inType, outType, inDescOverride, outDesc, &hashKey));
  }
  else
  {
    strcpy(outDesc, mapping->outDesc);
  }
  DBUG_RETURN(0);
}


/**
  Convert an IANA character set name into a DB2 for i CCSID value.
    
  @param parmIANADesc  An IANA character set name
  @param[out] db2Ccsid  The equivalent CCSID value
    
  @return  0 if successful. Failure otherwise
*/
int32 convertIANAToDb2Ccsid(const char* parmIANADesc, uint16* db2Ccsid)
{
  int32 rc; 
  uint16 aixCcsid;
  char aixCcsidString[Qlg_MaxDescSize];
  int aixEncodingScheme;
  int db2EncodingScheme;
  rc = convertTextDesc(Qlg_TypeIANA, Qlg_TypeAS400CCSID, parmIANADesc, aixCcsidString);
  if (unlikely(rc))
  {
    if (rc == DB2I_ERR_UNSUPP_CHARSET)
      getErrTxt(DB2I_ERR_UNSUPP_CHARSET, parmIANADesc);
    
    return rc;
  }
  aixCcsid = atoi(aixCcsidString);
  rc = getEncodingScheme(aixCcsid, aixEncodingScheme);     
  if (rc != 0) 
    return rc;                   
  switch(aixEncodingScheme) { // Select on encoding scheme     
    case 0x1100: // EDCDIC SBCS                   
    case 0x2100: // ASCII SBCS                    
    case 0x4100: // AIX SBCS                      
    case 0x4105: // MS Windows                    
    case 0x5100: // ISO 7 bit ASCII               
      db2EncodingScheme = 0x1100;
      break;
    case 0x1200: // EDCDIC DBCS                   
    case 0x2200: // ASCII DBCS                    
      db2EncodingScheme = 0x1200;
      break;
    case 0x1301: // EDCDIC Mixed                  
    case 0x2300: // ASCII Mixed                   
    case 0x4403: // EUC (ISO 2022)                
      db2EncodingScheme = 0x1301;
      break;
    case 0x7200: // UCS2                          
      db2EncodingScheme = 0x7200;
      break;
    case 0x7807: // UTF-8                         
      db2EncodingScheme = 0x7807;
      break;
    case 0x7500: // UTF-32                        
      db2EncodingScheme = 0x7500;
      break;
    default: // Unknown                       
      {
         getErrTxt(DB2I_ERR_UNKNOWN_ENCODING,aixEncodingScheme);
         return DB2I_ERR_UNKNOWN_ENCODING;
      }
      break;
  }
  if (aixEncodingScheme == db2EncodingScheme) 
  {
    *db2Ccsid = aixCcsid;
  } 
  else 
  {
    rc = getAssociatedCCSID(aixCcsid, db2EncodingScheme, db2Ccsid); // EDCDIC SBCS
    if (rc != 0)
      return rc;
  }
  
  return 0;
}


/**
  Obtain the encoding scheme of a CCSID.
    
  @param inCcsid  An IBM i CCSID
  @param[out] outEncodingScheme  The associated encoding scheme
    
  @return  0 if successful. Failure otherwise
*/
int32 getEncodingScheme(const uint16 inCcsid, int32& outEncodingScheme)
{
  DBUG_ENTER("db2i_charsetSupport::getEncodingScheme");
  
  static bool ptrInited = FALSE;
  static char ptrSpace[sizeof(ILEpointer) + 15];
  static ILEpointer* ptrToPtr = (ILEpointer*)roundToQuadWordBdy(ptrSpace);
  int rc;
    
  if (!ptrInited)
  {  
    rc = _RSLOBJ2(ptrToPtr, RSLOBJ_TS_PGM, "QTQGESP", "QSYS");

    if (rc)
    {
      getErrTxt(DB2I_ERR_RESOLVE_OBJ,"QTQGESP","QSYS","*PGM",errno);
      DBUG_RETURN(DB2I_ERR_RESOLVE_OBJ);
    }
    ptrInited = TRUE;
  }
  
  DBUG_ASSERT(inCcsid != 0);
  
  int GESPCCSID = inCcsid;
  int GESPLen = 32;
  int GESPNbrVal = 0;
  int32 GESPES;
  int GESPCSCPL[32];
  int GESPFB[3];
  void* ILEArgv[7];
  ILEArgv[0] = &GESPCCSID;
  ILEArgv[1] = &GESPLen;
  ILEArgv[2] = &GESPNbrVal;
  ILEArgv[3] = &GESPES;
  ILEArgv[4] = &GESPCSCPL;
  ILEArgv[5] = &GESPFB;
  ILEArgv[6] = NULL;
  
  rc = _PGMCALL(ptrToPtr, (void**)&ILEArgv, 0);
  
  if (rc)
  {
     getErrTxt(DB2I_ERR_PGMCALL,"QTQGESP","QSYS",rc);
     DBUG_RETURN(DB2I_ERR_PGMCALL);
  }
  if (GESPFB[0] != 0 ||
      GESPFB[1] != 0 ||
      GESPFB[2] != 0) 
  {
    getErrTxt(DB2I_ERR_QTQGESP,GESPFB[0],GESPFB[1],GESPFB[2]);
    DBUG_RETURN(DB2I_ERR_QTQGESP);
  }
  outEncodingScheme = GESPES;
  
  DBUG_RETURN(0);
}


/**
  Get the best fit equivalent CCSID. (Wrapper for QTQGRDC API)
    
  @param inCcsid  An IBM i CCSID
  @param inEncodingScheme  The encoding scheme
  @param[out] outCcsid  The equivalent CCSID
    
  @return  0 if successful. Failure otherwise
*/
int32 getAssociatedCCSID(const uint16 inCcsid, const int inEncodingScheme, uint16* outCcsid)
{
  DBUG_ENTER("db2i_charsetSupport::getAssociatedCCSID");
  static bool ptrInited = FALSE;
  static char ptrSpace[sizeof(ILEpointer) + 15];
  static ILEpointer* ptrToPtr = (ILEpointer*)roundToQuadWordBdy(ptrSpace);
  int rc;
  
  // Override non-standard charsets
  if ((inCcsid == 923) && (inEncodingScheme == 0x1100))
  {
    *outCcsid = 1148;
    DBUG_RETURN(0);
  }
  else if ((inCcsid == 1250) && (inEncodingScheme == 0x1100))
  {
    *outCcsid = 1153;
    DBUG_RETURN(0);
  }

  if (!ptrInited)
  {  
    rc = _RSLOBJ2(ptrToPtr, RSLOBJ_TS_PGM, "QTQGRDC", "QSYS");

    if (rc)
    {
       getErrTxt(DB2I_ERR_RESOLVE_OBJ,"QTQGRDC","QSYS","*PGM",errno);
       DBUG_RETURN(DB2I_ERR_RESOLVE_OBJ);
    }
    ptrInited = TRUE;
  }

  int GRDCCCSID = inCcsid;
  int GRDCES = inEncodingScheme;
  int GRDCSel = 0;
  int GRDCAssCCSID;
  int GRDCFB[3];
  void* ILEArgv[7];
  ILEArgv[0] = &GRDCCCSID;
  ILEArgv[1] = &GRDCES;
  ILEArgv[2] = &GRDCSel;
  ILEArgv[3] = &GRDCAssCCSID;
  ILEArgv[4] = &GRDCFB;
  ILEArgv[5] = NULL;
  
  rc = _PGMCALL(ptrToPtr, (void**)&ILEArgv, 0);

  if (rc)  
  {
     getErrTxt(DB2I_ERR_PGMCALL,"QTQGRDC","QSYS",rc);
     DBUG_RETURN(DB2I_ERR_PGMCALL);
  }
  if (GRDCFB[0] != 0 ||
      GRDCFB[1] != 0 ||
      GRDCFB[2] != 0)
  {
    getErrTxt(DB2I_ERR_QTQGRDC,GRDCFB[0],GRDCFB[1],GRDCFB[2]);
    DBUG_RETURN(DB2I_ERR_QTQGRDC);
  }
 
  *outCcsid = GRDCAssCCSID;

  DBUG_RETURN(0);
}

/**
  Open an iconv conversion between a MySQL charset and the respective IBM i CCSID
    
  @param direction  The direction of the conversion
  @param mysqlCSName  Name of the MySQL character set
  @param db2CCSID  The IBM i CCSID
  @param hashKey  The key to use for inserting the opened conversion into the cache
  @param[out] newConversion  The iconv descriptor
    
  @return  0 if successful. Failure otherwise
*/
static int32 openNewConversion(enum_conversionDirection direction, 
                               const char* mysqlCSName, 
                               uint16 db2CCSID, 
                               IconvMap::HashKey* hashKey, 
                               iconv_t& newConversion)
{
  DBUG_ENTER("db2i_charsetSupport::openNewConversion");
  
  char mysqlAix41Desc[Qlg_MaxDescSize];
  char db2Aix41Desc[Qlg_MaxDescSize];
  char db2CcsidString[6] = "";
  int32 rc;

  /*
     First we have to convert the MySQL IANA-like name and the DB2 CCSID into
     there equivalent iconv descriptions.
  */
  rc = convertTextDesc(Qlg_TypeIANA, Qlg_TypeAix41, mysqlCSName, mysqlAix41Desc);
  if (unlikely(rc))
  {
    if (rc == DB2I_ERR_UNSUPP_CHARSET)
      getErrTxt(DB2I_ERR_UNSUPP_CHARSET, mysqlCSName);
    
    DBUG_RETURN(rc);
  }
  CHARSET_INFO *cs= &my_charset_bin;
  (uint)(cs->cset->long10_to_str)(cs,db2CcsidString,sizeof(db2CcsidString), 10, db2CCSID);  
  rc = convertTextDesc(Qlg_TypeAS400CCSID, Qlg_TypeAix41, db2CcsidString, db2Aix41Desc);
  if (unlikely(rc))
  {
    if (rc == DB2I_ERR_UNSUPP_CHARSET)
      getErrTxt(DB2I_ERR_UNSUPP_CHARSET, mysqlCSName);
    
    DBUG_RETURN(rc);
  }
  
  /* Call iconv to open the conversion. */
  if (direction == toDB2)
  {
    newConversion = iconv_open(db2Aix41Desc, mysqlAix41Desc);
  }
  else
  {
    newConversion = iconv_open(mysqlAix41Desc, db2Aix41Desc);
  }

  if (unlikely(newConversion == (iconv_t) -1))
  {
    getErrTxt(DB2I_ERR_UNSUPP_CHARSET, mysqlCSName);
    DBUG_RETURN(DB2I_ERR_UNSUPP_CHARSET);
  }
 
  /* Insert the new conversion into the cache. */
  IconvMap* mapping = (IconvMap*)alloc_root(&iconvMapMemroot, sizeof(IconvMap));
  if (!mapping)
  {
    my_error(ER_OUTOFMEMORY, MYF(0), sizeof(IconvMap));
    DBUG_RETURN( HA_ERR_OUT_OF_MEM);
  }
  memcpy(&(mapping->hashKey), hashKey, sizeof(mapping->hashKey));
  mapping->iconvDesc = newConversion;
  pthread_mutex_lock(&iconvMapHashMutex);
  my_hash_insert(&iconvMapHash, (const uchar*)mapping);
  pthread_mutex_unlock(&iconvMapHashMutex);
  
  DBUG_RETURN(0);
}


/**
  Open an iconv conversion between a MySQL charset and the respective IBM i CCSID
    
  @param direction  The direction of the conversion
  @param cs  The MySQL character set
  @param db2CCSID  The IBM i CCSID
  @param[out] newConversion  The iconv descriptor
    
  @return  0 if successful. Failure otherwise
*/
int32 getConversion(enum_conversionDirection direction, const CHARSET_INFO* cs, uint16 db2CCSID, iconv_t& conversion)
{
  DBUG_ENTER("db2i_charsetSupport::getConversion");
  
  int32 rc;

  /* Build the hash key */  
  IconvMap::HashKey hashKey;
  hashKey.direction= direction;
  hashKey.myCharset= cs;
  hashKey.db2CCSID= db2CCSID;
  
  /* Look for the conversion in the cache and add it if it is not there. */
  IconvMap *mapping;
  if (!(mapping= (IconvMap *) hash_search(&iconvMapHash,
                                         (const uchar*)&hashKey,
                                         sizeof(hashKey))))
  {
    DBUG_PRINT("getConversion", ("Hash miss for direction=%d, cs=%s, ccsid=%d", direction, cs->name, db2CCSID));
    rc= openNewConversion(direction, cs->csname, db2CCSID, &hashKey, conversion);
    if (rc)
      DBUG_RETURN(rc);
  }
  else
  {
    conversion= mapping->iconvDesc;
  }

  DBUG_RETURN(0);
}

/**
  Fast-path conversion from ASCII to EBCDIC for use in converting
  identifiers to be sent to the QMY APIs.
  
  @param input  ASCII data
  @param[out] ouput  EBCDIC data
  @param ilen  Size of input buffer and output buffer
*/
int convToEbcdic(const char* input, char* output, size_t ilen)
{
  static bool inited = FALSE;
  static iconv_t ic;
  
  if (ilen == 0)
    return 0;
  
  if (!inited)
  {
    ic = iconv_open( "IBM-037", "ISO8859-1" );
    inited = TRUE;
  }
  size_t substitutedChars;
  size_t olen = ilen;
  if (iconv( ic, (char**)&input, &ilen, &output, &olen, &substitutedChars ) == -1)
    return errno;
  
  return 0;
}


/**
  Fast-path conversion from EBCDIC to ASCII for use in converting
  data received from the QMY APIs.
  
  @param input  EBCDIC data
  @param[out] ouput  ASCII data
  @param ilen  Size of input buffer and output buffer
*/
int convFromEbcdic(const char* input, char* output, size_t ilen)
{
  static bool inited = FALSE;
  static iconv_t ic;
  
  if (ilen == 0)
    return 0;

  if (!inited)
  {
    ic = iconv_open("ISO8859-1", "IBM-037");
    inited = TRUE;
  }
  
  size_t substitutedChars;
  size_t olen = ilen;
  if (iconv( ic, (char**)&input, &ilen, &output, &olen, &substitutedChars) == -1)
    return errno;
  
  return 0;
}
