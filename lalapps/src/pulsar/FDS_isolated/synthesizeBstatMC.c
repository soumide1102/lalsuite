/*
 * Copyright (C) 2008 Reinhard Prix
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

/*********************************************************************************/
/** \author R. Prix
 * \file
 * \brief
 * Generate N samples of B-statistic (and F-statistic) values drawn from their
 * respective distributions, assuming Gaussian noise, for given signal parameters.
 *
 * This is mostly meant to be used for Monte-Carlos studies of ROC curves
 *
 *********************************************************************************/
#include "config.h"

/* System includes */
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* GSL includes */
#include <lal/LALGSL.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_permutation.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>

/* LAL-includes */
#include <lal/AVFactories.h>
#include <lal/LALInitBarycenter.h>
#include <lal/UserInput.h>
#include <lal/SFTfileIO.h>
#include <lal/ExtrapolatePulsarSpins.h>
#include <lal/NormalizeSFTRngMed.h>
#include <lal/ComputeFstat.h>
#include <lal/LALHough.h>
#include <lal/LogPrintf.h>

#include <lal/lalGitID.h>
#include <lalappsGitID.h>

#include <lalapps.h>

/*---------- DEFINES ----------*/

#define MAXFILENAMELENGTH 256   /* Maximum # of characters of a SFT filename */

#define EPHEM_YEARS  "00-04"	/**< default range: override with --ephemYear */

#define TRUE (1==1)
#define FALSE (1==0)

/*----- Error-codes -----*/
#define PREDICTFSTAT_ENULL 	1
#define PREDICTFSTAT_ESYS     	2
#define PREDICTFSTAT_EINPUT   	3
#define PREDICTFSTAT_EMEM   	4
#define PREDICTFSTAT_ENONULL 	5
#define PREDICTFSTAT_EXLAL	6

#define PREDICTFSTAT_MSGENULL 	"Arguments contained an unexpected null pointer"
#define PREDICTFSTAT_MSGESYS	"System call failed (probably file IO)"
#define PREDICTFSTAT_MSGEINPUT  "Invalid input"
#define PREDICTFSTAT_MSGEMEM   	"Out of memory. Bad."
#define PREDICTFSTAT_MSGENONULL "Output pointer is non-NULL"
#define PREDICTFSTAT_MSGEXLAL	"XLALFunction-call failed"

/** convert GPS-time to REAL8 */
#define GPS2REAL8(gps) (1.0 * (gps).gpsSeconds + 1.e-9 * (gps).gpsNanoSeconds )

#define MYMAX(x,y) ( (x) > (y) ? (x) : (y) )
#define MYMIN(x,y) ( (x) < (y) ? (x) : (y) )

#define SQ(x) ((x)*(x))

#define LAL_INT4_MAX 2147483647

/** Configuration settings required for and defining a coherent pulsar search.
 * These are 'pre-processed' settings, which have been derived from the user-input.
 */
typedef struct {
  gsl_matrix *M_mu_nu;		/**< antenna-pattern matrix and normalization */
  gsl_vector  *s_mu, *A_Mu;	/**< signal amplitude vectors */
} ConfigVariables;

/*---------- Global variables ----------*/
extern int vrbflg;		/**< defined in lalapps.c */

ConfigVariables GV;		/**< global container for various derived configuration settings */

/* ----- User-variables: can be set from config-file or command-line */
typedef struct {
  BOOLEAN help;		/**< trigger output of help string */

  REAL8 aPlus;		/**< '+' polarization amplitude: aPlus  [alternative to {h0, cosi}: aPlus = 0.5*h0*(1+cosi^2)] */
  REAL8 aCross;		/**< 'x' polarization amplitude: aCross [alternative to {h0, cosi}: aCross= h0 * cosi] */
  REAL8 h0;		/**< overall GW amplitude h0 [alternative to {aPlus, aCross}] */
  REAL8 cosi;		/**< cos(inclination angle)  [alternative to {aPlus, aCross}] */
  REAL8 psi;		/**< polarization angle psi */
  REAL8 phi0;		/**< initial GW phase phi_0 in radians */
  REAL8 Freq;		/**< GW signal frequency */
  REAL8 Alpha;		/**< sky-position angle 'alpha', which is right ascencion in equatorial coordinates */
  REAL8 Delta;		/**< sky-position angle 'delta', which is declination in equatorial coordinates */

  REAL8 Mmunu_A;	/**< componentent of A_mu: A */
  REAL8 Mmunu_B;	/**< componentent of A_mu: B */
  REAL8 Mmunu_C;	/**< componentent of A_mu: C */

  INT4 numDraws;	/**< number of random 'draws' to simulate for F-stat and B-stat */

  CHAR *outputStats;	/**< output file to write F-stat estimation results into */

  BOOLEAN version;	/**< output version-info */
} UserInput_t;

static UserInput_t empty_UserInput;

RCSID( "$Id$");

/* ---------- local prototypes ---------- */
int main(int argc,char *argv[]);

void initUserVars (LALStatus *status, UserInput_t *uvar );
void InitCode ( LALStatus *, ConfigVariables *cfg, const UserInput_t *uvar );

/*---------- empty initializers ---------- */


/*----------------------------------------------------------------------*/
/* Main Function starts here */
/*----------------------------------------------------------------------*/
/**
 * MAIN function
 * Generates samples of B-stat and F-stat according to their pdfs for given signal-params.
 */
int main(int argc,char *argv[])
{
  LALStatus status = blank_status;
  UserInput_t uvar = empty_UserInput;

  gsl_matrix *M_chol;
  REAL8 rho2;

  gsl_vector *FStat, *BStat;
  gsl_matrix *normal, *n_mu_i;

  const gsl_rng_type * T;
  gsl_rng * r;  /* gsl random-number generator */
  UINT4 i, row, col;

  lalDebugLevel = 0;
  vrbflg = 1;	/* verbose error-messages */

  /* set LAL error-handler */
  lal_errhandler = LAL_ERR_EXIT;

  /* register all user-variable */
  LAL_CALL (LALGetDebugLevel(&status, argc, argv, 'v'), &status);
  LAL_CALL (initUserVars(&status, &uvar), &status);

  /* do ALL cmdline and cfgfile handling */
  LAL_CALL (LALUserVarReadAllInput(&status, argc, argv), &status);

  if (uvar.help)	/* if help was requested, we're done here */
    exit (0);

  if ( uvar.version ) {
    printf ( "%s\n", lalGitID );
    printf ( "%s\n", lalappsGitID );
    return 0;
  }

  /* Initialize code-setup */
  LAL_CALL ( InitCode(&status, &GV, &uvar ), &status);

  if ( (FStat = gsl_vector_calloc ( uvar.numDraws ) ) == NULL) {
    LogPrintf ( LOG_CRITICAL, "Out of memory?\n");
    return 1;
  }
  if ( (BStat = gsl_vector_calloc ( uvar.numDraws ) ) == NULL) {
    LogPrintf ( LOG_CRITICAL, "Out of memory?\n");
    return 1;
  }
  if ( (normal = gsl_vector_calloc ( uvar.numDraws ) ) == NULL) {
    LogPrintf ( LOG_CRITICAL, "Out of memory?\n");
    return 1;
  }
  if ( ( n_mu_i = gsl_matrix_calloc ( uvar.numDraws, 4 ) ) == NULL ) {
    LogPrintf ( LOG_CRITICAL, "Out of memory?\n");
    return 1;
  }

  /* ----- compute Cholesky decomposition of M_mu_nu */
  if ( (M_chol = gsl_matrix_calloc ( 4, 4 ) ) == NULL) {
    LogPrintf ( LOG_CRITICAL, "Out of memory?\n");
    return 1;
  }
  {
    gsl_matrix *tmp;
    if ( (tmp = gsl_matrix_calloc ( 4, 4 ) ) == NULL) {
      LogPrintf ( LOG_CRITICAL, "Out of memory?\n");
      return 1;
    }
    gsl_matrix_memcpy ( tmp, GV.M_mu_nu );
    /* Cholesky decompose M_mu_nu = L^T * L */
    gsl_linalg_cholesky_decomp ( tmp );
    /* copy lower triangular matrix, which is L */
    for ( row = 0; row < 4; row ++ )
      for ( col = 0; col <= row; col ++ )
	gsl_matrix_set ( M_chol, row, col, gsl_matrix_get ( tmp, row, col ) );
    gsl_matrix_free ( tmp );
  } /* Cholesky-decompose M_mu_nu */


  /* ----- generate 'numDraws' normal-distributed random numbers ----- */
  /* initialize gsl random-number generator */
  gsl_rng_env_setup();
  T = gsl_rng_default;
  r = gsl_rng_alloc (T);

  printf ("generator type: %s\n", gsl_rng_name (r));
  printf ("seed = %lu\n", gsl_rng_default_seed);
  printf ("first value = %lu\n", gsl_rng_get (r));

  /* generate 'numDraws' random number with normal distribution: mean=0, sigma=1 */
  for ( i = 0; i < (UINT4)uvar.numDraws; i ++ )
    {
      gsl_matrix_set (normal, i, 0,  gsl_ran_gaussian ( r, 1.0 ) );
      gsl_matrix_set (normal, i, 1,  gsl_ran_gaussian ( r, 1.0 ) );
      gsl_matrix_set (normal, i, 2,  gsl_ran_gaussian ( r, 1.0 ) );
      gsl_matrix_set (normal, i, 3,  gsl_ran_gaussian ( r, 1.0 ) );
    }


  /* use normal-variates with Cholesky decomposed matrix to get n_mu with cov(n_mu,n_nu) = M_mu_nu */
  {
    for ( i = 0; i < (UINT4)uvar.numDraws; i ++ )
      {
	gsl_vector_const_view normi = gsl_matrix_const_row ( normal, i );
	gsl_vector_view ni = gsl_matrix_row ( n_mu_i, i );

	/* int gsl_blas_dgemv (CBLAS_TRANSPOSE_t TransA, double alpha, const gsl_matrix * A, const gsl_vector * x, double beta, gsl_vector * y)
	 * compute the matrix-vector product and sum y = \alpha op(A) x + \beta y, where op(A) = A, A^T, A^H
	 * for TransA = CblasNoTrans, CblasTrans, CblasConjTrans.
	 */
	gsl_blas_dgemv (CblasTrans, 1.0, M_chol, &(normi.vector), 0.0, &(ni.vector));

      }
  }



  /* output F-statistic and B-statistic samples into file, if requested */
  if (uvar.outputStats)
    {
      FILE *fpStat = NULL;
      CHAR *logstr = NULL;
      CHAR *id1, *id2;

      if ( (fpStat = fopen (uvar.outputStats, "wb")) == NULL)
	{
	  LALPrintError ("\nError opening file '%s' for writing..\n\n", uvar.outputStats);
	  return (PREDICTFSTAT_ESYS);
	}

      /* log search-footprint at head of output-file */
      LAL_CALL( LALUserVarGetLog (&status, &logstr,  UVAR_LOGFMT_CMDLINE ), &status );

      fprintf(fpStat, "%%%% cmdline: %s\n", logstr );
      LALFree ( logstr );
      id1 = XLALClearLinebreaks ( lalGitID );
      id2 = XLALClearLinebreaks ( lalappsGitID );
      fprintf ( fpStat, "%%%% %s\n%%%%%s\n", id1, id2 );
      LALFree ( id1 ); LALFree ( id2 );

      /* append 'dataSummary' */
      fprintf (fpStat, "%%%% E[2F]   sigma[2F] \n");
      fprintf (fpStat, "twoF_expected = %g;\n", 4.0 + rho2);
      fprintf (fpStat, "twoF_sigma    = %g;\n", sqrt( 4.0 * ( 2.0 + rho2 ) ) );
      fprintf (fpStat, "M_mu_nu = ");
      XLALfprintfGSLmatrix ( fpStat, "%f", GV.M_mu_nu );
      fprintf (fpStat, "x_mu = ");
      XLALfprintfGSLvector ( fpStat, "%f", GV.s_mu );
      fprintf (fpStat, "A_Mu = ");
      XLALfprintfGSLvector ( fpStat, "%f", GV.A_Mu );

      fclose (fpStat);
    } /* if outputStat */

  /* Free config-Variables and userInput stuff */
  LAL_CALL (LALDestroyUserVars (&status), &status);

  gsl_matrix_free ( GV.M_mu_nu );
  gsl_vector_free ( GV.s_mu );
  gsl_vector_free ( GV.A_Mu );
  gsl_vector_free ( FStat );
  gsl_vector_free ( BStat );
  gsl_matrix_free ( normal );
  gsl_matrix_free ( n_mu_i );

  gsl_rng_free (r);

  /* did we forget anything ? */
  LALCheckMemoryLeaks();

  return 0;

} /* main() */

/**
 * Register all our "user-variables" that can be specified from cmd-line and/or config-file.
 * Here we set defaults for some user-variables and register them with the UserInput module.
 */
void
initUserVars (LALStatus *status, UserInput_t *uvar )
{

  INITSTATUS( status, "initUserVars", rcsid );
  ATTATCHSTATUSPTR (status);

  /* set a few defaults */
  uvar->help = FALSE;
  uvar->outputStats = NULL;

  uvar->phi0 = 0;
  uvar->psi = 0;

  uvar->numDraws = 1;

  /* register all our user-variables */
  LALregBOOLUserStruct(status,	help, 		'h', UVAR_HELP,     "Print this message");

  LALregREALUserStruct(status, 	aPlus,	 	 0 , UVAR_OPTIONAL, "'Plus' polarization amplitude: aPlus  [alternative to {h0, cosi}");
  LALregREALUserStruct(status,	aCross,  	 0 , UVAR_OPTIONAL, "'Cross' polarization amplitude: aCross [alternative to {h0, cosi}");
  LALregREALUserStruct(status,	h0,		's', UVAR_OPTIONAL, "Overall GW amplitude h0 [alternative to {aPlus, aCross}]");
  LALregREALUserStruct(status,	cosi,		'i', UVAR_OPTIONAL, "Inclination angle of rotation axis cos(iota) [alternative to {aPlus, aCross}]");

  LALregREALUserStruct(status,	psi,		'Y', UVAR_OPTIONAL, "Polarisation angle in radians");
  LALregREALUserStruct(status,	phi0,		'Y', UVAR_OPTIONAL, "Initial GW phase phi0 in radians");

  LALregREALUserStruct(status,	Mmunu_A,	  0, UVAR_REQUIRED, "Antenna-pattern matrix M_mu_nu: component A");
  LALregREALUserStruct(status,	Mmunu_B,	  0, UVAR_REQUIRED, "Antenna-pattern matrix M_mu_nu: component A");
  LALregREALUserStruct(status,	Mmunu_C,	  0, UVAR_REQUIRED, "Antenna-pattern matrix M_mu_nu: component A");

  LALregINTUserStruct(status,	numDraws,	'N', UVAR_OPTIONAL, "Number of random 'draws' to simulate for F-stat and B-stat");

  LALregBOOLUserStruct(status,	version,        'V', UVAR_SPECIAL,   "Output code version");

  DETATCHSTATUSPTR (status);
  RETURN (status);

} /* initUserVars() */


/** Initialized Fstat-code: handle user-input and set everything up. */
void
InitCode ( LALStatus *status, ConfigVariables *cfg, const UserInput_t *uvar )
{
  REAL8 aPlus, aCross;		/* internally always use Aplus, Across */

  INITSTATUS (status, "InitCode", rcsid);
  ATTATCHSTATUSPTR (status);

  { /* Check user-input consistency */
    BOOLEAN have_h0, have_cosi, have_Ap, have_Ac;

    have_h0 = LALUserVarWasSet ( &uvar->h0 );
    have_cosi = LALUserVarWasSet ( &uvar->cosi );
    have_Ap = LALUserVarWasSet ( &uvar->aPlus );
    have_Ac = LALUserVarWasSet ( &uvar->aCross );

    /* ----- handle {h0,cosi} || {aPlus,aCross} freedom ----- */
    if ( ( have_h0 && !have_cosi ) || ( !have_h0 && have_cosi ) )
      {
	LogPrintf (LOG_CRITICAL, "Need both (h0, cosi) to specify signal!\n");
	ABORT ( status, PREDICTFSTAT_EINPUT, PREDICTFSTAT_MSGEINPUT );
      }
    if ( ( have_Ap && !have_Ac) || ( !have_Ap && have_Ac ) )
      {
	LogPrintf (LOG_CRITICAL, "Need both (aPlus, aCross) to specify signal!\n");
	ABORT ( status, PREDICTFSTAT_EINPUT, PREDICTFSTAT_MSGEINPUT );
      }
    if ( have_h0 && have_Ap )
      {
	LogPrintf (LOG_CRITICAL, "Overdetermined: specify EITHER (h0,cosi) OR (aPlus,aCross)!\n");
	ABORT ( status, PREDICTFSTAT_EINPUT, PREDICTFSTAT_MSGEINPUT );
      }
    /* ----- internally we always use Aplus, Across */
    if ( have_h0 )
      {
	aPlus = 0.5 * uvar->h0 * ( 1.0 + SQ( uvar->cosi) );
	aCross = uvar->h0 * uvar->cosi;
      }
    else
      {
	aPlus = uvar->aPlus;
	aCross = uvar->aCross;
      }
  }/* check user-input */

  /* ----- set up M_mu_nu matrix ----- */
  if ( ( cfg->M_mu_nu = gsl_matrix_calloc ( 4, 4 )) == NULL )
    {
      LogPrintf (LOG_CRITICAL, "Seem to be out of memory!\n");
      ABORT ( status, PREDICTFSTAT_EMEM, PREDICTFSTAT_MSGEMEM );
    }
  {
    REAL8 Ad = uvar->Mmunu_A;
    REAL8 Bd = uvar->Mmunu_B;
    REAL8 Cd = uvar->Mmunu_C;

    gsl_matrix_set (cfg->M_mu_nu, 0, 0,   Ad );
    gsl_matrix_set (cfg->M_mu_nu, 1, 1,   Bd );
    gsl_matrix_set (cfg->M_mu_nu, 0, 1,   Cd );
    gsl_matrix_set (cfg->M_mu_nu, 1, 0,   Cd );

    /*
      gsl_matrix_set (cfg->M_mu_nu, 3, 0,   Ed );
      gsl_matrix_set (cfg->M_mu_nu, 1, 2,  -Ed );
    */

    gsl_matrix_set (cfg->M_mu_nu, 2, 2,   Ad );
    gsl_matrix_set (cfg->M_mu_nu, 3, 3,   Bd );
    gsl_matrix_set (cfg->M_mu_nu, 2, 3,   Cd );
    gsl_matrix_set (cfg->M_mu_nu, 3, 2,   Cd );

    /*
      gsl_matrix_set (cfg->M_mu_nu, 2, 1,  -Ed );
      gsl_matrix_set (cfg->M_mu_nu, 3, 2,   Ed );
    */
  }


  /* ----- generate signal amplitude vectors ---------- */
  if ( (cfg->A_Mu = gsl_vector_calloc ( 4 )) == NULL ) {
    LogPrintf ( LOG_CRITICAL, "Out of memory?\n");
    ABORT ( status, PREDICTFSTAT_EMEM, PREDICTFSTAT_MSGEMEM );
  }
  if ( (cfg->s_mu = gsl_vector_calloc ( 4 )) == NULL ) {
    LogPrintf ( LOG_CRITICAL, "Out of memory?\n");
    ABORT ( status, PREDICTFSTAT_EMEM, PREDICTFSTAT_MSGEMEM );
  }

  /* get signal A^mu and s_mu */
  {
    REAL8 cos2psi = cos ( 2.0 * uvar->psi );
    REAL8 sin2psi = sin ( 2.0 * uvar->psi );
    REAL8 cosphi0 = cos ( uvar->phi0 );
    REAL8 sinphi0 = sin ( uvar->phi0 );
    REAL8 A1, A2, A3, A4;

    A1 =  aPlus * cos2psi * cosphi0 - aCross * sin2psi * sinphi0;
    A2 =  aPlus * sin2psi * cosphi0 + aCross * cos2psi * sinphi0;
    A3 = -aPlus * cos2psi * sinphi0 - aCross * sin2psi * cosphi0;
    A4 = -aPlus * sin2psi * sinphi0 + aCross * cos2psi * cosphi0;

    /* ----- fill vector A_Mu ----- */
    gsl_vector_set (cfg->A_Mu, 0,   A1 );
    gsl_vector_set (cfg->A_Mu, 1,   A2 );
    gsl_vector_set (cfg->A_Mu, 2,   A3 );
    gsl_vector_set (cfg->A_Mu, 3,   A4 );

    /* GSL-doc: int gsl_blas_dsymv (CBLAS_UPLO_t Uplo, double alpha, const gsl_matrix * A,
     *                              const gsl_vector * x, double beta, gsl_vector * y )
     *
     * compute the matrix-vector product and sum: y = alpha A x + beta y
     * for the symmetric matrix A. Since the matrix A is symmetric only its
     * upper half or lower half need to be stored. When Uplo is CblasUpper
     * then the upper triangle and diagonal of A are used, and when Uplo
     * is CblasLower then the lower triangle and diagonal of A are used.
     */
    gsl_blas_dsymv (CblasUpper, 1.0, cfg->M_mu_nu, cfg->A_Mu, 0.0, cfg->s_mu);
  }






  DETATCHSTATUSPTR (status);
  RETURN (status);

} /* InitPFS() */
