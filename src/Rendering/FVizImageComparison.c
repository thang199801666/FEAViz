#include <math.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Rendering/FVizImageComparison.h>

#include <FViz/Core/FVizErrorInternal.h>

static double fviz_image_srgb_to_linear(uint8_t value)
{
    const double normalized = (double)value / 255.0;
    return normalized <= 0.04045 ? normalized / 12.92 : pow((normalized + 0.055) / 1.055, 2.4);
}

void fviz_image_comparison_options_initialize(FVizImageComparisonOptions* options)
{
    if (options == NULL) return;
    memset(options, 0, sizeof(*options));
    options->struct_size = (uint32_t)sizeof(*options);
    options->mode = FVIZ_IMAGE_COMPARE_EXACT;
}

void fviz_image_comparison_result_initialize(FVizImageComparisonResult* result)
{
    if (result == NULL) return;
    memset(result, 0, sizeof(*result));
    result->struct_size = (uint32_t)sizeof(*result);
}

FVizResult fviz_image_compare_rgba8(const uint8_t* reference, const uint8_t* actual, FVizSize pixel_count,
                                    const FVizImageComparisonOptions* options, FVizImageComparisonResult* result,
                                    uint8_t* diff_rgba8, FVizSize diff_byte_capacity)
{
    FVizImageComparisonOptions defaults;
    double squared[4] = {0.0, 0.0, 0.0, 0.0};
    double perceptual_squared = 0.0;
    FVizSize pixel;
    FVizBool tolerance_match = FVIZ_TRUE;
    if (options == NULL)
    {
        fviz_image_comparison_options_initialize(&defaults);
        options = &defaults;
    }
    if ((pixel_count != 0u && (reference == NULL || actual == NULL)) || result == NULL ||
        options->struct_size < sizeof(*options) || options->mode < FVIZ_IMAGE_COMPARE_EXACT ||
        options->mode > FVIZ_IMAGE_COMPARE_PERCEPTUAL || !isfinite(options->rmse_threshold) ||
        options->rmse_threshold < 0.0 || !isfinite(options->perceptual_threshold) ||
        options->perceptual_threshold < 0.0 ||
        (diff_rgba8 != NULL && (pixel_count > (FVizSize)-1 / 4u || diff_byte_capacity < pixel_count * 4u)))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "invalid RGBA8 image comparison arguments");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    fviz_image_comparison_result_initialize(result);
    for (pixel = 0u; pixel < pixel_count; ++pixel)
    {
        uint32_t channel;
        FVizBool pixel_differs = FVIZ_FALSE;
        double reference_luma = 0.0;
        double actual_luma = 0.0;
        static const double luma_weights[3] = {0.2126, 0.7152, 0.0722};
        for (channel = 0u; channel < 4u; ++channel)
        {
            const int difference = (int)actual[pixel * 4u + channel] - (int)reference[pixel * 4u + channel];
            const uint8_t absolute = (uint8_t)(difference < 0 ? -difference : difference);
            squared[channel] += (double)difference * (double)difference;
            if (absolute > result->maximum_channel_error[channel]) result->maximum_channel_error[channel] = absolute;
            if (absolute != 0u) pixel_differs = FVIZ_TRUE;
            if (absolute > options->channel_tolerance[channel]) tolerance_match = FVIZ_FALSE;
            if (diff_rgba8 != NULL && channel < 3u)
            {
                const uint32_t amplified = (uint32_t)absolute * 4u;
                diff_rgba8[pixel * 4u + channel] = (uint8_t)(amplified > 255u ? 255u : amplified);
            }
        }
        if (diff_rgba8 != NULL) diff_rgba8[pixel * 4u + 3u] = 255u;
        if (pixel_differs != FVIZ_FALSE) ++result->differing_pixels;
        reference_luma = luma_weights[0] * fviz_image_srgb_to_linear(reference[pixel * 4u]) +
                         luma_weights[1] * fviz_image_srgb_to_linear(reference[pixel * 4u + 1u]) +
                         luma_weights[2] * fviz_image_srgb_to_linear(reference[pixel * 4u + 2u]);
        actual_luma = luma_weights[0] * fviz_image_srgb_to_linear(actual[pixel * 4u]) +
                      luma_weights[1] * fviz_image_srgb_to_linear(actual[pixel * 4u + 1u]) +
                      luma_weights[2] * fviz_image_srgb_to_linear(actual[pixel * 4u + 2u]);
        perceptual_squared += (actual_luma - reference_luma) * (actual_luma - reference_luma);
    }
    if (pixel_count != 0u)
    {
        uint32_t channel;
        double total = 0.0;
        for (channel = 0u; channel < 4u; ++channel)
        {
            result->channel_rmse[channel] = sqrt(squared[channel] / (double)pixel_count);
            total += squared[channel];
        }
        result->rmse = sqrt(total / ((double)pixel_count * 4.0));
        result->perceptual_error = sqrt(perceptual_squared / (double)pixel_count);
    }
    if (options->mode == FVIZ_IMAGE_COMPARE_EXACT)
        result->matches = result->differing_pixels == 0u ? FVIZ_TRUE : FVIZ_FALSE;
    else if (options->mode == FVIZ_IMAGE_COMPARE_CHANNEL_TOLERANCE)
        result->matches = tolerance_match;
    else if (options->mode == FVIZ_IMAGE_COMPARE_RMSE)
        result->matches = result->rmse <= options->rmse_threshold ? FVIZ_TRUE : FVIZ_FALSE;
    else
        result->matches = result->perceptual_error <= options->perceptual_threshold ? FVIZ_TRUE : FVIZ_FALSE;
    return FVIZ_OK;
}
