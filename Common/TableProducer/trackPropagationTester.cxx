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

//===============================================================
//
// Experimental version of the track propagation task 
// this utilizes an analysis task module that can be employed elsewhere
// and allows for the re-utilization of a material LUT 
// 
// candidate approach for core service approach
//
//===============================================================

#include "TableHelper.h"
#include "Common/Tools/TrackTuner.h"

// The Run 3 AO2D stores the tracks at the point of innermost update. For a track with ITS this is the innermost (or second innermost)
// ITS layer. For a track without ITS, this is the TPC inner wall or for loopers in the TPC even a radius beyond that.
// In order to use the track parameters, the tracks have to be propagated to the collision vertex which is done by this task.
// The task consumes the TracksIU and TracksCovIU tables and produces Tracks and TracksCov to which then the user analysis can subscribe.
//
// This task is not needed for Run 2 converted data.
// There are two versions of the task (see process flags), one producing also the covariance matrix and the other only the tracks table.

using namespace o2;
using namespace o2::framework;
// using namespace o2::framework::expressions;

struct TrackPropagation {
  // produces group to be passed to track propagation module
  struct : ProducesGroup {
    //__________________________________________________
    // V0 tables
    Produces<aod::StoredTracks> tracksParPropagated;
    Produces<aod::TracksExtension> tracksParExtensionPropagated;
    Produces<aod::StoredTracksCov> tracksParCovPropagated;
    Produces<aod::TracksCovExtension> tracksParCovExtensionPropagated;
    Produces<aod::TracksDCA> tracksDCA;
    Produces<aod::TracksDCACov> tracksDCACov;
    Produces<aod::TrackTunerTable> tunertable;
  } trackPropagationProducts

  Service<o2::ccdb::BasicCCDBManager> ccdb;

  using TracksIUWithMc = soa::Join<aod::StoredTracksIU, aod::McTrackLabels, aod::TracksCovIU>;

  HistogramRegistry registry{"registry"};

  void init(o2::framework::InitContext& initContext)
  {

    ccdb->setURL(ccdburl);
    ccdb->setCaching(true);
    ccdb->setLocalObjectValidityChecking();

  }

  void initCCDB(aod::BCsWithTimestamps::iterator const& bc)
  {

  }

  void processStandard(aod::StoredTracksIU const& tracks, aod::Collisions const& collisions, aod::BCsWithTimestamps const& bcs)
  {
    fillTrackTables</*TTrack*/ aod::StoredTracksIU, /*Particle*/ aod::StoredTracksIU, /*isMc = */ false, /*fillCovMat =*/false, /*useTrkPid =*/false>(tracks, tracks, collisions, bcs);
  }
  PROCESS_SWITCH(TrackPropagation, processStandard, "Process without covariance", true);

  // -----------------------
  void processMc(TracksIUWithMc const& tracks, aod::McParticles const& mcParticles, aod::Collisions const& collisions, aod::BCsWithTimestamps const& bcs)
  {
    // auto table_extension = soa::Extend<TracksIUWithMc, aod::extension::MomX>(tracks);
    fillTrackTables</*TTrack*/ TracksIUWithMc, /*Particle*/ aod::McParticles, /*isMc = */ true, /*fillCovMat =*/true, /*useTrkPid =*/false>(tracks, mcParticles, collisions, bcs);
  }
  PROCESS_SWITCH(TrackPropagation, processMc, "Process with covariance on MC", false);
};

//****************************************************************************************
/**
 * Workflow definition.
 */
//****************************************************************************************
WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  WorkflowSpec workflow{adaptAnalysisTask<TrackPropagation>(cfgc)};
  return workflow;
}
