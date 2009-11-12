#ifndef RIJNDAEL_INCLUDED
#define RIJNDAEL_INCLUDED

/* Copyright (C) 2002 MySQL AB

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/*
  rijndael-alg-fst.h

  @version 3.0 (December 2000)
  Optimised ANSI C code for the Rijndael cipher (now AES)
  @author Vincent Rijmen <vincent.rijmen@esat.kuleuven.ac.be>
  @author Antoon Bosselaers <antoon.bosselaers@esat.kuleuven.ac.be>
  @author Paulo Barreto <paulo.barreto@terra.com.br>

  This code is hereby placed in the public domain.
  Modified by Peter Zaitsev to fit MySQL coding style.
 */

#define AES_MAXKC	(256/32)
#define AES_MAXKB	(256/8)
#define AES_MAXNR	14

int rijndaelKeySetupEnc(uint32 rk[/*4*(Nr + 1)*/], const uint8 cipherKey[],
			int keyBits);
int rijndaelKeySetupDec(uint32 rk[/*4*(Nr + 1)*/], const uint8 cipherKey[],
			int keyBits);
void rijndaelEncrypt(const uint32 rk[/*4*(Nr + 1)*/], int Nr,
		     const uint8 pt[16], uint8 ct[16]);
void rijndaelDecrypt(const uint32 rk[/*4*(Nr + 1)*/], int Nr,
		     const uint8 ct[16], uint8 pt[16]);

#endif /* RIJNDAEL_INCLUDED */
