//
// Copyright (C) 2013 Karl Wette
// Copyright (C) 2005 Reinhard Prix
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with with program; see the file COPYING. If not, write to the
// Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
// MA  02111-1307  USA
//

#ifndef _CWFASTMATH_H
#define _CWFASTMATH_H

#include <lal/LALStdlib.h>

#ifdef  __cplusplus
extern "C" {
#endif

///
/// \defgroup CWFastMath_h Header CWFastMath.h
/// \ingroup pkg_pulsarCommon
/// \authors Reinhard Prix, Karl Wette
///
/// \brief Various functions for performing fast math in CW functions.
///

// @{
void XLALSinCosLUTInit (void);

int XLALSinCosLUT ( REAL4 *sinx, REAL4 *cosx, REAL8 x );
int XLALSinCos2PiLUT ( REAL4 *sin2pix, REAL4 *cos2pix, REAL8 x );
int XLALSinCos2PiLUTtrimmed ( REAL4 *s, REAL4 *c, REAL8 x );

/* these functions operate on the module-local lookup-table for logarithms,
 * which will dynamically be generated on first use of XLALFastLog(), and can
 * be destroyed at any time using XLALDestroyLogLUT()
 */
REAL8 XLALFastLog ( REAL8 x );
void XLALDestroyLogLUT( void );

REAL8 XLALFastNegExp ( REAL8 mx );
void XLALDestroyExpLUT( void );

// @}

#ifdef  __cplusplus
}
#endif

#endif // _CWFASTMATH_H
