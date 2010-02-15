#ifndef TARGETMAKER_HH
#define TARGETMAKER_HH
//
// Construct and return an Target.
//
//
// $Id: TargetMaker.hh,v 1.1 2010/02/15 16:56:50 shanahan Exp $
// $Author: shanahan $ 
// $Date: 2010/02/15 16:56:50 $
//
// Original author Peter Shanahan
//

#include <vector>
#include <string>
#include <memory>

#include "CLHEP/Vector/ThreeVector.h"

namespace mu2e {

class Target;
class SimpleConfig;

class TargetMaker{

public:

  TargetMaker( SimpleConfig const& config );  

  ~TargetMaker ();

  // This is depracted and will go away soon.  
  // Still needed for root graphics version.
  const Target& getTarget() const { return *_targ;}

  // This is the accessor that will remain.
  std::auto_ptr<Target> getTargetPtr() { return _targ; }

private:

  void BuildIt();
  void PrintConfig();// prints the target configuration as TargetMaker
                     // understands it...

  //  SimpleConfig const& _config;

  // pointer to the Mu2E Geometry Target being made
  std::auto_ptr<Target> _targ;

  // variables needed to build the Target.  Read in from config file, 
  // data base, etc.  These (and the TargetMaker object itself) only need 
  // to persist long enough to make the Target.  After that, the definition 
  // resides entirely in the Target object.

  double _z0 ; // nominal center of foils.  
  double _deltaZ ; // nominal spacing of foils.
  double _rIn; // inner radius of foils.  Currently 0.

  std::vector<double> _rOut; //outer Radii of foils
  std::vector<double> _halfThicknesses; //half thicknesses of foils
  std::vector<double> _xVars; //variation of x position from axis of foils
  std::vector<double> _yVars; //variation of y position from axis of foils
  std::vector<double> _zVars; //variation of z position from nominal of foils
  std::vector<double> _xCos; // x directional cosines of foils
  std::vector<double> _yCos; // y directional cosines of foils
  std::vector<std::string> _materials; //material of foils
  std::string _fillMaterial; // material of enclosing cylinder

};

}  //namespace mu2e

#endif //TARGETMAKER_HH
