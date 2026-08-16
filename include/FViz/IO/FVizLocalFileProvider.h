#ifndef FVIZ_IO_LOCAL_FILE_PROVIDER_H
#define FVIZ_IO_LOCAL_FILE_PROVIDER_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Pipeline/FVizDataProvider.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizLocalFileProvider FVizLocalFileProvider;

typedef struct FVizLocalFileProviderOptions
{
    uint32_t struct_size;
    FVizDataProviderOptions provider;
} FVizLocalFileProviderOptions;

FVIZ_IO_API void fviz_local_file_provider_options_initialize(FVizLocalFileProviderOptions* options);
FVIZ_IO_API FVizResult fviz_local_file_provider_create(const FVizLocalFileProviderOptions* options,
                                                    FVizLocalFileProvider** out_provider);
/* Async requests already accepted by the underlying provider retain all state. */
FVIZ_IO_API void fviz_local_file_provider_destroy(FVizLocalFileProvider* provider);
FVIZ_IO_API FVizResult fviz_local_file_provider_register(FVizLocalFileProvider* provider, uint64_t resource_key,
                                                      const char* path);
FVIZ_IO_API FVizResult fviz_local_file_provider_unregister(FVizLocalFileProvider* provider, uint64_t resource_key);
/* Borrowed; valid until local provider destruction. */
FVIZ_IO_API FVizDataProvider* fviz_local_file_provider_data_provider(FVizLocalFileProvider* provider);
FVIZ_IO_API FVizBool fviz_local_file_provider_format_supported(const char* path);

FVIZ_EXTERN_C_END

#endif /* FVIZ_IO_LOCAL_FILE_PROVIDER_H */
