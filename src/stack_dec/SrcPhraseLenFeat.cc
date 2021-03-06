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
/* Module: SrcPhraseLenFeat                                         */
/*                                                                  */
/* Definitions file: SrcPhraseLenFeat.cc                            */
/*                                                                  */
/********************************************************************/


//--------------- Include files --------------------------------------

#include "SrcPhraseLenFeat.h"

//--------------- SrcPhraseLenFeat class functions

template<>
SrcPhraseLenFeat<PhrScoreInfo>::HypScoreInfo
SrcPhraseLenFeat<PhrScoreInfo>::extensionScore(const std::vector<std::string>& srcSent,
                                               const HypScoreInfo& predHypScrInf,
                                               const PhrHypDataStr& predHypDataStr,
                                               const PhrHypDataStr& newHypDataStr,
                                               Score& unweightedScore)
{
      // Obtain score for hypothesis extension
  HypScoreInfo hypScrInf=predHypScrInf;
  unweightedScore=0;
    
  for(unsigned int i=predHypDataStr.sourceSegmentation.size();i<newHypDataStr.sourceSegmentation.size();++i)
  {
        // Initialize variables
    unsigned int trgLeft;
    unsigned int trgRight=newHypDataStr.targetSegmentCuts[i];
    if(i==0)
      trgLeft=1;
    else
      trgLeft=newHypDataStr.targetSegmentCuts[i-1]+1;
    unsigned int nextTrgPhraseLen=trgRight-trgLeft+1;

        // Update score
    Score iterScore=srcPhraseLenScore(i,newHypDataStr.sourceSegmentation,srcSent.size(),nextTrgPhraseLen);
    unweightedScore+= iterScore;
    hypScrInf.score+= weight*iterScore;
  }

      // NOTE: There are no additional score contributions when the
      // hypothesis is complete
    
  return hypScrInf;
}
