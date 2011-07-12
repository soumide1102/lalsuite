/*
 *  Copyright (C) 2011 Evan Goetz
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with with program; see the file COPYING. If not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 *  MA  02111-1307  USA
 */

#ifndef __STATISTICS_H__
#define __STATISTICS_H__

#include <gsl/gsl_rng.h>

#include <lal/LALStdlib.h>
#include <lal/AVFactories.h>

REAL4Vector * sampleREAL4Vector(REAL4Vector *input, INT4 sampleSize);

REAL8 calcMeanD(REAL8Vector *vector);
REAL8 calcStddevD(REAL8Vector *vector);
REAL8 expRandNum(REAL8 mu, gsl_rng *ptrToGenerator);
REAL8 ncx2cdf(REAL8 x, REAL8 dof, REAL8 delta);
REAL8 ncx2pdf(REAL8 x, REAL8 dof, REAL8 delta);
REAL8 binodeviance(REAL8 x, REAL8 np);
REAL8 epsval(REAL8 val);
REAL8 ncx2inv(REAL8 p, REAL8 dof, REAL8 delta);
REAL8 norminv(REAL8 p, REAL8 mu, REAL8 sigma);
REAL8 ks_test_exp(REAL4Vector *vector);

REAL4 ncx2cdf_float(REAL4 x, REAL4 dof, REAL4 delta);
REAL4 calcMean(REAL4Vector *vector);
REAL4 calcStddev(REAL4Vector *vector);
REAL4 calcRms(REAL4Vector *vector);
REAL4 calcMedian(REAL4Vector *vector);

void sort_float_largest(REAL4Vector *output, REAL4Vector *input);
void sort_float_smallest(REAL4Vector *output, REAL4Vector *input);
void sort_double_descend(REAL8Vector *vector);
void sort_double_ascend(REAL8Vector *vector);
void sort_float_ascend(REAL4Vector *vector);
void sumseries(REAL8 *computedprob, REAL8 P, REAL8 C, REAL8 E, INT4 counter, REAL8 x, REAL8 dof, REAL8 halfdelta, REAL8 err, INT4 countdown);

INT4 max_index(REAL4Vector *vector);
INT4 max_index_double(REAL8Vector *vector);

INT4 qsort_REAL4_compar(const void *a, const void *b);
INT4 qsort_REAL8_compar(const void *a, const void *b);

#endif
