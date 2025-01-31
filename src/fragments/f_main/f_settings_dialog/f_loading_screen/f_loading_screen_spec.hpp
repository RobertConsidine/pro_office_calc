#ifndef __PROCALC_FRAGMENTS_F_LOADING_SCREEN_SPEC_HPP__
#define __PROCALC_FRAGMENTS_F_LOADING_SCREEN_SPEC_HPP__


#include <string>
#include "fragment_spec.hpp"


struct FLoadingScreenSpec : public FragmentSpec {
  FLoadingScreenSpec()
    : FragmentSpec("FLoadingScreen", {}) {}

  std::string targetValue;
};


#endif
