/*
*  Copyright (C) 2010 Craig Robinson 
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


/**
 * \author Craig Robinson
 *
 * \brief Function to compute the factorized flux as uses in the new EOBNR_PP
 * model. Flux function given by Phys.Rev.D79:064004,2009.
 */

#ifndef _LALSIMIMRSPINEOBFACTORIZEDFLUX_C
#define _LALSIMIMRSPINEOBFACTORIZEDFLUX_C

#include <lal/LALComplex.h>
#include <lal/LALSimInspiral.h>
#include <lal/LALSimIMR.h>

#include "LALSimIMREOBNRv2.h"
#include "LALSimIMRSpinEOB.h"

#include "LALSimIMRSpinEOBAuxFuncs.c"
#include "LALSimIMREOBNQCCorrection.c"
#include "LALSimIMRSpinEOBFactorizedWaveform.c"

/*------------------------------------------------------------------------------------------
 *
 *          Prototypes of functions defined in this code.
 *
 *------------------------------------------------------------------------------------------
 */

static REAL8 XLALInspiralSpinFactorizedFlux(
                      REAL8Vector           *values,
                      const REAL8           omega,
                      SpinEOBParams         *ak,
                      const REAL8            H,
                      const INT4             lMax
                     );

/*------------------------------------------------------------------------------------------
 *
 *          Defintions of functions.
 *
 *------------------------------------------------------------------------------------------
 */

static REAL8 XLALInspiralSpinFactorizedFlux(
                      REAL8Vector           *values,
                      const REAL8           omega,
                      SpinEOBParams         *ak,
                      const REAL8            H,
                      const INT4             lMax
                     )

{

  REAL8 flux = 0.0;
  REAL8 v;
  REAL8 omegaSq;
  COMPLEX16 hLM;
  INT4 l, m;

  EOBNonQCCoeffs nqcCoeffs;

#ifndef LAL_NDEBUG
  if ( !values || !ak )
  {
    XLAL_ERROR_REAL8( XLAL_EFAULT );
  }
#endif

  if ( lMax < 2 )
  {
    XLAL_ERROR_REAL8( XLAL_EINVAL );
  }

  /* Omegs is the derivative of phi */
  omegaSq = omega*omega;

  v = cbrt( omega );
//  printf( "v = %.16e\n", v );
  for ( l = 2; l <= lMax; l++ )
  {
    for ( m = 1; m <= l; m++ )
    {

      if ( XLALSimIMRSpinEOBGetSpinFactorizedWaveform( &hLM, values, v, H, l, m, ak )
             == XLAL_FAILURE )
      {
        XLAL_ERROR_REAL8( XLAL_EFUNC );
      }
      /* For the 2,2 mode, we apply NQC correction to the flux */
      if ( l == 2 && m == 2 )
      {
        COMPLEX16 hNQC;
        XLALSimIMRGetEOBCalibratedSpinNQC( &nqcCoeffs, l, m, ak->eobParams->eta, ak->a );    
        XLALSimIMREOBNonQCCorrection( &hNQC, values, omega, &nqcCoeffs );

        hLM = XLALCOMPLEX16Mul( hNQC, hLM );
      }
//      printf( "l = %d, m = %d, mag(hLM) = %.17e\n", l, m,  XLALCOMPLEX16Abs2( hLM ) );
      flux += (REAL8)(m * m) * omegaSq * XLALCOMPLEX16Abs2( hLM );
    }
  }
  return flux * LAL_1_PI / 8.0;
}

#endif /* _LALSIMIMRSPINEOBFACTORIZEDFLUX_C */
