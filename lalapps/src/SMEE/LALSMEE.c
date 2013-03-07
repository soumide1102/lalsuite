
/*  InferenceNest.c:  Nested Sampling using LALInference
 *
 *  Copyright (C) 2009 Ilya Mandel, Vivien Raymond, Christian Roever, Marc van der Sluys and John Veitch
 *
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


#include <stdio.h>
#include <lal/Date.h>
#include <lal/GenerateInspiral.h>
#include <lal/LALInference.h>
#include <lal/FrequencySeries.h>
#include <lal/Units.h>
#include <lal/StringInput.h>
#include <lal/LIGOLwXMLInspiralRead.h>
#include <lal/TimeSeries.h>
#include <lalapps.h>
#include <lal/LALInferenceNestedSampler.h>
#include <SMEEPrior.h>
#include <SMEEReadData.h>
#include <SMEELikelihood.h>
#include <lal/LALInferenceTemplate.h>
#include <lal/LALInferenceProposal.h>

LALInferenceRunState *initialize(ProcessParamsTable *commandLine);
void initializeNS(LALInferenceRunState *runState);
void initVariables(LALInferenceRunState *state);
void initialiseProposal( LALInferenceRunState *runState );
void initializeTemplate(LALInferenceRunState *runState);
static void mc2masses(double mc, double eta, double *m1, double *m2);
void LogNSSampleAsMCMCSampleToArray(LALInferenceRunState *state, LALInferenceVariables *vars);                             
void LogNSSampleAsMCMCSampleToFile(LALInferenceRunState *state, LALInferenceVariables *vars);                              
 


static void mc2masses(double mc, double eta, double *m1, double *m2)
/*  Compute individual companion masses (m1, m2)   */
/*  for given chirp mass (m_c) & mass ratio (eta)  */
/*  (note: m1 >= m2).                              */
{
  double root = sqrt(0.25-eta);
  double fraction = (0.5+root) / (0.5-root);
  *m2 = mc * (pow(1+fraction,0.2) / pow(fraction,0.6));
  *m1 = mc * (pow(1+1.0/fraction,0.2) / pow(1.0/fraction,0.6));
  return;
}


void LogNSSampleAsMCMCSampleToArray(LALInferenceRunState *state, LALInferenceVariables *vars)
{
  NSFillMCMCVariables(vars,state->priorArgs);
  LALInferenceLogSampleToArray(state, vars);
  return;
}

void LogNSSampleAsMCMCSampleToFile(LALInferenceRunState *state, LALInferenceVariables *vars)
{
  NSFillMCMCVariables(vars,state->priorArgs);
  LALInferenceLogSampleToFile(state, vars);
  return;
}

LALInferenceRunState *initialize(ProcessParamsTable *commandLine)
/* calls the "ReadData()" function to gather data & PSD from files, */
/* and initializes other variables accordingly.                     */
{
	char help[]="\
Initialisation arguments:\n\
--rawwaveform 		waveform file to be used \n\
(--seed seed           Random seed for Nested Sampling)\n\n";
	LALInferenceRunState *irs=NULL;
	LALInferenceIFOData *ifoPtr, *ifoListStart;
	ProcessParamsTable *ppt=NULL;
	unsigned long int randomseed;
	struct timeval tv;
	FILE *devrandom;
	
	irs = calloc(1, sizeof(LALInferenceRunState));
	/* read data from files: */
	fprintf(stdout, " readData(): started.\n");
	irs->commandLine=commandLine;
	
	irs->data = LALInferenceReadData(commandLine);
	/* (this will already initialise each LALIFOData's following elements:  */
        ppt=LALInferenceGetProcParamVal(commandLine,"--help");
        if(ppt)
        {
                fprintf(stdout,"%s",help);
                return(irs);
        }

	/*     fLow, fHigh, detector, timeToFreqFFTPlan, freqToTimeFFTPlan,     */
	/*     window, oneSidedNoisePowerSpectrum, timeDate, freqData         ) */
	fprintf(stdout, " LALInferenceReadData(): finished.\n");
	if (irs->data != NULL) {
		fprintf(stdout, " initialize(): successfully read data.\n");
		
		fprintf(stdout, " LALInferenceInjectSNSignal(): started.\n");
		LALInferenceInjectSNSignal(irs->data,commandLine);
		fprintf(stdout, " LALInferenceInjectSNSignal(): finished.\n");
		
		ifoPtr = irs->data;
		ifoListStart = irs->data;
		while (ifoPtr != NULL) {
			/*If two IFOs have the same sampling rate, they should have the same timeModelh*,
			 freqModelh*, and modelParams variables to avoid excess computation 
			 in model waveform generation in the future*/
			//LALInferenceIFOData * ifoPtrCompare=ifoListStart;
			int foundIFOwithSameSampleRate=0;
			/*while(ifoPtrCompare != NULL && ifoPtrCompare!=ifoPtr) {
				if(ifoPtrCompare->timeData->deltaT == ifoPtr->timeData->deltaT){
					ifoPtr->timeModelhPlus=ifoPtrCompare->timeModelhPlus;
					ifoPtr->freqModelhPlus=ifoPtrCompare->freqModelhPlus;
					ifoPtr->timeModelhCross=ifoPtrCompare->timeModelhCross;				
					ifoPtr->freqModelhCross=ifoPtrCompare->freqModelhCross;				
					ifoPtr->modelParams=ifoPtrCompare->modelParams;	
					foundIFOwithSameSampleRate=1;	
					break;
				}
			}*/
			if(!foundIFOwithSameSampleRate){
				ifoPtr->timeModelhPlus  = XLALCreateREAL8TimeSeries("timeModelhPlus",
																	&(ifoPtr->timeData->epoch),
																	0.0,
																	ifoPtr->timeData->deltaT,
																	&lalDimensionlessUnit,
																	ifoPtr->timeData->data->length);
				ifoPtr->timeModelhCross = XLALCreateREAL8TimeSeries("timeModelhCross",
																	&(ifoPtr->timeData->epoch),
																	0.0,
																	ifoPtr->timeData->deltaT,
																	&lalDimensionlessUnit,
																	ifoPtr->timeData->data->length);
				ifoPtr->freqModelhPlus = XLALCreateCOMPLEX16FrequencySeries("freqModelhPlus",
																			&(ifoPtr->freqData->epoch),
																			0.0,
																			ifoPtr->freqData->deltaF,
																			&lalDimensionlessUnit,
																			ifoPtr->freqData->data->length);
				ifoPtr->freqModelhCross = XLALCreateCOMPLEX16FrequencySeries("freqModelhCross",
																			 &(ifoPtr->freqData->epoch),
																			 0.0,
																			 ifoPtr->freqData->deltaF,
																			 &lalDimensionlessUnit,
																			 ifoPtr->freqData->data->length);
				ifoPtr->modelParams = calloc(1, sizeof(LALInferenceVariables));
			}
			ifoPtr = ifoPtr->next;
		}
		irs->currentLikelihood=LALInferenceSMEENullLogLikelihood(irs->data);
		printf("Injection Null Log Likelihood: %g\n", irs->currentLikelihood);
		
	}
	else
		fprintf(stdout, " initialize(): no data read.\n");
	//fprintf(stdout, "I'm here \n");
	/* set up GSL random number generator: */
	gsl_rng_env_setup();
	//fprintf(stdout, "I'm here \n");
	irs->GSLrandom = gsl_rng_alloc(gsl_rng_mt19937);
	/* (try to) get random seed from command line: */
	//fprintf(stdout, "I'm here \n");
	ppt = LALInferenceGetProcParamVal(commandLine, "--randomseed");
	
	if (ppt != NULL)
		randomseed = atoi(ppt->value);
	else { /* otherwise generate "random" random seed: */
		if ((devrandom = fopen("/dev/random","r")) == NULL) {
			gettimeofday(&tv, 0);
			randomseed = tv.tv_sec + tv.tv_usec;
		} 
		else {
			if(1!=fread(&randomseed, sizeof(randomseed), 1, devrandom)){
			  fprintf(stderr,"Error: Unable to read random seed from /dev/random\n");
			  exit(1);
			}
			fclose(devrandom);
		}
	}
	
	fprintf(stdout, " initialize(): random seed: %lu\n", randomseed);
	gsl_rng_set(irs->GSLrandom, randomseed);
	
	return(irs);
}

void initializeTemplate(LALInferenceRunState *runState)
{
	char help[]="\
--templatereal PCs files you want to use, real or the absolute value of FFT \n\
--templateimag PCs files you want to use, imaginary parts \n\
--phase 0 if you want to reject phase data, 1 if you want to keep it \n\
(--distance scale distance of waveform) \n\
(--SNR scale SNR of waveform";
	ProcessParamsTable *ppt=NULL;
	ProcessParamsTable *ppt1=NULL;
	ProcessParamsTable *ppt2=NULL;
	ProcessParamsTable *scale=NULL;
	ProcessParamsTable *phase=NULL;
	
	int ph=0;
	ProcessParamsTable *commandLine=runState->commandLine;
	REAL8 scale_factor=1.0;
	/* Print command line arguments if help requested */
	ppt1=LALInferenceGetProcParamVal(commandLine,"--help");
	gsl_matrix *PCvectorsreal=NULL;
	gsl_matrix *PCvectorsimag=NULL;
	UINT4 Nifo=0;
	char **IFOnames=NULL;
	
	LALInferenceIFOData *ifodata;
	LALInferenceIFOData *prev = NULL;
	ppt=LALInferenceGetProcParamVal(commandLine,"--ifo");
	LALInferenceParseCharacterOptionString(ppt->value,&IFOnames,&Nifo);
	
	if(ppt1)
	{
		fprintf(stdout,"%s",help);
		return;
	}
	/* This is the LAL template generator for inspiral signals */
	//runState->template=&LALInferenceTemplateLAL;
	ppt1=LALInferenceGetProcParamVal(commandLine,"--templatereal");
	ppt2=LALInferenceGetProcParamVal(commandLine,"--templateimag");
	
	phase=LALInferenceGetProcParamVal(commandLine,"--phase");
	
	ph=(atof(phase->value));
	
	
	
	scale=LALInferenceGetProcParamVal(commandLine,"--distance");
	if(scale){
		scale_factor=10./(atof(scale->value));
	}
	
	FILE *fp1=fopen(ppt1->value,"r");
	FILE *fp2=fopen(ppt2->value,"r");
     int n=0;
     int i=0;
     int j=0;
     PCvectorsreal = gsl_matrix_alloc(6145,7);
     PCvectorsimag = gsl_matrix_alloc(6145,7);
     
     while( 1 ){
        REAL8 PCreal1, PCreal2, PCreal3, PCreal4, PCreal5, PCreal6, PCreal7;
	fscanf(fp1, "%lf %lf %lf %lf %lf %lf %lf", &PCreal1, &PCreal2, &PCreal3, &PCreal4, &PCreal5, &PCreal6, &PCreal7);
	if ( feof(fp1) ) break;
	if(ph==1){
	gsl_matrix_set(PCvectorsreal, i, 0, PCreal1*1E-22*scale_factor);
	gsl_matrix_set(PCvectorsreal, i, 1, PCreal2*1E-22*scale_factor);
	gsl_matrix_set(PCvectorsreal, i, 2, PCreal3*1E-22*scale_factor);
	gsl_matrix_set(PCvectorsreal, i, 3, PCreal4*1E-22*scale_factor);
	gsl_matrix_set(PCvectorsreal, i, 4, PCreal5*1E-22*scale_factor);
	gsl_matrix_set(PCvectorsreal, i, 5, PCreal6*1E-22*scale_factor);
	gsl_matrix_set(PCvectorsreal, i, 6, PCreal7*1E-22*scale_factor);
	}
	else if(ph==0){
	  gsl_matrix_set(PCvectorsreal, i, 0, PCreal1*1E-20*scale_factor);
	  gsl_matrix_set(PCvectorsreal, i, 1, PCreal2*1E-20*scale_factor);
	  gsl_matrix_set(PCvectorsreal, i, 2, PCreal3*1E-20*scale_factor);
	  gsl_matrix_set(PCvectorsreal, i, 3, PCreal4*1E-20*scale_factor);
	  gsl_matrix_set(PCvectorsreal, i, 4, PCreal5*1E-20*scale_factor);
	  gsl_matrix_set(PCvectorsreal, i, 5, PCreal6*1E-20*scale_factor);
	  gsl_matrix_set(PCvectorsreal, i, 6, PCreal7*1E-20*scale_factor);
	}
	i++;
      }
      
      while( 1 ){
        REAL8 PCimag1, PCimag2, PCimag3, PCimag4, PCimag5, PCimag6, PCimag7;
	fscanf(fp2, "%lf %lf %lf %lf%lf %lf %lf ", &PCimag1, &PCimag2, &PCimag3, &PCimag4, &PCimag5, &PCimag6, &PCimag7);
	if ( feof(fp2) ) break;
	if(ph==1){
	gsl_matrix_set(PCvectorsimag, j, 0, PCimag1*1E-22*scale_factor);
	gsl_matrix_set(PCvectorsimag, j, 1, PCimag2*1E-22*scale_factor);
	gsl_matrix_set(PCvectorsimag, j, 2, PCimag3*1E-22*scale_factor);
	gsl_matrix_set(PCvectorsimag, j, 3, PCimag4*1E-22*scale_factor);
	gsl_matrix_set(PCvectorsimag, j, 4, PCimag5*1E-22*scale_factor);
	gsl_matrix_set(PCvectorsimag, j, 5, PCimag6*1E-22*scale_factor);
	gsl_matrix_set(PCvectorsimag, j, 6, PCimag7*1E-22*scale_factor);
	} 
	else if(ph==0){
	  gsl_matrix_set(PCvectorsimag, j, 0, PCimag1*0*scale_factor);
	  gsl_matrix_set(PCvectorsimag, j, 1, PCimag2*0*scale_factor);
	  gsl_matrix_set(PCvectorsimag, j, 2, PCimag3*0*scale_factor);
	  gsl_matrix_set(PCvectorsimag, j, 3, PCimag4*0*scale_factor);
	  gsl_matrix_set(PCvectorsimag, j, 4, PCimag5*0*scale_factor);
	  gsl_matrix_set(PCvectorsimag, j, 5, PCimag6*0*scale_factor);
	  gsl_matrix_set(PCvectorsimag, j, 6, PCimag7*0*scale_factor);
	}
	j++;
      }
      
      for(n=0, prev=NULL;n<Nifo;n++, prev=ifodata){
	
	ifodata = XLALCalloc( 1, sizeof(LALInferenceIFOData) );
	
	ifodata->modelParams = XLALCalloc( 1, sizeof(LALInferenceVariables) );
	
	ifodata->next = NULL;
	if( n == 0 )  ifodata =runState->data;
	if( n > 0 )   ifodata =prev->next ;
	
      LALInferenceAddVariable(ifodata->modelParams,"PCvectorsreal",&PCvectorsreal, LALINFERENCE_gslMatrix_t,
					LALINFERENCE_PARAM_FIXED);
      LALInferenceAddVariable(ifodata->modelParams,"PCvectorsimag",&PCvectorsimag, LALINFERENCE_gslMatrix_t,
					LALINFERENCE_PARAM_FIXED);
      LALInferenceAddVariable(ifodata->modelParams,"phase",&ph, LALINFERENCE_INT4_t,
					LALINFERENCE_PARAM_FIXED);				
      }
	/*if(ppt) {
		if(!strcmp("LALSTPN",ppt->value)){
			fprintf(stderr,"ERROR: --template LALSTPN is deprecated. Try LALGenerateInspiral instead...\n");
			exit(1);
		}
		else if(!strcmp("PhenSpin",ppt->value))
			runState->template=&LALInferenceTemplatePSTRD;
		else if(!strcmp("LALGenerateInspiral",ppt->value))
			runState->template=&LALInferenceTemplateLALGenerateInspiral;
		else if(!strcmp("SpinTaylor",ppt->value))
			runState->template=&LALInferenceTemplateLALGenerateInspiral;
		else if(!strcmp("LAL",ppt->value))
			runState->template=&LALInferenceTemplateLAL;
        else if(!strcmp("LALSim",ppt->value))
            runState->template=&LALInferenceTemplateXLALSimInspiralChooseWaveform;
		else {
			XLALPrintError("Error: unknown template %s\n",ppt->value);
			XLALPrintError(help);
			XLAL_ERROR_VOID(XLAL_EINVAL);
		}
	}*/
	return;
}

/***** Initialise Nested Sampling structures ****/
/* Fill in samples from the prior distribution */
/* runState->algorithmParams must contain a variable "logLikelihoods" */
/* which contains a REAL8 array of likelihood values for the live */
/* points. */
/************************************************/
void initializeNS(LALInferenceRunState *runState)
{
	char help[]="\
Nested sampling arguments:\n\
 --Nlive N\tNumber of live points to use\n\
 --Nmcmc M\tNumber of MCMC point to use when evolving live points\n\
(--Nruns R)\tNumber of parallel samples from logt to use(1)\n\
(--tolerance dZ)\tTolerance of nested sampling algorithm (0.1)\n\
(--randomseed seed)\tRandom seed of sampling distribution\n\
(--verbose)\tProduce progress information\n\
(--mcmcprop)\tUse PTMCMC proposal engine\n\
\t(--iotaDistance FRAC)\tPTMCMC: Use iota-distance jump FRAC of the time\n\
\t(--covarianceMatrix)\tPTMCMC: Propose jumps from covariance matrix of current live points\n\
\t(--differential-evolution)\tPTMCMC:Use differential evolution jumps\n";

	ProcessParamsTable *ppt=NULL;
	ProcessParamsTable *commandLine=runState->commandLine;
	/* Print command line arguments if help requested */
	ppt=LALInferenceGetProcParamVal(commandLine,"--help");
	if(ppt)
	{
		fprintf(stdout,"%s",help);
		return;
	}

	INT4 verbose=0,tmpi=0,randomseed=0;
	REAL8 tmp=0;
	
	/* Initialise parameters structure */
	runState->algorithmParams=XLALCalloc(1,sizeof(LALInferenceVariables));
	runState->priorArgs=XLALCalloc(1,sizeof(LALInferenceVariables));
	runState->proposalArgs=XLALCalloc(1,sizeof(LALInferenceVariables));
	
	/* Set up the appropriate functions for the nested sampling algorithm */
	runState->algorithm=&LALInferenceNestedSamplingAlgorithm;
	runState->evolve=&LALInferenceNestedSamplingOneStep;
	if(LALInferenceGetProcParamVal(commandLine,"--mcmcprop")){
	  /* Use the PTMCMC proposal to sample prior */
	  runState->proposal=&NSWrapMCMCLALProposal;
	  REAL8 temp=1.0;
	  UINT4 dummy=0;
	  LALInferenceAddVariable(runState->proposalArgs, "adaptableStep", &dummy, LALINFERENCE_INT4_t, LALINFERENCE_PARAM_OUTPUT);
	  LALInferenceAddVariable(runState->proposalArgs, "proposedVariableNumber", &dummy, LALINFERENCE_INT4_t, LALINFERENCE_PARAM_OUTPUT);
	  LALInferenceAddVariable(runState->proposalArgs, "proposedArrayNumber", &dummy, LALINFERENCE_INT4_t, LALINFERENCE_PARAM_OUTPUT);
	  LALInferenceAddVariable(runState->proposalArgs,"temperature",&temp,LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);
	}
	else
	  //runState->proposal=&LALInferenceProposalSMEE;
	//initialiseProposal(&runState);
	runState->likelihood=&LALInferenceSMEEFreqDomainLogLikelihood;
	runState->prior = &LALInferenceSMEEPrior;
	
	#ifdef HAVE_LIBLALXML
	runState->logsample=LogNSSampleAsMCMCSampleToArray;
	#else
	runState->logsample=LogNSSampleAsMCMCSampleToFile;
	#endif
	
	ppt=LALInferenceGetProcParamVal(commandLine,"--verbose");
	if(ppt) {
		verbose=1;
		LALInferenceAddVariable(runState->algorithmParams,"verbose", &verbose , LALINFERENCE_INT4_t,
					LALINFERENCE_PARAM_FIXED);		
	}
	if(verbose) set_debug_level("ERROR|INFO");
	else set_debug_level("NDEBUG");
		
	printf("set number of live points.\n");
	/* Number of live points */
	ppt=LALInferenceGetProcParamVal(commandLine,"--Nlive");
	if(ppt)
		tmpi=atoi(ppt->value);
	else {
		fprintf(stderr,"Error, must specify number of live points\n");
		exit(1);
	}
	LALInferenceAddVariable(runState->algorithmParams,"Nlive",&tmpi, LALINFERENCE_INT4_t,LALINFERENCE_PARAM_FIXED);
	
	printf("set number of MCMC points.\n");
	/* Number of points in MCMC chain */
	ppt=LALInferenceGetProcParamVal(commandLine,"--Nmcmc");
	if(ppt)
	  tmpi=atoi(ppt->value);
	else {
	  fprintf(stderr,"Error, must specify number of MCMC points\n");
	  exit(1);
	}
	LALInferenceAddVariable(runState->algorithmParams,"Nmcmc",&tmpi,
				LALINFERENCE_INT4_t,LALINFERENCE_PARAM_FIXED);
	
	printf("set number of parallel runs.\n");
	/* Optionally specify number of parallel runs */
	ppt=LALInferenceGetProcParamVal(commandLine,"--Nruns");
	if(ppt) {
		tmpi=atoi(ppt->value);
		LALInferenceAddVariable(runState->algorithmParams,"Nruns",&tmpi,LALINFERENCE_INT4_t,LALINFERENCE_PARAM_FIXED);
	}
	
	printf("set tolerance.\n");
	/* Tolerance of the Nested sampling integrator */
	ppt=LALInferenceGetProcParamVal(commandLine,"--tolerance");
	if(ppt){
		tmp=strtod(ppt->value,(char **)NULL);
		LALInferenceAddVariable(runState->algorithmParams,"tolerance",&tmp, LALINFERENCE_REAL8_t,
					LALINFERENCE_PARAM_FIXED);
	}
	
	printf("set random seed.\n");
	/* Set up the random number generator */
	gsl_rng_env_setup();
	runState->GSLrandom = gsl_rng_alloc(gsl_rng_mt19937);
	
	/* (try to) get random seed from command line: */
	ppt = LALInferenceGetProcParamVal(commandLine, "--randomseed");
	if (ppt != NULL)
		randomseed = atoi(ppt->value);
	fprintf(stdout, " initialize(): random seed: %u\n", randomseed);
	LALInferenceAddVariable(runState->algorithmParams,"random_seed",&randomseed, LALINFERENCE_INT4_t,LALINFERENCE_PARAM_FIXED);
	gsl_rng_set(runState->GSLrandom, randomseed);
	
	return;
	
}

/* Setup the variables to control template generation */
/* Includes specification of prior ranges */

void initVariables(LALInferenceRunState *state)
{
	//LALStatus status;
	//SimInspiralTable *injTable=NULL;
	LALInferenceVariables *priorArgs=state->priorArgs;
	state->currentParams=XLALCalloc(1,sizeof(LALInferenceVariables));
	LALInferenceVariables *currentParams=state->currentParams;
	ProcessParamsTable *commandLine=state->commandLine;
	/*REAL8 endtime;
	ProcessParamsTable *ppt=NULL;
	INT4 AmpOrder=0;
	LALPNOrder PhaseOrder=LAL_PNORDER_THREE_POINT_FIVE;
	Approximant approx=TaylorF2;
	REAL8 logDmin=log(1.0);
	REAL8 logDmax=log(100.0);
	REAL8 mcMin=1.0;
	REAL8 mcMax=20.5;
	REAL8 logmcMax,logmcMin,mMin=1.0,mMax=30.0;
	REAL8 a_spin2_max=1.0, a_spin1_max=1.0;
	REAL8 a_spin2_min=0.0, a_spin1_min=0.0;
	REAL8 phi_spin1_min=-LAL_PI;
	REAL8 phi_spin1_max=LAL_PI;
	REAL8 theta_spin1_min=0.0;
	REAL8 theta_spin1_max=LAL_PI;
	REAL8 etaMin=0.01;
	REAL8 etaMax=0.25;
	REAL8 dt=0.1;  */          /* Width of time prior */
	/*REAL8 tmpMin,tmpMax,tmpVal;
	REAL8 m1_min=0.;	
	REAL8 m1_max=0.;
	REAL8 m2_min=0.;
	REAL8 m2_max=0.;
    REAL8 mtot_min=0.0;
    REAL8 mtot_max=0.0;
	memset(currentParams,0,sizeof(LALInferenceVariables));
	memset(&status,0,sizeof(LALStatus));
	INT4 event=0;	
	INT4 i=0;
	INT4 enable_spin=0;
	INT4 aligned_spin=0;*/
	ProcessParamsTable *ppt=NULL;
	
	REAL8 tmpVal;
	//INT4 event=0;	
	//INT4 i=0;
	//INT4 AmpOrder=0;
	
	//Approximant approx=TaylorF2;
	//LALPNOrder PhaseOrder=LAL_PNORDER_THREE_POINT_FIVE;
	REAL8 beta1_min=-45;
	REAL8 beta1_max=7217;
	REAL8 beta2_min=-2030;
	REAL8 beta2_max=476;
	REAL8 beta3_min=-473;
	REAL8 beta3_max=1046;
	REAL8 beta4_min=-830;
	REAL8 beta4_max=380;
	REAL8 beta5_min=-260;
	REAL8 beta5_max=1069;
	REAL8 beta6_min=-189;
	REAL8 beta6_max=848;
	REAL8 beta7_min=-529;
	REAL8 beta7_max=187;
	double ra, dec, pol;
	REAL8 endtime;
	REAL8 dt=0.1;           /* Width of time prior */
	REAL8 tmpMin,tmpMax;
	//REAL8 Dmin=0.9;
	//REAL8 Dmax=1.1;
	char help[]="\
Parameter arguments:\n\
(--inj injections.xml)\tInjection XML file to use\n\
(--beta1_min beta1_min)\tMinimum chirp mass\n\
(--beta1_max beta1_max)\tMaximum chirp mass\n\
(--beta2_min beta2_min)\tMinimum eta\n\
(--beta2_max beta2_max)\tMaximum eta\n\
(--Dmin dist)\tMinimum distance in Mpc (1)\n\
(--Dmax dist)\tMaximum distance in Mpc (100)\n\
(--dt time)\tWidth of time prior, centred around trigger (0.1s)\n";

/*(--Mmin mchirp)\tMinimum chirp mass\n\
(--Mmax mchirp)\tMaximum chirp mass\n\
(--etamin eta)\tMinimum eta\n\
(--etamax eta)\tMaximum eta\n\
(--dt time)\tWidth of time prior, centred around trigger (0.1s)\n\
(--trigtime time)\tTrigger time to use\n\
(--Dmin dist)\tMinimum distance in Mpc (1)\n\
(--Dmax dist)\tMaximum distance in Mpc (100)\n\
(--approx ApproximantorderPN)\tSpecify a waveform to use, (default TaylorF2threePointFivePN)\n\
(--compmin min)\tMinimum component mass (1.0)\n\
(--compmax max)\tMaximum component mass (30.0)\n\
(--mtotalmin)\tMinimum total mass (2*compmin)\n\
(--mtotalmax)\tMaximum total mass (2*compmax)\n\
(--enable-spin)\tEnable spin parameters\n\
(--aligned-spin)\tUse only aligned spin parameters (uses spins between -1 and 1)\n\
(--approx ApproximantphaseOrderPN)\tSet approximant (PhenSpin implicitly enables spin)\n\
(--s1max SPIN)\tMax magnitude of spin (on both bodies!)\n\
(--s1min SPIN)\tMin magnitude of spin (on both bodies!)\n\
(--mcq)\tUse chirp mass and asymmetric mass ratio (m1/m2) as variables\n\
(--crazyinjectionhlsign)\tFlip the sign of HL signal in likelihood function\n";*/

	/* Print command line arguments if help requested */
	ppt=LALInferenceGetProcParamVal(commandLine,"--help");
	if(ppt)
	{
		fprintf(stdout,"%s",help);
		return;
	}

	
	/* Read injection XML file for parameters if specified */
	/*ppt=LALInferenceGetProcParamVal(commandLine,"--inj");
	if(ppt){
		SimInspiralTableFromLIGOLw(&injTable,ppt->value,0,0);
		if(!injTable){
			fprintf(stderr,"Unable to open injection file %s\n",ppt->value);
			exit(1);
		}
		//Select event
		ppt=LALInferenceGetProcParamVal(commandLine,"--event");
		if(ppt){
		  event = atoi(ppt->value);
		  while(i<event) {i++; injTable = injTable->next;}
		}
		endtime=XLALGPSGetREAL8(&(injTable->geocent_end_time));
        fprintf(stderr,"Read trig time %lf from injection XML file\n",endtime);
		AmpOrder=injTable->amp_order;
		XLALGetOrderFromString(injTable->waveform,&PhaseOrder);
		XLALGetApproximantFromString(injTable->waveform,&approx);
	}*/

/* Over-ride approximant if user specifies */
/*	ppt=LALInferenceGetProcParamVal(commandLine,"--approx");
	if(ppt){
		if(strstr(ppt->value,"TaylorF2")) approx=TaylorF2;
		else
		    XLALGetApproximantFromString(ppt->value,&approx);
        XLALGetOrderFromString(ppt->value,&PhaseOrder);
	}
	fprintf(stdout,"Templates will run using Approximant %i, phase order %i\n",approx,PhaseOrder);*/

	/* Over-ride end time if specified */
	ppt=LALInferenceGetProcParamVal(commandLine,"--trigtime");
	if(ppt){
		endtime=atof(ppt->value);
	}
	
	/* Over-ride time prior if specified */
	ppt=LALInferenceGetProcParamVal(commandLine,"--dt");
	if(ppt){
		dt=atof(ppt->value);
	}

	/* Over-ride Distance min if specified */
	/*ppt=LALInferenceGetProcParamVal(commandLine,"--Dmin");
	if(ppt){
		logDmin=log(atof(ppt->value));
	}*/
	
	/* Over-ride Distance max if specified */
	/*ppt=LALInferenceGetProcParamVal(commandLine,"--Dmax");
	if(ppt){
		logDmax=log(atof(ppt->value));
	}*/
	
	ppt=LALInferenceGetProcParamVal(commandLine,"--beta1_min");
	if(ppt){
		beta1_min=(atof(ppt->value));
	}
	
	/* Over-ride Distance max if specified */
	ppt=LALInferenceGetProcParamVal(commandLine,"--beta1_max");
	if(ppt){
		beta1_max=(atof(ppt->value));
	}
	
	ppt=LALInferenceGetProcParamVal(commandLine,"--beta2_min");
        if(ppt)
                beta2_min=atof(ppt->value);

        ppt=LALInferenceGetProcParamVal(commandLine,"--beta2_max");
	if(ppt)
                beta2_max=atof(ppt->value);
	
	ppt=LALInferenceGetProcParamVal(commandLine,"--beta3_min");
	if(ppt){
		beta3_min=(atof(ppt->value));
	}
	
	/* Over-ride Distance max if specified */
	ppt=LALInferenceGetProcParamVal(commandLine,"--beta3_max");
	if(ppt){
		beta3_max=(atof(ppt->value));
	}
	
	ppt=LALInferenceGetProcParamVal(commandLine,"--beta4_min");
        if(ppt)
                beta4_min=atof(ppt->value);

        ppt=LALInferenceGetProcParamVal(commandLine,"--beta4_max");
	if(ppt)
                beta4_max=atof(ppt->value);
	
	
	ppt=LALInferenceGetProcParamVal(commandLine,"--beta5_min");
	if(ppt){
		beta5_min=(atof(ppt->value));
	}
	
	/* Over-ride Distance max if specified */
	ppt=LALInferenceGetProcParamVal(commandLine,"--beta5_max");
	if(ppt){
		beta5_max=(atof(ppt->value));
	}
	
	ppt=LALInferenceGetProcParamVal(commandLine,"--beta6_min");
        if(ppt)
                beta6_min=atof(ppt->value);

        ppt=LALInferenceGetProcParamVal(commandLine,"--beta6_max");
	if(ppt)
                beta6_max=atof(ppt->value);
	
	ppt=LALInferenceGetProcParamVal(commandLine,"--beta7_min");
	if(ppt){
		beta7_min=(atof(ppt->value));
	}
	
	/* Over-ride Distance max if specified */
	ppt=LALInferenceGetProcParamVal(commandLine,"--beta7_max");
	if(ppt){
		beta7_max=(atof(ppt->value));
	}
	
	ppt=LALInferenceGetProcParamVal(commandLine,"--rightascension");
	if(ppt){
		ra=(atof(ppt->value));
	}
	
	ppt=LALInferenceGetProcParamVal(commandLine,"--declination");
	if(ppt){
		dec=(atof(ppt->value));
	}
	
	ppt=LALInferenceGetProcParamVal(commandLine,"--polarisation");
	if(ppt){
		pol=(atof(ppt->value));
	}
	

	tmpVal=0.0;

	LALInferenceAddVariable(currentParams, "beta1",       &tmpVal,             LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_LINEAR);
	LALInferenceAddVariable(currentParams, "beta2",       &tmpVal,             LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_LINEAR);
	LALInferenceAddVariable(currentParams, "beta3",       &tmpVal,             LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_LINEAR);
	LALInferenceAddVariable(currentParams, "beta4",       &tmpVal,             LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_LINEAR);
	LALInferenceAddVariable(currentParams, "beta5",       &tmpVal,             LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_LINEAR);
	LALInferenceAddVariable(currentParams, "beta6",       &tmpVal,             LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_LINEAR);
	LALInferenceAddVariable(currentParams, "beta7",       &tmpVal,             LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_LINEAR);

	
	//LALInferenceAddVariable(priorArgs,"beta1_min",&beta1_min,LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);
	//LALInferenceAddVariable(priorArgs,"beta1_max",&beta1_max,LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);
	LALInferenceAddMinMaxPrior(priorArgs, "beta1",     &beta1_min, &beta1_max,   LALINFERENCE_REAL8_t);
	
	//LALInferenceAddVariable(priorArgs,"beta2_min",&beta2_min,LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);
	//LALInferenceAddVariable(priorArgs,"beta2_max",&beta2_max,LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);
	LALInferenceAddMinMaxPrior(priorArgs, "beta2",     &beta2_min, &beta2_max,   LALINFERENCE_REAL8_t);
	
	//LALInferenceAddVariable(priorArgs,"beta3_min",&beta3_min,LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);
	//LALInferenceAddVariable(priorArgs,"beta3_max",&beta3_max,LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);
	LALInferenceAddMinMaxPrior(priorArgs, "beta3",     &beta3_min, &beta3_max,   LALINFERENCE_REAL8_t);
	
	//LALInferenceAddVariable(priorArgs,"beta4_min",&beta4_min,LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);
	//LALInferenceAddVariable(priorArgs,"beta4_max",&beta4_max,LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);
	LALInferenceAddMinMaxPrior(priorArgs, "beta4",     &beta4_min, &beta4_max,   LALINFERENCE_REAL8_t);
	
	//LALInferenceAddVariable(priorArgs,"beta5_min",&beta5_min,LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);
	//LALInferenceAddVariable(priorArgs,"beta5_max",&beta5_max,LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);
	LALInferenceAddMinMaxPrior(priorArgs, "beta5",     &beta5_min, &beta5_max,   LALINFERENCE_REAL8_t);
	
	//LALInferenceAddVariable(priorArgs,"beta6_min",&beta6_min,LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);
	//LALInferenceAddVariable(priorArgs,"beta6_max",&beta6_max,LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);
	LALInferenceAddMinMaxPrior(priorArgs, "beta6",     &beta6_min, &beta6_max,   LALINFERENCE_REAL8_t);
	
	//LALInferenceAddVariable(priorArgs,"beta7_min",&beta7_min,LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);
	//LALInferenceAddVariable(priorArgs,"beta7_max",&beta7_max,LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);
	LALInferenceAddMinMaxPrior(priorArgs, "beta7",     &beta7_min, &beta7_max,   LALINFERENCE_REAL8_t);
	
	
	
	//LALInferenceAddVariable(currentParams, "ph",       &phase,             LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_LINEAR);

	/* Over-ride Mass prior if specified */
	/*ppt=LALInferenceGetProcParamVal(commandLine,"--Mmin");
	if(ppt){
		mcMin=atof(ppt->value);
		mc2masses( mcMin,  etaMin,  &m1_min,  &m2_min);
		mMin=m2_min;
	}
	ppt=LALInferenceGetProcParamVal(commandLine,"--Mmax");
	if(ppt){	
		mcMax=atof(ppt->value);
		mc2masses(mcMax, etaMax, &m1_max, &m2_max);
		mMax=m1_max;
	}*/
	/* Over-ride Spin prior if specified*/

	/*ppt=LALInferenceGetProcParamVal(commandLine,"--s1max");
	if(ppt){
		a_spin2_max=atof(ppt->value);
		a_spin1_max=atof(ppt->value);
	}
	ppt=LALInferenceGetProcParamVal(commandLine,"--s1min");
	if(ppt){
		a_spin2_min=atof(ppt->value);
		a_spin1_min=atof(ppt->value);
	}*/
	/* Over-ride component masses */
	/*ppt=LALInferenceGetProcParamVal(commandLine,"--compmin");
	if(ppt)	mMin=atof(ppt->value);
	//fprintf(stderr,"Mmin %f, Mmax %f\n",mMin,mMax);
	LALInferenceAddVariable(priorArgs,"component_min",&mMin,LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);
	ppt=LALInferenceGetProcParamVal(commandLine,"--compmax");
	if(ppt)	mMax=atof(ppt->value);
	LALInferenceAddVariable(priorArgs,"component_max",&mMax,LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);*/
	
    /* Set the minimum and maximum total mass, using user values if specified */
   /* ppt=LALInferenceGetProcParamVal(commandLine,"--mtotalmin");
    if(ppt) mtot_min=atof(ppt->value);
    else mtot_min=2.*mMin;
    LALInferenceAddVariable(priorArgs,"MTotMin",&mtot_min,LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);

    ppt=LALInferenceGetProcParamVal(commandLine,"--mtotalmax");
    if(ppt) mtot_max=atof(ppt->value);
    else mtot_max=2.*(mMax-mMin);
    LALInferenceAddVariable(priorArgs,"MTotMax",&mtot_max,LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);

    INT4 tempint=1;
	if(LALInferenceGetProcParamVal(commandLine,"--crazyinjectionhlsign") || LALInferenceGetProcParamVal(commandLine,"--crazyInjectionHLSign"))
    {
        printf("Using signal sign flip in Hanford and Livingston");
        LALInferenceAddVariable(currentParams,"crazyInjectionHLSign",&tempint,LALINFERENCE_INT4_t,LALINFERENCE_PARAM_FIXED);
    }
	printf("Read end time %f\n",endtime);
	
	LALInferenceAddVariable(currentParams, "LAL_APPROXIMANT", &approx,        LALINFERENCE_INT4_t, LALINFERENCE_PARAM_FIXED);
    	LALInferenceAddVariable(currentParams, "LAL_PNORDER",     &PhaseOrder,        LALINFERENCE_INT4_t, LALINFERENCE_PARAM_FIXED);
	
    ppt=LALInferenceGetProcParamVal(commandLine,"--mcq");*/
   // if(ppt) /* Use MC and Q as sampling variables */
   // {
        /* Set up the variable parameters */
       /* tmpVal=mcMin+(mcMax-mcMin)/2.0;
        LALInferenceAddMinMaxPrior(priorArgs,   "chirpmass",    &mcMin, &mcMax,     LALINFERENCE_REAL8_t);
        LALInferenceAddVariable(currentParams,"chirpmass",&tmpVal, LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_LINEAR);
        tmpVal=1.5;
        REAL8 qMax=1.0;
        REAL8 qMin=mMin/mMax;
        LALInferenceAddVariable(currentParams, "asym_massratio",       &tmpVal,             LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_LINEAR);
        LALInferenceAddMinMaxPrior(priorArgs,   "asym_massratio",    &qMin,    &qMax,    LALINFERENCE_REAL8_t);

    }*/
    
    
   // else /* Use log chirp mass and eta (default) //
   // {
    	/* Set up the variable parameters */
    	//tmpVal=log(mcMin+(mcMax-mcMin)/2.0);
    	/*LALInferenceAddVariable(currentParams, "chirpmass",    &tmpVal,    LALINFERENCE_REAL8_t,	LALINFERENCE_PARAM_LINEAR);
        LALInferenceAddMinMaxPrior(priorArgs,	"chirpmass",	&mcMin,	&mcMax,		LALINFERENCE_REAL8_t); */
    	/*LALInferenceAddVariable(currentParams,"logmc",&tmpVal, LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_LINEAR);
    	logmcMin=log(mcMin); logmcMax=log(mcMax);
    	LALInferenceAddMinMaxPrior(priorArgs,	"logmc",	&logmcMin,	&logmcMax,		LALINFERENCE_REAL8_t);
    	tmpVal=0.24;
	    LALInferenceAddVariable(currentParams, "massratio",       &tmpVal,             LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_LINEAR);
    	LALInferenceAddMinMaxPrior(priorArgs,	"massratio",	&etaMin,	&etaMax,	LALINFERENCE_REAL8_t);
	}*/
	//endtime=944697615.0;
    	LALInferenceAddVariable(currentParams, "time",            &endtime   ,           LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_LINEAR); 
	tmpMin=endtime-0.5*dt; tmpMax=endtime+0.5*dt;
	//tmpMin=endtime-0.001; tmpMax=endtime+0.001;
	//LALInferenceAddVariable(priorArgs,"time_min",&tmpMin,LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);
	//LALInferenceAddVariable(priorArgs,"time_max",&tmpMax,LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);
	LALInferenceAddMinMaxPrior(priorArgs, "time",     &tmpMin, &tmpMax,   LALINFERENCE_REAL8_t);
	

	/*tmpVal=1.0;
    	LALInferenceAddVariable(currentParams, "phase",           &tmpVal,             LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_CIRCULAR);
	tmpMin=0.0; tmpMax=LAL_TWOPI;
	LALInferenceAddMinMaxPrior(priorArgs, "phase",     &tmpMin, &tmpMax,   LALINFERENCE_REAL8_t);
	
	tmpVal=logDmin+(logDmax-logDmin)/2.0;
	LALInferenceAddVariable(currentParams,"logdistance", &tmpVal, LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_LINEAR);
	LALInferenceAddMinMaxPrior(priorArgs, "logdistance",     &logDmin, &logDmax,   LALINFERENCE_REAL8_t);
	*/
	tmpVal=1.0;
	LALInferenceAddVariable(currentParams, "rightascension",  &tmpVal,      LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_CIRCULAR);
	//tmpMin=0.0; tmpMax=LAL_TWOPI;
	tmpMin=(ra)-0.1; tmpMax=(ra)+0.1;
	//LALInferenceAddVariable(priorArgs,"ra_min",&tmpMin,LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);
	//LALInferenceAddVariable(priorArgs,"ra_max",&tmpMax,LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);
	LALInferenceAddMinMaxPrior(priorArgs, "rightascension",     &tmpMin, &tmpMax,   LALINFERENCE_REAL8_t);
	//tmpVal=0.0;
	LALInferenceAddVariable(currentParams, "declination",     &tmpVal,     LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_LINEAR);
	//tmpMin=-LAL_PI/2.0; tmpMax=LAL_PI/2.0;
	tmpMin=(dec)-0.1; tmpMax=(dec)+0.1;
	//LALInferenceAddVariable(priorArgs,"dec_min",&tmpMin,LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);
	//LALInferenceAddVariable(priorArgs,"dec_max",&tmpMax,LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);
	LALInferenceAddMinMaxPrior(priorArgs, "declination",     &tmpMin, &tmpMax,   LALINFERENCE_REAL8_t);
    
	LALInferenceAddVariable(currentParams, "polarisation",    &tmpVal,     LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_CIRCULAR);
	//tmpMin=0.0; tmpMax=LAL_PI;
	tmpMin=(pol)-0.1; tmpMax=(pol)+0.1;
	//LALInferenceAddVariable(priorArgs,"polarisation_min",&tmpMin,LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);
	//LALInferenceAddVariable(priorArgs,"polarisation_max",&tmpMax,LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);
	LALInferenceAddMinMaxPrior(priorArgs, "polarisation",     &tmpMin, &tmpMax,   LALINFERENCE_REAL8_t);
	
	
	//tmpVal=Dmin+(Dmax-Dmin)/2.0;
	//LALInferenceAddVariable(currentParams,"distance", &tmpVal, LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_LINEAR);
	//LALInferenceAddMinMaxPrior(priorArgs, "distance",     &Dmin, &Dmax,   LALINFERENCE_REAL8_t);
	//LALInferenceAddVariable(priorArgs,"logdis_min",&logDmin,LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);
	//LALInferenceAddVariable(priorArgs,"logdis_max",&logDmax,LALINFERENCE_REAL8_t,LALINFERENCE_PARAM_FIXED);*/
	
 	/*LALInferenceAddVariable(currentParams, "inclination",     &tmpVal,            LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_LINEAR);
	tmpMin=0.0; tmpMax=LAL_PI;
	LALInferenceAddMinMaxPrior(priorArgs, "inclination",     &tmpMin, &tmpMax,   LALINFERENCE_REAL8_t);*/
	
	/* Additional parameters for spinning waveforms */
	/*ppt=LALInferenceGetProcParamVal(commandLine,"--template");
	if(ppt) if(!strcmp("PhenSpin",ppt->value)){ enable_spin=1;}

	if(LALInferenceGetProcParamVal(commandLine,"--enable-spin")) enable_spin=1;*/
	
	/* If aligned spins use magnitude in (-1,1) */
	/*ppt=LALInferenceGetProcParamVal(commandLine,"--aligned-spin");
	if(ppt) {enable_spin=1; aligned_spin=1; a_spin1_min=-1; a_spin2_min=-1;}
	
	if(enable_spin){
		tmpVal=a_spin1_min+(a_spin1_max-a_spin1_min)/2.0;
		LALInferenceAddVariable(currentParams, "a_spin1",		&tmpVal,	LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_LINEAR);
		LALInferenceAddMinMaxPrior(priorArgs, "a_spin1",     &a_spin1_min, &a_spin1_max,   LALINFERENCE_REAL8_t); 
	        
		tmpVal=a_spin2_min+(a_spin2_max-a_spin2_min)/2.0;
		LALInferenceAddVariable(currentParams, "a_spin2",		&tmpVal,	LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_LINEAR); 
		LALInferenceAddMinMaxPrior(priorArgs, "a_spin2",     &a_spin2_min, &a_spin2_max,   LALINFERENCE_REAL8_t); */
	
		
		//if(aligned_spin){ /* Set the spin angles to be parallel to orbital */
			/*tmpVal=0;
			LALInferenceAddVariable(currentParams,"theta_spin1",&tmpVal, LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_FIXED);
			LALInferenceAddVariable(currentParams,"theta_spin2",&tmpVal, LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_FIXED);
			LALInferenceAddVariable(currentParams,"phi_spin1",&tmpVal, LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_FIXED);
			LALInferenceAddVariable(currentParams,"phi_spin2",&tmpVal, LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_FIXED);
		}*/
		//else{ /* Use full spinning parameters */
			/*tmpVal=theta_spin1_min+(theta_spin1_max - theta_spin1_min)/2.0;

			LALInferenceAddVariable(currentParams,"theta_spin1",	&tmpVal,	LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_LINEAR);
			LALInferenceAddMinMaxPrior(priorArgs, "theta_spin1",     &theta_spin1_min, &theta_spin1_max,   LALINFERENCE_REAL8_t); 
	
			tmpVal=theta_spin1_min+(theta_spin1_max - theta_spin1_min)/2.0;
			LALInferenceAddVariable(currentParams,"theta_spin2",	&tmpVal,	LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_LINEAR);
			LALInferenceAddMinMaxPrior(priorArgs, "theta_spin2",     &theta_spin1_min, &theta_spin1_max,   LALINFERENCE_REAL8_t); 
	
			tmpVal=phi_spin1_min+(phi_spin1_max - phi_spin1_min)/2.0;
	
			LALInferenceAddVariable(currentParams,"phi_spin1",		&tmpVal,	LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_CIRCULAR);
			LALInferenceAddMinMaxPrior(priorArgs, "phi_spin1",     &phi_spin1_min, &phi_spin1_max,   LALINFERENCE_REAL8_t); 
	
			tmpVal=phi_spin1_min+(phi_spin1_max - phi_spin1_min)/2.0;
			LALInferenceAddVariable(currentParams,"phi_spin2",		&tmpVal,	LALINFERENCE_REAL8_t, LALINFERENCE_PARAM_CIRCULAR);
			LALInferenceAddMinMaxPrior(priorArgs, "phi_spin2",     &phi_spin1_min, &phi_spin1_max,   LALINFERENCE_REAL8_t);
		}
	}
	*/
	return;
}




/** \brief Initialise the MCMC proposal distribution for sampling new points
 * 
 * There are various proposal distributions that can be used to sample new live
 * points via an MCMC. A combination of different ones can be used to help
 * efficiency for awkward posterior distributions. Here the proposals that can
 * be used are:
 *   \c covariance Drawing from a multi-variate Gaussian described by the
 * covariance matrix of the current live points, with the spread of the
 * distribution controlled by the \c temperature. One parameter is evolved
 * during a single draw.
 *   \c diffev Drawing a new point by differential evolution of two randomly
 * chosen live points. All parameters are evolved during a single draw.
 *   \c kDTree Drawing points from a distributions created from a k-D tree of
 * the current live points, with probabilities of each leaf being inversely
 * their volume. All parameters are evolved during a single draw.
 * 
 * This function sets up the relative weights with which each of above
 * distributions is used.
 * 
 * \param runState [in] A pointer to the run state
*/
void initialiseProposal( LALInferenceRunState *runState ){
  ProcessParamsTable *ppt = NULL;
  UINT4 covfrac = 0, defrac = 0, kdfrac = 0;
  REAL8 temperature = 0.;
  const CHAR *defaultPropName = NULL;
  defaultPropName = XLALStringDuplicate( "none" );
  
  ppt = LALInferenceGetProcParamVal( runState->commandLine, "--covariance" );
  if( ppt ) covfrac = atoi( ppt->value );

  else covfrac = 14; /* default value */
    
  ppt = LALInferenceGetProcParamVal( runState->commandLine, "--diffev" );
  if( ppt ) defrac = atoi( ppt->value );
  else defrac = 6; /* default value */

  

  ppt = LALInferenceGetProcParamVal( runState->commandLine, "--kDTree" );
  if( ppt ) kdfrac = atoi( ppt->value );
  else kdfrac = 0; /* default value */

  if( !covfrac && !defrac && !kdfrac ){
    XLALPrintError("All proposal weights are zero!\n");
    XLAL_ERROR_VOID(XLAL_EFAILED);
  }
 
  //runState->proposalStats = NULL;
  if(!runState->proposalStats) runState->proposalStats = calloc(1,sizeof(LALInferenceVariables));
  /* add proposals */
  if( covfrac ){
    LALInferenceAddProposalToCycle( runState, covarianceEigenvectorJumpName,
                                    &LALInferenceCovarianceEigenvectorJump,
                                    covfrac );
  }
  
  if( defrac ){
    LALInferenceAddProposalToCycle( runState, differentialEvolutionFullName,
                                    &LALInferenceDifferentialEvolutionFull,
                                    defrac );
  }
  
  if( kdfrac ){
    /* set the maximum number of points in a kd-tree cell if given */
    ppt = LALInferenceGetProcParamVal( runState->commandLine, 
                                       "--kDNCell" );
    if( ppt ){
      INT4 kdncells = atoi( ppt->value );

      LALInferenceAddVariable( runState->proposalArgs, "KDNCell", 
                               &kdncells, LALINFERENCE_INT4_t,
                               LALINFERENCE_PARAM_FIXED );
    } 

    LALInferenceAddProposalToCycle( runState, KDNeighborhoodProposalName,
                                    &LALInferenceKDNeighborhoodProposal,
                                    kdfrac );
    

    LALInferenceSetupkDTreeNSLivePoints( runState );
  }
      
  LALInferenceRandomizeProposalCycle( runState );
  /* set temperature */
  ppt = LALInferenceGetProcParamVal( runState->commandLine, "--temperature" );
  if( ppt ) temperature = atof( ppt->value );
  else temperature = 0.1;
 
  LALInferenceAddVariable( runState->proposalArgs, "temperature", &temperature, 
                           LALINFERENCE_REAL8_t,
                           LALINFERENCE_PARAM_FIXED );
  
  /* add default proposal name */
  LALInferenceAddVariable( runState->proposalArgs,
    LALInferenceCurrentProposalName, &defaultPropName, LALINFERENCE_string_t,
    LALINFERENCE_PARAM_OUTPUT );
  
  /* set proposal */
  runState->proposal = LALInferenceDefaultProposal;
}




/*************** MAIN **********************/


int main(int argc, char *argv[]){
        char help[]="\
LALSMEE:\n\
Bayesian analysis tool using Nested Sampling algorithm\n\
for Supernova model analysis. Uses LALInference library for back-end.\n\n";

	LALInferenceRunState *state;
	ProcessParamsTable *procParams=NULL;

	/* Read command line and parse */
	procParams=LALInferenceParseCommandLine(argc,argv);
	
	/* initialise runstate based on command line */
	/* This includes reading in the data */
	/* And performing any injections specified */
	/* And allocating memory */
	state = initialize(procParams);
	
	/* Set template function */
	initializeTemplate(state);
	
	/* Set up structures for nested sampling */
	initializeNS(state);
	
	
	/* Set up currentParams with variables to be used */
	initVariables(state);
	
	
	
	/* Check for student-t and apply */
	

       /* Print command line arguments if help requested */
        if(LALInferenceGetProcParamVal(state->commandLine,"--help"))
        {
                fprintf(stdout,"%s",help);
		exit(0);
        }

	/* Call setupLivePointsArray() to populate live points structures */
	LALInferenceSetupLivePointsArray(state);
	initialiseProposal(state);
	/* Call nested sampling algorithm */
	state->algorithm(state);

	/* end */
	return(0);
}

