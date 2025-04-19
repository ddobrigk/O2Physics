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

#ifndef PWGLF_UTILS_STRANGENESSBUILDERHELPER_H_
#define PWGLF_UTILS_STRANGENESSBUILDERHELPER_H_

#include <cstdlib>
#include <cmath>
#include <array>
#include "DCAFitter/DCAFitterN.h"
#include "Framework/AnalysisDataModel.h"
#include "ReconstructionDataFormats/Track.h"
#include "DetectorsBase/GeometryManager.h"
#include "CommonConstants/PhysicsConstants.h"
#include "Common/Core/trackUtilities.h"
#include "Tools/KFparticle/KFUtilities.h"

//__________________________________________
// track propagation module 
// 
// this class is capable of performing the usual track propagation
// and table creation it is a demonstration of core service 
// plug-in functionality that could be used to reduce the number of 
// heavyweight (e.g. mat-LUT-using, propagating) core services to 
// reduce overhead and make it easier to pipeline / parallelize 
// bottlenecks in core services

class trackPropagationModule
{
 public:
  strangenessBuilderHelper()
  {


    v0selections.minCrossedRows = -1;
    v0selections.dcanegtopv = -1.0f;
    v0selections.dcapostopv = -1.0f;
    v0selections.v0cospa = -2;
    v0selections.dcav0dau = 1e+6;
    v0selections.v0radius = 0.0f;
    v0selections.maxDaughterEta = 2.0;

    // LUT has to be loaded later
    lut = nullptr;
    fitter.setMatCorrType(o2::base::Propagator::MatCorrType::USEMatCorrLUT);

    // mag field has to be set later
    fitter.setBz(-999.9f); // will NOT make sense if not changed
  };

  o2::base::MatLayerCylSet* lut;       // material LUT for DCA fitter

 private:
  


};

#endif // PWGLF_UTILS_STRANGENESSBUILDERHELPER_H_
