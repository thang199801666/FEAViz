// FEAViz C++ binding - file IO.
//
// Convenience readers for the mesh formats supported by the C core. All return
// owning RAII wrappers and throw fviz::Error on failure.

#ifndef FVIZ_CPP_IO_HPP
#define FVIZ_CPP_IO_HPP

#include <FViz/IO/FVizVTUReader.h>
#include <FViz/IO/FVizVTKLegacyReader.h>
#include <FViz/IO/FVizOBJReader.h>
#include <FViz/IO/FVizSTLReader.h>

#include "FVizCppObject.hpp"
#include "FVizCppData.hpp"

#include <string>

namespace fviz {

// Reads a VTK XML UnstructuredGrid (.vtu) into an UnstructuredGrid.
inline UnstructuredGrid readVtu(const std::string& path)
{
    FVizUnstructuredGrid* grid = nullptr;
    detail::checkResult(fviz_vtu_read(path.c_str(), &grid));
    return UnstructuredGrid(grid);
}

// Reads a legacy ASCII .vtk file into an UnstructuredGrid.
inline UnstructuredGrid readVtkLegacy(const std::string& path)
{
    FVizUnstructuredGrid* grid = nullptr;
    detail::checkResult(fviz_vtk_legacy_read(path.c_str(), &grid));
    return UnstructuredGrid(grid);
}

// Reads a Wavefront .obj file into a PolyData.
inline PolyData readObj(const std::string& path)
{
    FVizPolyData* poly = nullptr;
    detail::checkResult(fviz_obj_read(path.c_str(), &poly));
    return PolyData(poly);
}

// Reads an STL file (ascii or binary) into a PolyData.
inline PolyData readStl(const std::string& path)
{
    FVizPolyData* poly = nullptr;
    detail::checkResult(fviz_stl_read(path.c_str(), &poly));
    return PolyData(poly);
}

} // namespace fviz

#endif // FVIZ_CPP_IO_HPP
