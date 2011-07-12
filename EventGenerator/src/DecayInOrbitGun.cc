//
// Generate some number of DIO electrons.
//
// $Id: DecayInOrbitGun.cc,v 1.32 2011/07/12 04:52:27 kutschke Exp $
// $Author: kutschke $
// $Date: 2011/07/12 04:52:27 $
//
// Original author Rob Kutschke
//

#include "cetlib/pow.h"
#include "CLHEP/Random/RandFlat.h"
#include "CLHEP/Units/PhysicalConstants.h"
#include "ConditionsService/inc/AcceleratorParams.hh"
#include "ConditionsService/inc/ConditionsHandle.hh"
#include "ConditionsService/inc/DAQParams.hh"
#include "ConditionsService/inc/ParticleDataTable.hh"
#include "EventGenerator/inc/DecayInOrbitGun.hh"
#include "MCDataProducts/inc/PDGCode.hh"
#include "Mu2eUtilities/inc/SimpleConfig.hh"
#include "Mu2eUtilities/inc/safeSqrt.hh"
#include "TH1D.h"
#include "art/Framework/Core/TFileDirectory.h"
#include "art/Framework/Services/Optional/TFileService.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "messagefacility/MessageLogger/MessageLogger.h"
#include <iostream>

using namespace std;

//using CLHEP::Hep3Vector;
//using CLHEP::HepLorentzVector;
//using CLHEP::RandFlat;
//using CLHEP::twopi;

namespace mu2e {

  // Need a Conditions entity to hold info about conversions:
  // endpoints and lifetimes for different materials etc
  // Grab them from Andrew's minimc package?
  static const double conversionEnergyAluminum = 104.96;

  DecayInOrbitGun::DecayInOrbitGun( art::Run& run, const SimpleConfig& config ):

    // Base class
    GeneratorBase(),

    // Information from config file.
    _mean(config.getDouble("decayinorbitGun.mean",1.)),
    _elow(config.getDouble("decayinorbitGun.elow",100.)),
    _ehi(config.getDouble("decayinorbitGun.ehi",conversionEnergyAluminum)),
    _nbins(config.getInt("decayinorbitGun.nbins",1000)),
    _czmin(config.getDouble("decayinorbitGun.czmin", -1.0)),
    _czmax(config.getDouble("decayinorbitGun.czmax",  1.0)),
    _phimin(config.getDouble("decayinorbitGun.phimin", 0. )),
    _phimax(config.getDouble("decayinorbitGun.phimax", CLHEP::twopi )),
    _PStoDSDelay(config.getBool("decayinorbitGun.PStoDSDelay", true)),
    _pPulseDelay(config.getBool("decayinorbitGun.pPulseDelay", false)),
    _doHistograms(config.getBool("decayinorbitGun.doHistograms", true)),
    _spectrumResolution(config.getDouble("decayinorbitGun.spectrumResolution", 0.1)),
    _useSimpleSpectrum(config.getBool("decayinorbitGun.useSimpleSpectrum", false)),

    // Random number distributions; getEngine comes from the base class.
    _randSimpleEnergy(getEngine(), &(binnedEnergySpectrum()[0]), _nbins ),

    _randPoissonQ( getEngine(), std::abs(_mean) ),
    _randomUnitSphere ( getEngine(), _czmin, _czmax, _phimin, _phimax ),
    _nToSkip (config.getInt("decayinorbitGun.nToSkip",0)),


    // Histograms.
    _hMultiplicity(0),
    _hEElec(0),
    _hEElecZ(0),
    _hzPosition(0),
    _hcz(0),
    _hphi(0),
    _ht(),
    _hmudelay(),
    _hpulsedelay()  {

    // Sanity check.
    if ( std::abs(_mean) > 99999. ) {
      throw cet::exception("RANGE")
        << "DecayInOrbit Gun has been asked to produce a crazily large number of electrons."
        << _mean
        << "\n";
    }

    // About the ConditionsService:
    // The argument to the constructor is ignored for now.  It will be a
    // data base key.  There is a second argument that I have let take its
    // default value of "current"; it will be used to specify a version number.
    ConditionsHandle<AcceleratorParams> accPar("ignored");
    ConditionsHandle<DAQParams>         daqPar("ignored");
    ConditionsHandle<ParticleDataTable> pdt("ignored");

    //pick up particle mass
    const HepPDT::ParticleData& e_data = pdt->particle(PDGCode::e_minus).ref();
    _mass = e_data.mass().value();

    _tmin   = config.getDouble("decayinorbitGun.tmin",  daqPar->t0 );
    _tmax   = config.getDouble("decayinorbitGun.tmax",  accPar->deBuncherPeriod );

    // Make ROOT subdirectory to hold diagnostic histograms; book those histograms.

    if ( _doHistograms ){
      art::ServiceHandle<art::TFileService> tfs;
      art::TFileDirectory tfdir = tfs->mkdir( "DecayInOrbit" );
      _hMultiplicity = tfdir.make<TH1D>( "hMultiplicity", "DIO Multiplicity",               100,     0.,   200.   );
      _hEElec        = tfdir.make<TH1D>( "hEElec",        "DIO Electron Energy",            100,     0.,   105.   );
      _hEElecZ       = tfdir.make<TH1D>( "hEElecZ",       "DIO Electron Energy (zoom)",     200,  _elow, _ehi     );
      _hzPosition    = tfdir.make<TH1D>( "hzPosition",    "DIO z Position (Tracker Coord)", 200, -6600., -5600.   );
      _hcz           = tfdir.make<TH1D>( "hcz",           "DIO cos(theta)",                 100,    -1.,     1.   );
      _hphi          = tfdir.make<TH1D>( "hphi",          "DIO azimuth",                    100,  -M_PI,   M_PI   );
      _ht            = tfdir.make<TH1D>( "ht",            "DIO time ", 210, -200., 3000. );
      _hmudelay      = tfdir.make<TH1D>( "hmudelay",      "Production delay due to muons arriving at ST;(ns)", 300, 0., 2000. );
      _hpulsedelay   = tfdir.make<TH1D>( "hpdelay",       "Production delay due to the proton pulse;(ns)", 60, 0., 300. );
    }

    _fGenerator = auto_ptr<FoilParticleGenerator>(new FoilParticleGenerator( getEngine(), _tmin, _tmax,
                                                                             FoilParticleGenerator::muonFileInputFoil,
                                                                             FoilParticleGenerator::muonFileInputPos,
                                                                             FoilParticleGenerator::negExp,                                                           
									     _PStoDSDelay,
                                                                             _pPulseDelay,
									     _nToSkip));

    _randEnergy = auto_ptr<DIOShankerWanatabe>(new DIOShankerWanatabe(13,_elow, _ehi, _spectrumResolution, getEngine()));



  }

  DecayInOrbitGun::~DecayInOrbitGun(){
  }

  void DecayInOrbitGun::generate( GenParticleCollection& genParts ){
    // Choose the number of electrons to generate this event.
    long n = (_mean < 0 ? static_cast<long>(-_mean): _randPoissonQ.fire());

    if ( _doHistograms ) {
      _hMultiplicity->Fill(n);
    }

    //Loop over particles to generate

    for (int i=0; i<n; ++i) {

      //Pick up position and momentum
      CLHEP::Hep3Vector pos(0,0,0);
      double time;
      _fGenerator->generatePositionAndTime(pos, time);


      //Pick up energy and momentum vector
      double e;
      if (_useSimpleSpectrum) {
        e = _elow + _randSimpleEnergy.fire() * (_ehi - _elow);
      } else if (!_useSimpleSpectrum) {
        e = _randEnergy->fire();
      }

      _p = safeSqrt(e*e - _mass*_mass);
      CLHEP::Hep3Vector p3 = _randomUnitSphere.fire(_p);

      //Set Four-momentum
      CLHEP::HepLorentzVector mom(p3,e);

      // Add the particle to  the list.
      genParts.push_back( GenParticle( PDGCode::e_minus, GenId::dio1, pos, mom, time));

      if( _doHistograms ){
        _hEElec     ->Fill( e );
        _hEElecZ    ->Fill( e );
        _hzPosition ->Fill( pos.z() );
        _hcz        ->Fill( p3.cosTheta() );
        _hphi       ->Fill( p3.phi() );
        _ht         ->Fill( time );
	_hmudelay   ->Fill(_fGenerator->muDelay());
	_hpulsedelay->Fill(_fGenerator->pulseDelay());
      }

    } // End of loop over particles

  } // DecayInOrbitGun::generate


  // Energy spectrum of the electron from DIO.
  // Input energy in MeV
  double DecayInOrbitGun::energySpectrum( double e )
  {
    return cet::pow<5>(conversionEnergyAluminum - e) ;
  }


  // Compute a binned representation of the energy spectrum of the electron from DIO.
  // Not used. Still in the code until a new class can reproduce it.

  std::vector<double> DecayInOrbitGun::binnedEnergySpectrum(){

    // Sanity check.
    if (_nbins <= 0) {
      throw cet::exception("RANGE")
        << "Nonsense nbins requested in "
        << "DecayInOrbit = "
        << _nbins
        << "\n";
    }

    // Bin width.
    double dE = (_ehi - _elow) / _nbins;

    // Vector to hold the binned representation of the energy spectrum.
    std::vector<double> spectrum;
    spectrum.reserve(_nbins);

    for (int ib=0; ib<_nbins; ib++) {
      double x = _elow+(ib+0.5) * dE;
      spectrum.push_back(energySpectrum(x));
    }

    return spectrum;
  } // DecayInOrbitGun::binnedEnergySpectrum


}
