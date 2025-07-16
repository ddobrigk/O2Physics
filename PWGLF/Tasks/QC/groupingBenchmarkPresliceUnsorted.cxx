// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//
// grouping benchmark task 
// =======================
// 
// allows for the estimation of performance when gouping 

#include <Math/Vector4D.h>
#include <cmath>
#include <array>
#include <cstdlib>

#include <TFile.h>
#include <TH2F.h>
#include <TProfile.h>
#include <TLorentzVector.h>
#include <TPDGCode.h>
#include <TDatabasePDG.h>

#include "Framework/runDataProcessing.h"
#include "Framework/AnalysisTask.h"
#include "Framework/AnalysisDataModel.h"
#include "Framework/ASoAHelpers.h"
#include "PWGLF/DataModel/LFStrangenessTables.h"

using namespace o2;
using namespace o2::framework;
using namespace o2::framework::expressions;
using namespace std;
using std::array;

struct groupingBenchmarkPresliceUnsorted {
  // Histogram registry
  HistogramRegistry histos{"Histos", {}, OutputObjHandlingPolicy::AnalysisObject};

  // slice command 
  PresliceUnsorted<aod::V0CollRefs> perCollision = o2::aod::v0data::straCollisionId;

  std::chrono::high_resolution_clock::time_point previous{};

  void init(InitContext const&)
  {
    histos.add("hV0sPerEvent", "hV0sPerEvent", framework::kTH1D, {{100,-0.5f,99.5f}});
    histos.add("hCollisionsVsDF", "hCollisionsVsDF", framework::kTH1D, {{100,-0.5f,99.5f}});\
    histos.add("hV0sVsDF", "hV0sVsDF", framework::kTH1D, {{100,-0.5f,99.5f}});
    histos.add("hTimeVsDF", "hTimesVsDF", framework::kTH1D, {{100,-0.5f,99.5f}});
  }

  int atDF = 0; // index of DF
  int atFilledDF = 0; // index of DF
  int collisionsThisDF; 
  int V0sThisDF; 
  double timeThisDF;

  void process(aod::StraCollisions const& collisions, aod::V0CollRefs const& fullV0s)
  {
    // first DF capture
    if(atDF==0){
      previous = std::chrono::high_resolution_clock::now();
    }

    // process if not empty
    for (const auto& coll : collisions) {
      const uint64_t collIdx = coll.globalIndex();
      auto V0s = fullV0s.sliceBy(perCollision, collIdx);
    }

    // take the values in case this is a non-empty DF
    if(collisions.size()>0){ 
      collisionsThisDF = collisions.size(); 
      V0sThisDF = fullV0s.size();
    }

    // this is the empty DF that succeeds a filled DF, mark time now
    if(atDF>0&&collisions.size()==0){
      auto end = std::chrono::high_resolution_clock::now();
      std::chrono::duration<double> elapsed = end - previous;
      LOGF(info, "[DF processed, indexed %i, filled %i] N. Collisions: %i, N. V0s: %i, Processing time (s): %lf", atDF, atFilledDF, collisionsThisDF, V0sThisDF, elapsed.count());
      histos.template get<TH1>(HIST("hCollisionsVsDF"))->Fill(atFilledDF, collisionsThisDF);
      histos.template get<TH1>(HIST("hV0sVsDF"))->Fill(atFilledDF, V0sThisDF);
      histos.template get<TH1>(HIST("hTimeVsDF"))->Fill(atFilledDF, elapsed.count());
      previous = std::chrono::high_resolution_clock::now();
      atFilledDF++;
    }
    atDF++;
  }
};

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  return WorkflowSpec{
    adaptAnalysisTask<groupingBenchmarkPresliceUnsorted>(cfgc)};
}
