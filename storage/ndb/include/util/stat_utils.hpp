/*
   Copyright (c) 2016, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>


/**
 * Provide an implementation of an incremental calculation
 * of means and standard deviation, without storing the
 * entire series of samples. Statistics is calculated over
 * a window of latest n-samples, such that peaks in the stat.
 * will eventually expire, and the stat adjusts to the load
 * as it varies over time.
 *
 * Implementation is loosely based on:
 *   http://en.wikipedia.org/wiki/Moving_average
 *   http://www-uxsup.csx.cam.ac.uk/~fanf2/hermes/doc/antiforgery/stats.pdf
 *
 * A 'simple moving average' is calculated up to the 
 * specified 'sampleSize', short description:
 *
 *   Suppose that the data set is x1, x2,..., xn then for each xn
 *   we can find an updated mean (M) and square of sums (S) as:
 *
 *   M(1) = x(1), M(k) = M(k-1) + (x(k) - M(k-1)) / k
 *   S(1) = 0, S(k) = S(k-1) + (x(k) - M(k-1)) * (x(k) - M(k))
 *
 * When 'sampleSize' has been reached, we transition into
 * calculation an 'exponentially weighted moving average' (EWMA).
 * The existing 'simple moving average' is used as an initial
 * value for the EWMA.
 *
 * The EWMA has the nice property, that the weight for older samples
 * decrease exponentially as they become 'outdated'.
 */
class NdbStatistics
{
public:
  NdbStatistics(Uint32 sampleSize = 10)
    : m_maxSamples(sampleSize),
      m_noOfSamples(0),
      m_mean(0.0), m_sumSquare(0.0)
  {
    assert(m_maxSamples > 1);
  }

  void init()
  {
    m_noOfSamples = 0;
    m_mean = m_sumSquare = 0.0;
  }

  void update(double sample)
  {
    if (unlikely(m_noOfSamples == 0))
    {
      //First sample, see def of 'M(1)' and 'S(1)' above.
      m_mean        = sample;
      m_sumSquare   = 0;
      m_noOfSamples = 1;
    }
    else
    {
      const double delta = sample - m_mean;

      if (m_noOfSamples == m_maxSamples)
      {
        /**
         * An 'exponentially weighted moving average' need to expire
         * an average 'sumSquare' sample from the moving average window.
         * Then the most recent 'sumSquare' sample can later be added in
         * the same way as calculating it in a 'simple moving average'.
         */
        m_sumSquare -= (m_sumSquare / m_noOfSamples);
        m_noOfSamples--;
      }

      /* Add 'sample' as 'simple moving average' */
      m_noOfSamples++;
      m_mean      += (delta / m_noOfSamples);
      m_sumSquare += fabs(delta * (sample - m_mean));
    }
  }

  double getMean() const
  {
    return m_mean;
  }

  double getStdDev() const
  {
    return likely(m_noOfSamples > 1)
            ? sqrt(m_sumSquare / (m_noOfSamples - 1))
            : 0;
  }

protected:
  // Size of 'window' we calculate over
  const Uint32 m_maxSamples;

  // Current number of samples used, <= m_maxSamples
  Uint32 m_noOfSamples;

  // Moving average of all current samples
  double m_mean;

  //Sum of square of differences from the current mean.
  double m_sumSquare;
};
