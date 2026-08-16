// FEAViz C++ binding - umbrella header.
//
// A header-only C++17 layer over the FEAViz C API. The C ABI remains the
// source of truth; these wrappers add RAII ownership, typed math value types,
// and ergonomic methods so application code reads like C++ while staying ABI
// compatible with the core library.
//
// Include this single header to use the binding:
//
//   #include <FVizCpp/FVizCpp.hpp>
//
// Link against the FEAViz core library (FEAViz::Core / libFEAViz).

#ifndef FVIZ_CPP_HPP
#define FVIZ_CPP_HPP

#include "FVizCppMath.hpp"
#include "FVizCppObject.hpp"
#include "FVizCppData.hpp"
#include "FVizCppRendering.hpp"
#include "FVizCppIO.hpp"

#endif // FVIZ_CPP_HPP
