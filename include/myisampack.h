/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/*
  Storing of values in high byte first order.

  integer keys and file pointers are stored with high byte first to get
  better compression
*/

#define mi_sint2korr(A) (int16) (((int16) ((uchar) (A)[1])) +\
				 ((int16) ((int16) (A)[0]) << 8))
#define mi_sint3korr(A) ((int32) ((((uchar) (A)[0]) & 128) ? \
				  (((uint32) 255L << 24) | \
				   (((uint32) (uchar) (A)[0]) << 16) |\
				   (((uint32) (uchar) (A)[1]) << 8) | \
				   ((uint32) (uchar) (A)[2])) : \
				  (((uint32) (uchar) (A)[0]) << 16) |\
				  (((uint32) (uchar) (A)[1]) << 8) | \
				  ((uint32) (uchar) (A)[2])))
#define mi_sint4korr(A) (int32) (((int32) ((uchar) (A)[3])) +\
				(((int32) ((uchar) (A)[2]) << 8)) +\
				(((int32) ((uchar) (A)[1]) << 16)) +\
				(((int32) ((int16) (A)[0]) << 24)))
#define mi_sint8korr(A) (longlong) mi_uint8korr(A)
#define mi_uint2korr(A) (uint16) (((uint16) ((uchar) (A)[1])) +\
				  ((uint16) ((uchar) (A)[0]) << 8))
#define mi_uint3korr(A) (uint32) (((uint32) ((uchar) (A)[2])) +\
				  (((uint32) ((uchar) (A)[1])) << 8) +\
				  (((uint32) ((uchar) (A)[0])) << 16))
#define mi_uint4korr(A) (uint32) (((uint32) ((uchar) (A)[3])) +\
				  (((uint32) ((uchar) (A)[2])) << 8) +\
				  (((uint32) ((uchar) (A)[1])) << 16) +\
				  (((uint32) ((uchar) (A)[0])) << 24))
#define mi_uint5korr(A) ((ulonglong)(((uint32) ((uchar) (A)[4])) +\
				    (((uint32) ((uchar) (A)[3])) << 8) +\
				    (((uint32) ((uchar) (A)[2])) << 16) +\
				    (((uint32) ((uchar) (A)[1])) << 24)) +\
				    (((ulonglong) ((uchar) (A)[0])) << 32))
#define mi_uint6korr(A) ((ulonglong)(((uint32) ((uchar) (A)[5])) +\
				    (((uint32) ((uchar) (A)[4])) << 8) +\
				    (((uint32) ((uchar) (A)[3])) << 16) +\
				    (((uint32) ((uchar) (A)[2])) << 24)) +\
			(((ulonglong) (((uint32) ((uchar) (A)[1])) +\
				    (((uint32) ((uchar) (A)[0]) << 8)))) <<\
			 	    32))
#define mi_uint7korr(A) ((ulonglong)(((uint32) ((uchar) (A)[6])) +\
				    (((uint32) ((uchar) (A)[5])) << 8) +\
				    (((uint32) ((uchar) (A)[4])) << 16) +\
				    (((uint32) ((uchar) (A)[3])) << 24)) +\
			(((ulonglong) (((uint32) ((uchar) (A)[2])) +\
				    (((uint32) ((uchar) (A)[1])) << 8) +\
				    (((uint32) ((uchar) (A)[0])) << 16))) <<\
			 	    32))
#define mi_uint8korr(A) ((ulonglong)(((uint32) ((uchar) (A)[7])) +\
				    (((uint32) ((uchar) (A)[6])) << 8) +\
				    (((uint32) ((uchar) (A)[5])) << 16) +\
				    (((uint32) ((uchar) (A)[4])) << 24)) +\
			(((ulonglong) (((uint32) ((uchar) (A)[3])) +\
				    (((uint32) ((uchar) (A)[2])) << 8) +\
				    (((uint32) ((uchar) (A)[1])) << 16) +\
				    (((uint32) ((uchar) (A)[0])) << 24))) <<\
				    32))

#define mi_int2store(T,A)  { uint def_temp= (uint) (A) ;\
			     *((uchar*) ((T)+1))= (uchar)(def_temp); \
			     *((uchar*) ((T)+0))= (uchar)(def_temp >> 8); }
#define mi_int3store(T,A)  { /*lint -save -e734 */\
  			     ulong def_temp= (ulong) (A);\
			     *(((T)+2))=(char) (def_temp);\
			      *((T)+1)= (char) (def_temp >> 8);\
			      *((T)+0)= (char) (def_temp >> 16);\
			     /*lint -restore */}
#define mi_int4store(T,A)  { ulong def_temp= (ulong) (A);\
  			     *((T)+3)=(char) (def_temp);\
			     *((T)+2)=(char) (def_temp >> 8);\
			     *((T)+1)=(char) (def_temp >> 16);\
			     *((T)+0)=(char) (def_temp >> 24); }
#define mi_int5store(T,A)  { ulong def_temp= (ulong) (A),\
			     def_temp2= (ulong) ((A) >> 32);\
			     *((T)+4)=(char) (def_temp);\
			     *((T)+3)=(char) (def_temp >> 8);\
			     *((T)+2)=(char) (def_temp >> 16);\
			     *((T)+1)=(char) (def_temp >> 24);\
			     *((T)+0)=(char) (def_temp2); }
#define mi_int6store(T,A)  { ulong def_temp= (ulong) (A),\
			     def_temp2= (ulong) ((A) >> 32);\
			     *((T)+5)=(char) (def_temp);\
			     *((T)+4)=(char) (def_temp >> 8);\
			     *((T)+3)=(char) (def_temp >> 16);\
			     *((T)+2)=(char) (def_temp >> 24);\
			     *((T)+1)=(char) (def_temp2);\
			     *((T)+0)=(char) (def_temp2 >> 8); }
#define mi_int7store(T,A)  { ulong def_temp= (ulong) (A),\
			     def_temp2= (ulong) ((A) >> 32);\
			     *((T)+6)=(char) (def_temp);\
			     *((T)+5)=(char) (def_temp >> 8);\
			     *((T)+4)=(char) (def_temp >> 16);\
			     *((T)+3)=(char) (def_temp >> 24);\
			     *((T)+2)=(char) (def_temp2);\
			     *((T)+1)=(char) (def_temp2 >> 8);\
			     *((T)+0)=(char) (def_temp2 >> 16); }
#define mi_int8store(T,A)    { ulong def_temp3= (ulong) (A), \
			       def_temp4= (ulong) ((A) >> 32); \
			       mi_int4store((T),def_temp4); \
			       mi_int4store((T+4),def_temp3); \
			     }

#ifdef WORDS_BIGENDIAN

#define mi_float4store(T,A)  { *(T)= ((byte *) &A)[0];\
			      *((T)+1)=(char) ((byte *) &A)[1];\
			      *((T)+2)=(char) ((byte *) &A)[2];\
			      *((T)+3)=(char) ((byte *) &A)[3]; }

#define mi_float4get(V,M)   { float def_temp;\
			      ((byte*) &def_temp)[0]=(M)[0];\
			      ((byte*) &def_temp)[1]=(M)[1];\
			      ((byte*) &def_temp)[2]=(M)[2];\
			      ((byte*) &def_temp)[3]=(M)[3];\
			      (V)=def_temp; }

#define mi_float8store(T,V) { *(T)= ((byte *) &V)[0];\
			      *((T)+1)=(char) ((byte *) &V)[1];\
			      *((T)+2)=(char) ((byte *) &V)[2];\
			      *((T)+3)=(char) ((byte *) &V)[3];\
			      *((T)+4)=(char) ((byte *) &V)[4];\
			      *((T)+5)=(char) ((byte *) &V)[5];\
			      *((T)+6)=(char) ((byte *) &V)[6];\
			      *((T)+7)=(char) ((byte *) &V)[7]; }

#define mi_float8get(V,M)   { double def_temp;\
			      ((byte*) &def_temp)[0]=(M)[0];\
			      ((byte*) &def_temp)[1]=(M)[1];\
			      ((byte*) &def_temp)[2]=(M)[2];\
			      ((byte*) &def_temp)[3]=(M)[3];\
			      ((byte*) &def_temp)[4]=(M)[4];\
			      ((byte*) &def_temp)[5]=(M)[5];\
			      ((byte*) &def_temp)[6]=(M)[6];\
			      ((byte*) &def_temp)[7]=(M)[7]; \
			      (V)=def_temp; }
#else

#define mi_float4store(T,A)  { *(T)= ((byte *) &A)[3];\
			       *((T)+1)=(char) ((byte *) &A)[2];\
			       *((T)+2)=(char) ((byte *) &A)[1];\
			       *((T)+3)=(char) ((byte *) &A)[0]; }

#define mi_float4get(V,M)   { float def_temp;\
			      ((byte*) &def_temp)[0]=(M)[3];\
			      ((byte*) &def_temp)[1]=(M)[2];\
			      ((byte*) &def_temp)[2]=(M)[1];\
			      ((byte*) &def_temp)[3]=(M)[0];\
			      (V)=def_temp; }

#if defined(__FLOAT_WORD_ORDER) && (__FLOAT_WORD_ORDER == __BIG_ENDIAN)
#define mi_float8store(T,V) { *(T)= ((byte *) &V)[3];\
			      *((T)+1)=(char) ((byte *) &V)[2];\
			      *((T)+2)=(char) ((byte *) &V)[1];\
			      *((T)+3)=(char) ((byte *) &V)[0];\
			      *((T)+4)=(char) ((byte *) &V)[7];\
			      *((T)+5)=(char) ((byte *) &V)[6];\
			      *((T)+6)=(char) ((byte *) &V)[5];\
			      *((T)+7)=(char) ((byte *) &V)[4];}

#define mi_float8get(V,M)   { double def_temp;\
			      ((byte*) &def_temp)[0]=(M)[3];\
			      ((byte*) &def_temp)[1]=(M)[2];\
			      ((byte*) &def_temp)[2]=(M)[1];\
			      ((byte*) &def_temp)[3]=(M)[0];\
			      ((byte*) &def_temp)[4]=(M)[7];\
			      ((byte*) &def_temp)[5]=(M)[6];\
			      ((byte*) &def_temp)[6]=(M)[5];\
			      ((byte*) &def_temp)[7]=(M)[4];\
			      (V)=def_temp; }

#else
#define mi_float8store(T,V) { *(T)= ((byte *) &V)[7];\
			      *((T)+1)=(char) ((byte *) &V)[6];\
			      *((T)+2)=(char) ((byte *) &V)[5];\
			      *((T)+3)=(char) ((byte *) &V)[4];\
			      *((T)+4)=(char) ((byte *) &V)[3];\
			      *((T)+5)=(char) ((byte *) &V)[2];\
			      *((T)+6)=(char) ((byte *) &V)[1];\
			      *((T)+7)=(char) ((byte *) &V)[0];}

#define mi_float8get(V,M)   { double def_temp;\
			      ((byte*) &def_temp)[0]=(M)[7];\
			      ((byte*) &def_temp)[1]=(M)[6];\
			      ((byte*) &def_temp)[2]=(M)[5];\
			      ((byte*) &def_temp)[3]=(M)[4];\
			      ((byte*) &def_temp)[4]=(M)[3];\
			      ((byte*) &def_temp)[5]=(M)[2];\
			      ((byte*) &def_temp)[6]=(M)[1];\
			      ((byte*) &def_temp)[7]=(M)[0];\
			      (V)=def_temp; }
#endif /* __FLOAT_WORD_ORDER */
#endif /* WORDS_BIGENDIAN */

/* Fix to avoid warnings when sizeof(ha_rows) == sizeof(long) */

#ifdef BIG_TABLE
#define mi_rowstore(T,A)    mi_int8store(T,A)
#define mi_rowkorr(T,A)     mi_uint8korr(T)
#else
#define mi_rowstore(T,A)    { mi_int4store(T,0); mi_int4store(((T)+4),A); }
#define mi_rowkorr(T)	    mi_uint4korr((T)+4)
#endif

#if SIZEOF_OFF_T > 4
#define mi_sizestore(T,A)    mi_int8store(T,A)
#define mi_sizekorr(T)	     mi_uint8korr(T)
#else
#define mi_sizestore(T,A)    { if ((A) == HA_OFFSET_ERROR) bfill((char*) (T),8,255);  else { mi_int4store((T),0); mi_int4store(((T)+4),A); }}
#define mi_sizekorr(T)	  mi_uint4korr((T)+4)
#endif
