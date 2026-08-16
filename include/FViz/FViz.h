#ifndef FVIZ_H
#define FVIZ_H

#include <FViz/FVizConfig.h>
#include <FViz/FVizVersion.h>
#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizCommand.h>
#include <FViz/Core/FVizAllocator.h>
#include <FViz/Core/FVizArena.h>
#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizBitArray.h>
#include <FViz/Core/FVizBuffer.h>
#include <FViz/Core/FVizHashMap.h>
#include <FViz/Core/FVizCacheKey.h>
#include <FViz/Core/FVizString.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Data/FVizDataObject.h>
#include <FViz/Data/FVizDataSet.h>
#include <FViz/Data/FVizProvenance.h>
#include <FViz/Data/FVizImageData.h>
#include <FViz/Data/FVizStructuredGrid.h>
#include <FViz/Data/FVizRectilinearGrid.h>
#include <FViz/Data/FVizPartitionedDataSet.h>
#include <FViz/Data/FVizMultiBlockDataSet.h>
#include <FViz/Data/FVizTemporalDataSet.h>
#include <FViz/Data/FVizTemporalFrameCache.h>
#include <FViz/Data/FVizDataType.h>
#include <FViz/Data/FVizGhost.h>
#include <FViz/Math/FVizMath.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Mesh/FVizPoints.h>
#include <FViz/Mesh/FVizCellArray.h>
#include <FViz/Mesh/FVizCellTypeTraits.h>
#include <FViz/Mesh/FVizCellLinks.h>
#include <FViz/Mesh/FVizCellAdjacency.h>
#include <FViz/Data/FVizUnstructuredGrid.h>
#include <FViz/Data/FVizFieldStatistics.h>
#include <FViz/Interaction/FVizInteraction.h>
#include <FViz/Pipeline/FVizFilter.h>
#include <FViz/Pipeline/FVizAlgorithm.h>
#include <FViz/Pipeline/FVizExecutive.h>
#include <FViz/Pipeline/FVizDataProvider.h>
#include <FViz/Parallel/FVizParallelModule.h>
#include <FViz/Algorithms/FVizContourFilter.h>
#include <FViz/Algorithms/FVizDeformation.h>
#include <FViz/Algorithms/FVizPlaneSource.h>
#include <FViz/Algorithms/FVizCubeSource.h>
#include <FViz/Algorithms/FVizSphereSource.h>
#include <FViz/Algorithms/FVizArrowSource.h>
#include <FViz/Algorithms/FVizConeSource.h>
#include <FViz/Algorithms/FVizCylinderSource.h>
#include <FViz/Algorithms/FVizDiskSource.h>
#include <FViz/Algorithms/FVizLineSource.h>
#include <FViz/Algorithms/FVizArrayCalculator.h>
#include <FViz/Algorithms/FVizFieldOperations.h>
#include <FViz/Algorithms/FVizExpression.h>
#include <FViz/Algorithms/FVizPolyDataFilters.h>
#include <FViz/Algorithms/FVizGeometryFilters.h>
#include <FViz/Algorithms/FVizMeshProcessingFilters.h>
#include <FViz/Algorithms/FVizProbeFilter.h>
#include <FViz/Algorithms/FVizResampleWithDataSet.h>
#include <FViz/Algorithms/FVizImageDataGeometryFilter.h>
#include <FViz/Algorithms/FVizStructuredGridGeometryFilter.h>
#include <FViz/Algorithms/FVizRectilinearGridGeometryFilter.h>
#include <FViz/Algorithms/FVizRectilinearGridExtractFilter.h>
#include <FViz/Algorithms/FVizCompositeGeometryFilter.h>
#include <FViz/Algorithms/FVizStructuredGridExtractFilter.h>
#include <FViz/Algorithms/FVizUnstructuredGridPieceFilter.h>
#include <FViz/Algorithms/FVizUnstructuredGridPartitionFilter.h>
#include <FViz/Algorithms/FVizUnstructuredGridGeometryFilter.h>
#include <FViz/Algorithms/FVizMeshQualityFilter.h>
#include <FViz/Algorithms/FVizShellExtrusionFilter.h>
#include <FViz/Algorithms/FVizTubeFilter.h>
#include <FViz/Algorithms/FVizWarpVectorFilter.h>
#include <FViz/Spatial/FVizBVH.h>
#include <FViz/Spatial/FVizPointLocator.h>
#include <FViz/IO/FVizMeshReader.h>
#include <FViz/IO/FVizLocalFileProvider.h>
#include <FViz/IO/FVizOBJReader.h>
#include <FViz/IO/FVizPLYWriter.h>
#include <FViz/IO/FVizSTLReader.h>
#include <FViz/IO/FVizVTKLegacyReader.h>
#include <FViz/IO/FVizVTUReader.h>
#include <FViz/IO/FVizPVTUReader.h>
#include <FViz/IO/FVizPVTUWriter.h>
#include <FViz/IO/FVizVTUWriter.h>
#include <FViz/IO/FVizVTPReader.h>
#include <FViz/IO/FVizVTPWriter.h>
#include <FViz/IO/FVizPVD.h>
#include <FViz/IO/FVizPVDReader.h>
#include <FViz/Rendering/FVizRendering.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizLog.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/System/FVizPlatform.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizVersionInfo
{
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
    uint32_t abi;
} FVizVersionInfo;

FVIZ_API FVizVersionInfo fviz_version(void);
FVIZ_API const char* fviz_version_string(void);
FVIZ_API uint32_t fviz_abi_version(void);

FVIZ_EXTERN_C_END

#endif /* FVIZ_H */
