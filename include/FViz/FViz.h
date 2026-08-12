#ifndef FVIZ_H
#define FVIZ_H

#include <FViz/FVizConfig.h>
#include <FViz/FVizVersion.h>
#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizAllocator.h>
#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizBitArray.h>
#include <FViz/Core/FVizBuffer.h>
#include <FViz/Core/FVizHashMap.h>
#include <FViz/Core/FVizString.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Data/FVizDataSet.h>
#include <FViz/Data/FVizDataType.h>
#include <FViz/Math/FVizMath.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Mesh/FVizPoints.h>
#include <FViz/Mesh/FVizCellArray.h>
#include <FViz/FEA/FVizUnstructuredGrid.h>
#include <FViz/Pipeline/FVizFilter.h>
#include <FViz/Algorithms/FVizContourFilter.h>
#include <FViz/Spatial/FVizBVH.h>
#include <FViz/Spatial/FVizPointLocator.h>
#include <FViz/IO/FVizMeshReader.h>
#include <FViz/IO/FVizOBJReader.h>
#include <FViz/IO/FVizSTLReader.h>
#include <FViz/IO/FVizVTUReader.h>
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
