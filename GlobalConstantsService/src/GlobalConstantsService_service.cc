//
// Need to have the main implementation in GlobalConstantsService.so
// so that the link of GeomHandle will have no undefined references.
//
#include "GlobalConstantsService/inc/GlobalConstantsService.hh"

DEFINE_ART_SERVICE(mu2e::GlobalConstantsService);
