/*
   Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef CHECKSUM_HPP
#define CHECKSUM_HPP


/**
  Optimized XOR checksum calculation. Loop unrolling will 
  reduce relative loop overhead and encourace usage of parallel
  arithmetic adders which are common on most modern CPUs.
*/
inline
Uint32
computeXorChecksumShort(const Uint32 *buf, Uint32 words, Uint32 sum = 0)
{
  const Uint32 *end_unroll = buf + (words & ~3);
  const Uint32 *end        = buf + words;

  /**
   * Aggregate as chunks of 4*Uint32 words:
   * Take care if rewriting this part, code has intentionally
   * been unrolled in order to take advantage of HW parallelism
   * where there are multiple adders in the CPU core.
   */
  while (buf < end_unroll)
  {
    sum ^= buf[0] ^ buf[1] ^ buf[2] ^ buf[3];
    buf += 4;
  }
  // Wrap up remaining part
  while (buf < end)
  {
    sum ^= buf[0];
    buf++;
  }
  return sum;
}

/**
  Optimized XOR checksum calculation intended for longer strings.
  Temporary aggregate XOR-sums into Uint64 which are folded into
  Uint32 in the final stage.
  Also unrool loop as above to take advantage of HW parallelism.
  Callee is responsible for checking that there are sufficient 'words'
  to be checksumed to complete at least a chunk of 4*Uint64 words.
*/
inline
Uint32
computeXorChecksumLong(const Uint32 *buf, Uint32 words, Uint32 sum = 0)
{
  // Align to Uint64 boundary to optimize mem. access below
  if (((size_t)(buf) % 8) != 0)
  {
    sum ^= buf[0];
    buf++;
    words--;
  }

  const Uint64 *p = reinterpret_cast<const Uint64*>(buf);
  Uint64 sum64 = *p++;

  const Uint32 words64 = (words/2) - 1;  // Rem. after init of sum64
  const Uint64 *end = p + (words64 & ~3);

  /**
   * Aggregate as chunks of 4*Uint64 words:
   * Take care if rewriting this part: code has intentionally
   * been unrolled in order to take advantage of HW parallelism
   * where there are multiple adders in the CPU core.
   */
  do
  {
    sum64 ^= p[0] ^ p[1] ^ p[2] ^ p[3];
    p+=4;
  } while (p < end);

  // Wrap up last part which didn't fit in a 4*Uint64 chunk
  end += (words64 % 4);
  while (p < end)
  {
    sum64 ^= p[0];
    p++;
  }

  // Fold temp Uint64 sum into a final Uint32 sum
  sum ^= (Uint32)(sum64 & 0xffffffff) ^ 
         (Uint32)(sum64 >> 32);
  
  // Append last odd Uint32 word
  if ((words%2) != 0)
    sum ^= buf[words-1];

  return sum;
}


inline
Uint32
computeXorChecksum(const Uint32 *buf, Uint32 words, Uint32 sum = 0)
{
  if (words < 16)  // Decided by empirical experiments 
    return computeXorChecksumShort(buf,words,sum);
  else
    return computeXorChecksumLong(buf,words,sum);
}


#endif // CHECKSUM_HPP

