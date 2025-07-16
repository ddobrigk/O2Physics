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

struct groupingBenchmarkCustom {
  // Histogram registry
  HistogramRegistry histos{"Histos", {}, OutputObjHandlingPolicy::AnalysisObject};

  // slice command 
  Preslice<aod::V0CollRefs> perCollision = o2::aod::v0data::straCollisionId;

  void init(InitContext const&)
  {
    histos.add("hV0sPerEvent", "hV0sPerEvent", framework::kTH1D, {{100,-0.5f,99.5f}});
    histos.add("hCollisionsVsDF", "hCollisionsVsDF", framework::kTH1D, {{100,-0.5f,99.5f}});\
    histos.add("hV0sVsDF", "hV0sVsDF", framework::kTH1D, {{100,-0.5f,99.5f}});
    histos.add("hTimeVsDF", "hTimesVsDF", framework::kTH1D, {{100,-0.5f,99.5f}});
  }

  int atDF = 0; // index of DF

  void process(aod::StraCollisions const& collisions, aod::V0CollRefs const& fullV0s)
  {
    if(collisions.size()==0){
      return; //skip empty
    }

    // mark beginning of DF
    auto start = std::chrono::high_resolution_clock::now();

    // custom grouping procedure that should be slower due to vector use 
    // brute force grouped index construction
    std::vector<std::vector<int>> v0grouped(collisions.size());

    for (const auto& v0 : fullV0s) {
      v0grouped[v0.straCollisionId()].push_back(v0.globalIndex());
    }

    for (const auto& coll : collisions) {
      histos.template get<TH1>(HIST("hV0sPerEvent"))->Fill(v0grouped[coll.globalIndex()].size());
    }

    // mark end of DF
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    LOGF(info, "[DF processed] N. Collisions: %i, N. V0s: %i, Processing time (s): %lf", collisions.size(), fullV0s.size(), elapsed.count());
    histos.template get<TH1>(HIST("hCollisionsVsDF"))->Fill(atDF, collisions.size());
    histos.template get<TH1>(HIST("hV0sVsDF"))->Fill(atDF, fullV0s.size());
    histos.template get<TH1>(HIST("hTimeVsDF"))->Fill(atDF, elapsed.count());
    atDF++;
  }
};

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  return WorkflowSpec{
    adaptAnalysisTask<groupingBenchmarkCustom>(cfgc)};
}
