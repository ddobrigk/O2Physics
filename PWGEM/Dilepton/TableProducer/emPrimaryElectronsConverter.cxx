// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3)(), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
#include "Framework/runDataProcessing.h"
#include "Framework/AnalysisTask.h"
#include "Framework/AnalysisDataModel.h"
#include "PWGEM/Dilepton/DataModel/dileptonTables.h"

using namespace o2;
using namespace o2::framework;

// Converts EMPrimaryElectrons 000 to 001
struct emprimaryelectronsconverter {
  Produces<aod::EMPrimaryElectrons_001> EMPrimaryElectrons_001;

  void process(aod::EMPrimaryElectrons_000 const& EMPrimaryElectrons_000)
  {
    for (auto& emprimaryelectron : EMPrimaryElectrons_000) {
      EMPrimaryElectrons_001(emprimaryelectron.collisionId(),
                  emprimaryelectron.trackId(), emprimaryelectron.sign(),
                  emprimaryelectron.pt(), emprimaryelectron.eta(), emprimaryelectron.phi(), emprimaryelectron.dcaXY(), emprimaryelectron.dcaZ(),
                  emprimaryelectron.tpcNClsFindable(), emprimaryelectron.tpcNClsFindableMinusFound(), 
                  emprimaryelectron.tpcNClsFindableMinusCrossedRows(), 0 /* not available in old data*/,
                  emprimaryelectron.tpcChi2NCl(), emprimaryelectron.tpcInnerParam(),
                  emprimaryelectron.tpcSignal(), emprimaryelectron.tpcNSigmaEl(), emprimaryelectron.tpcNSigmaMu(), 
                  emprimaryelectron.tpcNSigmaPi(), emprimaryelectron.tpcNSigmaKa(), emprimaryelectron.tpcNSigmaPr(),
                  emprimaryelectron.beta(), emprimaryelectron.tofNSigmaEl(), emprimaryelectron.tofNSigmaMu(), 
                  emprimaryelectron.tofNSigmaPi(), emprimaryelectron.tofNSigmaKa(), emprimaryelectron.tofNSigmaPr(),
                  emprimaryelectron.itsClusterSizes(), emprimaryelectron.itsChi2NCl(), emprimaryelectron.detectorMap(),
                  emprimaryelectron.x(), emprimaryelectron.alpha(), emprimaryelectron.y(), emprimaryelectron.z(), 
                  emprimaryelectron.snp(), emprimaryelectron.tgl(), emprimaryelectron.isAssociatedToMPC());
    }
  }
};

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  return WorkflowSpec{
    adaptAnalysisTask<emprimaryelectronsconverter>(cfgc)};
}
