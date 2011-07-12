//
//
// Simulate the protons that come from the stopping target when muons capture
// on an Al nucleus.  Use the MECO distribution for the kinetic energy of the
// protons.
//
// $Id: EjectedProtonGun.cc,v 1.26 2011/07/12 04:52:27 kutschke Exp $
// $Author: kutschke $
// $Date: 2011/07/12 04:52:27 $
//
// Original author Rob Kutschke, heavily modified by R. Bernstein
//
//

// C++ includes.
#include <iostream>

// Framework includes
#include "art/Framework/Core/Run.h"
#include "art/Framework/Core/TFileDirectory.h"
#include "art/Framework/Services/Optional/TFileService.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

// Mu2e includes
#include "ConditionsService/inc/AcceleratorParams.hh"
#include "ConditionsService/inc/ConditionsHandle.hh"
#include "ConditionsService/inc/DAQParams.hh"
#include "ConditionsService/inc/ParticleDataTable.hh"
#include "EventGenerator/inc/EjectedProtonGun.hh"
#include "GeometryService/inc/GeomHandle.hh"
#include "MCDataProducts/inc/PDGCode.hh"
#include "Mu2eUtilities/inc/SimpleConfig.hh"
#include "Mu2eUtilities/inc/safeSqrt.hh"
#include "TargetGeom/inc/Target.hh"

// Other external includes.
#include "CLHEP/Random/RandFlat.h"
#include "CLHEP/Units/PhysicalConstants.h"

//ROOT Includes
#include "TH1D.h"
#include "TMath.h"

using namespace std;

namespace mu2e {

  EjectedProtonGun::EjectedProtonGun( art::Run& run, const SimpleConfig& config ):

    // Base class.
    GeneratorBase(),

    // Configurable parameters
    _mean(config.getDouble("ejectedProtonGun.mean",1.)),
    _elow(config.getDouble("ejectedProtonGun.elow",0.)),
    _ehi(config.getDouble("ejectedProtonGun.ehi",.100)),
    _czmin(config.getDouble("ejectedProtonGun.czmin",  -1.)),
    _czmax(config.getDouble("ejectedProtonGun.czmax",  1.)),
    _phimin(config.getDouble("ejectedProtonGun.phimin", 0. )),
    _phimax(config.getDouble("ejectedProtonGun.phimax", CLHEP::twopi )),
    _nbins(config.getInt("ejectedProtonGun.nbins",1000)),
    _doHistograms(config.getBool("ejectedProtonGun.doHistograms",true)),
    _PStoDSDelay(config.getBool("ejectedProtonGun.PStoDSDelay", false)),
    _pPulseDelay(config.getBool("ejectedProtonGun.pPulseDelay", false)),
    // Initialize random number distributions; getEngine comes from the base class.
    _randPoissonQ( getEngine(), std::abs(_mean) ),
    _randomUnitSphere ( getEngine(), _czmin, _czmax, _phimin, _phimax ),
    _shape ( getEngine() , &(binnedEnergySpectrum()[0]), _nbins ),
    _nToSkip (config.getInt("ejectedProtonGun.nToSkip",0)),

    // Histogram pointers
    _hMultiplicity(),
    _hKE(),
    _hKEZoom(),
    _hMomentumMeV(),
    _hzPosition(),
    _hcz(),
    _hphi(),
    _htime(),
    _hmudelay(),
    _hpulsedelay()  {


    // About the ConditionsService:
    // The argument to the constructor is ignored for now.  It will be a
    // data base key.  There is a second argument that I have let take its
    // default value of "current"; it will be used to specify a version number.
    ConditionsHandle<AcceleratorParams> accPar("ignored");
    ConditionsHandle<DAQParams>         daqPar("ignored");
    ConditionsHandle<ParticleDataTable> pdt("ignored");

    //Set particle mass
    const HepPDT::ParticleData& p_data = pdt->particle(PDGCode::p_plus).ref();
    _mass = p_data.mass().value();


    // Default values for the start and end of the live window.
    // Can be overriden by the run-time config; see below.
    _tmin = daqPar->t0;
    _tmax = accPar->deBuncherPeriod;

    _tmin = config.getDouble("ejectedProtonGun.tmin",  _tmin );
    _tmax = config.getDouble("ejectedProtonGun.tmax",  _tmax );


    // Book histograms.
    if ( _doHistograms ){
      art::ServiceHandle<art::TFileService> tfs;
      art::TFileDirectory tfdir  = tfs->mkdir( "EjectedProtonGun" );
      _hMultiplicity = tfdir.make<TH1D>( "hMultiplicity", "Proton Multiplicity",                20,     0,     20  );
      _hKE           = tfdir.make<TH1D>( "hKE",           "Proton Kinetic Energy",              50, _elow,   _ehi  );
      _hMomentumMeV  = tfdir.make<TH1D>( "hMomentumMeV",  "Proton Momentum in MeV",             50, _elow,   _ehi  );
      _hKEZoom       = tfdir.make<TH1D>( "hEZoom",        "Proton Kinetic Energy (zoom)",      200, _elow,   _ehi  );
      _hzPosition    = tfdir.make<TH1D>( "hzPosition",    "Proton z Position (Tracker Coord)", 200, -6600., -5600. );
      _hcz           = tfdir.make<TH1D>( "hcz",           "Proton cos(theta)",                 100,    -1.,     1. );
      _hphi          = tfdir.make<TH1D>( "hphi",          "Proton azimuth",                    100,  -M_PI,  M_PI  );
      _htime         = tfdir.make<TH1D>( "htime",         "Proton time ",                      210,   -200.,  3000. );
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
  }

  EjectedProtonGun::~EjectedProtonGun(){
  }

  void EjectedProtonGun::generate( GenParticleCollection& genParts ){

    // Choose the number of protons to generate this event.
    long n = _mean < 0 ? static_cast<long>(-_mean): _randPoissonQ.fire();
    if ( _doHistograms ) {
      _hMultiplicity->Fill(n);
    }

    //Loop over particles to generate

    for (int i=0; i<n; ++i) {

      //Pick up position and momentum
      CLHEP::Hep3Vector pos(0,0,0);
      double time;
      _fGenerator->generatePositionAndTime(pos, time);

      //Pick up energy
      double eKine = _elow + _shape.fire() * ( _ehi - _elow );
      double e   = eKine + _mass;

      //Pick up momentum vector

      _p = safeSqrt(e*e - _mass*_mass);
      CLHEP::Hep3Vector p3 = _randomUnitSphere.fire(_p);

      //Set Four-momentum
      CLHEP::HepLorentzVector mom(0,0,0,0);
      mom.setPx( p3.x() );
      mom.setPy( p3.y() );
      mom.setPz( p3.z() );
      mom.setE( e );

      // Add the particle to  the list.
      genParts.push_back( GenParticle(PDGCode::p_plus, GenId::ejectedProtonGun, pos, mom, time));

      // Fill histograms.
      if ( _doHistograms) {
        _hKE->Fill( eKine );
        _hKEZoom->Fill( eKine );
        _hMomentumMeV->Fill( _p );
        _hzPosition->Fill( pos.z() );
        _hcz->Fill( mom.cosTheta() );
        _hphi->Fill( mom.phi() );
        _htime->Fill( time );
	_hmudelay   ->Fill(_fGenerator->muDelay());
	_hpulsedelay->Fill(_fGenerator->pulseDelay());
      }
    } // end of loop on particles

  } // end generate


  // Energy spectrum of the electron from DIO.
  // Input energy in MeV
  double EjectedProtonGun::energySpectrum( double e )
  {

    //taken from GMC
    //
    //   Ed Hungerford  Houston University May 17 1999
    //   Rashid Djilkibaev New York University (modified) May 18 1999
    //
    //   e - proton kinetic energy (MeV)
    //   p - proton Momentum (MeV/c)
    //
    //   Generates a proton spectrum similar to that observed in
    //   u capture in Si.  JEPT 33(1971)11 and PRL 20(1967)569

    //these numbers are in MeV!!!!
    static const double emn = 1.4; // replacing par1 from GMC
    static const double par2 = 1.3279;
    static const double par3=17844.0;
    static const double par4=.32218;
    static const double par5=100.;
    static const double par6=10.014;
    static const double par7=1050.;
    static const double par8=5.103;

    double spectrumWeight;

    if (e >= 20)
      {
        spectrumWeight=par5*TMath::Exp(-(e-20.)/par6);
      }

    else if(e >= 8.0 && e <= 20.0)
      {
        spectrumWeight=par7*exp(-(e-8.)/par8);
      }
    else if (e > emn)
      {
        double xw=(1.-emn/e);
        double xu=TMath::Power(xw,par2);
        double xv=par3*TMath::Exp(-par4*e);
        spectrumWeight=xv*xu;
      }
    else
      {
        spectrumWeight = 0.;
      }
    return spectrumWeight;
  }

  // Compute a binned representation of the energy spectrum of the proton.
  std::vector<double> EjectedProtonGun::binnedEnergySpectrum(){

    // Sanity check.
    if (_nbins <= 0) {
      throw cet::exception("RANGE")
        << "Nonsense nbins requested in "
        << "ejectedProtonGun = "
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
  } // EjectedProtonGun::binnedEnergySpectrum




} // namespace mu2e
