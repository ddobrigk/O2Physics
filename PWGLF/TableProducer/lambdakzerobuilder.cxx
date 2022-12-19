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
//  *+-+*+-+*+-+*+-+*+-+*+-+*
//  Strangeness builder task
//  *+-+*+-+*+-+*+-+*+-+*+-+*
//
//  This task loops over a set of V0 and cascade indices and
//  creates the corresponding analysis tables that contain
//  the typical information required for analysis.
//
//  PERFORMANCE WARNING: this task includes several track
//  propagation calls that are intrinsically heavy. Please
//  also be cautious when adjusting selections: these can
//  increase / decrease CPU consumption quite significantly.
//
//  IDEAL USAGE: if you are interested in taking V0s and
//  cascades and propagating TrackParCovs based on these,
//  please do not re-propagate the daughters. Instead,
//  the tables generated by this builder task can be used
//  to instantiate a TrackPar object (default operation)
//  or even a TrackParCov object (for which you will
//  need to enable the option of producing the V0Cov and
//  CascCov tables too).
//
//    Comments, questions, complaints, suggestions?
//    Please write to:
//    david.dobrigkeit.chinellato@cern.ch
//

#include <cmath>
#include <array>
#include <cstdlib>
#include <map>
#include <iterator>
#include <utility>

#include "Framework/runDataProcessing.h"
#include "Framework/RunningWorkflowInfo.h"
#include "Framework/AnalysisTask.h"
#include "Framework/AnalysisDataModel.h"
#include "Framework/ASoAHelpers.h"
#include "DetectorsVertexing/DCAFitterN.h"
#include "ReconstructionDataFormats/Track.h"
#include "Common/Core/RecoDecay.h"
#include "Common/Core/trackUtilities.h"
#include "PWGLF/DataModel/LFStrangenessTables.h"
#include "Common/Core/TrackSelection.h"
#include "Common/DataModel/TrackSelectionTables.h"
#include "DetectorsBase/Propagator.h"
#include "DetectorsBase/GeometryManager.h"
#include "DataFormatsParameters/GRPObject.h"
#include "DataFormatsParameters/GRPMagField.h"
#include "CCDB/BasicCCDBManager.h"

#include "TFile.h"
#include "TH2F.h"
#include "TProfile.h"
#include "TLorentzVector.h"
#include "Math/Vector4D.h"
#include "TPDGCode.h"
#include "TDatabasePDG.h"

using namespace o2;
using namespace o2::framework;
using namespace o2::framework::expressions;
using std::array;

namespace o2::aod
{
namespace v0tag
{
DECLARE_SOA_COLUMN(IsInteresting, isInteresting, int); //! will this be built or not?
}
DECLARE_SOA_TABLE(V0Tags, "AOD", "V0TAGS",
                  v0tag::IsInteresting);
} // namespace o2::aod

// use parameters + cov mat non-propagated, aux info + (extension propagated)
using FullTracksExt = soa::Join<aod::Tracks, aod::TracksExtra, aod::TracksCov, aod::TracksDCA>;
using FullTracksExtIU = soa::Join<aod::TracksIU, aod::TracksExtra, aod::TracksCovIU, aod::TracksDCA>;
using FullTracksExtIUMC = soa::Join<aod::TracksIU, aod::TracksExtra, aod::TracksCovIU, aod::TracksDCA, aod::McTrackLabels>;
using LabeledTracks = soa::Join<aod::Tracks, aod::McTrackLabels>;
using TaggedV0s = soa::Join<aod::V0s, aod::V0Tags>;

struct lambdakzeroBuilder {
  Produces<aod::StoredV0Datas> v0data;
  Produces<aod::V0Covs> v0covs; // covariances
  Service<o2::ccdb::BasicCCDBManager> ccdb;

  // Configurables related to table creation
  Configurable<int> createV0CovMats{"createV0CovMats", -1, {"Produces V0 cov matrices. -1: auto, 0: don't, 1: yes. Default: auto (-1)"}};

  // use auto-detect configuration
  Configurable<bool> d_UseAutodetectMode{"d_UseAutodetectMode", true, "Autodetect requested topo sels"};

  // Topological selection criteria
  Configurable<int> mincrossedrows{"mincrossedrows", 70, "min crossed rows"};

  Configurable<float> dcanegtopv{"dcanegtopv", .1, "DCA Neg To PV"};
  Configurable<float> dcapostopv{"dcapostopv", .1, "DCA Pos To PV"};
  Configurable<double> v0cospa{"v0cospa", 0.995, "V0 CosPA"}; // double -> N.B. dcos(x)/dx = 0 at x=0)
  Configurable<float> dcav0dau{"dcav0dau", 1.0, "DCA V0 Daughters"};
  Configurable<float> v0radius{"v0radius", 0.9, "v0radius"};

  Configurable<int> tpcrefit{"tpcrefit", 0, "demand TPC refit"};

  // Operation and minimisation criteria
  Configurable<double> d_bz_input{"d_bz", -999, "bz field, -999 is automatic"};
  Configurable<bool> d_UseAbsDCA{"d_UseAbsDCA", true, "Use Abs DCAs"};
  Configurable<bool> d_UseWeightedPCA{"d_UseWeightedPCA", false, "Vertices use cov matrices"};
  Configurable<int> useMatCorrType{"useMatCorrType", 0, "0: none, 1: TGeo, 2: LUT"};
  Configurable<int> rejDiffCollTracks{"rejDiffCollTracks", 0, "rejDiffCollTracks"};

  // CCDB options
  Configurable<std::string> ccdburl{"ccdb-url", "http://alice-ccdb.cern.ch", "url of the ccdb repository"};
  Configurable<std::string> grpPath{"grpPath", "GLO/GRP/GRP", "Path of the grp file"};
  Configurable<std::string> grpmagPath{"grpmagPath", "GLO/Config/GRPMagField", "CCDB path of the GRPMagField object"};
  Configurable<std::string> lutPath{"lutPath", "GLO/Param/MatLUT", "Path of the Lut parametrization"};
  Configurable<std::string> geoPath{"geoPath", "GLO/Config/GeometryAligned", "Path of the geometry file"};

  int mRunNumber;
  float d_bz;
  float maxSnp;  // max sine phi for propagation
  float maxStep; // max step size (cm) for propagation
  o2::base::MatLayerCylSet* lut = nullptr;

  // Define o2 fitter, 2-prong, active memory (no need to redefine per event)
  o2::vertexing::DCAFitterN<2> fitter;

  Filter taggedFilter = aod::v0tag::isInteresting > 0;

  enum v0step { kV0All = 0,
                kV0TPCrefit,
                kV0CrossedRows,
                kV0DCAxy,
                kV0DCADau,
                kV0CosPA,
                kV0Radius,
                kNV0Steps };

  // Helper struct to pass V0 information
  struct {
    float posTrackX;
    float negTrackX;
    std::array<float, 3> pos;
    std::array<float, 3> posP;
    std::array<float, 3> negP;
    float dcaV0dau;
    float posDCAxy;
    float negDCAxy;
    float cosPA;
    float V0radius;
    float lambdaMass;
    float antilambdaMass;
  } v0candidate;

  // Helper struct to do bookkeeping of building parameters
  struct {
    std::array<long, kNV0Steps> v0stats;
    long exceptions;
    long eventCounter;
  } statisticsRegistry;

  HistogramRegistry registry{
    "registry",
    {{"hEventCounter", "hEventCounter", {HistType::kTH1F, {{1, 0.0f, 1.0f}}}},
     {"hCaughtExceptions", "hCaughtExceptions", {HistType::kTH1F, {{1, 0.0f, 1.0f}}}},
     {"hV0Criteria", "hV0Criteria", {HistType::kTH1F, {{10, -0.5f, 9.5f}}}}}};

  void resetHistos()
  {
    statisticsRegistry.exceptions = 0;
    statisticsRegistry.eventCounter = 0;
    for (Int_t ii = 0; ii < kNV0Steps; ii++)
      statisticsRegistry.v0stats[ii] = 0;
  }

  void fillHistos()
  {
    registry.fill(HIST("hEventCounter"), 0.0, statisticsRegistry.eventCounter);
    registry.fill(HIST("hCaughtExceptions"), 0.0, statisticsRegistry.exceptions);
    for (Int_t ii = 0; ii < kNV0Steps; ii++)
      registry.fill(HIST("hV0Criteria"), ii, statisticsRegistry.v0stats[ii]);
  }

  o2::track::TrackParCov lPositiveTrack;
  o2::track::TrackParCov lNegativeTrack;

  void init(InitContext& context)
  {
    resetHistos();

    mRunNumber = 0;
    d_bz = 0;
    maxSnp = 0.85f;  // could be changed later
    maxStep = 2.00f; // could be changed later

    ccdb->setURL(ccdburl);
    ccdb->setCaching(true);
    ccdb->setLocalObjectValidityChecking();
    ccdb->setFatalWhenNull(false);

    lut = o2::base::MatLayerCylSet::rectifyPtrFromFile(ccdb->get<o2::base::MatLayerCylSet>(lutPath));
    if (!o2::base::GeometryManager::isGeometryLoaded()) {
      ccdb->get<TGeoManager>(geoPath);
    }

    if (doprocessRun2 == false && doprocessRun3 == false && doprocessRun3associated == false) {
      LOGF(fatal, "Neither processRun2, processRun3 nor processRun3associated enabled. Please choose one.");
    }
    if (doprocessRun2 == true && doprocessRun3 == true) {
      LOGF(fatal, "Cannot enable processRun2 and processRun3 at the same time. Please choose one.");
    }
    if (doprocessRun2 == true && doprocessRun3associated == true) {
      LOGF(fatal, "Cannot enable processRun2 and processRun3associated at the same time. Please choose one.");
    }
    if (doprocessRun3 == true && doprocessRun3associated == true) {
      LOGF(fatal, "Cannot enable processRun3 and processRun3associated at the same time. Please choose one.");
    }

    if (d_UseAutodetectMode) {
      // Checking for subscriptions to:
      double loosest_v0cospa = 100;
      float loosest_dcav0dau = -100;
      float loosest_dcapostopv = 100;
      float loosest_dcanegtopv = 100;
      float loosest_radius = 100;

      double detected_v0cospa = -100;
      float detected_dcav0dau = -100;
      float detected_dcapostopv = 100;
      float detected_dcanegtopv = 100;
      float detected_radius = 100;

      LOGF(info, "*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*");
      LOGF(info, " Single-strange builder self-configuration");
      LOGF(info, "*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*");
      auto& workflows = context.services().get<RunningWorkflowInfo const>();
      for (DeviceSpec const& device : workflows.devices) {
        // Step 1: check if this device subscribed to the V0data table
        for (auto const& input : device.inputs) {
          if (device.name.compare("lambdakzero-initializer") == 0)
            continue; // don't listen to the initializer, it's just to extend stuff
          const std::string v0DataName = "V0Datas";
          if (input.matcher.binding == v0DataName && device.name.compare("multistrange-builder") != 0) {
            LOGF(info, "Device named %s has subscribed to V0datas table! Will now scan for desired settings...", device.name);
            for (auto const& option : device.options) {
              // 5 V0 topological selections
              if (option.name.compare("v0setting_cospa") == 0) {
                detected_v0cospa = option.defaultValue.get<double>();
                LOGF(info, "%s requested V0 cospa = %f", device.name, detected_v0cospa);
                if (detected_v0cospa < loosest_v0cospa)
                  loosest_v0cospa = detected_v0cospa;
              }
              if (option.name.compare("v0setting_dcav0dau") == 0) {
                detected_dcav0dau = option.defaultValue.get<float>();
                LOGF(info, "%s requested DCA V0 daughters = %f", device.name, detected_dcav0dau);
                if (detected_dcav0dau > loosest_dcav0dau)
                  loosest_dcav0dau = detected_dcav0dau;
              }
              if (option.name.compare("v0setting_dcapostopv") == 0) {
                detected_dcapostopv = option.defaultValue.get<float>();
                LOGF(info, "%s requested DCA positive daughter to PV = %f", device.name, detected_dcapostopv);
                if (detected_dcapostopv < loosest_dcapostopv)
                  loosest_dcapostopv = detected_dcapostopv;
              }
              if (option.name.compare("v0setting_dcanegtopv") == 0) {
                detected_dcanegtopv = option.defaultValue.get<float>();
                LOGF(info, "%s requested DCA negative daughter to PV = %f", device.name, detected_dcanegtopv);
                if (detected_dcanegtopv < loosest_dcanegtopv)
                  loosest_dcanegtopv = detected_dcanegtopv;
              }
              if (option.name.compare("v0setting_radius") == 0) {
                detected_radius = option.defaultValue.get<float>();
                LOGF(info, "%s requested minimum V0 radius = %f", device.name, detected_radius);
                if (detected_radius < loosest_radius)
                  loosest_radius = detected_radius;
              }
            }
          }
          const std::string V0CovsName = "V0Covs";
          if (input.matcher.binding == V0CovsName) {
            LOGF(info, "Device named %s has subscribed to V0Covs table! Enabling.", device.name);
            createV0CovMats.value = 1;
          }
        }
      }
      LOGF(info, "Self-configuration finished! Decided on selections:");
      LOGF(info, " -+*> V0 cospa ..............: %.6f", loosest_v0cospa);
      LOGF(info, " -+*> DCA V0 daughters ......: %.6f", loosest_dcav0dau);
      LOGF(info, " -+*> DCA positive daughter .: %.6f", loosest_dcapostopv);
      LOGF(info, " -+*> DCA negative daughter .: %.6f", loosest_dcanegtopv);
      LOGF(info, " -+*> Minimum V0 radius .....: %.6f", loosest_radius);

      dcanegtopv.value = loosest_dcanegtopv;
      dcapostopv.value = loosest_dcapostopv;
      v0cospa.value = loosest_v0cospa;
      dcav0dau.value = loosest_dcav0dau;
      v0radius.value = loosest_radius;
    }

    //*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*
    LOGF(info, " -+*> process call configuration:");
    if (doprocessRun2 == true) {
      LOGF(info, " ---+*> Run 2 processing enabled. Will subscribe to Tracks table.");
    };
    if (doprocessRun3 == true) {
      LOGF(info, " ---+*> Run 3 processing enabled. Will subscribe to TracksIU table.");
    };
    if (createV0CovMats > 0) {
      LOGF(info, " ---+*> Will produce V0 cov mat table");
    };
    //*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*

    // initialize O2 2-prong fitter (only once)
    fitter.setPropagateToPCA(true);
    fitter.setMaxR(200.);
    fitter.setMinParamChange(1e-3);
    fitter.setMinRelChi2Change(0.9);
    fitter.setMaxDZIni(1e9);
    fitter.setMaxChi2(1e9);
    fitter.setUseAbsDCA(d_UseAbsDCA);
    fitter.setWeightedFinalPCA(d_UseWeightedPCA);

    // Material correction in the DCA fitter
    o2::base::Propagator::MatCorrType matCorr = o2::base::Propagator::MatCorrType::USEMatCorrNONE;
    if (useMatCorrType == 1)
      matCorr = o2::base::Propagator::MatCorrType::USEMatCorrTGeo;
    if (useMatCorrType == 2)
      matCorr = o2::base::Propagator::MatCorrType::USEMatCorrLUT;
    fitter.setMatCorrType(matCorr);
  }

  void initCCDB(aod::BCsWithTimestamps::iterator const& bc)
  {
    if (mRunNumber == bc.runNumber()) {
      return;
    }
    auto run3grp_timestamp = bc.timestamp();

    o2::parameters::GRPObject* grpo = ccdb->getForTimeStamp<o2::parameters::GRPObject>(grpPath, run3grp_timestamp);
    o2::parameters::GRPMagField* grpmag = 0x0;
    if (grpo) {
      o2::base::Propagator::initFieldFromGRP(grpo);
      if (d_bz_input < -990) {
        // Fetch magnetic field from ccdb for current collision
        d_bz = grpo->getNominalL3Field();
        LOG(info) << "Retrieved GRP for timestamp " << run3grp_timestamp << " with magnetic field of " << d_bz << " kZG";
      } else {
        d_bz = d_bz_input;
      }
    } else {
      grpmag = ccdb->getForTimeStamp<o2::parameters::GRPMagField>(grpmagPath, run3grp_timestamp);
      if (!grpmag) {
        LOG(fatal) << "Got nullptr from CCDB for path " << grpmagPath << " of object GRPMagField and " << grpPath << " of object GRPObject for timestamp " << run3grp_timestamp;
      }
      o2::base::Propagator::initFieldFromGRP(grpmag);
      if (d_bz_input < -990) {
        // Fetch magnetic field from ccdb for current collision
        d_bz = std::lround(5.f * grpmag->getL3Current() / 30000.f);
        LOG(info) << "Retrieved GRP for timestamp " << run3grp_timestamp << " with magnetic field of " << d_bz << " kZG";
      } else {
        d_bz = d_bz_input;
      }
    }
    o2::base::Propagator::Instance()->setMatLUT(lut);
    mRunNumber = bc.runNumber();
    // Set magnetic field value once known
    fitter.setBz(d_bz);
  }

  template <class TTracksTo>
  bool buildV0Candidate(aod::Collision const& collision, TTracksTo const& posTrack, TTracksTo const& negTrack)
  {

    // value 0.5: any considered V0
    statisticsRegistry.v0stats[kV0All]++;
    if (tpcrefit) {
      if (!(posTrack.trackType() & o2::aod::track::TPCrefit)) {
        return false;
      }
      if (!(negTrack.trackType() & o2::aod::track::TPCrefit)) {
        return false;
      }
    }

    // Passes TPC refit
    statisticsRegistry.v0stats[kV0TPCrefit]++;
    if (posTrack.tpcNClsCrossedRows() < mincrossedrows || negTrack.tpcNClsCrossedRows() < mincrossedrows) {
      return false;
    }

    // passes crossed rows
    statisticsRegistry.v0stats[kV0CrossedRows]++;
    if (fabs(posTrack.dcaXY()) < dcapostopv || fabs(negTrack.dcaXY()) < dcanegtopv) {
      return false;
    }

    // Initialize properly, please
    v0candidate.posDCAxy = posTrack.dcaXY();
    v0candidate.negDCAxy = negTrack.dcaXY();

    // passes DCAxy
    statisticsRegistry.v0stats[kV0DCAxy]++;

    // Change strangenessBuilder tracks
    lPositiveTrack = getTrackParCov(posTrack);
    lNegativeTrack = getTrackParCov(negTrack);

    //---/---/---/
    // Move close to minima
    int nCand = 0;
    try {
      nCand = fitter.process(lPositiveTrack, lNegativeTrack);
    } catch (...) {
      statisticsRegistry.exceptions++;
      LOG(error) << "Exception caught in DCA fitter process call!";
      return false;
    }
    if (nCand == 0) {
      return false;
    }

    v0candidate.posTrackX = fitter.getTrack(0).getX();
    v0candidate.negTrackX = fitter.getTrack(1).getX();

    lPositiveTrack = fitter.getTrack(0);
    lNegativeTrack = fitter.getTrack(1);
    lPositiveTrack.getPxPyPzGlo(v0candidate.posP);
    lNegativeTrack.getPxPyPzGlo(v0candidate.negP);

    // get decay vertex coordinates
    const auto& vtx = fitter.getPCACandidate();
    for (int i = 0; i < 3; i++) {
      v0candidate.pos[i] = vtx[i];
    }

    v0candidate.dcaV0dau = TMath::Sqrt(fitter.getChi2AtPCACandidate());

    // Apply selections so a skimmed table is created only
    if (v0candidate.dcaV0dau > dcav0dau) {
      return false;
    }

    // Passes DCA between daughters check
    statisticsRegistry.v0stats[kV0DCADau]++;

    v0candidate.cosPA = RecoDecay::cpa(array{collision.posX(), collision.posY(), collision.posZ()}, array{v0candidate.pos[0], v0candidate.pos[1], v0candidate.pos[2]}, array{v0candidate.posP[0] + v0candidate.negP[0], v0candidate.posP[1] + v0candidate.negP[1], v0candidate.posP[2] + v0candidate.negP[2]});
    if (v0candidate.cosPA < v0cospa) {
      return false;
    }

    // Passes CosPA check
    statisticsRegistry.v0stats[kV0CosPA]++;

    v0candidate.V0radius = RecoDecay::sqrtSumOfSquares(v0candidate.pos[0], v0candidate.pos[1]);
    if (v0candidate.V0radius < v0radius) {
      return false;
    }

    // Passes radius check
    statisticsRegistry.v0stats[kV0Radius]++;
    // Return OK: passed all v0 candidate selecton criteria
    return true;
  }

  template <class TTracksTo, typename TV0Objects>
  void buildStrangenessTables(aod::Collision const& collision, TV0Objects const& V0s, TTracksTo const& tracks)
  {
    statisticsRegistry.eventCounter++;

    for (auto& V0 : V0s) {
      // Track preselection part
      auto posTrackCast = V0.template posTrack_as<TTracksTo>();
      auto negTrackCast = V0.template negTrack_as<TTracksTo>();

      // populates v0candidate struct declared inside strangenessbuilder
      bool validCandidate = buildV0Candidate(collision, posTrackCast, negTrackCast);

      if (!validCandidate) {
        continue; // doesn't pass selections
      }

      // populates table for V0 analysis
      v0data(V0.posTrackId(),
             V0.negTrackId(),
             V0.collisionId(),
             V0.globalIndex(),
             v0candidate.posTrackX, v0candidate.negTrackX,
             v0candidate.pos[0], v0candidate.pos[1], v0candidate.pos[2],
             v0candidate.posP[0], v0candidate.posP[1], v0candidate.posP[2],
             v0candidate.negP[0], v0candidate.negP[1], v0candidate.negP[2],
             v0candidate.dcaV0dau,
             v0candidate.posDCAxy,
             v0candidate.negDCAxy);

      // populate V0 covariance matrices if required by any other task
      if (createV0CovMats) {
        // Calculate position covariance matrix
        auto covVtxV = fitter.calcPCACovMatrix(0);
        // std::array<float, 6> positionCovariance;
        float positionCovariance[6];
        positionCovariance[0] = covVtxV(0, 0);
        positionCovariance[1] = covVtxV(1, 0);
        positionCovariance[2] = covVtxV(1, 1);
        positionCovariance[3] = covVtxV(2, 0);
        positionCovariance[4] = covVtxV(2, 1);
        positionCovariance[5] = covVtxV(2, 2);
        // store momentum covariance matrix
        std::array<float, 21> covTpositive = {0.};
        std::array<float, 21> covTnegative = {0.};
        // std::array<float, 6> momentumCovariance;
        float momentumCovariance[6];
        lPositiveTrack.getCovXYZPxPyPzGlo(covTpositive);
        lNegativeTrack.getCovXYZPxPyPzGlo(covTnegative);
        constexpr int MomInd[6] = {9, 13, 14, 18, 19, 20}; // cov matrix elements for momentum component
        for (int i = 0; i < 6; i++) {
          momentumCovariance[i] = covTpositive[MomInd[i]] + covTnegative[MomInd[i]];
        }
        v0covs(positionCovariance, momentumCovariance);
      }
    }
    // En masse histo filling at end of process call
    fillHistos();
    resetHistos();
  }

  void processRun2(aod::Collision const& collision, aod::V0s const& V0s, FullTracksExt const& tracks, aod::BCsWithTimestamps const&)
  {
    /* check the previous run number */
    auto bc = collision.bc_as<aod::BCsWithTimestamps>();
    initCCDB(bc);

    // do v0s, typecase correctly into tracks (Run 2 use case)
    buildStrangenessTables<FullTracksExt>(collision, V0s, tracks);
  }
  PROCESS_SWITCH(lambdakzeroBuilder, processRun2, "Produce Run 2 V0 tables", true);

  void processRun3(aod::Collision const& collision, aod::V0s const& V0s, FullTracksExtIU const& tracks, aod::BCsWithTimestamps const&)
  {
    /* check the previous run number */
    auto bc = collision.bc_as<aod::BCsWithTimestamps>();
    initCCDB(bc);

    // do v0s, typecase correctly into tracksIU (Run 3 use case)
    buildStrangenessTables<FullTracksExtIU>(collision, V0s, tracks);
  }
  PROCESS_SWITCH(lambdakzeroBuilder, processRun3, "Produce Run 3 V0 tables", false);

  void processRun3associated(aod::Collision const& collision, soa::Filtered<TaggedV0s> const& V0s, FullTracksExtIU const& tracks, aod::BCsWithTimestamps const&)
  {
    /* check the previous run number */
    auto bc = collision.bc_as<aod::BCsWithTimestamps>();
    initCCDB(bc);

    // do v0s, typecase correctly into tracksIU (Run 3 use case)
    buildStrangenessTables<FullTracksExtIU>(collision, V0s, tracks);
  }
  PROCESS_SWITCH(lambdakzeroBuilder, processRun3associated, "Produce Run 3 V0 tables only for MC associated", false);
};

//*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*
struct lambdakzeroLabelBuilder {
  Produces<aod::McV0Labels> v0labels; // MC labels for V0s

  void init(InitContext const&) {}

  void processDoNotBuildLabels(aod::Collisions::iterator const& collision)
  {
    // dummy process function - should not be required in the future
  }
  PROCESS_SWITCH(lambdakzeroLabelBuilder, processDoNotBuildLabels, "Do not produce MC label tables", true);

  //*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*
  // build V0 labels if requested to do so
  void processBuildV0Labels(aod::Collision const& collision, aod::V0Datas const& v0table, LabeledTracks const&, aod::McParticles const& particlesMC)
  {
    for (auto& v0 : v0table) {
      int lLabel = -1;

      auto lNegTrack = v0.negTrack_as<LabeledTracks>();
      auto lPosTrack = v0.posTrack_as<LabeledTracks>();

      // Association check
      // There might be smarter ways of doing this in the future
      if (lNegTrack.has_mcParticle() && lPosTrack.has_mcParticle()) {
        auto lMCNegTrack = lNegTrack.mcParticle_as<aod::McParticles>();
        auto lMCPosTrack = lPosTrack.mcParticle_as<aod::McParticles>();
        if (lMCNegTrack.has_mothers() && lMCPosTrack.has_mothers()) {

          for (auto& lNegMother : lMCNegTrack.mothers_as<aod::McParticles>()) {
            for (auto& lPosMother : lMCPosTrack.mothers_as<aod::McParticles>()) {
              if (lNegMother.globalIndex() == lPosMother.globalIndex()) {
                lLabel = lNegMother.globalIndex();
              }
            }
          }
        }
      } // end association check
      // Construct label table (note: this will be joinable with V0Datas!)
      v0labels(
        lLabel);
    }
  }
  PROCESS_SWITCH(lambdakzeroLabelBuilder, processBuildV0Labels, "Produce V0 MC label tables for analysis", false);
};

//*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*
struct lambdakzeroTagBuilder {
  Produces<aod::V0Tags> v0tags; // MC tags

  void init(InitContext const&) {}

  void processDoNotBuildTags(aod::Collisions::iterator const& collision)
  {
    // dummy process function - should not be required in the future
  }
  PROCESS_SWITCH(lambdakzeroTagBuilder, processDoNotBuildTags, "Do not produce MC tag tables", true);
  //*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*
  // build V0 tags if requested to do so
  // WARNING: this is an internal table meant to have the builder
  // build only associated candidates. It is not, in principle, part
  // of the main data model for strangeness analyses.
  //
  // The main difference:
  // --- the V0Tags table is joinable with v0s (for building)
  // --- the V0labels is joinable with V0Data (for analysis)
  void processBuildV0Tags(aod::Collision const& collision, aod::V0s const& v0table, LabeledTracks const&, aod::McParticles const& particlesMC)
  {
    for (auto& v0 : v0table) {
      int lPDG = -1;

      auto lNegTrack = v0.negTrack_as<LabeledTracks>();
      auto lPosTrack = v0.posTrack_as<LabeledTracks>();

      // Association check
      // There might be smarter ways of doing this in the future
      if (lNegTrack.has_mcParticle() && lPosTrack.has_mcParticle()) {
        auto lMCNegTrack = lNegTrack.mcParticle_as<aod::McParticles>();
        auto lMCPosTrack = lPosTrack.mcParticle_as<aod::McParticles>();
        if (lMCNegTrack.has_mothers() && lMCPosTrack.has_mothers()) {

          for (auto& lNegMother : lMCNegTrack.mothers_as<aod::McParticles>()) {
            for (auto& lPosMother : lMCPosTrack.mothers_as<aod::McParticles>()) {
              if (lNegMother.globalIndex() == lPosMother.globalIndex()) {
                lPDG = lNegMother.pdgCode();
              }
            }
          }
        }
      } // end association check
      // Construct label table (note: this will be joinable with V0s!)

      int lInteresting = 0;
      if (lPDG == 310 || TMath::Abs(lPDG) == 3122 || TMath::Abs(lPDG) == 1010010030)
        lInteresting = 1;
      v0tags(lInteresting);
    }
  }
  PROCESS_SWITCH(lambdakzeroTagBuilder, processBuildV0Tags, "Produce V0 MC tag tables for MC associated building", false);
  //*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*
};

//*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*
struct lambdakzeroV0DataLinkBuilder {
  Produces<aod::V0DataLink> v0dataLink;

  void init(InitContext const&) {}

  //*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*
  // build V0 -> V0Data link table
  void process(aod::V0s const& v0table, aod::V0Datas const& v0datatable)
  {
    std::vector<int> lIndices;
    lIndices.reserve(v0table.size());
    for (int ii = 0; ii < v0table.size(); ii++)
      lIndices[ii] = -1;
    for (auto& v0data : v0datatable) {
      lIndices[v0data.v0Id()] = v0data.globalIndex();
    }
    for (int ii = 0; ii < v0table.size(); ii++) {
      v0dataLink(lIndices[ii]);
    }
  }
  //*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*
};

// Extends the v0data table with expression columns
struct lambdakzeroInitializer {
  Spawns<aod::V0Datas> v0datas;
  void init(InitContext const&) {}
};

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  return WorkflowSpec{
    adaptAnalysisTask<lambdakzeroBuilder>(cfgc),
    adaptAnalysisTask<lambdakzeroLabelBuilder>(cfgc),
    adaptAnalysisTask<lambdakzeroTagBuilder>(cfgc),
    adaptAnalysisTask<lambdakzeroV0DataLinkBuilder>(cfgc),
    adaptAnalysisTask<lambdakzeroInitializer>(cfgc)};
}
