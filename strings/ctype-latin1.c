/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <my_global.h>
#include "m_string.h"
#include "m_ctype.h"


static uint16 latin1_uni[256]={
     0,0x0001,0x0002,0x0003,0x0004,0x0005,0x0006,0x0007,
0x0008,0x0009,0x000A,0x000B,0x000C,0x000D,0x000E,0x000F,
0x0010,0x0011,0x0012,0x0013,0x0014,0x0015,0x0016,0x0017,
0x0018,0x0019,0x001A,0x001B,0x001C,0x001D,0x001E,0x001F,
0x0020,0x0021,0x0022,0x0023,0x0024,0x0025,0x0026,0x0027,
0x0028,0x0029,0x002A,0x002B,0x002C,0x002D,0x002E,0x002F,
0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037,
0x0038,0x0039,0x003A,0x003B,0x003C,0x003D,0x003E,0x003F,
0x0040,0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047,
0x0048,0x0049,0x004A,0x004B,0x004C,0x004D,0x004E,0x004F,
0x0050,0x0051,0x0052,0x0053,0x0054,0x0055,0x0056,0x0057,
0x0058,0x0059,0x005A,0x005B,0x005C,0x005D,0x005E,0x005F,
0x0060,0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067,
0x0068,0x0069,0x006A,0x006B,0x006C,0x006D,0x006E,0x006F,
0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077,
0x0078,0x0079,0x007A,0x007B,0x007C,0x007D,0x007E,     0,
     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,
0x00A0,0x00A1,0x00A2,0x00A3,0x00A4,0x00A5,0x00A6,0x00A7,
0x00A8,0x00A9,0x00AA,0x00AB,0x00AC,0x00AD,0x00AE,0x00AF,
0x00B0,0x00B1,0x00B2,0x00B3,0x00B4,0x00B5,0x00B6,0x00B7,
0x00B8,0x00B9,0x00BA,0x00BB,0x00BC,0x00BD,0x00BE,0x00BF,
0x00C0,0x00C1,0x00C2,0x00C3,0x00C4,0x00C5,0x00C6,0x00C7,
0x00C8,0x00C9,0x00CA,0x00CB,0x00CC,0x00CD,0x00CE,0x00CF,
0x00D0,0x00D1,0x00D2,0x00D3,0x00D4,0x00D5,0x00D6,0x00D7,
0x00D8,0x00D9,0x00DA,0x00DB,0x00DC,0x00DD,0x00DE,0x00DF,
0x00E0,0x00E1,0x00E2,0x00E3,0x00E4,0x00E5,0x00E6,0x00E7,
0x00E8,0x00E9,0x00EA,0x00EB,0x00EC,0x00ED,0x00EE,0x00EF,
0x00F0,0x00F1,0x00F2,0x00F3,0x00F4,0x00F5,0x00F6,0x00F7,
0x00F8,0x00F9,0x00FA,0x00FB,0x00FC,0x00FD,0x00FE,0x00FF
};

static uchar uni_latin1[]={
0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,
0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,
0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,
0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x5B,0x5C,0x5D,0x5E,0x5F,
0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,
0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x7B,0x7C,0x7D,0x7E,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,
0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF,
0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,
0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF,
0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,
0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF};

static uchar ctype_latin1[] = {
    0,
   32, 32, 32, 32, 32, 32, 32, 32, 32, 40, 40, 40, 40, 40, 32, 32,
   32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
   72, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
  132,132,132,132,132,132,132,132,132,132, 16, 16, 16, 16, 16, 16,
   16,129,129,129,129,129,129,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1, 16, 16, 16, 16, 16,
   16,130,130,130,130,130,130,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 16, 16, 16, 16, 32,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   72, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
   16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1, 16,  1,  1,  1,  1,  1,  1,  1,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2, 16,  2,  2,  2,  2,  2,  2,  2,  2
};

static uchar to_lower_latin1[] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   64, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
  112,113,114,115,116,117,118,119,120,121,122, 91, 92, 93, 94, 95,
   96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
  112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
  224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
  240,241,242,243,244,245,246,215,248,249,250,251,252,253,254,223,
  224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
  240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
};

static uchar to_upper_latin1[] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
   96, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90,123,124,125,126,127,
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
  192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
  192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,247,216,217,218,219,220,221,222,255
};

static uchar sort_order_latin1[] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
   96, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90,123,124,125,126,127,
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
   65, 65, 65, 65, 92, 91, 92, 67, 69, 69, 69, 69, 73, 73, 73, 73,
   68, 78, 79, 79, 79, 79, 93,215,216, 85, 85, 85, 89, 89,222,223,
   65, 65, 65, 65, 92, 91, 92, 67, 69, 69, 69, 69, 73, 73, 73, 73,
   68, 78, 79, 79, 79, 79, 93,247,216, 85, 85, 85, 89, 89,222,255
};


static
int my_mb_wc_latin1(CHARSET_INFO *cs __attribute__((unused)),
		  my_wc_t *wc,
		  const unsigned char *str,
		  const unsigned char *end)
{
  if (str >= end)
    return MY_CS_TOOFEW(0);
  
  return ((wc[0]= latin1_uni[*str]) || (!str[0])) ? 1 : MY_CS_ILSEQ;
}

static
int my_wc_mb_latin1(CHARSET_INFO *cs __attribute__((unused)),
		  my_wc_t wc,
		  unsigned char *str,
		  unsigned char *end)
{
  if (str >= end)
    return MY_CS_TOOSMALL;

  return ((wc < 256) && ((str[0]=uni_latin1[wc]) || (!wc))) ? 1 : MY_CS_ILUNI;
}

static MY_CHARSET_HANDLER my_charset_handler=
{
    NULL,
    my_mbcharlen_8bit,
    my_numchars_8bit,
    my_charpos_8bit,
    my_lengthsp_8bit,
    my_mb_wc_latin1,
    my_wc_mb_latin1,
    my_caseup_str_8bit,
    my_casedn_str_8bit,
    my_caseup_8bit,
    my_casedn_8bit,
    my_snprintf_8bit,
    my_long10_to_str_8bit,
    my_longlong10_to_str_8bit,
    my_fill_8bit,
    my_strntol_8bit,
    my_strntoul_8bit,
    my_strntoll_8bit,
    my_strntoull_8bit,
    my_strntod_8bit,
    my_scan_8bit
};


CHARSET_INFO my_charset_latin1=
{
    8,0,0,				/* number    */
    MY_CS_COMPILED | MY_CS_PRIMARY,	/* state     */
    "latin1",				/* cs name    */
    "latin1_swedish_ci",		/* name      */
    "",					/* comment   */
    ctype_latin1,
    to_lower_latin1,
    to_upper_latin1,
    sort_order_latin1,
    latin1_uni,		/* tab_to_uni   */
    NULL,		/* tab_from_uni */
    "","",
    1,			/* strxfrm_multiply */
    1,			/* mbmaxlen  */
    0,
    &my_charset_handler,
    &my_collation_8bit_simple_ci_handler
};




/*
 * This file is the latin1 character set with German sorting
 *
 * The modern sort order is used, where:
 *
 * 'ä'  ->  "ae"
 * 'ö'  ->  "oe"
 * 'ü'  ->  "ue"
 * 'ß'  ->  "ss"
 */


/*
 * This is a simple latin1 mapping table, which maps all accented
 * characters to their non-accented equivalents.  Note: in this
 * table, 'ä' is mapped to 'A', 'ÿ' is mapped to 'Y', etc. - all
 * accented characters except the following are treated the same way.
 * Ü, ü, Ö, ö, Ä, ä
 */

static uchar sort_order_latin1_de[] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
   96, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90,123,124,125,126,127,
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
   65, 65, 65, 65,196, 65, 92, 67, 69, 69, 69, 69, 73, 73, 73, 73,
   68, 78, 79, 79, 79, 79,214,215,216, 85, 85, 85,220, 89,222,223,
   65, 65, 65, 65,196, 65, 92, 67, 69, 69, 69, 69, 73, 73, 73, 73,
   68, 78, 79, 79, 79, 79,214,247,216, 85, 85, 85,220, 89,222, 89
};


/*
  same as sort_order_latin_de, but maps ALL accented chars to unaccented ones
*/

uchar combo1map[]={
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
   32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
   48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
   64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
   96, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90,123,124,125,126,127,
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
   65, 65, 65, 65, 65, 65, 92, 67, 69, 69, 69, 69, 73, 73, 73, 73,
   68, 78, 79, 79, 79, 79, 79,215,216, 85, 85, 85, 85, 89,222, 83,
   65, 65, 65, 65, 65, 65, 92, 67, 69, 69, 69, 69, 73, 73, 73, 73,
   68, 78, 79, 79, 79, 79, 79,247,216, 85, 85, 85, 85, 89,222, 89
};

uchar combo2map[]={
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,69, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0,69, 0, 0, 0, 0, 0,69, 0, 0,83, 0, 0, 0, 0,69, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,69, 0, 0, 0, 0, 0,69, 0, 0, 0, 0
};


/*
  Some notes about the following comparison rules:
  By definition, my_strnncoll_latin_de must works exactly as if had called
  my_strnxfrm_latin_de() on both strings and compared the result strings.

  This means that:
  Ä must also matches ÁE and Aè, because my_strxn_frm_latin_de() will convert
  both to AE.

  The other option would be to not do any accent removal in
  sort_order_latin_de[] at all
*/


static int my_strnncoll_latin1_de(CHARSET_INFO *cs __attribute__((unused)),
				  const uchar *s1, uint len1,
				  const uchar *s2, uint len2)
{
  const uchar *e1 = s1 + len1;
  const uchar *e2 = s2 + len2;
  uchar c1, c12=0, c2, c22=0;

  while ((s1 < e1 || c12) && (s2 < e2 || c22))
  {
    if (c12)
    {
      c1=c12; c12=0;
    }
    else
    {
      c12=combo2map[*s1];
      c1=combo1map[*s1++];
    }
    if (c22)
    {
      c2=c22; c22=0;
    }
    else
    {
      c22=combo2map[*s2];
      c2=combo1map[*s2++];
    }
    if (c1 != c2) return (int)c1 - (int)c2;
  }

  /*
    A simple test of string lengths won't work -- we test to see
    which string ran out first
  */
  return (s1 < e1 || c12) ? 1 : (s2 < e2 || c22) ? -1 : 0;
}


static int my_strnncollsp_latin1_de(CHARSET_INFO *cs,
				    const uchar *s, uint slen,
				    const uchar *t, uint tlen)
{
  for ( ; slen && my_isspace(cs, s[slen-1]) ; slen--);
  for ( ; tlen && my_isspace(cs, t[tlen-1]) ; tlen--);
  return my_strnncoll_latin1_de(cs,s,slen,t,tlen);
}


static int my_strnxfrm_latin1_de(CHARSET_INFO *cs __attribute__((unused)),
				 uchar * dest, uint len,
				 const uchar * src, uint srclen)
{
  const uchar *dest_orig = dest;
  const uchar *de = dest + len;
  const uchar *se = src + srclen;
  for ( ; src < se && dest < de ; src++)
  {
    uchar chr=combo1map[*src];
    *dest++=chr;
    if ((chr=combo2map[*src]) && dest < de)
      *dest++=chr;
  }
  return (int) (dest - dest_orig);
}


static MY_COLLATION_HANDLER my_collation_german2_ci_handler=
{
  my_strnncoll_latin1_de,
  my_strnncollsp_latin1_de,
  my_strnxfrm_latin1_de,
  my_like_range_simple,
  my_wildcmp_8bit,
  my_strcasecmp_8bit,
  my_instr_simple,
  my_hash_sort_simple
};


CHARSET_INFO my_charset_latin1_german2_ci=
{
  31,0,0,				/* number    */
  MY_CS_COMPILED|MY_CS_STRNXFRM,	/* state     */
  "latin1",				/* cs name    */
  "latin1_german2_ci",			/* name      */
  "",					/* comment   */
  ctype_latin1,
  to_lower_latin1,
  to_upper_latin1,
  sort_order_latin1_de,
  latin1_uni,				/* tab_to_uni   */
  NULL,					/* tab_from_uni */
  "","",
  2,					/* strxfrm_multiply */
  1,					/* mbmaxlen  */
  0,
  &my_charset_handler,
  &my_collation_german2_ci_handler
};


CHARSET_INFO my_charset_latin1_bin=
{
  47,0,0,				/* number    */
  MY_CS_COMPILED|MY_CS_BINSORT,		/* state     */
  "latin1",				/* cs name    */
  "latin1_bin",				/* name      */
  "",					/* comment   */
  ctype_latin1,
  to_lower_latin1,
  to_upper_latin1,
  sort_order_latin1_de,
  latin1_uni,				/* tab_to_uni   */
  NULL,					/* tab_from_uni */
  "",
  "",
  1,					/* strxfrm_multiply */
  1,					/* mbmaxlen  */
  0,
  &my_charset_handler,
  &my_collation_8bit_bin_handler
};

