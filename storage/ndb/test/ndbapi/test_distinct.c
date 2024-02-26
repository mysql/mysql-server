/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.
  
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
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define DOUBLE long double

static DOUBLE one = (DOUBLE)1.0;
static DOUBLE minus_one = (DOUBLE)-1.0;

DOUBLE prob_key_in_fragment(DOUBLE fragments,
                            DOUBLE rpk)
{
  DOUBLE p_key_not_in_fragment = one - (one / fragments);
  DOUBLE p_no_key_in_fragment = powl(p_key_not_in_fragment, rpk);
  DOUBLE p_key_in_fragment = one - p_no_key_in_fragment;
  return p_key_in_fragment;
}

DOUBLE model_predictor(DOUBLE rpk,
                       DOUBLE rows,
                       DOUBLE uniques_found,
                       DOUBLE fragments)
{
  DOUBLE p_key_in_fragment = prob_key_in_fragment(fragments, rpk);
  DOUBLE multiplier = one / p_key_in_fragment;
  DOUBLE est_unique_keys = uniques_found * multiplier;
  printf("Estimated unique keys = %.2Lf\n", est_unique_keys);
  return est_unique_keys;
}

double estimator(DOUBLE uniques_found,
                 DOUBLE rows,
                 DOUBLE fragments)
{
  DOUBLE one = (DOUBLE)1.0;
  DOUBLE estimate = one + (fragments - one) * powl((fragments * uniques_found)/rows, (fragments - one));
  return (double)estimate;
}

void iterative_solution(DOUBLE rows,
                        DOUBLE uniques_found,
                        DOUBLE fragments)
{
  DOUBLE estimate = estimator(uniques_found, rows, fragments);
  DOUBLE est_rpk = rows / (estimate * uniques_found);
  printf("First est_rpk based on old solution is %.2Lf\n", est_rpk);
  DOUBLE percent_change = (DOUBLE)0.5;
  DOUBLE prev_est_uniques_found = (DOUBLE)0.0;
  unsigned prev_decreased = 1;
  unsigned decreased;
  unsigned i = 0;
  do
  {
    DOUBLE p_key_in_fragment = prob_key_in_fragment(fragments, est_rpk);
    DOUBLE est_uniques_found = p_key_in_fragment * rows / est_rpk;
    if (est_uniques_found < prev_est_uniques_found)
    {
      if (est_uniques_found + one > prev_est_uniques_found)
        break;
    }
    else
    {
      if (est_uniques_found - one < prev_est_uniques_found)
        break;
    }
    if (est_uniques_found < uniques_found)
    {
      decreased = 1;
      est_rpk *= (one - percent_change);
    }
    else
    {
      decreased = 0;
      est_rpk *= (one + percent_change);
    }
    if (prev_decreased != decreased)
      percent_change /= (DOUBLE)2;
    prev_decreased = decreased;
    prev_est_uniques_found = est_uniques_found;
  } while (++i < 20);
  printf("After %u iterations we estimate rpk to %.2Lf\n", i, est_rpk);
}

int main(int argc, char** argv)
{
  if (argc != 4)
  {
    printf("3 arguments needed, rows fragments rec_per_key\n");
    exit(1);
  }
  DOUBLE origin_rows = (DOUBLE)10000.0;
  DOUBLE fragments = (DOUBLE)16.0;
  DOUBLE rec_per_key = (DOUBLE)10.0;
  for (unsigned i = 0; i < 3; i++)
  {
    unsigned number;
    sscanf(argv[i+1], "%u", &number);
    switch (i)
    {
      case 0:
        origin_rows = (DOUBLE)number;
        break;
      case 1:
        fragments = (DOUBLE)number;
        break;
      case 2:
        rec_per_key = (DOUBLE)number;
        break;
    }
  }
  for (unsigned int i = 0; i < 10; i++)
  {
    DOUBLE rows = origin_rows;
    DOUBLE samples = rows / fragments;
    DOUBLE uniques_found = (DOUBLE)0.0;
    DOUBLE uniques_selected = (DOUBLE)0.0;
    DOUBLE one = (DOUBLE)1.0;

    while (samples + (DOUBLE)0.01 > one)
    {
      DOUBLE prob_selecting = uniques_selected / rows;
      DOUBLE r = (DOUBLE)random();
      unsigned int u_max = 0x7FFFFFFF;
      DOUBLE d_max = (DOUBLE)u_max;
      DOUBLE r_est = r / d_max;
      if (r_est > prob_selecting)
      {
        rows -= one;
        uniques_selected += (rec_per_key - one);
        uniques_found += one;
      }
      else
      {
        rows -= one;
        uniques_selected -= one;
      }
      samples -= one;
    }
    double real_uniques = (double)(origin_rows / rec_per_key);
    double used_samples = (double)(origin_rows / fragments);
    printf("rows: %.2f, rec_per_key: %.2f\n", (double)origin_rows, (double)rec_per_key);
    printf("real_uniques: %.2f, samples: %.2f, uniques_found = %.2f\n",
           real_uniques,
           used_samples,
          (double)uniques_found);
    model_predictor(rec_per_key, origin_rows, uniques_found, fragments);
    iterative_solution(origin_rows, uniques_found, fragments);
    printf("\n");
  }
  exit(0);
}
