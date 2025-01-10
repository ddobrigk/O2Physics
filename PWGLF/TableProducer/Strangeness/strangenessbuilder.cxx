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
// Strangeness builder task 
// ========================
// 
// This task produces all tables that may be necessary for 
// strangeness analyses. A single device is provided to 
// ensure better computing resource (memory) management. 
//
//  process functions: 
//  -- processPreselectTPCPID ..: pre-selects TPC dE/dx-compatible candidates.
//  -- processRealData .........: use this OR processSimulation but NOT both
//  -- processSimulation .......: use this OR processRealData but NOT both
//

#include "Framework/runDataProcessing.h"
#include "Framework/AnalysisTask.h"
#include "Framework/AnalysisDataModel.h"
#include "Common/DataModel/PIDResponse.h"
#include "TableHelper.h"
#include "PWGLF/DataModel/LFStrangenessTables.h"
#include "PWGLF/DataModel/LFStrangenessPIDTables.h"
#include "PWGLF/Utils/strangenessBuilderHelper.h"
#include "CCDB/BasicCCDBManager.h"

using namespace o2;
using namespace o2::framework;

static constexpr int nParameters = 1;
static const std::vector<std::string> tableNames{"V0Indices",           // 0 (standard analysis: V0Data)
                                                 "V0CoresBase",         // 1 (standard analyses: V0Data)
                                                 "V0Covs",              // 2
                                                 "CascIndices",         // 3 (standard analyses: CascData)
                                                 "KFCascIndices",       // 4 (standard analyses: KFCascData)
                                                 "TraCascIndices",      // 5 (standard analyses: TraCascData)
                                                 "StoredCascCores",     // 6 (standard analyses: CascData)
                                                 "StoredKFCascCores",   // 7 (standard analyses: KFCascData)
                                                 "StoredTraCascCores",  // 8 (standard analyses: TraCascData)
                                                 "CascCovs",            // 9
                                                 "KFCascCovs",          // 10
                                                 "TraCascCovs",         // 11
                                                 "V0TrackXs",           // 12
                                                 "CascTrackXs",         // 13
                                                 "CascBBs",             // 14
                                                 "V0DauCovs",           // 15 (requested: tracking studies)
                                                 "V0DauCovIUs",         // 16 (requested: tracking studies)
                                                 "V0TraPosAtDCAs",      // 17 (requested: tracking studies)
                                                 "V0TraPosAtIUs",       // 18 (requested: tracking studies)
                                                 "V0Ivanovs",           // 19 (requested: tracking studies)
                                                 "McV0Labels",          // 20 (MC/standard analysis)
                                                 "V0MCCores",           // 21 (MC)
                                                 "V0CoreMCLabels",      // 22 (MC)
                                                 "V0MCCollRefs",        // 23 (MC)
                                                 "McCascLabels",        // 24 (MC/standard analysis)
                                                 "McKFCascLabels",      // 25 (MC)
                                                 "McTraCascLabels",     // 26 (MC)
                                                 "McCascBBTags",        // 27 (MC)
                                                 "CascMCCores",         // 28 (MC)
                                                 "CascCoreMCLabels",    // 29 (MC)
                                                 "CascMCCollRefs",      // 30 (MC)
                                                 "StraCollision",       // 31 (derived)
                                                 "StraCollLabels",      // 32 (derived)
                                                 "StraMCCollisions",    // 33 (MC/derived)
                                                 "StraMCCollMults",     // 34 (MC/derived)
                                                 "StraCents",           // 35 (derived)
                                                 "StraEvSels",          // 36 (derived)
                                                 "StraStamps",          // 37 (derived)
                                                 "V0CollRefs",          // 38 (derived)
                                                 "CascCollRefs",        // 39 (derived)
                                                 "KFCascCollRefs",      // 40 (derived)
                                                 "TraCascCollRefs",     // 41 (derived)
                                                 "DauTrackExtras",      // 42 (derived)
                                                 "DauTrackMCIds",       // 43 (MC/derived)
                                                 "DauTrackTPCPIDs",     // 44 (derived)
                                                 "DauTrackTOFPIDs",     // 45 (derived)
                                                 "V0Extras",            // 46 (derived)
                                                 "CascExtras",          // 47 (derived)
                                                 "StraTrackExtras",     // 48 (derived)
                                                 "CascToTraRefs",       // 49 (derived)
                                                 "CascToKFRefs",        // 50 (derived)
                                                 "TraToCascRefs",       // 51 (derived)
                                                 "KFToCascRefs",        // 52 (derived)
                                                 "V0MCMothers",         // 53 (MC/derived)
                                                 "CascMCMothers",       // 54 (MC/derived)
                                                 "MotherMCParts",       // 55 (MC/derived)
                                                 "StraFT0AQVs",         // 56 (derived)
                                                 "StraFT0CQVs",         // 57 (derived)
                                                 "StraFT0MQVs",         // 58 (derived)
                                                 "StraFV0AQVs",         // 59 (derived)
                                                 "StraTPCQVs",          // 60 (derived)
                                                 "StraFT0CQVsEv",       // 61 (derived)
                                                 "StraZDCSP",           // 62 (derived)
                                                 "GeK0Short",           // 63 (MC/derived)
                                                 "GeLambda",            // 64 (MC/derived)
                                                 "GeAntiLambda",        // 65 (MC/derived)
                                                 "GeXiMinus",           // 66 (MC/derived)
                                                 "GeXiPlus",            // 67 (MC/derived)
                                                 "GeOmegaMinus",        // 68 (MC/derived)
                                                 "GeOmegaPlus",         // 69 (MC/derived)
                                                 "V0FoundTags",         // 70 (MC/derived)
                                                 "CascFoundTags",       // 71 (MC/derived)
                                                 "StraOrigins"          // 72 (derived)
                                                 };

static constexpr int nTablesConst = 73;

static const std::vector<std::string> parameterNames{"enable"};
static const int defaultParameters[nTablesConst][nParameters]{
  {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, //0-9
  {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, //10-19
  {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, //20-29
  {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, //30-39
  {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, //40-49
  {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, //50-59
  {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, //60-69
  {-1}, {-1}, {-1}                                            //70-72
  };

// use parameters + cov mat non-propagated, aux info + (extension propagated)
using FullTracksExt = soa::Join<aod::Tracks, aod::TracksExtra, aod::TracksCov>;
using FullTracksExtIU = soa::Join<aod::TracksIU, aod::TracksExtra, aod::TracksCovIU>;
using TracksWithExtra = soa::Join<aod::Tracks, aod::TracksExtra>;

// For dE/dx association in pre-selection
using TracksExtraWithPID = soa::Join<aod::TracksExtra, aod::pidTPCFullEl, aod::pidTPCFullPi, aod::pidTPCFullPr, aod::pidTPCFullHe>;

struct StrangenessBuilder {
  // helper object
  o2::pwglf::strangenessBuilderHelper straHelper; 

  // table index : match order above
  enum tableIndex { kV0Indices = 0,
                    kV0CoresBase,
                    kV0Covs,
                    kCascIndices,
                    kKFCascIndices,
                    kTraCascIndices,
                    kStoredCascCores,
                    kStoredKFCascCores,
                    kStoredTraCascCores,
                    kCascCovs,
                    kKFCascCovs,
                    kTraCascCovs,
                    kV0TrackXs,
                    kCascTrackXs,
                    kCascBBs,
                    kV0DauCovs,
                    kV0DauCovIUs,
                    kV0TraPosAtDCAs,
                    kV0TraPosAtIUs,
                    kV0Ivanovs,
                    kMcV0Labels,
                    kV0MCCores,
                    kV0CoreMCLabels,
                    kV0MCCollRefs,
                    kMcCascLabels,
                    kMcKFCascLabels,
                    kMcTraCascLabels,
                    kMcCascBBTags,
                    kCascMCCores,
                    kCascCoreMCLabels,
                    kCascMCCollRefs,
                    kStraCollision,
                    kStraCollLabels,
                    kStraMCCollisions,
                    kStraMCCollMults,
                    kStraCents,
                    kStraEvSels,
                    kStraStamps,
                    kV0CollRefs,
                    kCascCollRefs,
                    kKFCascCollRefs,
                    kTraCascCollRefs,
                    kDauTrackExtras,
                    kDauTrackMCIds,
                    kDauTrackTPCPIDs,
                    kDauTrackTOFPIDs,
                    kV0Extras,
                    kCascExtras,
                    kStraTrackExtras,
                    kCascToTraRefs,
                    kCascToKFRefs,
                    kTraToCascRefs,
                    kKFToCascRefs,
                    kV0MCMothers,
                    kCascMCMothers,
                    kMotherMCParts,
                    kStraFT0AQVs,
                    kStraFT0CQVs,
                    kStraFT0MQVs,
                    kStraFV0AQVs,
                    kStraTPCQVs,
                    kStraFT0CQVsEv,
                    kStraZDCSP,
                    kGeK0Short,
                    kGeLambda,
                    kGeAntiLambda,
                    kGeXiMinus,
                    kGeXiPlus,
                    kGeOmegaMinus,
                    kGeOmegaPlus,
                    kV0FoundTags,
                    kCascFoundTags,
                    kStraOrigins,
                    nTables};

  //__________________________________________________
  // V0 tables
  Produces<aod::V0Indices> v0indices;         // standard part of V0Datas
  Produces<aod::V0CoresBase> v0cores;         // standard part of V0Datas
  Produces<aod::V0Covs> v0covs;               // for decay chain reco

  //__________________________________________________
  // cascade tables
  Produces<aod::CascIndices> cascidx;                 // standard part of CascDatas
  Produces<aod::KFCascIndices> kfcascidx;             // standard part of KFCascDatas
  Produces<aod::TraCascIndices> trackedcascidx;       // standard part of TraCascDatas
  Produces<aod::StoredCascCores> cascdata;            // standard part of CascDatas
  Produces<aod::StoredKFCascCores> kfcascdata;        // standard part of KFCascDatas
  Produces<aod::StoredTraCascCores> trackedcascdata;  // standard part of TraCascDatas
  Produces<aod::CascCovs> casccovs;                   // for decay chain reco
  Produces<aod::KFCascCovs> kfcasccovs;               // for decay chain reco
  Produces<aod::TraCascCovs> tracasccovs;             // for decay chain reco

  //__________________________________________________
  // secondary auxiliary tables
  Produces<aod::V0TrackXs> v0trackXs;       // for decay chain reco
  Produces<aod::CascTrackXs> cascTrackXs;   // for decay chain reco

  // further auxiliary / optional if desired
  Produces<aod::CascBBs> cascbb;      
  Produces<aod::V0DauCovs> v0daucovs;            // covariances of daughter tracks
  Produces<aod::V0DauCovIUs> v0daucovIUs;        // covariances of daughter tracks
  Produces<aod::V0TraPosAtDCAs> v0dauPositions;  // auxiliary debug information
  Produces<aod::V0TraPosAtIUs> v0dauPositionsIU; // auxiliary debug information
  Produces<aod::V0Ivanovs> v0ivanovs;            // information for Marian's tests

  //__________________________________________________
  // MC information: V0
  Produces<aod::McV0Labels> v0labels;           // MC labels for V0s
  Produces<aod::V0MCCores> v0mccores;           // mc info storage
  Produces<aod::V0CoreMCLabels> v0CoreMCLabels; // interlink V0Cores -> V0MCCores
  Produces<aod::V0MCCollRefs> v0mccollref;      // references collisions from V0MCCores
  
  // MC information: Cascades
  Produces<aod::McCascLabels> casclabels;            // MC labels for cascades
  Produces<aod::McKFCascLabels> kfcasclabels;        // MC labels for KF cascades
  Produces<aod::McTraCascLabels> tracasclabels;      // MC labels for tracked cascades
  Produces<aod::McCascBBTags> bbtags;                // bb tags (inv structure tagging in mc)
  Produces<aod::CascMCCores> cascmccores;            // mc info storage
  Produces<aod::CascCoreMCLabels> cascCoreMClabels;  // interlink CascCores -> CascMCCores
  Produces<aod::CascMCCollRefs> cascmccollrefs;      // references MC collisions from MC cascades

  //__________________________________________________
  // fundamental building blocks of derived data
  Produces<aod::StraCollision> strangeColl;        // characterises collisions
  Produces<aod::StraCollLabels> strangeCollLabels; // characterises collisions
  Produces<aod::StraMCCollisions> strangeMCColl;   // characterises collisions / MC
  Produces<aod::StraMCCollMults> strangeMCMults;   // characterises collisions / MC mults
  Produces<aod::StraCents> strangeCents;           // characterises collisions / centrality
  Produces<aod::StraEvSels> strangeEvSels;         // characterises collisions / centrality / sel8 selection
  Produces<aod::StraStamps> strangeStamps;         // provides timestamps, run numbers
  Produces<aod::V0CollRefs> v0collref;             // references collisions from V0s
  Produces<aod::CascCollRefs> casccollref;         // references collisions from cascades
  Produces<aod::KFCascCollRefs> kfcasccollref;     // references collisions from KF cascades
  Produces<aod::TraCascCollRefs> tracasccollref;   // references collisions from tracked cascades

  //__________________________________________________
  // track extra references
  Produces<aod::DauTrackExtras> dauTrackExtras;   // daughter track detector properties
  Produces<aod::DauTrackMCIds> dauTrackMCIds;     // daughter track MC Particle ID
  Produces<aod::DauTrackTPCPIDs> dauTrackTPCPIDs; // daughter track TPC PID
  Produces<aod::DauTrackTOFPIDs> dauTrackTOFPIDs; // daughter track TOF PID
  Produces<aod::V0Extras> v0Extras;               // references DauTracks from V0s
  Produces<aod::CascExtras> cascExtras;           // references DauTracks from cascades
  Produces<aod::StraTrackExtras> straTrackExtras; // references DauTracks from tracked cascades (for the actual tracked cascade, not its daughters)

  //__________________________________________________
  // cascade interlinks
  Produces<aod::CascToTraRefs> cascToTraRefs; // cascades -> tracked
  Produces<aod::CascToKFRefs> cascToKFRefs;   // cascades -> KF
  Produces<aod::TraToCascRefs> traToCascRefs; // tracked -> cascades
  Produces<aod::KFToCascRefs> kfToCascRefs;   // KF -> cascades

  //__________________________________________________
  // mother information
  Produces<aod::V0MCMothers> v0mothers;       // V0 mother references
  Produces<aod::CascMCMothers> cascmothers;   // casc mother references
  Produces<aod::MotherMCParts> motherMCParts; // mc particles for mothers

  //__________________________________________________
  // Q-vectors
  Produces<aod::StraFT0AQVs> StraFT0AQVs;     // FT0A Q-vector
  Produces<aod::StraFT0CQVs> StraFT0CQVs;     // FT0C Q-vector
  Produces<aod::StraFT0MQVs> StraFT0MQVs;     // FT0M Q-vector
  Produces<aod::StraFV0AQVs> StraFV0AQVs;     // FV0A Q-vector
  Produces<aod::StraTPCQVs> StraTPCQVs;       // TPC Q-vector
  Produces<aod::StraFT0CQVsEv> StraFT0CQVsEv; // events used to compute FT0C Q-vector (LF)
  Produces<aod::StraZDCSP> StraZDCSP;         // ZDC Sums and Products

  //__________________________________________________
  // Generated binned data
  // this is a hack while the system does not do better
  Produces<aod::GeK0Short> geK0Short;
  Produces<aod::GeLambda> geLambda;
  Produces<aod::GeAntiLambda> geAntiLambda;
  Produces<aod::GeXiMinus> geXiMinus;
  Produces<aod::GeXiPlus> geXiPlus;
  Produces<aod::GeOmegaMinus> geOmegaMinus;
  Produces<aod::GeOmegaPlus> geOmegaPlus;

  //__________________________________________________
  // Found tags for findable exercise
  Produces<aod::V0FoundTags> v0FoundTags;
  Produces<aod::CascFoundTags> cascFoundTags;

  //__________________________________________________
  // Debug
  Produces<aod::StraOrigins> straOrigin;

  Configurable<LabeledArray<int>> enabledTables{"enabledTables",
                                                {defaultParameters[0], nTables, nParameters, tableNames, parameterNames},
                                                "Produce this table: -1 for autodetect; otherwise, 0/1 is false/true"};
  std::vector<int> mEnabledTables; // Vector of enabled tables

  // CCDB options
  struct : ConfigurableGroup {
    Configurable<std::string> ccdburl{"ccdb-url", "http://alice-ccdb.cern.ch", "url of the ccdb repository"};
    Configurable<std::string> grpPath{"grpPath", "GLO/GRP/GRP", "Path of the grp file"};
    Configurable<std::string> grpmagPath{"grpmagPath", "GLO/Config/GRPMagField", "CCDB path of the GRPMagField object"};
    Configurable<std::string> lutPath{"lutPath", "GLO/Param/MatLUT", "Path of the Lut parametrization"};
    Configurable<std::string> geoPath{"geoPath", "GLO/Config/GeometryAligned", "Path of the geometry file"};
  } ccdbConfigurations;

  o2::ccdb::CcdbApi ccdbApi;
  Service<o2::ccdb::BasicCCDBManager> ccdb;

  int mRunNumber;
  o2::base::MatLayerCylSet* lut = nullptr;

  // for tagging V0s used in cascades
  std::vector<o2::pwglf::v0candidate> v0sFromCascades; // Vector of v0 candidates used in cascades
  std::vector<int> v0Map;                              // index to relate V0s -> v0sFromCascades

  HistogramRegistry histos{"Histos", {}, OutputObjHandlingPolicy::AnalysisObject};

  void init(InitContext&)
  {
    mRunNumber = 0;
    
    mEnabledTables.resize(nTables, 0);

    for (int i = 0; i < nTables; i++) {
      int f = enabledTables->get(tableNames[i].c_str(), "enable");
      if (f == 1) {
        mEnabledTables[i] = 1;
      }
    }

    ccdb->setURL(ccdbConfigurations.ccdburl);
    ccdb->setCaching(true);
    ccdb->setLocalObjectValidityChecking();
    ccdb->setFatalWhenNull(false);
  }

  bool initCCDB(aod::BCsWithTimestamps const& bcs, aod::Collisions const& collisions)
  {
    auto bc = collisions.size() ? collisions.begin().bc_as<aod::BCsWithTimestamps>() : bcs.begin();
    if (!bcs.size()) {
      LOGF(warn, "No BC found, skipping this DF.");
      return false; // signal to skip this DF
    }

    if (mRunNumber == bc.runNumber()) {
      return true;
    }

    // acquire LUT for this timestamp
    auto timestamp = bc.timestamp();
    LOG(info) << "Loading material look-up table for timestamp: " << timestamp;
    lut = o2::base::MatLayerCylSet::rectifyPtrFromFile(ccdb->getForTimeStamp<o2::base::MatLayerCylSet>(ccdbConfigurations.lutPath, timestamp));
    o2::base::Propagator::Instance()->setMatLUT(lut);
    straHelper.lut = lut;

    return true;
  }

  //__________________________________________________
  template <typename TV0s, typename TCascades>
  void markV0sUsedInCascades(TV0s const& v0s, TCascades const& cascades)
  {
    v0Map.resize(v0s.size(), -2); // marks not used
    for (auto& cascade : cascades) {
      v0Map[cascade.v0Id()] = -1; // marks used (but isn't the index of a properly built V0, which would be >= 0)
    }
  }

  template <class TTracks, typename TCollisions, typename TV0s>
  void buildV0s(TCollisions const& collisions, TV0s const& v0s)
  {
    // Loops over all V0s in the time frame
    for (auto& v0 : v0s) {
      // Get tracks and generate candidate
      auto const& collision = v0.collision();
      auto const& posTrack = v0.template posTrack_as<TTracks>();
      auto const& negTrack = v0.template negTrack_as<TTracks>();
      straHelper.buildV0Candidate(collision, posTrack, negTrack, v0.isCollinearV0());
    }
  }

  void processPreselectTPCPID(aod::Collisions const& collisions, aod::V0s const& V0s, aod::Cascades const& Cascades, FullTracksExtIU const&, aod::BCsWithTimestamps const& bcs)
  {

  }

  //__________________________________________________
  template <typename TCollisions, typename TV0s, typename TCascades, typename TTracks, typename TBCs>
  void dataProcess(TCollisions const& collisions, TV0s const& v0s, TCascades const& cascades, TTracks const&, TBCs const& bcs)
  {
    if(!initCCDB(bcs, collisions)) return;
    markV0sUsedInCascades(v0s, cascades);
    if(mEnabledTables[kV0CoresBase]){ // V0s have been requested
      buildV0s<TTracks>(collisions, v0s);
    }
    if(mEnabledTables[kStoredCascCores]){ // Cascades have been requested
      //buildCascades<FullTracksExtIU>(Cascades);
    }
  }

  void processRealData(aod::Collisions const& collisions, aod::V0s const& v0s, aod::Cascades const& cascades, FullTracksExtIU const& tracks, aod::BCsWithTimestamps const& bcs)
  {
    dataProcess(collisions, v0s, cascades, tracks, bcs);
  }

  void processRealDataRun2(aod::Collisions const& collisions, aod::V0s const& v0s, aod::Cascades const& cascades, FullTracksExt const& tracks, aod::BCsWithTimestamps const& bcs)
  {
    //dataProcess(collisions, v0s, cascades, tracks, bcs);
  }

  void processSimulationFindable(aod::Collisions const& collisions, aod::V0s const& V0s, aod::Cascades const& Cascades, FullTracksExtIU const&, aod::BCsWithTimestamps const& bcs)
  {

  }

  PROCESS_SWITCH(StrangenessBuilder, processPreselectTPCPID, "only build candidates compatible with a broad TPC dE/dx configuration", false);
  PROCESS_SWITCH(StrangenessBuilder, processRealData, "process real data", true);
  PROCESS_SWITCH(StrangenessBuilder, processRealDataRun2, "process real data (Run 2)", false);
  PROCESS_SWITCH(StrangenessBuilder, processSimulationFindable, "process simulation findable (requires lambdakzeromcfinder)", false);
};

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  return WorkflowSpec{
    adaptAnalysisTask<StrangenessBuilder>(cfgc)};
}