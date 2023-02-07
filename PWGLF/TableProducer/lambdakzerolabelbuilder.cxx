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
//  Lambdakzero builder task
//  *+-+*+-+*+-+*+-+*+-+*+-+*
//
//  This task loops over a set of V0 indices and
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
#include "DCAFitter/DCAFitterN.h"
#include "ReconstructionDataFormats/Track.h"
#include "Common/Core/RecoDecay.h"
#include "Common/Core/trackUtilities.h"
#include "PWGLF/DataModel/LFStrangenessTables.h"
#include "PWGLF/DataModel/LFParticleIdentification.h"
#include "Common/Core/TrackSelection.h"
#include "Common/DataModel/TrackSelectionTables.h"
#include "DetectorsBase/Propagator.h"
#include "DetectorsBase/GeometryManager.h"
#include "DataFormatsParameters/GRPObject.h"
#include "DataFormatsParameters/GRPMagField.h"
#include "CCDB/BasicCCDBManager.h"

using namespace o2;
using namespace o2::framework;
using namespace o2::framework::expressions;
using std::array;

namespace o2::aod
{
namespace v0tag
{
// Global bool
DECLARE_SOA_COLUMN(IsInteresting, isInteresting, bool); //! will this be built or not?

// MC association bools
DECLARE_SOA_COLUMN(IsTrueGamma, isTrueGamma, bool);                     //! PDG checked correctly in MC
DECLARE_SOA_COLUMN(IsTrueK0Short, isTrueK0Short, bool);                 //! PDG checked correctly in MC
DECLARE_SOA_COLUMN(IsTrueLambda, isTrueLambda, bool);                   //! PDG checked correctly in MC
DECLARE_SOA_COLUMN(IsTrueAntiLambda, isTrueAntiLambda, bool);           //! PDG checked correctly in MC
DECLARE_SOA_COLUMN(IsTrueHypertriton, isTrueHypertriton, bool);         //! PDG checked correctly in MC
DECLARE_SOA_COLUMN(IsTrueAntiHypertriton, isTrueAntiHypertriton, bool); //! PDG checked correctly in MC

// dE/dx compatibility bools
DECLARE_SOA_COLUMN(IsGammaCandidate, isGammaCandidate, bool);                     //! compatible with dE/dx hypotheses
DECLARE_SOA_COLUMN(IsK0ShortCandidate, isK0ShortCandidate, bool);                 //! compatible with dE/dx hypotheses
DECLARE_SOA_COLUMN(IsLambdaCandidate, isLambdaCandidate, bool);                   //! compatible with dE/dx hypotheses
DECLARE_SOA_COLUMN(IsAntiLambdaCandidate, isAntiLambdaCandidate, bool);           //! compatible with dE/dx hypotheses
DECLARE_SOA_COLUMN(IsHypertritonCandidate, isHypertritonCandidate, bool);         //! compatible with dE/dx hypotheses
DECLARE_SOA_COLUMN(IsAntiHypertritonCandidate, isAntiHypertritonCandidate, bool); //! compatible with dE/dx hypotheses
}
DECLARE_SOA_TABLE(V0Tags, "AOD", "V0TAGS",
                  v0tag::IsInteresting,
                  v0tag::IsTrueGamma,
                  v0tag::IsTrueK0Short,
                  v0tag::IsTrueLambda,
                  v0tag::IsTrueAntiLambda,
                  v0tag::IsTrueHypertriton,
                  v0tag::IsTrueAntiHypertriton,
                  v0tag::IsGammaCandidate,
                  v0tag::IsK0ShortCandidate,
                  v0tag::IsLambdaCandidate,
                  v0tag::IsAntiLambdaCandidate,
                  v0tag::IsHypertritonCandidate,
                  v0tag::IsAntiHypertritonCandidate);
} // namespace o2::aod

// use parameters + cov mat non-propagated, aux info + (extension propagated)
using FullTracksExt = soa::Join<aod::Tracks, aod::TracksExtra, aod::TracksCov>;
using FullTracksExtIU = soa::Join<aod::TracksIU, aod::TracksExtra, aod::TracksCovIU>;
using TracksWithExtra = soa::Join<aod::Tracks, aod::TracksExtra>;

// For dE/dx association in pre-selection
using TracksExtraWithPID = soa::Join<aod::TracksExtra, aod::pidTPCFullEl, aod::pidTPCFullPi, aod::pidTPCFullPr, aod::pidTPCFullHe>;

// For MC and dE/dx association
using TracksExtraWithPIDandLabels = soa::Join<aod::TracksExtra, aod::pidTPCFullEl, aod::pidTPCFullPi, aod::pidTPCFullPr, aod::pidTPCFullHe, aod::McTrackLabels>;

// Pre-selected V0s
using TaggedV0s = soa::Join<aod::V0s, aod::V0Tags>;

// For MC association in pre-selection
using LabeledTracksExtra = soa::Join<aod::TracksExtra, aod::McTrackLabels>;

struct lambdakzeroBuilder {
  Produces<aod::StoredV0Datas> v0data;
  Produces<aod::V0Covs> v0covs; // covariances
  Service<o2::ccdb::BasicCCDBManager> ccdb;

  // Configurables related to table creation
  Configurable<int> createV0CovMats{"createV0CovMats", -1, {"Produces V0 cov matrices. -1: auto, 0: don't, 1: yes. Default: auto (-1)"}};

  // use auto-detect configuration
  Configurable<bool> d_UseAutodetectMode{"d_UseAutodetectMode", true, "Autodetect requested topo sels"};

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
  Configurable<bool> d_doTrackQA{"d_doTrackQA", false, "do track QA"};

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

  Filter taggedFilter = aod::v0tag::isInteresting == true;

  // For manual sliceBy
  Preslice<aod::V0s> perCollision = o2::aod::v0::collisionId;

  enum v0step { kV0All = 0,
                kV0TPCrefit,
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
    std::array<long, 10> posITSclu;
    std::array<long, 10> negITSclu;
    long exceptions;
    long eventCounter;
  } statisticsRegistry;

  HistogramRegistry registry{
    "registry",
    {{"hEventCounter", "hEventCounter", {HistType::kTH1F, {{1, 0.0f, 1.0f}}}},
     {"hCaughtExceptions", "hCaughtExceptions", {HistType::kTH1F, {{1, 0.0f, 1.0f}}}},
     {"hPositiveITSClusters", "hPositiveITSClusters", {HistType::kTH1F, {{10, -0.5f, 9.5f}}}},
     {"hNegativeITSClusters", "hNegativeITSClusters", {HistType::kTH1F, {{10, -0.5f, 9.5f}}}},
     {"hV0Criteria", "hV0Criteria", {HistType::kTH1F, {{10, -0.5f, 9.5f}}}}}};

  void resetHistos()
  {
    statisticsRegistry.exceptions = 0;
    statisticsRegistry.eventCounter = 0;
    for (Int_t ii = 0; ii < kNV0Steps; ii++)
      statisticsRegistry.v0stats[ii] = 0;
    for (Int_t ii = 0; ii < 10; ii++) {
      statisticsRegistry.posITSclu[ii] = 0;
      statisticsRegistry.negITSclu[ii] = 0;
    }
  }

  void fillHistos()
  {
    registry.fill(HIST("hEventCounter"), 0.0, statisticsRegistry.eventCounter);
    registry.fill(HIST("hCaughtExceptions"), 0.0, statisticsRegistry.exceptions);
    for (Int_t ii = 0; ii < kNV0Steps; ii++)
      registry.fill(HIST("hV0Criteria"), ii, statisticsRegistry.v0stats[ii]);
    if (d_doTrackQA) {
      for (Int_t ii = 0; ii < 10; ii++) {
        registry.fill(HIST("hPositiveITSClusters"), ii, statisticsRegistry.posITSclu[ii]);
        registry.fill(HIST("hNegativeITSClusters"), ii, statisticsRegistry.negITSclu[ii]);
      }
    }
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

    if (useMatCorrType == 1) {
      LOGF(info, "TGeo correction requested, loading geometry");
      if (!o2::base::GeometryManager::isGeometryLoaded()) {
        ccdb->get<TGeoManager>(geoPath);
      }
    }
    if (useMatCorrType == 2) {
      LOGF(info, "LUT correction requested, loading LUT");
      lut = o2::base::MatLayerCylSet::rectifyPtrFromFile(ccdb->get<o2::base::MatLayerCylSet>(lutPath));
    }

    if (doprocessRun2 == false && doprocessRun3 == false) {
      LOGF(fatal, "Neither processRun2 nor processRun3 enabled. Please choose one.");
    }
    if (doprocessRun2 == true && doprocessRun3 == true) {
      LOGF(fatal, "Cannot enable processRun2 and processRun3 at the same time. Please choose one.");
    }

    if (d_UseAutodetectMode) {
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

    // In case override, don't proceed, please - no CCDB access required
    if (d_bz_input > -990) {
      d_bz = d_bz_input;
      fitter.setBz(d_bz);
      o2::parameters::GRPMagField grpmag;
      if (fabs(d_bz) > 1e-5) {
        grpmag.setL3Current(30000.f / (d_bz / 5.0f));
      }
      o2::base::Propagator::initFieldFromGRP(&grpmag);
      mRunNumber = bc.runNumber();
      return;
    }

    auto run3grp_timestamp = bc.timestamp();
    o2::parameters::GRPObject* grpo = ccdb->getForTimeStamp<o2::parameters::GRPObject>(grpPath, run3grp_timestamp);
    o2::parameters::GRPMagField* grpmag = 0x0;
    if (grpo) {
      o2::base::Propagator::initFieldFromGRP(grpo);
      // Fetch magnetic field from ccdb for current collision
      d_bz = grpo->getNominalL3Field();
      LOG(info) << "Retrieved GRP for timestamp " << run3grp_timestamp << " with magnetic field of " << d_bz << " kZG";
    } else {
      grpmag = ccdb->getForTimeStamp<o2::parameters::GRPMagField>(grpmagPath, run3grp_timestamp);
      if (!grpmag) {
        LOG(fatal) << "Got nullptr from CCDB for path " << grpmagPath << " of object GRPMagField and " << grpPath << " of object GRPObject for timestamp " << run3grp_timestamp;
      }
      o2::base::Propagator::initFieldFromGRP(grpmag);
      // Fetch magnetic field from ccdb for current collision
      d_bz = std::lround(5.f * grpmag->getL3Current() / 30000.f);
      LOG(info) << "Retrieved GRP for timestamp " << run3grp_timestamp << " with magnetic field of " << d_bz << " kZG";
    }
    mRunNumber = bc.runNumber();
    // Set magnetic field value once known
    fitter.setBz(d_bz);

    if (useMatCorrType == 2) {
      // setMatLUT only after magfield has been initalized
      // (setMatLUT has implicit and problematic init field call if not)
      o2::base::Propagator::Instance()->setMatLUT(lut);
    }
  }

  template <class TTrackTo, typename TV0Object>
  bool buildV0Candidate(TV0Object const& V0)
  {
    // Get tracks
    auto const& posTrack = V0.template posTrack_as<TTrackTo>();
    auto const& negTrack = V0.template negTrack_as<TTrackTo>();
    auto const& collision = V0.collision();

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

    // Calculate DCA with respect to the collision associated to the V0, not individual tracks
    gpu::gpustd::array<float, 2> dcaInfo;

    auto posTrackPar = getTrackPar(posTrack);
    o2::base::Propagator::Instance()->propagateToDCABxByBz({collision.posX(), collision.posY(), collision.posZ()}, posTrackPar, 2.f, fitter.getMatCorrType(), &dcaInfo);
    auto posTrackdcaXY = dcaInfo[0];

    auto negTrackPar = getTrackPar(negTrack);
    o2::base::Propagator::Instance()->propagateToDCABxByBz({collision.posX(), collision.posY(), collision.posZ()}, negTrackPar, 2.f, fitter.getMatCorrType(), &dcaInfo);
    auto negTrackdcaXY = dcaInfo[0];

    if (fabs(posTrackdcaXY) < dcapostopv || fabs(negTrackdcaXY) < dcanegtopv) {
      return false;
    }

    // Initialize properly, please
    v0candidate.posDCAxy = posTrackdcaXY;
    v0candidate.negDCAxy = negTrackdcaXY;

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
    if (d_doTrackQA) {
      if (posTrack.itsNCls() < 10)
        statisticsRegistry.posITSclu[posTrack.itsNCls()]++;
      if (negTrack.itsNCls() < 10)
        statisticsRegistry.negITSclu[negTrack.itsNCls()]++;
    }
    return true;
  }

  template <class TTrackTo, typename TV0Table>
  void buildStrangenessTables(TV0Table const& V0s)
  {
    statisticsRegistry.eventCounter++;

    // Loops over all V0s in the time frame
    for (auto& V0 : V0s) {
      // populates v0candidate struct declared inside strangenessbuilder
      bool validCandidate = buildV0Candidate<TTrackTo>(V0);

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

  void processRun2(aod::Collisions const& collisions, soa::Filtered<TaggedV0s> const& V0s, FullTracksExt const&, aod::BCsWithTimestamps const&)
  {
    for (const auto& collision : collisions) {
      // Fire up CCDB
      auto bc = collision.bc_as<aod::BCsWithTimestamps>();
      initCCDB(bc);
      // Do analysis with collision-grouped V0s, retain full collision information
      const uint64_t collIdx = collision.globalIndex();
      auto V0Table_thisCollision = V0s.sliceBy(perCollision, collIdx);
      buildStrangenessTables<FullTracksExt>(V0Table_thisCollision);
    }
  }
  PROCESS_SWITCH(lambdakzeroBuilder, processRun2, "Produce Run 2 V0 tables", false);

  void processRun3(aod::Collisions const& collisions, soa::Filtered<TaggedV0s> const& V0s, FullTracksExtIU const&, aod::BCsWithTimestamps const&)
  {
    for (const auto& collision : collisions) {
      // Fire up CCDB
      auto bc = collision.bc_as<aod::BCsWithTimestamps>();
      initCCDB(bc);
      // Do analysis with collision-grouped V0s, retain full collision information
      const uint64_t collIdx = collision.globalIndex();
      auto V0Table_thisCollision = V0s.sliceBy(perCollision, collIdx);
      buildStrangenessTables<FullTracksExtIU>(V0Table_thisCollision);
    }
  }
  PROCESS_SWITCH(lambdakzeroBuilder, processRun3, "Produce Run 3 V0 tables", true);
};

//*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*
struct lambdakzeroPreselector {
  Produces<aod::V0Tags> v0tags; // MC tags

  Configurable<bool> dIfMCgenerateK0Short{"dIfMCgenerateK0Short", true, "if MC, generate MC true K0Short (yes/no)"};
  Configurable<bool> dIfMCgenerateLambda{"dIfMCgenerateLambda", true, "if MC, generate MC true Lambda (yes/no)"};
  Configurable<bool> dIfMCgenerateAntiLambda{"dIfMCgenerateAntiLambda", true, "if MC, generate MC true AntiLambda (yes/no)"};
  Configurable<bool> dIfMCgenerateGamma{"dIfMCgenerateGamma", false, "if MC, generate MC true gamma (yes/no)"};
  Configurable<bool> dIfMCgenerateHypertriton{"dIfMCgenerateHypertriton", false, "if MC, generate MC true hypertritons (yes/no)"};
  Configurable<bool> dIfMCgenerateAntiHypertriton{"dIfMCgenerateAntiHypertriton", false, "if MC, generate MC true antihypertritons (yes/no)"};

  Configurable<bool> ddEdxPreSelectK0Short{"ddEdxPreSelectK0Short", true, "pre-select dE/dx compatibility with K0Short (yes/no)"};
  Configurable<bool> ddEdxPreSelectLambda{"ddEdxPreSelectLambda", true, "pre-select dE/dx compatibility with Lambda (yes/no)"};
  Configurable<bool> ddEdxPreSelectAntiLambda{"ddEdxPreSelectAntiLambda", true, "pre-select dE/dx compatibility with AntiLambda (yes/no)"};
  Configurable<bool> ddEdxPreSelectGamma{"ddEdxPreSelectGamma", false, "pre-select dE/dx compatibility with gamma (yes/no)"};
  Configurable<bool> ddEdxPreSelectHypertriton{"ddEdxPreSelectHypertriton", false, "pre-select dE/dx compatibility with hypertritons (yes/no)"};
  Configurable<bool> ddEdxPreSelectAntiHypertriton{"ddEdxPreSelectAntiHypertriton", false, "pre-select dE/dx compatibility with antihypertritons (yes/no)"};

  // dEdx pre-selection compatibility
  Configurable<float> ddEdxPreSelectionWindow{"ddEdxPreSelectionWindow", 7, "Nsigma window for dE/dx preselection"};

  // tpc quality pre-selection
  Configurable<int> dTPCNCrossedRows{"dTPCNCrossedRows", 50, "Minimum TPC crossed rows"};

  // context-aware selections
  Configurable<bool> dPreselectOnlyBaryons{"dPreselectOnlyBaryons", false, "apply TPC dE/dx and quality only to baryon daughters"};

  void init(InitContext const&) {}

  //*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*
  /// function to check track quality
  template <class TTrackTo, typename TV0Object>
  void checkTrackQuality(TV0Object const& lV0Candidate, bool& lIsInteresting, bool lIsGamma, bool lIsK0Short, bool lIsLambda, bool lIsAntiLambda, bool lIsHypertriton, bool lIsAntiHypertriton)
  {
    lIsInteresting = false;
    auto lNegTrack = lV0Candidate.template negTrack_as<TTrackTo>();
    auto lPosTrack = lV0Candidate.template posTrack_as<TTrackTo>();

    // No baryons in decay
    if ((lIsGamma || lIsK0Short) && (lPosTrack.tpcNClsCrossedRows() >= dTPCNCrossedRows && lNegTrack.tpcNClsCrossedRows() >= dTPCNCrossedRows))
      lIsInteresting = true;
    // With baryons in decay
    if (lIsLambda && (lPosTrack.tpcNClsCrossedRows() >= dTPCNCrossedRows && (lNegTrack.tpcNClsCrossedRows() >= dTPCNCrossedRows || dPreselectOnlyBaryons)))
      lIsInteresting = true;
    if (lIsAntiLambda && (lNegTrack.tpcNClsCrossedRows() >= dTPCNCrossedRows && (lPosTrack.tpcNClsCrossedRows() >= dTPCNCrossedRows || dPreselectOnlyBaryons)))
      lIsInteresting = true;
    if (lIsHypertriton && (lPosTrack.tpcNClsCrossedRows() >= dTPCNCrossedRows && (lNegTrack.tpcNClsCrossedRows() >= dTPCNCrossedRows || dPreselectOnlyBaryons)))
      lIsInteresting = true;
    if (lIsHypertriton && (lNegTrack.tpcNClsCrossedRows() >= dTPCNCrossedRows && (lPosTrack.tpcNClsCrossedRows() >= dTPCNCrossedRows || dPreselectOnlyBaryons)))
      lIsInteresting = true;
  }
  //*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*
  /// function to check PDG association
  template <class TTrackTo, typename TV0Object>
  void checkPDG(TV0Object const& lV0Candidate, bool& lIsInteresting, bool& lIsGamma, bool& lIsK0Short, bool& lIsLambda, bool& lIsAntiLambda, bool& lIsHypertriton, bool& lIsAntiHypertriton)
  {
    int lPDG = -1;
    auto lNegTrack = lV0Candidate.template negTrack_as<TTrackTo>();
    auto lPosTrack = lV0Candidate.template posTrack_as<TTrackTo>();

    // Association check
    // There might be smarter ways of doing this in the future
    if (lNegTrack.has_mcParticle() && lPosTrack.has_mcParticle()) {
      auto lMCNegTrack = lNegTrack.template mcParticle_as<aod::McParticles>();
      auto lMCPosTrack = lPosTrack.template mcParticle_as<aod::McParticles>();
      if (lMCNegTrack.has_mothers() && lMCPosTrack.has_mothers()) {

        for (auto& lNegMother : lMCNegTrack.template mothers_as<aod::McParticles>()) {
          for (auto& lPosMother : lMCPosTrack.template mothers_as<aod::McParticles>()) {
            if (lNegMother.globalIndex() == lPosMother.globalIndex()) {
              lPDG = lNegMother.pdgCode();
            }
          }
        }
      }
    } // end association check
    if (lPDG == 310 && dIfMCgenerateK0Short) {
      lIsK0Short = true;
      lIsInteresting = true;
    }
    if (lPDG == 3122 && dIfMCgenerateLambda) {
      lIsLambda = true;
      lIsInteresting = true;
    }
    if (lPDG == -3122 && dIfMCgenerateAntiLambda) {
      lIsAntiLambda = true;
      lIsInteresting = true;
    }
    if (lPDG == 22 && dIfMCgenerateGamma) {
      lIsGamma = true;
      lIsInteresting = true;
    }
    if (lPDG == 1010010030 && dIfMCgenerateHypertriton) {
      lIsHypertriton = true;
      lIsInteresting = true;
    }
    if (lPDG == -1010010030 && dIfMCgenerateAntiHypertriton) {
      lIsAntiHypertriton = true;
      lIsInteresting = true;
    }
  }
  //*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*
  template <class TTrackTo, typename TV0Object>
  void checkdEdx(TV0Object const& lV0Candidate, bool& lIsInteresting, bool& lIsGamma, bool& lIsK0Short, bool& lIsLambda, bool& lIsAntiLambda, bool& lIsHypertriton, bool& lIsAntiHypertriton)
  {
    auto lNegTrack = lV0Candidate.template negTrack_as<TTrackTo>();
    auto lPosTrack = lV0Candidate.template posTrack_as<TTrackTo>();

    // dEdx check with LF PID
    if (TMath::Abs(lNegTrack.tpcNSigmaEl()) < ddEdxPreSelectionWindow &&
        TMath::Abs(lPosTrack.tpcNSigmaEl()) < ddEdxPreSelectionWindow &&
        ddEdxPreSelectGamma) {
      lIsGamma = 1;
      lIsInteresting = 1;
    }
    if (TMath::Abs(lNegTrack.tpcNSigmaPi()) < ddEdxPreSelectionWindow &&
        TMath::Abs(lPosTrack.tpcNSigmaPi()) < ddEdxPreSelectionWindow &&
        ddEdxPreSelectK0Short) {
      lIsK0Short = 1;
      lIsInteresting = 1;
    }
    if ((TMath::Abs(lNegTrack.tpcNSigmaPi()) < ddEdxPreSelectionWindow || dPreselectOnlyBaryons) &&
        TMath::Abs(lPosTrack.tpcNSigmaPr()) < ddEdxPreSelectionWindow &&
        ddEdxPreSelectLambda) {
      lIsLambda = 1;
      lIsInteresting = 1;
    }
    if (TMath::Abs(lNegTrack.tpcNSigmaPr()) < ddEdxPreSelectionWindow &&
        (TMath::Abs(lPosTrack.tpcNSigmaPi()) < ddEdxPreSelectionWindow || dPreselectOnlyBaryons) &&
        ddEdxPreSelectAntiLambda) {
      lIsAntiLambda = 1;
      lIsInteresting = 1;
    }
    if (TMath::Abs(lNegTrack.tpcNSigmaPi()) < ddEdxPreSelectionWindow &&
        (TMath::Abs(lPosTrack.tpcNSigmaHe()) < ddEdxPreSelectionWindow || dPreselectOnlyBaryons) &&
        ddEdxPreSelectHypertriton) {
      lIsHypertriton = 1;
      lIsInteresting = 1;
    }
    if ((TMath::Abs(lNegTrack.tpcNSigmaHe()) < ddEdxPreSelectionWindow || dPreselectOnlyBaryons) &&
        TMath::Abs(lPosTrack.tpcNSigmaPi()) < ddEdxPreSelectionWindow &&
        ddEdxPreSelectAntiHypertriton) {
      lIsAntiHypertriton = 1;
      lIsInteresting = 1;
    }
  }
  //*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*
  /// This process function ensures that all V0s are built. It will simply tag everything as true.
  void processBuildAll(aod::V0s const& v0table, aod::TracksExtra const&)
  {
    for (auto& v0 : v0table) {
      bool lIsQualityInteresting = false;
      checkTrackQuality<aod::TracksExtra>(v0, lIsQualityInteresting, true, true, true, true, true, true);
      v0tags(lIsQualityInteresting,
             true, true, true, true, true, true,
             true, true, true, true, true, true);
    }
  }
  PROCESS_SWITCH(lambdakzeroPreselector, processBuildAll, "Switch to build all V0s", true);
  //*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*
  void processBuildMCAssociated(aod::Collision const& collision, aod::V0s const& v0table, LabeledTracksExtra const&, aod::McParticles const& particlesMC)
  {
    for (auto& v0 : v0table) {
      bool lIsInteresting = false;
      bool lIsTrueGamma = false;
      bool lIsTrueK0Short = false;
      bool lIsTrueLambda = false;
      bool lIsTrueAntiLambda = false;
      bool lIsTrueHypertriton = false;
      bool lIsTrueAntiHypertriton = false;

      bool lIsQualityInteresting = false;

      checkPDG<LabeledTracksExtra>(v0, lIsInteresting, lIsTrueGamma, lIsTrueK0Short, lIsTrueLambda, lIsTrueAntiLambda, lIsTrueHypertriton, lIsTrueAntiHypertriton);
      checkTrackQuality<LabeledTracksExtra>(v0, lIsQualityInteresting, true, true, true, true, true, true);
      v0tags(lIsInteresting * lIsQualityInteresting,
             lIsTrueGamma, lIsTrueK0Short, lIsTrueLambda, lIsTrueAntiLambda, lIsTrueHypertriton, lIsTrueAntiHypertriton,
             true, true, true, true, true, true);
    }
  }
  PROCESS_SWITCH(lambdakzeroPreselector, processBuildMCAssociated, "Switch to build MC-associated V0s", false);
  //*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*
  void processBuildValiddEdx(aod::Collision const& collision, aod::V0s const& v0table, TracksExtraWithPID const&)
  {
    for (auto& v0 : v0table) {
      bool lIsInteresting = false;
      bool lIsdEdxGamma = false;
      bool lIsdEdxK0Short = false;
      bool lIsdEdxLambda = false;
      bool lIsdEdxAntiLambda = false;
      bool lIsdEdxHypertriton = false;
      bool lIsdEdxAntiHypertriton = false;

      bool lIsQualityInteresting = false;

      checkdEdx<TracksExtraWithPID>(v0, lIsInteresting, lIsdEdxGamma, lIsdEdxK0Short, lIsdEdxLambda, lIsdEdxAntiLambda, lIsdEdxHypertriton, lIsdEdxAntiHypertriton);
      checkTrackQuality<TracksExtraWithPID>(v0, lIsQualityInteresting, lIsdEdxGamma, lIsdEdxK0Short, lIsdEdxLambda, lIsdEdxAntiLambda, lIsdEdxHypertriton, lIsdEdxAntiHypertriton);
      v0tags(lIsInteresting * lIsQualityInteresting,
             true, true, true, true, true, true,
             lIsdEdxGamma, lIsdEdxK0Short, lIsdEdxLambda, lIsdEdxAntiLambda, lIsdEdxHypertriton, lIsdEdxAntiHypertriton);
    }
  }
  PROCESS_SWITCH(lambdakzeroPreselector, processBuildValiddEdx, "Switch to build V0s with dE/dx preselection", false);
  //*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*+-+*
  void processBuildValiddEdxMCAssociated(aod::Collision const& collision, aod::V0s const& v0table, TracksExtraWithPIDandLabels const&)
  {
    for (auto& v0 : v0table) {
      bool lIsTrueInteresting = false;
      bool lIsTrueGamma = false;
      bool lIsTrueK0Short = false;
      bool lIsTrueLambda = false;
      bool lIsTrueAntiLambda = false;
      bool lIsTrueHypertriton = false;
      bool lIsTrueAntiHypertriton = false;

      bool lIsdEdxInteresting = false;
      bool lIsdEdxGamma = false;
      bool lIsdEdxK0Short = false;
      bool lIsdEdxLambda = false;
      bool lIsdEdxAntiLambda = false;
      bool lIsdEdxHypertriton = false;
      bool lIsdEdxAntiHypertriton = false;

      bool lIsQualityInteresting = false;

      checkPDG<TracksExtraWithPIDandLabels>(v0, lIsTrueInteresting, lIsTrueGamma, lIsTrueK0Short, lIsTrueLambda, lIsTrueAntiLambda, lIsTrueHypertriton, lIsTrueAntiHypertriton);
      checkdEdx<TracksExtraWithPIDandLabels>(v0, lIsdEdxInteresting, lIsdEdxGamma, lIsdEdxK0Short, lIsdEdxLambda, lIsdEdxAntiLambda, lIsdEdxHypertriton, lIsdEdxAntiHypertriton);
      checkTrackQuality<TracksExtraWithPIDandLabels>(v0, lIsQualityInteresting, lIsdEdxGamma, lIsdEdxK0Short, lIsdEdxLambda, lIsdEdxAntiLambda, lIsdEdxHypertriton, lIsdEdxAntiHypertriton);
      v0tags(lIsTrueInteresting * lIsdEdxInteresting * lIsQualityInteresting,
             lIsTrueGamma, lIsTrueK0Short, lIsTrueLambda, lIsTrueAntiLambda, lIsTrueHypertriton, lIsTrueAntiHypertriton,
             lIsdEdxGamma, lIsdEdxK0Short, lIsdEdxLambda, lIsdEdxAntiLambda, lIsdEdxHypertriton, lIsdEdxAntiHypertriton);
    }
  }
  PROCESS_SWITCH(lambdakzeroPreselector, processBuildValiddEdxMCAssociated, "Switch to build MC-associated V0s with dE/dx preselection", false);
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
    adaptAnalysisTask<lambdakzeroPreselector>(cfgc),
    adaptAnalysisTask<lambdakzeroV0DataLinkBuilder>(cfgc),
    adaptAnalysisTask<lambdakzeroInitializer>(cfgc)};
}
