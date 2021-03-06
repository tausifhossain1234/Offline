#ifndef Mu2eUtilities_toHepPoint_hh
#define Mu2eUtilities_toHepPoint_hh
//
//  Free functions to convert between  HepPoint and CLHEP::Hep3Vector.
//
// $Id: toHepPoint.hh,v 1.4 2011/05/18 02:27:18 wb Exp $
// $Author: wb $
// $Date: 2011/05/18 02:27:18 $
//
// Original author Rob Kutschke
//

// Modern CLHEP
#include "CLHEP/Vector/ThreeVector.h"

// Ancient CLHEP, copied from BaBar
#include "BTrk/BbrGeom/HepPoint.h"

namespace mu2e {

  inline const HepPoint toHepPoint( const CLHEP::Hep3Vector& v){
    return HepPoint( v.x(), v.y(), v.z());
  }

  inline const CLHEP::Hep3Vector fromHepPoint( const HepPoint& v){
    return CLHEP::Hep3Vector( v.x(), v.y(), v.z() );
  }

}
#endif /* Mu2eUtilities_toHepPoint_hh */
