// Andrei Gaponenko, 2011

#include <string>
#include <vector>
#include <algorithm>

// Mu2e includes.
#include "MCDataProducts/inc/StatusG4.hh"
#include "MCDataProducts/inc/StepPointMC.hh"
#include "MCDataProducts/inc/StepPointMCCollection.hh"

// art includes.
#include "fhiclcpp/ParameterSet.h"
#include "art/Framework/Core/EDFilter.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Services/Optional/TFileService.h"

//#define AGDEBUG(stuff) std::cerr<<"AG: "<<__FILE__<<", line "<<__LINE__<<": "<<stuff<<std::endl;
#define AGDEBUG(stuff)

namespace mu2e {
  class FilterVDHits : public art::EDFilter {
    std::string _outInstanceName;
    std::string _inModuleLabel;
    std::string _inInstanceName;

    typedef StepPointMC::VolumeId_type VolumeId;

    // The anticipated use case is to select hits for one or a few
    // VDs.  Therefore we use a vector: the log-vs-linear efficiency
    // gain from using a set is probably negated by the overheads
    // of dealing with a more complicated data structure that is not
    // local in memory.
    std::vector<VolumeId> _vids;  // preserve hits in just those volumes

  public:
    explicit FilterVDHits(const fhicl::ParameterSet& pset);
    virtual bool filter(art::Event& event);
    virtual bool beginRun(art::Run& run) { std::cout<<"AG: beginRun() called"<<std::endl; return true; }
    virtual bool endRun(art::Run& run) { std::cout<<"AG: endRun() called"<<std::endl; return true; }
  };

  //================================================================
  FilterVDHits::FilterVDHits(const fhicl::ParameterSet& pset)
    : _inModuleLabel(pset.get<std::string>("inputModuleLabel"))
    , _inInstanceName(pset.get<std::string>("inputInstanceName"))
    , _vids(pset.get<std::vector<VolumeId> >("acceptedVids"))
  {
    produces<StepPointMCCollection>();
    produces<SimParticleCollection>();
  }

  //================================================================
  bool FilterVDHits::filter(art::Event& event) {
    AGDEBUG("FilterVDHits begin event "<<event.id());

    // Use art::Ptr instead of bare ptr to get the desired sorting
    typedef std::set<art::Ptr<SimParticle> > TrackSet;
    TrackSet particlesWithHits;

    const bool accept_event = true;
    if(accept_event) {

      art::Handle<StepPointMCCollection> ih;
      event.getByLabel(_inModuleLabel, _inInstanceName, ih);
      const StepPointMCCollection& inhits(*ih);

      std::auto_ptr<StepPointMCCollection> outhits(new StepPointMCCollection());

      for(StepPointMCCollection::const_iterator i=inhits.begin(); i!=inhits.end(); ++i) {

        if(std::find(_vids.begin(), _vids.end(), i->volumeId()) != _vids.end()) {

          outhits->push_back(*i);

          AGDEBUG("here");
          const art::Ptr<SimParticle>& particle = outhits->back().simParticle();
          AGDEBUG("here");

          if(!particle.get()) {
            throw cet::exception("MISSINGINFO")
              <<"NULL particle pointer for StepPointMC = "<<*i
              <<" in event "<<event.id()
              ;
          }
          else {
            AGDEBUG("here: particle = "<<particle<<" (internal id = "<< particle->id()<<")"<<" for hit "<<*i);
            particlesWithHits.insert(particle);
          }
        }
      }
      AGDEBUG("here");

      // Need to save SimParticles that produced the hits Particles
      // must come in order, and there may be no one-to-one
      // correspondence with saved hits, so this must be a separate
      // loop.
      //
      // Note that the art::Ptr comparison operator sorts the set in
      // the correct order for our use.
      std::auto_ptr<SimParticleCollection> outparts(new SimParticleCollection());
      for(TrackSet::const_iterator i=particlesWithHits.begin(); i!=particlesWithHits.end(); ++i) {

        AGDEBUG("here");
        cet::map_vector_key key = (*i)->id();
        AGDEBUG("here: key = "<<key);
        // Copy the original particle to the output

        // FIXME: why does push_back() screw up the given key?
        // outparts->push_back(std::make_pair(key, **i));

        (*outparts)[key] = **i;

        SimParticle& particle(outparts->getOrThrow(key));

        // Zero internal pointers: intermediate particles are not preserved to reduce data size
        AGDEBUG("here");
        particle.setDaughterPtrs(std::vector<art::Ptr<SimParticle> >());
        AGDEBUG("here");
        particle.parent() = art::Ptr<SimParticle>();

        AGDEBUG("after p/d reset: particle id = "<<particle.id());
      }
      AGDEBUG("here");
      event.put(outparts);

      // Update pointers in the hit collection
      art::ProductID newProductId(getProductID<SimParticleCollection>(event));
      const art::EDProductGetter *newProductGetter(event.productGetter(newProductId));

      AGDEBUG("here");
      for(StepPointMCCollection::iterator i=outhits->begin(); i!=outhits->end(); ++i) {
        AGDEBUG("here");
        art::Ptr<SimParticle> oldPtr(i->simParticle());
        AGDEBUG("here: settting id = "<< oldPtr->id()<<", newProductId = "<<newProductId <<" for hit "<<*i);
        i->simParticle() = art::Ptr<SimParticle>(newProductId, oldPtr->id().asUint(), newProductGetter);
      }
      AGDEBUG("here");
      event.put(outhits);

      return true;
    }
    else {
      return false;
    }
  }

  //================================================================

} // namespace mu2e

DEFINE_ART_MODULE(mu2e::FilterVDHits);
