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
///

#include <string>
#include <vector>
#include "Framework/runDataProcessing.h"
#include "Framework/AnalysisTask.h"
#include "Common/DataModel/EventSelection.h"
#include "PWGLF/DataModel/LFStrangenessTables.h"
#include "PWGLF/DataModel/LFStrangenessPIDTables.h"
#include "Framework/O2DatabasePDGPlugin.h"
#include "CCDB/BasicCCDBManager.h"
#include "Framework/ASoAHelpers.h"

// for zorro 
#include "EventFiltering/Zorro.h"
#include "EventFiltering/ZorroSummary.h"

using namespace o2;
using namespace o2::framework;
using namespace o2::framework::expressions;

// STEP 0
// Starting point: loop over all cascades and fill invariant mass histogram

struct omegaExample {
  // Histograms are defined with HistogramRegistry
  HistogramRegistry histos{"histos", {}, OutputObjHandlingPolicy::AnalysisObject, true, true};

  // event filtering
  Configurable<string> zorroMask{"zorroMask", "", "zorro trigger class to select on (empty: none)"};

  // Zorro-related variables
  Service<o2::ccdb::BasicCCDBManager> ccdb;
  Zorro zorro;
  OutputObj<ZorroSummary> zorroSummary{"zorroSummary"};
  int mRunNumber; // for zorro configuration


  // Configurable for event selection
  Configurable<float> cutzvertex{"cutzvertex", 10.0f, "Accepted z-vertex range (cm)"};

  // Axis definitions
  ConfigurableAxis axisPt{"axisPt", {VARIABLE_WIDTH, 0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.1f, 1.2f, 1.3f, 1.4f, 1.5f, 1.6f, 1.7f, 1.8f, 1.9f, 2.0f, 2.2f, 2.4f, 2.6f, 2.8f, 3.0f, 3.2f, 3.4f, 3.6f, 3.8f, 4.0f, 4.4f, 4.8f, 5.2f, 5.6f, 6.0f, 6.5f, 7.0f, 7.5f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 17.0f, 19.0f, 21.0f, 23.0f, 25.0f, 30.0f, 35.0f, 40.0f, 50.0f}, "p_{T} (GeV/c)"};
  
  ConfigurableAxis axisOmegaMass{"axisOmegaMass", {200, 1.57f, 1.77f}, "M (GeV/c2)"};
  

  struct : ConfigurableGroup {
    std::string prefix = "cascadeSelection";
    Configurable<float> dcaPositiveToPV{"dcaPositiveToPV", 0.05, "DCA of baryon daughter track To PV"};
    Configurable<float> dcaNegativeToPV{"dcaNegativeToPV", 0.1, "DCA of meson daughter track To PV"};
    Configurable<float> dcaBachelorToPV{"dcaBachelorToPV", 0.04, "DCA Bach To PV"};
    Configurable<double> casccospa{"casccospa", 0.97, "Casc CosPA"};
    Configurable<double> v0cospa{"v0cospa", 0.97, "V0 CosPA"};
    Configurable<float> dcacascdau{"dcacascdau", 1., "DCA Casc Daughters"};
    Configurable<float> dcav0dau{"dcav0dau", 1.5, "DCA V0 Daughters"};
    Configurable<float> v0massWindow{"v0massWindow", 0.010, "Mass window around Lambda"};
    Configurable<float> minCascRadius{"minCascRadius", 0.5, "Minimum cascade 2D radius"};
    Configurable<float> minV0Radius{"minV0Radius", 1.2, "Minimum V0 2D radius"};
    Configurable<float> NSigmaTPCPion{"NSigmaTPCPion", 5, "Nsigma TPC for pion from lambda"};
    Configurable<float> NSigmaTPCProton{"NSigmaTPCProton", 5, "Nsigma TPC for proton from lambda"};
    Configurable<float> NSigmaTPCKaon{"NSigmaTPCKaon", 5, "Nsigma TPC for kaon from omega"};
  } cascadeSelections; 

  // PDG data base
  Service<o2::framework::O2DatabasePDG> pdgDB;

  void init(InitContext const&)
  {
    mRunNumber = -1;
    zorroSummary.setObject(zorro.getZorroSummary());

    // Add histograms
    // Event selection
    histos.add("hVertexZRec", "hVertexZRec", {HistType::kTH1F, {{300, -15.0f, +15.0f}}});

    // Xi/Omega reconstruction
    histos.add("h2dMassOmegaMinus", "h2dMassOmegaMinus", {HistType::kTH2F, {axisPt, axisOmegaMass}});
    histos.add("h2dMassOmegaPlus", "h2dMassOmegaPlus", {HistType::kTH2F, {axisPt, axisOmegaMass}});
  }

  /// Function to load zorro
  template <typename TCollision>
  void initZorro(TCollision const& col)
  {
    if (mRunNumber == col.runNumber()) {
      return;
    }

    zorro.initCCDB(ccdb.service, col.runNumber(), col.timestamp(), zorroMask.value);
    zorro.populateHistRegistry(histos, col.runNumber());

    mRunNumber = col.runNumber();
  }

  // Defining filters for events (event selection)
  // Processed events will be already fulfilling the event selection requirements
  Filter posZFilter = (nabs(o2::aod::collision::posZ) < cutzvertex);

  // filter on simple regular columns
  Filter preFilterCascades = (aod::cascdata::dcaV0daughters < cascadeSelections.dcav0dau &&
                              nabs(aod::cascdata::dcapostopv) > cascadeSelections.dcaPositiveToPV &&
                              nabs(aod::cascdata::dcanegtopv) > cascadeSelections.dcaNegativeToPV &&
                              nabs(aod::cascdata::dcabachtopv) > cascadeSelections.dcaBachelorToPV &&
                              aod::cascdata::dcacascdaughters < cascadeSelections.dcacascdau);

  // Defining the type of the daughter tracks
  using dauTracks = soa::Join<aod::DauTrackExtras, aod::DauTrackTPCPIDs>;

  void process(soa::Filtered<soa::Join<aod::StraCollisions, aod::StraEvSels, aod::StraStamps>>::iterator const& collision,
               soa::Filtered<soa::Join<aod::CascCores, aod::CascExtras>> const& Cascades, dauTracks const&)
  {
    // Zorro event selection if requested
    // all necessary information is provided in 'StraStamps'
    if (zorroMask.value != "") {
      initZorro(collision);
      bool zorroSelected = zorro.isSelected(collision.globalBC());
      if (!zorroSelected) {
        return;
      }
    }

    // add extra event selections here as needed

    // Fill the event counter
    histos.fill(HIST("hVertexZRec"), collision.posZ());

    // Cascades
    for (const auto& casc : Cascades) {
      // apply selections 
      if (casc.casccosPA(collision.posX(), collision.posY(), collision.posZ()) < cascadeSelections.casccospa)
        continue;
      if (casc.v0cosPA(collision.posX(), collision.posY(), collision.posZ()) < cascadeSelections.v0cospa)
        continue;
      if (TMath::Abs(casc.mLambda() - pdgDB->Mass(3122)) > cascadeSelections.v0massWindow)
        continue;
      if (casc.cascradius() < cascadeSelections.minCascRadius)
        continue;
      if (casc.v0radius() < cascadeSelections.minV0Radius)
        continue;

      // get daughter tracks for dE/dx 
      const auto& bachTrack = casc.bachTrackExtra_as<dauTracks>();
      const auto& posTrack = casc.posTrackExtra_as<dauTracks>();
      const auto& negTrack = casc.negTrackExtra_as<dauTracks>();
      
      // PID selection
      if (casc.sign() < 0) {
        if (TMath::Abs(posTrack.tpcNSigmaPr()) > cascadeSelections.NSigmaTPCProton) {
          continue;
        }
        if (TMath::Abs(negTrack.tpcNSigmaPi()) > cascadeSelections.NSigmaTPCPion) {
          continue;
        }
      } else {
        if (TMath::Abs(negTrack.tpcNSigmaPr()) > cascadeSelections.NSigmaTPCProton) {
          continue;
        }
        if (TMath::Abs(posTrack.tpcNSigmaPi()) > cascadeSelections.NSigmaTPCPion) {
          continue;
        }
      }
      if (TMath::Abs(bachTrack.tpcNSigmaKa()) > cascadeSelections.NSigmaTPCKaon) {
        continue;
      }

      if(casc.sign()<0){
        histos.fill(HIST("h2dMassOmegaMinus"), casc.pt(), casc.mOmega());
      }else{
        histos.fill(HIST("h2dMassOmegaPlus"), casc.pt(), casc.mOmega());
      }
    }
  }
};

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  return WorkflowSpec{
    adaptAnalysisTask<omegaExample>(cfgc)};
}
