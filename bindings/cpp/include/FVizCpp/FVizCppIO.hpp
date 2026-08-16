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
#include <FViz/IO/FVizVTPReader.h>
#include <FViz/IO/FVizVTUWriter.h>
#include <FViz/IO/FVizVTPWriter.h>
#include <FViz/IO/FVizPLYWriter.h>
#include <FViz/IO/FVizPVD.h>

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

// Reads a VTK XML PolyData (.vtp) into a PolyData.
inline PolyData readVtp(const std::string& path)
{
    FVizPolyData* poly = nullptr;
    detail::checkResult(fviz_vtp_read(path.c_str(), &poly));
    return PolyData(poly);
}

// Writes an UnstructuredGrid to VTK XML (.vtu). Defaults to appended raw + zlib.
inline void writeVtu(const std::string& path, UnstructuredGrid& grid, bool compress = true)
{
    FVizVTUWriterOptions options;
    fviz_vtu_writer_options_initialize(&options);
    options.output_mode = FVIZ_VTU_OUTPUT_APPENDED_RAW;
    options.compress = detail::fbool(compress);
    detail::checkResult(fviz_vtu_write(path.c_str(), grid.get(), &options));
}

// Writes an UnstructuredGrid to VTK XML (.vtu) as ASCII.
inline void writeVtuAscii(const std::string& path, UnstructuredGrid& grid)
{
    FVizVTUWriterOptions options;
    fviz_vtu_writer_options_initialize(&options);
    options.output_mode = FVIZ_VTU_OUTPUT_ASCII;
    detail::checkResult(fviz_vtu_write(path.c_str(), grid.get(), &options));
}

// Writes a PolyData to VTK XML (.vtp).
inline void writeVtp(const std::string& path, PolyData& poly_data)
{
    FVizVTPWriterOptions options;
    fviz_vtp_writer_options_initialize(&options);
    detail::checkResult(fviz_vtp_write(path.c_str(), poly_data.get(), &options));
}

// Writes a PolyData to PLY.
inline void writePly(const std::string& path, PolyData& poly_data, FVizPLYOutputMode mode = FVIZ_PLY_OUTPUT_ASCII)
{
    detail::checkResult(fviz_ply_write(path.c_str(), poly_data.get(), mode));
}

// ---------------------------------------------------------------------------
// PVDCollection - temporal collection of dataset files.
// ---------------------------------------------------------------------------
class PVDCollection : public Object<FVizPVDCollection> {
public:
    PVDCollection() = default;
    explicit PVDCollection(FVizPVDCollection* owned) : Object<FVizPVDCollection>(owned) {}
    explicit PVDCollection(void* owned) : Object<FVizPVDCollection>(owned) {}

    static PVDCollection create()
    {
        FVizPVDCollection* collection = nullptr;
        detail::checkResult(fviz_pvd_collection_create(&collection));
        return PVDCollection(collection);
    }

    static PVDCollection read(const std::string& path)
    {
        FVizPVDCollection* collection = nullptr;
        detail::checkResult(fviz_pvd_read(path.c_str(), &collection));
        return PVDCollection(collection);
    }

    FVizSize count() const noexcept { return ptr_ ? fviz_pvd_collection_count(ptr_) : 0u; }
    void add(double time, uint32_t part, const char* group, const char* file)
    {
        detail::checkResult(fviz_pvd_collection_add(ptr_, time, part, group, file, nullptr));
    }
    double timeAt(FVizSize index) const noexcept { return ptr_ ? fviz_pvd_collection_time(ptr_, index) : 0.0; }
    const char* fileAt(FVizSize index) const noexcept { return ptr_ ? fviz_pvd_collection_file(ptr_, index) : nullptr; }
    void timeRange(double& out_minimum, double& out_maximum) const
    {
        detail::checkResult(fviz_pvd_collection_time_range(ptr_, &out_minimum, &out_maximum));
    }
    void write(const std::string& path) const { detail::checkResult(fviz_pvd_write(path.c_str(), ptr_)); }
    void clear() noexcept { if (ptr_) fviz_pvd_collection_clear(ptr_); }
};

} // namespace fviz

#endif // FVIZ_CPP_IO_HPP
