/*
thot package for statistical machine translation
Copyright (C) 2013 Daniel Ortiz-Mart\'inez
 
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public License
as published by the Free Software Foundation; either version 3
of the License, or (at your option) any later version.
 
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.
 
You should have received a copy of the GNU Lesser General Public License
along with this program; If not, see <http://www.gnu.org/licenses/>.
*/
 
/********************************************************************/
/*                                                                  */
/* Module: PhrLocalSwLiTm                                           */
/*                                                                  */
/* Definitions file: PhrLocalSwLiTm.cc                              */
/*                                                                  */
/********************************************************************/


//--------------- Include files --------------------------------------

#include "PhrLocalSwLiTm.h"
#include <_incrSwAligModel.h>

//--------------- PhrLocalSwLiTm class functions
//

//---------------------------------------
PhrLocalSwLiTm::PhrLocalSwLiTm(void):_phrSwTransModel<PhrLocalSwLiTmHypRec<HypEqClassF> >()
{
      // Initialize stepNum data member
  stepNum=0;
}

//---------------------------------------
BaseSmtModel<PhrLocalSwLiTmHypRec<HypEqClassF> >* PhrLocalSwLiTm::clone(void)
{
  return new PhrLocalSwLiTm(*this);
}

//---------------------------------
bool PhrLocalSwLiTm::loadAligModel(const char* prefixFileName)
{
  bool ret=_phrSwTransModel<PhrLocalSwLiTmHypRec<HypEqClassF> >::loadAligModel(prefixFileName);
  if(ret==THOT_ERROR) return THOT_ERROR;

      // Obtain prefix of main model
  std::string mainPrefixFileName=this->obtainMainModelAbsoluteNameFromPrefix(prefixFileName);

      // Load lambda file
  std::string lambdaFile=mainPrefixFileName;
  lambdaFile=lambdaFile+".lambda";
  ret=load_lambdas(lambdaFile.c_str());
  if(ret==THOT_ERROR) return THOT_ERROR;
    
  return THOT_OK;
}

//---------------------------------
bool PhrLocalSwLiTm::printAligModel(std::string printPrefix)
{
  bool ret=_phrSwTransModel<PhrLocalSwLiTmHypRec<HypEqClassF> >::printAligModel(printPrefix);
  if(ret==THOT_ERROR) return THOT_ERROR;

      // Obtain prefix of main model
  std::string mainPrintPrefix=this->obtainMainModelAbsoluteNameFromPrefix(printPrefix);

      // Print lambda file
  std::string lambdaFile=mainPrintPrefix;
  lambdaFile=lambdaFile+".lambda";
  ret=print_lambdas(lambdaFile.c_str());
  if(ret==THOT_ERROR) return THOT_ERROR;
  
  return THOT_OK;
}

//---------------------------------
void PhrLocalSwLiTm::clear(void)
{
  _phrSwTransModel<PhrLocalSwLiTmHypRec<HypEqClassF> >::clear();
  vecVecInvPhPair.clear();
  vecSrcSent.clear();
  vecTrgSent.clear();
  stepNum=0;
}

//---------------------------------
int PhrLocalSwLiTm::updateLinInterpWeights(std::string srcDevCorpusFileName,
                                           std::string trgDevCorpusFileName,
                                           int verbose/*=0*/)
{
      // Initialize downhill simplex input parameters
  std::vector<double> initial_weights;
  initial_weights.push_back(swModelInfoPtr->lambda_swm);
  initial_weights.push_back(swModelInfoPtr->lambda_invswm);
  int ndim=initial_weights.size();
  double* start=(double*) malloc(ndim*sizeof(double));
  int nfunk;
  double* x=(double*) malloc(ndim*sizeof(double));
  double y;

      // Create temporary file
  FILE* tmp_file=tmpfile();
  
  if(tmp_file==0)
  {
    std::cerr<<"Error updating linear interpolation weights of the phrase model, tmp file could not be created"<<std::endl;
    return THOT_ERROR;
  }

      // Extract phrase pairs from development corpus
  std::vector<std::vector<PhrasePair> > invPhrPairs;
  int ret=extractPhrPairsFromDevCorpus(srcDevCorpusFileName,trgDevCorpusFileName,invPhrPairs,verbose);
  if(ret!=THOT_OK)
    return THOT_ERROR;
  
      // Execute downhill simplex algorithm
  bool end=false;
  while(!end)
  {
        // Set initial weights (each call to step_by_step_simplex starts
        // from the initial weights)
    for(unsigned int i=0;i<initial_weights.size();++i)
      start[i]=initial_weights[i];
    
        // Execute step by step simplex
    double curr_dhs_ftol;
    ret=step_by_step_simplex(start,ndim,PHRSWLITM_DHS_FTOL,PHRSWLITM_DHS_SCALE_PAR,NULL,tmp_file,&nfunk,&y,x,&curr_dhs_ftol,false);
    switch(ret)
    {
      case THOT_OK: end=true;
        break;
      case DSO_NMAX_ERROR: std::cerr<<"Error updating linear interpolation weights of the phrase model, maximum number of iterations exceeded"<<std::endl;
        end=true;
        break;
      case DSO_EVAL_FUNC: // A new function evaluation is requested by downhill simplex
        double perp;
        int retEval=new_dhs_eval(invPhrPairs,tmp_file,x,perp);
        if(retEval==THOT_ERROR)
        {
          end=true;
          break;
        }
            // Print verbose information
        if(verbose>=1)
        {
          std::cerr<<"niter= "<<nfunk<<" ; current ftol= "<<curr_dhs_ftol<<" (FTOL="<<PHRSWLITM_DHS_FTOL<<") ; ";
          std::cerr<<"weights= "<<swModelInfoPtr->lambda_swm<<" "<<swModelInfoPtr->lambda_invswm;
          std::cerr<<" ; perp= "<<perp<<std::endl; 
        }
        break;
    }
  }
  
      // Set new weights if updating was successful
  if(ret==THOT_OK)
  {
    swModelInfoPtr->lambda_swm=start[0];
    swModelInfoPtr->lambda_invswm=start[1];
  }
  else
  {
    swModelInfoPtr->lambda_swm=initial_weights[0];
    swModelInfoPtr->lambda_invswm=initial_weights[1];
  }
  
      // Clear variables
  free(start);
  free(x);
  fclose(tmp_file);

  if(ret!=THOT_OK)
    return THOT_ERROR;
  else
    return THOT_OK; 
}


//---------------
_wbaIncrPhraseModel* PhrLocalSwLiTm::getWbaIncrPhraseModelPtr(void)
{
  _wbaIncrPhraseModel* wbaIncrPhraseModelPtr=dynamic_cast<_wbaIncrPhraseModel* >(phrModelInfoPtr->invPbModelPtr);
  return wbaIncrPhraseModelPtr;
}

//---------------
int PhrLocalSwLiTm::extractConsistentPhrasePairs(const std::vector<std::string>& srcSentStrVec,
                                                 const std::vector<std::string>& refSentStrVec,
                                                 std::vector<PhrasePair>& vecInvPhPair,
                                                 bool verbose/*=0*/)
{
      // Generate alignments
  WordAligMatrix waMatrix;
  WordAligMatrix invWaMatrix;
  
  swModelInfoPtr->swAligModelPtrVec[0]->obtainBestAlignmentVecStr(srcSentStrVec,refSentStrVec,waMatrix);
  swModelInfoPtr->invSwAligModelPtrVec[0]->obtainBestAlignmentVecStr(refSentStrVec,srcSentStrVec,invWaMatrix);
  
      // Operate alignments
  std::vector<std::string> nsrcSentStrVec=swModelInfoPtr->swAligModelPtrVec[0]->addNullWordToStrVec(srcSentStrVec);
  std::vector<std::string> nrefSentStrVec=swModelInfoPtr->swAligModelPtrVec[0]->addNullWordToStrVec(refSentStrVec);  

  waMatrix.transpose();

      // Execute symmetrization
  invWaMatrix.symmetr1(waMatrix);

      // Extract consistent pairs
  _wbaIncrPhraseModel* wbaIncrPhraseModelPtr=getWbaIncrPhraseModelPtr();
  if(wbaIncrPhraseModelPtr)
  {
    PhraseExtractParameters phePars;
    wbaIncrPhraseModelPtr->extractPhrasesFromPairPlusAlig(phePars,
                                                          nrefSentStrVec,
                                                          srcSentStrVec,
                                                          invWaMatrix,
                                                          vecInvPhPair,
                                                          verbose);
    return THOT_OK;
  }
  else
  {
        // If the model is not a subclass of _wbaIncrPhraseModel,
        // extract phrases using an instance of WbaIncrPhraseModel
    PhraseExtractParameters phePars;
    WbaIncrPhraseModel wbaIncrPhraseModel;
    wbaIncrPhraseModel.extractPhrasesFromPairPlusAlig(phePars,
                                                      nrefSentStrVec,
                                                      srcSentStrVec,
                                                      invWaMatrix,
                                                      vecInvPhPair,
                                                      verbose);
    return THOT_OK;
  }
}

//---------------
int PhrLocalSwLiTm::extractPhrPairsFromDevCorpus(std::string srcDevCorpusFileName,
                                                 std::string trgDevCorpusFileName,
                                                 std::vector<std::vector<PhrasePair> >& invPhrPairs,
                                                 int verbose/*=0*/)
{
// NOTE: this function requires the ability to extract new translation
// options. This can be achieved using the well-known phrase-extract
// algorithm.
  
  awkInputStream srcDevStream;
  awkInputStream trgDevStream;

      // Open files
  if(srcDevStream.open(srcDevCorpusFileName.c_str())==THOT_ERROR)
  {
    std::cerr<<"Unable to open file with source development sentences."<<std::endl;
    return THOT_ERROR;
  }  
  if(trgDevStream.open(trgDevCorpusFileName.c_str())==THOT_ERROR)
  {
    std::cerr<<"Unable to open file with target development sentences."<<std::endl;
    return THOT_ERROR;
  }  

      // Iterate over all sentences
  invPhrPairs.clear();
  while(srcDevStream.getln())
  {
    if(!trgDevStream.getln())
    {
      std::cerr<<"Unexpected end of file with target development sentences."<<std::endl;
      return THOT_ERROR;      
    }

        // Obtain sentence pair
    std::vector<std::string> srcSentStrVec;
    std::vector<std::string> refSentStrVec;
    Count c;

        // Extract source sentence
    for(unsigned int i=1;i<=srcDevStream.NF;++i)
      srcSentStrVec.push_back(srcDevStream.dollar(i));

        // Extract target sentence
    for(unsigned int i=1;i<=trgDevStream.NF;++i)
      refSentStrVec.push_back(trgDevStream.dollar(i));

        // Extract consistent phrase pairs
    std::vector<PhrasePair> vecInvPhPair;
    int ret=extractConsistentPhrasePairs(srcSentStrVec,refSentStrVec,vecInvPhPair,verbose);
    if(ret==THOT_ERROR)
      return THOT_ERROR;
      
        // Add vector of phrase pairs
    invPhrPairs.push_back(vecInvPhPair);
  }
    
      // Close files
  srcDevStream.close();
  trgDevStream.close();

  return THOT_OK;
}

//---------------
double PhrLocalSwLiTm::phraseModelPerplexity(const std::vector<std::vector<PhrasePair> >& invPhrPairs,
                                             int /*verbose=0*/)
{
      // Iterate over all sentences
  double loglikelihood=0;
  unsigned int numPhrPairs=0;
  
      // Obtain perplexity contribution for consistent phrase pairs
  for(unsigned int i=0;i<invPhrPairs.size();++i)
  {
    // std::cerr<<std::endl;
    for(unsigned int j=0;j<invPhrPairs[i].size();++j)
    {
      std::vector<WordIndex> srcPhrasePair=strVectorToSrcIndexVector(invPhrPairs[i][j].t_);
      std::vector<WordIndex> trgPhrasePair=strVectorToTrgIndexVector(invPhrPairs[i][j].s_);

          // Obtain unweighted score for target given source
      std::vector<Score> logptsScrVec=smoothedPhrScoreVec_t_s_(srcPhrasePair,trgPhrasePair);
      Score logptsScr=0;
      for(unsigned int k=0;k<logptsScrVec.size();++k)
        logptsScr+=logptsScrVec[k]/this->phrModelInfoPtr->phraseModelPars.ptsWeightVec[k];
      
          // Obtain unweighted score for source given target
      std::vector<Score> logpstScrVec=smoothedPhrScoreVec_s_t_(srcPhrasePair,trgPhrasePair);
      Score logpstScr=0;
      for(unsigned int k=0;k<logpstScrVec.size();++k)
        logpstScr+=logpstScrVec[k]/this->phrModelInfoPtr->phraseModelPars.pstWeightVec[k];

          // Update loglikelihood
      loglikelihood+=logptsScr+logpstScr;
        
      // for(unsigned int k=0;k<invPhrPairs[i][j].s_.size();++k)
      //   std::cerr<<invPhrPairs[i][j].s_[k]<<" ";
      // std::cerr<<"|||";
      // for(unsigned int k=0;k<invPhrPairs[i][j].t_.size();++k)
      //   std::cerr<<" "<<invPhrPairs[i][j].t_[k];
      // std::cerr<<" ||| "<<(double)logpstScr<<" "<<(double)logptsScr<<std::endl;
    }
        // Update number of phrase pairs
    numPhrPairs+=invPhrPairs[i].size();
  }

      // Return perplexity
  return -1*(loglikelihood/(double)numPhrPairs);
}


//---------------
int PhrLocalSwLiTm::new_dhs_eval(const std::vector<std::vector<PhrasePair> >& invPhrPairs,
                                 FILE* tmp_file,
                                 double* x,
                                 double& obj_func)
{
  LgProb totalLogProb;
  bool weightsArePositive=true;
  bool weightsAreBelowOne=true;
  
      // Fix weights to be evaluated
  swModelInfoPtr->lambda_swm=x[0];
  swModelInfoPtr->lambda_invswm=x[1];
  for(unsigned int i=0;i<2;++i)
  {
    if(x[i]<0) weightsArePositive=false;
    if(x[i]>=1) weightsAreBelowOne=false;
  }
  
  if(weightsArePositive && weightsAreBelowOne)
  {
        // Obtain perplexity
    obj_func=phraseModelPerplexity(invPhrPairs,obj_func);
  }
  else
  {
    obj_func=DBL_MAX;
  }
  
      // Print result to tmp file
  fprintf(tmp_file,"%g\n",obj_func);
  fflush(tmp_file);
      // step_by_step_simplex needs that the file position
      // indicator is set at the start of the stream
  rewind(tmp_file);

  return THOT_OK;
}

//---------------------------------
PhrLocalSwLiTm::Hypothesis PhrLocalSwLiTm::nullHypothesis(void)
{
  Hypothesis hyp;
  Hypothesis::DataType dataType;
  Hypothesis::ScoreInfo scoreInfo;

      // Init scoreInfo
  scoreInfo.score=0;

      // Init language model state
  langModelInfoPtr->lModelPtr->getStateForBeginOfSentence(scoreInfo.lmHist);

      // Initial word penalty lgprob
  scoreInfo.score+=sumWordPenaltyScore(0);

      // Add sentence length model contribution
  Hypothesis hypAux;
  hypAux.setData(nullHypothesisHypData());
  scoreInfo.score+=sentLenScoreForPartialHyp(hypAux.getKey(),0);

      // Set ScoreInfo
  hyp.setScoreInfo(scoreInfo);

      // Set DataType
  dataType=nullHypothesisHypData();
  hyp.setData(dataType);

  return hyp; 
}

//---------------------------------
PhrLocalSwLiTm::HypDataType
PhrLocalSwLiTm::nullHypothesisHypData(void)
{
  HypDataType dataType;

  dataType.ntarget.clear();  
  dataType.ntarget.push_back(NULL_WORD);
  dataType.sourceSegmentation.clear();  
  dataType.targetSegmentCuts.clear();

  return dataType;
}

//---------------------------------
bool PhrLocalSwLiTm::obtainPredecessorHypData(HypDataType& hypd)
{
  HypDataType predData;

  predData=hypd;
      // verify if hyp has a predecessor
  if(predData.ntarget.size()<=1) return false;
  else
  {
    unsigned int i;
    unsigned int cuts;

    if(predData.targetSegmentCuts.size()==0)
    {
      std::cerr<<"Warning: hypothesis data corrupted"<<std::endl;
      return false;
    }

        // get previous ntarget
    cuts=predData.targetSegmentCuts.size();
    if(cuts==1)
    {
      i=predData.targetSegmentCuts[0];
    }
    else
    {
      i=predData.targetSegmentCuts[cuts-1]-predData.targetSegmentCuts[cuts-2];
    }
    while(i>0)
    {
      predData.ntarget.pop_back();
      --i;
    }
        // get previous sourceSegmentation
    predData.sourceSegmentation.pop_back();
        // get previous targetSegmentCuts
    predData.targetSegmentCuts.pop_back();
        // set data
    hypd=predData;
    
    return true;
  }
}

//---------------------------------
bool PhrLocalSwLiTm::isCompleteHypData(const HypDataType& hypd)const
{
  if(numberOfUncoveredSrcWordsHypData(hypd)==0) return true;
  else return false;
}

//---------------------------------
void PhrLocalSwLiTm::setPmWeights(std::vector<float> wVec)
{
  if(wVec.size()>PTS) this->phrModelInfoPtr->phraseModelPars.ptsWeightVec[0]=this->smoothLlWeight(wVec[PTS]);
  if(wVec.size()>PST) this->phrModelInfoPtr->phraseModelPars.pstWeightVec[0]=this->smoothLlWeight(wVec[PST]);
}

//---------------------------------
void PhrLocalSwLiTm::setWeights(std::vector<float> wVec)
{
  if(wVec.size()>WPEN) langModelInfoPtr->langModelPars.wpScaleFactor=smoothLlWeight(wVec[WPEN]);
  if(wVec.size()>LMODEL) langModelInfoPtr->langModelPars.lmScaleFactor=smoothLlWeight(wVec[LMODEL]);
  if(wVec.size()>TSEGMLEN) phrModelInfoPtr->phraseModelPars.trgSegmLenWeight=smoothLlWeight(wVec[TSEGMLEN]);
  if(wVec.size()>SJUMP) phrModelInfoPtr->phraseModelPars.srcJumpWeight=smoothLlWeight(wVec[SJUMP]);
  if(wVec.size()>SSEGMLEN) phrModelInfoPtr->phraseModelPars.srcSegmLenWeight=smoothLlWeight(wVec[SSEGMLEN]);
  setPmWeights(wVec);
  if(wVec.size()>getNumWeights()-1) swModelInfoPtr->invSwModelPars.lenWeight=smoothLlWeight(wVec[getNumWeights()-1]);
}

//---------------------------------
void PhrLocalSwLiTm::getPmWeights(std::vector<std::pair<std::string,float> >& compWeights)
{
  std::pair<std::string,float> compWeight;
    
  compWeight.first="ptsw";
  compWeight.second=this->phrModelInfoPtr->phraseModelPars.ptsWeightVec[0];
  compWeights.push_back(compWeight);
  
  compWeight.first="pstw";
  compWeight.second=this->phrModelInfoPtr->phraseModelPars.pstWeightVec[0];
  compWeights.push_back(compWeight);
}

//---------------------------------
void PhrLocalSwLiTm::getWeights(std::vector<std::pair<std::string,float> >& compWeights)
{
  compWeights.clear();
  
  std::pair<std::string,float> compWeight;

  compWeight.first="wpw";
  compWeight.second=langModelInfoPtr->langModelPars.wpScaleFactor;
  compWeights.push_back(compWeight);

  compWeight.first="lmw";
  compWeight.second=langModelInfoPtr->langModelPars.lmScaleFactor;
  compWeights.push_back(compWeight);

  compWeight.first="tseglenw";
  compWeight.second=phrModelInfoPtr->phraseModelPars.trgSegmLenWeight;
  compWeights.push_back(compWeight);

  compWeight.first="sjumpw";
  compWeight.second=phrModelInfoPtr->phraseModelPars.srcJumpWeight;
  compWeights.push_back(compWeight);

  compWeight.first="sseglenw";
  compWeight.second=phrModelInfoPtr->phraseModelPars.srcSegmLenWeight;
  compWeights.push_back(compWeight);

  getPmWeights(compWeights);

  compWeight.first="swlenliw";
  compWeight.second=swModelInfoPtr->invSwModelPars.lenWeight;
  compWeights.push_back(compWeight);
}

//---------------------------------
void PhrLocalSwLiTm::printPmWeights(std::ostream &outS)
{
  if(!phrModelInfoPtr->phraseModelPars.ptsWeightVec.empty())
    outS<<"ptsw: "<<phrModelInfoPtr->phraseModelPars.ptsWeightVec[0] <<" , ";
  else
    outS<<"ptsw: "<<DEFAULT_PTS_WEIGHT<<" , ";
    
  if(!phrModelInfoPtr->phraseModelPars.pstWeightVec.empty())
    outS<<"pstw: "<<phrModelInfoPtr->phraseModelPars.pstWeightVec[0];
  else
    outS<<"pstw: "<<DEFAULT_PST_WEIGHT;
}

//---------------------------------
void PhrLocalSwLiTm::printWeights(std::ostream &outS)
{
  outS<<"wpw: "<<langModelInfoPtr->langModelPars.wpScaleFactor<<" , ";
  outS<<"lmw: "<<langModelInfoPtr->langModelPars.lmScaleFactor<<" , ";
  outS<<"tseglenw: "<<phrModelInfoPtr->phraseModelPars.trgSegmLenWeight<<" , ";
  outS<<"sjumpw: "<<phrModelInfoPtr->phraseModelPars.srcJumpWeight <<" , ";
  outS<<"sseglenw: "<<phrModelInfoPtr->phraseModelPars.srcSegmLenWeight<<" , ";
  printPmWeights(outS);
  outS<<" , ";
  outS<<"swlenliw: "<<swModelInfoPtr->invSwModelPars.lenWeight;
}

//---------------------------------
unsigned int PhrLocalSwLiTm::getNumWeights(void)
{
  return 8;
}

//---------------------------------
void PhrLocalSwLiTm::setOnlineTrainingPars(OnlineTrainingPars _onlineTrainingPars,
                                           int verbose)
{
      // Invoke base class function
  _phrSwTransModel<PhrLocalSwLiTmHypRec<HypEqClassF> >::setOnlineTrainingPars(_onlineTrainingPars,verbose);
    
      // Set R parameter for the direct and the inverse single word models
  _incrSwAligModel<PpInfo>* _incrSwAligModelPtr=
    dynamic_cast<_incrSwAligModel<PpInfo>*>(swModelInfoPtr->swAligModelPtrVec[0]);

  _incrSwAligModel<PpInfo>* _incrInvSwAligModelPtr=
    dynamic_cast<_incrSwAligModel<PpInfo>*>(swModelInfoPtr->invSwAligModelPtrVec[0]);

  if(_incrSwAligModelPtr && _incrInvSwAligModelPtr)
  {
    _incrSwAligModelPtr->set_expval_maxnsize(onlineTrainingPars.R_par);
    _incrInvSwAligModelPtr->set_expval_maxnsize(onlineTrainingPars.R_par);
  }
}

//---------------------------------
int PhrLocalSwLiTm::onlineTrainFeatsSentPair(const char *srcSent,
                                             const char *refSent,
                                             const char *sysSent,
                                             int verbose)
{
      // Check if input sentences are empty
  if(strlen(srcSent)==0 || strlen(refSent)==0)
  {
    std::cerr<<"Error: cannot process empty input sentences"<<std::endl;
    return THOT_ERROR;
  }

      // Train pair according to chosen algorithm
  switch(onlineTrainingPars.onlineLearningAlgorithm)
  {
    case BASIC_INCR_TRAINING:
      return incrTrainFeatsSentPair(srcSent,refSent,verbose);
      break;
    case MINIBATCH_TRAINING:
      return minibatchTrainFeatsSentPair(srcSent,refSent,sysSent,verbose);
      break;
    case BATCH_RETRAINING:
      return batchRetrainFeatsSentPair(srcSent,refSent,verbose);
      break;
    default:
      std::cerr<<"Warning: requested online learning algoritm with id="<<onlineTrainingPars.onlineLearningAlgorithm<<" is not implemented."<<std::endl;
      return THOT_ERROR;
      break;
  }
}

//---------------------------------
int PhrLocalSwLiTm::incrTrainFeatsSentPair(const char *srcSent,
                                           const char *refSent,
                                           int verbose/*=0*/)
{
  int ret;
  std::vector<std::string> srcSentStrVec=StrProcUtils::charItemsToVector(srcSent);
  std::vector<std::string> refSentStrVec=StrProcUtils::charItemsToVector(refSent);
  std::pair<unsigned int,unsigned int> sentRange;

      // Train language model
  if(verbose) std::cerr<<"Training language model..."<<std::endl;
  ret=langModelInfoPtr->lModelPtr->trainSentence(refSentStrVec,onlineTrainingPars.learnStepSize,0,verbose);
  if(ret==THOT_ERROR) return THOT_ERROR;

      // Revise vocabularies of the alignment models
  updateAligModelsSrcVoc(srcSentStrVec);
  updateAligModelsTrgVoc(refSentStrVec);

      // Add sentence pair to the single word models
  swModelInfoPtr->swAligModelPtrVec[0]->addSentPair(srcSentStrVec,refSentStrVec,onlineTrainingPars.learnStepSize,sentRange);
  swModelInfoPtr->invSwAligModelPtrVec[0]->addSentPair(refSentStrVec,srcSentStrVec,onlineTrainingPars.learnStepSize,sentRange);

      // Iterate over E_par interlaced samples
  unsigned int curr_sample=sentRange.second;
  unsigned int oldest_sample=curr_sample-onlineTrainingPars.R_par;
  for(unsigned int i=1;i<=onlineTrainingPars.E_par;++i)
  {
    int n=oldest_sample+(i*(onlineTrainingPars.R_par/onlineTrainingPars.E_par));
    if(n>=0)
    {
      if(verbose)
        std::cerr<<"Alig. model training iteration over sample "<<n<<" ..."<<std::endl;

          // Train sw model
      if(verbose) std::cerr<<"Training single-word model..."<<std::endl;
      swModelInfoPtr->swAligModelPtrVec[0]->trainSentPairRange(std::make_pair(n,n),verbose);

          // Train inverse sw model
      if(verbose) std::cerr<<"Training inverse single-word model..."<<std::endl;
      swModelInfoPtr->invSwAligModelPtrVec[0]->trainSentPairRange(std::make_pair(n,n),verbose);

          // Add new translation options
      if(verbose) std::cerr<<"Adding new translation options..."<<std::endl;
      ret=addNewTransOpts(n,verbose);
    }
  }

      // Discard unnecessary phrase-based model sufficient statistics
  int last_n=curr_sample-((onlineTrainingPars.E_par-1)*(onlineTrainingPars.R_par/onlineTrainingPars.E_par));
  if(last_n>=0)
  {
    int mapped_last_n=map_n_am_suff_stats(last_n);
    int idx_to_discard=mapped_last_n;
    if(idx_to_discard>0 && vecVecInvPhPair.size()>(unsigned int)idx_to_discard)
      vecVecInvPhPair[idx_to_discard].clear();
  }

  return ret;
}

//---------------------------------
int PhrLocalSwLiTm::minibatchTrainFeatsSentPair(const char *srcSent,
                                                const char *refSent,
                                                const char *sysSent,
                                                int verbose/*=0*/)
{
  std::vector<std::string> srcSentStrVec=StrProcUtils::charItemsToVector(srcSent);
  std::vector<std::string> trgSentStrVec=StrProcUtils::charItemsToVector(refSent);
  std::vector<std::string> sysSentStrVec=StrProcUtils::charItemsToVector(sysSent);

      // Store source and target sentences
  vecSrcSent.push_back(srcSentStrVec);
  vecTrgSent.push_back(trgSentStrVec);
  vecSysSent.push_back(sysSentStrVec);
  
      // Check if a mini-batch has to be processed
      // (onlineTrainingPars.learnStepSize determines the size of the
      // mini-batch)
  unsigned int minibatchSize=(unsigned int)onlineTrainingPars.learnStepSize;
  if(!vecSrcSent.empty() &&
     (vecSrcSent.size()%minibatchSize)==0)
  {    
    std::vector<WordAligMatrix> invWaMatrixVec;
    std::pair<unsigned int,unsigned int> sentRange;
    float learningRate=calculateNewLearningRate(verbose);
       
    for(unsigned int n=0;n<vecSrcSent.size();++n)
    {
          // Revise vocabularies of the alignment models
      updateAligModelsSrcVoc(vecSrcSent[n]);
      updateAligModelsTrgVoc(vecTrgSent[n]);

          // Add sentence pair to the single word models
      swModelInfoPtr->swAligModelPtrVec[0]->addSentPair(vecSrcSent[n],vecTrgSent[n],1,sentRange);
      swModelInfoPtr->invSwAligModelPtrVec[0]->addSentPair(vecTrgSent[n],vecSrcSent[n],1,sentRange);
    }

        // Initialize minibatchSentRange variable
    std::pair<unsigned int,unsigned int> minibatchSentRange;
    minibatchSentRange.first=sentRange.second-minibatchSize+1;
    minibatchSentRange.second=sentRange.second;

    if(verbose)
      std::cerr<<"Processing mini-batch of size "<<minibatchSize<<" , "<<minibatchSentRange.first<<" - "<<minibatchSentRange.second<<std::endl;

        // Set learning rate for sw model if possible
    BaseStepwiseAligModel* bswamPtr=dynamic_cast<BaseStepwiseAligModel*>(swModelInfoPtr->swAligModelPtrVec[0]);
    if(bswamPtr) bswamPtr->set_nu_val(learningRate);

        // Train sw model
    if(verbose) std::cerr<<"Training single-word model..."<<std::endl;
    for(unsigned int i=0;i<onlineTrainingPars.emIters;++i)
    {
      swModelInfoPtr->swAligModelPtrVec[0]->trainSentPairRange(minibatchSentRange,verbose);
    }

        // Set learning rate for inverse sw model if possible
    BaseStepwiseAligModel* ibswamPtr=dynamic_cast<BaseStepwiseAligModel*>(swModelInfoPtr->invSwAligModelPtrVec[0]);
    if(ibswamPtr) ibswamPtr->set_nu_val(learningRate);

        // Train inverse sw model
    if(verbose) std::cerr<<"Training inverse single-word model..."<<std::endl;
    for(unsigned int i=0;i<onlineTrainingPars.emIters;++i)
    {
      swModelInfoPtr->invSwAligModelPtrVec[0]->trainSentPairRange(minibatchSentRange,verbose);
    }

        // Generate word alignments
    if(verbose) std::cerr<<"Generating word alignments..."<<std::endl;
    for(unsigned int n=0;n<vecSrcSent.size();++n)
    {
          // Generate alignments
      WordAligMatrix waMatrix;
      WordAligMatrix invWaMatrix;
  
      swModelInfoPtr->swAligModelPtrVec[0]->obtainBestAlignmentVecStr(vecSrcSent[n],vecTrgSent[n],waMatrix);
      swModelInfoPtr->invSwAligModelPtrVec[0]->obtainBestAlignmentVecStr(vecTrgSent[n],vecSrcSent[n],invWaMatrix);
  
          // Operate alignments
      std::vector<std::string> nrefSentStrVec=swModelInfoPtr->swAligModelPtrVec[0]->addNullWordToStrVec(vecTrgSent[n]);  

      waMatrix.transpose();

          // Execute symmetrization
      invWaMatrix.symmetr1(waMatrix);
      if(verbose)
      {
        printAlignmentInGIZAFormat(std::cerr,nrefSentStrVec,vecSrcSent[n],invWaMatrix,"Operated word alignment for phrase model training:");
      }

          // Store word alignment matrix
      invWaMatrixVec.push_back(invWaMatrix);
    }

        // Train phrase-based model
    _wbaIncrPhraseModel* wbaIncrPhraseModelPtr=getWbaIncrPhraseModelPtr();
    if(wbaIncrPhraseModelPtr)
    {
      if(verbose) std::cerr<<"Training phrase-based model..."<<std::endl;
      PhraseExtractParameters phePars;
      wbaIncrPhraseModelPtr->extModelFromPairAligVec(phePars,false,vecTrgSent,vecSrcSent,invWaMatrixVec,(Count)learningRate,verbose);
    }
    
        // Train language model
    if(verbose) std::cerr<<"Training language model..."<<std::endl;    
    langModelInfoPtr->lModelPtr->trainSentenceVec(vecTrgSent,(Count)learningRate,(Count)0,verbose);

        // Clear vectors with source and target sentences
    vecSrcSent.clear();
    vecTrgSent.clear();
    vecSysSent.clear();

        // Increase stepNum
    ++stepNum;
  }
  
  return THOT_OK;
}

//---------------------------------
int PhrLocalSwLiTm::batchRetrainFeatsSentPair(const char *srcSent,
                                              const char *refSent,
                                              int verbose/*=0*/)
{
  std::vector<std::string> srcSentStrVec=StrProcUtils::charItemsToVector(srcSent);
  std::vector<std::string> trgSentStrVec=StrProcUtils::charItemsToVector(refSent);

      // Store source and target sentences
  vecSrcSent.push_back(srcSentStrVec);
  vecTrgSent.push_back(trgSentStrVec);
  
      // Check if a batch has to be processed
      // (onlineTrainingPars.learnStepSize determines the number of samples
      // that are to be seen before retraining)
  unsigned int batchSize=(unsigned int)onlineTrainingPars.learnStepSize;
  if(!vecSrcSent.empty() &&
     (vecSrcSent.size()%batchSize)==0)
  {    
    std::vector<WordAligMatrix> invWaMatrixVec;
    std::pair<unsigned int,unsigned int> sentRange;
    float learningRate=1;
      
        // Batch learning is being performed, clear models
    if(verbose) std::cerr<<"Clearing previous model..."<<std::endl;
    swModelInfoPtr->swAligModelPtrVec[0]->clear();
    swModelInfoPtr->invSwAligModelPtrVec[0]->clear();
    phrModelInfoPtr->invPbModelPtr->clear();
    langModelInfoPtr->lModelPtr->clear();

    for(unsigned int n=0;n<vecSrcSent.size();++n)
    {
          // Revise vocabularies of the alignment models
      updateAligModelsSrcVoc(vecSrcSent[n]);
      updateAligModelsTrgVoc(vecTrgSent[n]);

          // Add sentence pair to the single word models
      swModelInfoPtr->swAligModelPtrVec[0]->addSentPair(vecSrcSent[n],vecTrgSent[n],1,sentRange);
      swModelInfoPtr->invSwAligModelPtrVec[0]->addSentPair(vecTrgSent[n],vecSrcSent[n],1,sentRange);
    }

        // Initialize batchSentRange variable
    std::pair<unsigned int,unsigned int> batchSentRange;
    batchSentRange.first=0;
    batchSentRange.second=sentRange.second;

    if(verbose)
      std::cerr<<"Processing batch of size "<<batchSentRange.second-batchSentRange.first+1<<" , "<<batchSentRange.first<<" - "<<batchSentRange.second<<std::endl;

        // Set learning rate for sw model if possible
    BaseStepwiseAligModel* bswamPtr=dynamic_cast<BaseStepwiseAligModel*>(swModelInfoPtr->swAligModelPtrVec[0]);
    if(bswamPtr) bswamPtr->set_nu_val(learningRate);

        // Train sw model
    if(verbose) std::cerr<<"Training single-word model..."<<std::endl;
    for(unsigned int i=0;i<onlineTrainingPars.emIters;++i)
    {
          // Execute batch training
      _incrSwAligModel<PpInfo>* iswamPtr=
        dynamic_cast<_incrSwAligModel<PpInfo>*>(swModelInfoPtr->swAligModelPtrVec[0]);

      if(iswamPtr) iswamPtr->efficientBatchTrainingForRange(batchSentRange,verbose);
    }

        // Set learning rate for inverse sw model if possible
    BaseStepwiseAligModel* ibswamPtr=dynamic_cast<BaseStepwiseAligModel*>(swModelInfoPtr->invSwAligModelPtrVec[0]);
    if(ibswamPtr) ibswamPtr->set_nu_val(learningRate);

        // Train inverse sw model
    if(verbose) std::cerr<<"Training inverse single-word model..."<<std::endl;
    for(unsigned int i=0;i<onlineTrainingPars.emIters;++i)
    {
          // Execute batch training
      _incrSwAligModel<PpInfo>* iswamPtr=dynamic_cast<_incrSwAligModel<PpInfo>*>(swModelInfoPtr->invSwAligModelPtrVec[0]);
      if(iswamPtr) iswamPtr->efficientBatchTrainingForRange(batchSentRange,verbose);
    }

        // Generate word alignments
    if(verbose) std::cerr<<"Generating word alignments..."<<std::endl;
    for(unsigned int n=0;n<vecSrcSent.size();++n)
    {
          // Generate alignments
      WordAligMatrix waMatrix;
      WordAligMatrix invWaMatrix;
  
      swModelInfoPtr->swAligModelPtrVec[0]->obtainBestAlignmentVecStr(vecSrcSent[n],vecTrgSent[n],waMatrix);
      swModelInfoPtr->invSwAligModelPtrVec[0]->obtainBestAlignmentVecStr(vecTrgSent[n],vecSrcSent[n],invWaMatrix);
  
          // Operate alignments
      std::vector<std::string> nrefSentStrVec=swModelInfoPtr->swAligModelPtrVec[0]->addNullWordToStrVec(vecTrgSent[n]);  

      waMatrix.transpose();

          // Execute symmetrization
      invWaMatrix.symmetr1(waMatrix);
      if(verbose)
      {
        printAlignmentInGIZAFormat(std::cerr,nrefSentStrVec,vecSrcSent[n],invWaMatrix,"Operated word alignment for phrase model training:");
      }

          // Store word alignment matrix
      invWaMatrixVec.push_back(invWaMatrix);
    }

        // Train phrase-based model
    _wbaIncrPhraseModel* wbaIncrPhraseModelPtr=getWbaIncrPhraseModelPtr();
    if(wbaIncrPhraseModelPtr)
    {
      if(verbose) std::cerr<<"Training phrase-based model..."<<std::endl;
      PhraseExtractParameters phePars;
      wbaIncrPhraseModelPtr->extModelFromPairAligVec(phePars,false,vecTrgSent,vecSrcSent,invWaMatrixVec,(Count)learningRate,verbose);
    }
        // Train language model
    if(verbose) std::cerr<<"Training language model..."<<std::endl;    
    langModelInfoPtr->lModelPtr->trainSentenceVec(vecTrgSent,(Count)learningRate,(Count)0,verbose);
  }
  
  return THOT_OK;
}

//---------------------------------
float PhrLocalSwLiTm::calculateNewLearningRate(int verbose/*=0*/)

{
  if(verbose) std::cerr<<"Calculating new learning rate..."<<std::endl;
                
  float lr;
  
  switch(onlineTrainingPars.learningRatePolicy)
  {
    float alpha;
    float par1;
    float par2;
    case FIXED_LEARNING_RATE_POL:
      if(verbose) std::cerr<<"Using fixed learning rate."<<std::endl;
      lr=PHRSWLITM_DEFAULT_LR;
      break;
    case LIANG_LEARNING_RATE_POL:
      if(verbose) std::cerr<<"Using Liang learning rate."<<std::endl;
      alpha=PHRSWLITM_DEFAULT_LR_ALPHA_PAR;
      lr=1.0/(float)pow((float)stepNum+2,(float)alpha);
      break;
    case OWN_LEARNING_RATE_POL:
      if(verbose) std::cerr<<"Using own learning rate."<<std::endl;
      par1=PHRSWLITM_DEFAULT_LR_PAR1;
      par2=PHRSWLITM_DEFAULT_LR_PAR2;
      lr=par1/(1.0+((float)stepNum/par2));
      break;
    case WER_BASED_LEARNING_RATE_POL:
      if(verbose) std::cerr<<"Using WER-based learning rate."<<std::endl;
      lr=werBasedLearningRate(verbose);
      break;
    default:
      lr=PHRSWLITM_DEFAULT_LR;
      break;
  }

  if(verbose)
    std::cerr<<"New learning rate: "<<lr<<std::endl;

  if(lr>=1) std::cerr<<"WARNING: learning rate greater or equal than 1.0!"<<std::endl;
  
  return lr;
}

//---------------------------------
float PhrLocalSwLiTm::werBasedLearningRate(int verbose/*=0*/)
{
  EditDistForVec<std::string> edDistVecStr;
  unsigned int hCount,iCount,sCount,dCount;
  unsigned int totalOps=0;
  unsigned int totalTrgWords=0;
  float wer;
  float lr;
   
      // Set error model
  edDistVecStr.setErrorModel(0,1,1,1);

  for(unsigned int n=0;n<vecTrgSent.size();++n)
  {
    double dist=edDistVecStr.calculateEditDistOps(vecTrgSent[n],vecSysSent[n],hCount,iCount,sCount,dCount,0);
    unsigned int ops=(unsigned int) dist;
    unsigned int trgWords=vecTrgSent[n].size();
    totalOps+=ops;
    totalTrgWords+=trgWords;
    if(verbose)
    {
      std::cerr<<"Sentence pair "<<n;
      std::cerr<<" ; PARTIAL WER= "<<(float)ops/trgWords<<" ( "<<ops<<" , "<<trgWords<<" )";
      std::cerr<<" ; ACUM WER= "<<(float)totalOps/totalTrgWords<<" ( "<<totalOps<<" , "<<totalTrgWords<<" )"<<std::endl;
    }
  }

      // Obtain WER for block of sentences
  if(totalTrgWords>0) wer=(float)totalOps/totalTrgWords;
  else wer=0;

      // Obtain learning rate
  lr=wer-PHRSWLITM_LR_RESID_WER;
  if(lr>0.999) lr=0.999;
  if(lr<0.001) lr=0.001;
   
  if(verbose)
    std::cerr<<"WER of block: "<<wer<<std::endl;
   
  return lr;
}

//---------------------------------
unsigned int PhrLocalSwLiTm::map_n_am_suff_stats(unsigned int n)
{
  return n;
}

//---------------------------------
int PhrLocalSwLiTm::addNewTransOpts(unsigned int n,
                                    int verbose/*=0*/)
{
// NOTE: a complete training step requires the addition of new
// translation options. This can be achieved using the well-known
// phrase-extract algorithm. The required functionality is only
// implemented at this moment by the pb models deriving from the
// _wbaIncrPhraseModel class

  _wbaIncrPhraseModel* wbaIncrPhraseModelPtr=getWbaIncrPhraseModelPtr();
  if(wbaIncrPhraseModelPtr)
  {
        // Obtain sentence pair
    std::vector<std::string> srcSentStrVec;
    std::vector<std::string> refSentStrVec;
    Count c;
    swModelInfoPtr->swAligModelPtrVec[0]->nthSentPair(n,srcSentStrVec,refSentStrVec,c);

        // Extract consistent phrase pairs
    std::vector<PhrasePair> vecInvPhPair;
    extractConsistentPhrasePairs(srcSentStrVec,refSentStrVec,vecInvPhPair,verbose);

        // Obtain mapped_n
    unsigned int mapped_n=map_n_am_suff_stats(n);
  
        // Grow vecVecInvPhPair if necessary
    std::vector<PhrasePair> vpp;
    while(vecVecInvPhPair.size()<=mapped_n) vecVecInvPhPair.push_back(vpp);
    
        // Subtract current phrase model sufficient statistics
    for(unsigned int i=0;i<vecVecInvPhPair[mapped_n].size();++i)
    {
      wbaIncrPhraseModelPtr->strIncrCountsOfEntry(vecVecInvPhPair[mapped_n][i].s_,
                                                  vecVecInvPhPair[mapped_n][i].t_,
                                                  -1);
    }

        // Add new phrase model current sufficient statistics
    if(verbose) std::cerr<<"List of extracted consistent phrase pairs:"<<std::endl;
    for(unsigned int i=0;i<vecInvPhPair.size();++i)
    {
      wbaIncrPhraseModelPtr->strIncrCountsOfEntry(vecInvPhPair[i].s_,
                                                  vecInvPhPair[i].t_,
                                                  1);
      if(verbose)
      {
        for(unsigned int j=0;j<vecInvPhPair[i].s_.size();++j) std::cerr<<vecInvPhPair[i].s_[j]<<" ";
        std::cerr<<"|||";
        for(unsigned int j=0;j<vecInvPhPair[i].t_.size();++j) std::cerr<<" "<<vecInvPhPair[i].t_[j];
        std::cerr<<std::endl;
      }
    }
  
        // Store new phrase model current sufficient statistics
    vecVecInvPhPair[mapped_n]=vecInvPhPair;

    return THOT_OK;
  }
  else
  {
    std::cerr<<"Warning: addition of new translation options not supported in this configuration!"<<std::endl;
    return THOT_ERROR;
  }
}

//---------------------------------
bool PhrLocalSwLiTm::load_lambdas(const char* lambdaFileName)
{
  awkInputStream awk;
  
  if(awk.open(lambdaFileName)==THOT_ERROR)
  {
    std::cerr<<"Error in file containing the lambda value, file "<<lambdaFileName<<" does not exist. Current values-> lambda_swm="<<swModelInfoPtr->lambda_swm<<" , lambda_invswm="<<swModelInfoPtr->lambda_invswm<<std::endl;
    return THOT_OK;
  }
  else
  {
    if(awk.getln())
    {
      if(awk.NF==1)
      {
        swModelInfoPtr->lambda_swm=atof(awk.dollar(1).c_str());
        swModelInfoPtr->lambda_invswm=atof(awk.dollar(1).c_str());
        std::cerr<<"Read lambda value from file: "<<lambdaFileName<<" (lambda_swm="<<swModelInfoPtr->lambda_swm<<", lambda_invswm="<<swModelInfoPtr->lambda_invswm<<")"<<std::endl;
        return THOT_OK;
      }
      else
      {
        if(awk.NF==2)
        {
          swModelInfoPtr->lambda_swm=atof(awk.dollar(1).c_str());
          swModelInfoPtr->lambda_invswm=atof(awk.dollar(2).c_str());
          std::cerr<<"Read lambda value from file: "<<lambdaFileName<<" (lambda_swm="<<swModelInfoPtr->lambda_swm<<", lambda_invswm="<<swModelInfoPtr->lambda_invswm<<")"<<std::endl;
          return THOT_OK;
        }
        else
        {
          std::cerr<<"Anomalous file with lambda values."<<std::endl;
          return THOT_ERROR;
        }
      }
    }
    else
    {
      std::cerr<<"Anomalous file with lambda values."<<std::endl;
      return THOT_ERROR;
    }
  }  
  return THOT_OK;
}

//---------------------------------
bool PhrLocalSwLiTm::print_lambdas(const char* lambdaFileName)
{
  std::ofstream outF;

  outF.open(lambdaFileName,std::ios::out);
  if(!outF)
  {
    std::cerr<<"Error while printing file with lambda values."<<std::endl;
    return THOT_ERROR;
  }
  else
  {
    print_lambdas(outF);
    outF.close();	
    return THOT_OK;
  }   
}

//-------------------------
std::ostream& PhrLocalSwLiTm::print_lambdas(std::ostream &outS)
{
  outS<<swModelInfoPtr->lambda_swm<<" "<<swModelInfoPtr->lambda_invswm<<std::endl;
  return outS;
}

//---------------------------------
unsigned int
PhrLocalSwLiTm::numberOfUncoveredSrcWordsHypData(const HypDataType& hypd)const
{
  unsigned int k,n;

  n=0;
  for(k=0;k<hypd.sourceSegmentation.size();k++)
	n+=hypd.sourceSegmentation[k].second-hypd.sourceSegmentation[k].first+1; 

  return (pbtmInputVars.srcSentVec.size()-n);  
}

//---------------------------------
Score PhrLocalSwLiTm::incrScore(const Hypothesis& pred_hyp,
                                const HypDataType& new_hypd,
                                Hypothesis& new_hyp,
                                std::vector<Score>& scoreComponents)
{
  HypScoreInfo hypScoreInfo=pred_hyp.getScoreInfo();
  HypDataType pred_hypd=pred_hyp.getData();
  unsigned int trglen=pred_hypd.ntarget.size()-1;
  Bitset<MAX_SENTENCE_LENGTH_ALLOWED> hypKey=pred_hyp.getKey();
    
      // Init scoreComponents
  scoreComponents.clear();
  for(unsigned int i=0;i<getNumWeights();++i) scoreComponents.push_back(0);
  
  for(unsigned int i=pred_hypd.sourceSegmentation.size();i<new_hypd.sourceSegmentation.size();++i)
  {
        // Source segment is not present in the previous data
    unsigned int srcLeft=new_hypd.sourceSegmentation[i].first;
    unsigned int srcRight=new_hypd.sourceSegmentation[i].second;
    unsigned int trgLeft;
    unsigned int trgRight;
    std::vector<WordIndex> trgphrase;
    std::vector<WordIndex> s_;
      
    trgRight=new_hypd.targetSegmentCuts[i];
    if(i==0) trgLeft=1;
    else trgLeft=new_hypd.targetSegmentCuts[i-1]+1;
    for(unsigned int k=trgLeft;k<=trgRight;++k)
    {
      trgphrase.push_back(new_hypd.ntarget[k]);
    }
        // Calculate new sum word penalty score
    scoreComponents[WPEN]-=sumWordPenaltyScore(trglen);
    scoreComponents[WPEN]+=sumWordPenaltyScore(trglen+trgphrase.size());

        // Obtain language model score
    scoreComponents[LMODEL]+=getNgramScoreGivenState(trgphrase,hypScoreInfo.lmHist);

        // target segment length score
    scoreComponents[TSEGMLEN]+=this->trgSegmLenScore(trglen+trgphrase.size(),trglen,0);

        // phrase alignment score      
    int lastSrcPosStart=srcLeft;
    int prevSrcPosEnd;
    if(i>0) prevSrcPosEnd=new_hypd.sourceSegmentation[i-1].second;
    else prevSrcPosEnd=0;
    scoreComponents[SJUMP]+=this->srcJumpScore(abs(lastSrcPosStart-(prevSrcPosEnd+1))); 

        // source segment length score
    scoreComponents[SSEGMLEN]+=srcSegmLenScore(i,new_hypd.sourceSegmentation,this->pbtmInputVars.srcSentVec.size(),trgphrase.size());

        // Obtain translation score
    for(unsigned int k=srcLeft;k<=srcRight;++k)
    {
      s_.push_back(pbtmInputVars.nsrcSentIdVec[k]);
    }

    // p(t_|s_) smoothed phrase score
    std::vector<Score> logptsScrVec=smoothedPhrScoreVec_t_s_(s_,trgphrase);
    for(unsigned int i=0;i<logptsScrVec.size();++i)
      scoreComponents[PTS+i]+=logptsScrVec[i];

    // p(s_|t_) smoothed phrase score
    std::vector<Score> logpstScrVec=smoothedPhrScoreVec_s_t_(s_,trgphrase);
    for(unsigned int i=0;i<logpstScrVec.size();++i)
      scoreComponents[PTS+logptsScrVec.size()+i]+=logpstScrVec[i];

        // Calculate sentence length model contribution
    scoreComponents[PTS+logptsScrVec.size()*2]-=sentLenScoreForPartialHyp(hypKey,trglen);
    for(unsigned int j=srcLeft;j<=srcRight;++j)
      hypKey.set(j);
    scoreComponents[PTS+logptsScrVec.size()*2]+=sentLenScoreForPartialHyp(hypKey,trglen+trgphrase.size());

        // Increase trglen
    trglen+=trgphrase.size();

  }
  if(numberOfUncoveredSrcWordsHypData(new_hypd)==0 &&
     numberOfUncoveredSrcWordsHypData(pred_hypd)!=0)
  {
        // Calculate word penalty score
    scoreComponents[WPEN]-=sumWordPenaltyScore(trglen);
    scoreComponents[WPEN]+=wordPenaltyScore(trglen);

        // End of sentence score
    scoreComponents[LMODEL]+=getScoreEndGivenState(hypScoreInfo.lmHist);

        // Calculate sentence length score
    scoreComponents[PTS+phrModelInfoPtr->phraseModelPars.ptsWeightVec.size()*2]-=sentLenScoreForPartialHyp(hypKey,trglen);
    scoreComponents[PTS+phrModelInfoPtr->phraseModelPars.ptsWeightVec.size()*2]+=sentLenScore(pbtmInputVars.srcSentVec.size(),trglen);
  }

      // Accumulate the score stored in scoreComponents
  for(unsigned int i=0;i<scoreComponents.size();++i)
    hypScoreInfo.score +=scoreComponents[i];

  new_hyp.setScoreInfo(hypScoreInfo);
  new_hyp.setData(new_hypd);
  
  return hypScoreInfo.score;
}

//---------------------------------------
Score PhrLocalSwLiTm::smoothedPhrScore_s_t_(const std::vector<WordIndex>& s_,
                                            const std::vector<WordIndex>& t_)
{
  std::vector<Score> scoreVec=smoothedPhrScoreVec_s_t_(s_,t_);
  Score sum=0;
  for(unsigned int i=0;i<scoreVec.size();++i)
    sum+=scoreVec[i];
  return sum;
}

//---------------------------------------
Score PhrLocalSwLiTm::regularSmoothedPhrScore_s_t_(const std::vector<WordIndex>& s_,
                                                   const std::vector<WordIndex>& t_)
{
  if(swModelInfoPtr->lambda_invswm==1.0)
  {
    return phrModelInfoPtr->phraseModelPars.pstWeightVec[0] * (double)phrModelInfoPtr->invPbModelPtr->logpt_s_(t_,s_);
  }
  else
  {
    float sum1=log(swModelInfoPtr->lambda_invswm)+(float)phrModelInfoPtr->invPbModelPtr->logpt_s_(t_,s_);
    if(sum1<=log(PHRASE_PROB_SMOOTH))
      sum1=PHRSWLITM_LGPROB_SMOOTH;
    float sum2=log(1.0-swModelInfoPtr->lambda_invswm)+(float)invSwLgProb(0,s_,t_);
    float interp=MathFuncs::lns_sumlog(sum1,sum2);
      
    return phrModelInfoPtr->phraseModelPars.pstWeightVec[0] * (double)interp;
  }
}

//---------------------------------------
std::vector<Score> PhrLocalSwLiTm::smoothedPhrScoreVec_s_t_(const std::vector<WordIndex>& s_,
                                                            const std::vector<WordIndex>& t_)
{
  std::vector<Score> scoreVec;
  Score score=regularSmoothedPhrScore_s_t_(s_,t_);
  scoreVec.push_back(score);
  return scoreVec;
}

//---------------------------------------
Score PhrLocalSwLiTm::smoothedPhrScore_t_s_(const std::vector<WordIndex>& s_,
                                            const std::vector<WordIndex>& t_)
{
  std::vector<Score> scoreVec=smoothedPhrScoreVec_t_s_(s_,t_);
  Score sum=0;
  for(unsigned int i=0;i<scoreVec.size();++i)
    sum+=scoreVec[i];
  return sum;
}

//---------------------------------------
void PhrLocalSwLiTm::obtainSrcSwVocWordIdxVec(const std::vector<WordIndex>& s_,
                                              std::vector<WordIndex>& swVoc_s_)
{
      // Obtain string vector
  std::vector<std::string> strVec=srcIndexVectorToStrVector(s_);

      // Obtain word index vector from string vector
  swVoc_s_=swModelInfoPtr->swAligModelPtrVec[0]->strVectorToSrcIndexVector(strVec);
}

//---------------------------------------
void PhrLocalSwLiTm::obtainTrgSwVocWordIdxVec(const std::vector<WordIndex>& t_,
                                              std::vector<WordIndex>& swVoc_t_)
{
      // Obtain string vector
  std::vector<std::string> strVec=trgIndexVectorToStrVector(t_);

      // Obtain word index vector from string vector
  swVoc_t_=swModelInfoPtr->swAligModelPtrVec[0]->strVectorToTrgIndexVector(strVec);
}

//---------------------------------------
Score PhrLocalSwLiTm::regularSmoothedPhrScore_t_s_(const std::vector<WordIndex>& s_,
                                                   const std::vector<WordIndex>& t_)
{
  if(swModelInfoPtr->lambda_swm==1.0)
  {
    return phrModelInfoPtr->phraseModelPars.ptsWeightVec[0] * (double)phrModelInfoPtr->invPbModelPtr->logps_t_(t_,s_);
  }
  else
  {
    float sum1=log(swModelInfoPtr->lambda_swm)+(float)phrModelInfoPtr->invPbModelPtr->logps_t_(t_,s_);
    if(sum1<=log(PHRASE_PROB_SMOOTH))
      sum1=PHRSWLITM_LGPROB_SMOOTH;
    float sum2=log(1.0-swModelInfoPtr->lambda_swm)+(float)swLgProb(0,s_,t_);
    float interp=MathFuncs::lns_sumlog(sum1,sum2);
    return phrModelInfoPtr->phraseModelPars.ptsWeightVec[0] * (double)interp;
  }
}

//---------------------------------------
std::vector<Score> PhrLocalSwLiTm::smoothedPhrScoreVec_t_s_(const std::vector<WordIndex>& s_,
                                                       const std::vector<WordIndex>& t_)
{
  std::vector<Score> scoreVec;
  Score score=regularSmoothedPhrScore_t_s_(s_,t_);
  scoreVec.push_back(score);
  return scoreVec;
}

//---------------------------------------
Score PhrLocalSwLiTm::nbestTransScore(const std::vector<WordIndex>& s_,
                                      const std::vector<WordIndex>& t_)
{
  Score score=0;

      // word penalty contribution
  score+=wordPenaltyScore(t_.size());

      // Language model contribution
  score+=nbestLmScoringFunc(t_);

      // Phrase model contribution
  score+=smoothedPhrScore_t_s_(s_,t_);
  score+=smoothedPhrScore_s_t_(s_,t_);

  return score;
}

//---------------------------------------
Score PhrLocalSwLiTm::nbestTransScoreLast(const std::vector<WordIndex>& s_,
                                          const std::vector<WordIndex>& t_)
{
  return nbestTransScore(s_,t_);
}

//---------------------------------
void PhrLocalSwLiTm::extendHypDataIdx(PositionIndex srcLeft,
                                      PositionIndex srcRight,
                                      const std::vector<WordIndex>& trgPhraseIdx,
                                      HypDataType& hypd)
{
  std::pair<PositionIndex,PositionIndex> sourceSegm;
  
        // Add trgPhraseIdx to the target vector
  for(unsigned int i=0;i<trgPhraseIdx.size();++i)
  {
    hypd.ntarget.push_back(trgPhraseIdx[i]);
  }
  
      // Add source segment and target cut
  sourceSegm.first=srcLeft;
  sourceSegm.second=srcRight;
  hypd.sourceSegmentation.push_back(sourceSegm);
  
  hypd.targetSegmentCuts.push_back(hypd.ntarget.size()-1);
}

//---------------------------------
PositionIndex PhrLocalSwLiTm::getLastSrcPosCoveredHypData(const HypDataType& hypd)
{
  SourceSegmentation sourceSegmentation;

  sourceSegmentation=hypd.sourceSegmentation;
  if(sourceSegmentation.size()>0)
    return sourceSegmentation.back().second;
  else return 0;
}

//---------------------------------
bool PhrLocalSwLiTm::hypDataTransIsPrefixOfTargetRef(const HypDataType& hypd,
                                                     bool& equal)const
{
  PositionIndex ntrgSize,nrefSentSize;
  
  ntrgSize=hypd.ntarget.size();
  nrefSentSize=pbtmInputVars.nrefSentIdVec.size();	
	
  if(ntrgSize>nrefSentSize) return false;
  for(PositionIndex i=1;i<ntrgSize;++i)
  {
    if(pbtmInputVars.nrefSentIdVec[i]!=hypd.ntarget[i]) return false;
  }
  if(ntrgSize==nrefSentSize) equal=true;
  else equal=false;

  return true;
}

//---------------------------------
PhrLocalSwLiTm::~PhrLocalSwLiTm()
{
  
}
