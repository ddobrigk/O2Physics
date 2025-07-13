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

struct groupingBenchmarkPreslice {
  // Histogram registry
  HistogramRegistry histos{"Histos", {}, OutputObjHandlingPolicy::AnalysisObject};

  // slice command 
  Preslice<soa::Join<aod::V0CollRefs, aod::V0Cores>> perCollision = o2::aod::v0data::straCollisionId;

  void init(InitContext const&)
  {
    histos.add("hV0sPerEvent", "hV0sPerEvent", framework::kTH1D, {{100,-0.5f,99.5f}});
  }

  void process(aod::StraCollisions const& collisions, soa::Join<aod::V0CollRefs, aod::V0Cores> const& fullV0s)
  {
    // mark beginning of DF
    auto start = std::chrono::high_resolution_clock::now();

    for (const auto& coll : collisions) {
      const uint64_t collIdx = coll.globalIndex();
      auto V0s = fullV0s.sliceBy(perCollision, collIdx);
      histos.template get<TH1>(HIST("hV0sPerEvent"))->Fill(V0s.size());
    }

    // mark end of DF
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    LOGF(info, "[DF processed] N. Collisions: %i, N. V0s: %i, Processing time (s): %lf", collisions.size(), fullV0s.size(), elapsed.count());
  }
};

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  return WorkflowSpec{
    adaptAnalysisTask<groupingBenchmarkPreslice>(cfgc)};
}
