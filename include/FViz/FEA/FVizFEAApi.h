#ifndef FVIZ_FEA_API_H
#define FVIZ_FEA_API_H

#include <FViz/Core/FVizApi.h>

#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(FVIZ_SHARED)
#if defined(FVIZ_FEA_BUILDING_LIBRARY)
#define FVIZ_FEA_API __declspec(dllexport)
#else
#define FVIZ_FEA_API __declspec(dllimport)
#endif
#else
#define FVIZ_FEA_API
#endif
#else
#if defined(__GNUC__) || defined(__clang__)
#define FVIZ_FEA_API __attribute__((visibility("default")))
#else
#define FVIZ_FEA_API
#endif
#endif

#endif /* FVIZ_FEA_API_H */
