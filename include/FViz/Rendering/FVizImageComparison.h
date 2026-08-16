#ifndef FVIZ_RENDERING_IMAGE_COMPARISON_H
#define FVIZ_RENDERING_IMAGE_COMPARISON_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>

FVIZ_EXTERN_C_BEGIN

typedef enum FVizImageComparisonMode
{
    FVIZ_IMAGE_COMPARE_EXACT = 0,
    FVIZ_IMAGE_COMPARE_CHANNEL_TOLERANCE = 1,
    FVIZ_IMAGE_COMPARE_RMSE = 2,
    FVIZ_IMAGE_COMPARE_PERCEPTUAL = 3
} FVizImageComparisonMode;

typedef struct FVizImageComparisonOptions
{
    uint32_t struct_size;
    FVizImageComparisonMode mode;
    uint8_t channel_tolerance[4];
    double rmse_threshold;
    /* Linear-light luma RMSE threshold in [0,1]. */
    double perceptual_threshold;
} FVizImageComparisonOptions;

typedef struct FVizImageComparisonResult
{
    uint32_t struct_size;
    FVizBool matches;
    uint64_t differing_pixels;
    uint8_t maximum_channel_error[4];
    double channel_rmse[4];
    double rmse;
    double perceptual_error;
} FVizImageComparisonResult;

FVIZ_API void fviz_image_comparison_options_initialize(FVizImageComparisonOptions* options);
FVIZ_API void fviz_image_comparison_result_initialize(FVizImageComparisonResult* result);
/* Images and optional diff output are tightly packed RGBA8. Diff pixels contain
 * amplified absolute RGB error and opaque alpha; equal pixels are black. */
FVIZ_API FVizResult fviz_image_compare_rgba8(const uint8_t* reference, const uint8_t* actual, FVizSize pixel_count,
                                             const FVizImageComparisonOptions* options,
                                             FVizImageComparisonResult* result, uint8_t* diff_rgba8,
                                             FVizSize diff_byte_capacity);

FVIZ_EXTERN_C_END

#endif /* FVIZ_RENDERING_IMAGE_COMPARISON_H */
