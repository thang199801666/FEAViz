#include <math.h>
#include <stdio.h>
#include <string.h>

#include <FViz/FViz.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); return 1; } } while (0)

int main(void)
{
    uint8_t reference[4u * 4u] = {
        0,0,0,255, 255,255,255,255, 255,0,0,255, 0,255,0,255};
    uint8_t actual[sizeof(reference)];
    uint8_t diff[sizeof(reference)];
    FVizImageComparisonOptions options;
    FVizImageComparisonResult result;
    memcpy(actual, reference, sizeof(actual));
    fviz_image_comparison_options_initialize(&options);
    CHECK(fviz_image_compare_rgba8(reference, actual, 4u, &options, &result,
        diff, sizeof(diff)) == FVIZ_OK);
    CHECK(result.matches == FVIZ_TRUE && result.differing_pixels == 0u);
    actual[4u] = 254u;
    CHECK(fviz_image_compare_rgba8(reference, actual, 4u, &options, &result,
        diff, sizeof(diff)) == FVIZ_OK);
    CHECK(result.matches == FVIZ_FALSE && result.differing_pixels == 1u);
    CHECK(result.maximum_channel_error[0] == 1u);
    CHECK(diff[4u] == 4u && diff[5u] == 0u && diff[7u] == 255u);
    options.mode = FVIZ_IMAGE_COMPARE_CHANNEL_TOLERANCE;
    options.channel_tolerance[0] = 1u;
    CHECK(fviz_image_compare_rgba8(reference, actual, 4u, &options, &result,
        NULL, 0u) == FVIZ_OK && result.matches == FVIZ_TRUE);
    options.mode = FVIZ_IMAGE_COMPARE_RMSE;
    options.rmse_threshold = result.rmse + 1.0e-6;
    CHECK(fviz_image_compare_rgba8(reference, actual, 4u, &options, &result,
        NULL, 0u) == FVIZ_OK && result.matches == FVIZ_TRUE);
    options.rmse_threshold = 0.0;
    CHECK(fviz_image_compare_rgba8(reference, actual, 4u, &options, &result,
        NULL, 0u) == FVIZ_OK && result.matches == FVIZ_FALSE);
    options.mode = FVIZ_IMAGE_COMPARE_PERCEPTUAL;
    options.perceptual_threshold = 0.0;
    CHECK(fviz_image_compare_rgba8(reference, actual, 4u, &options, &result,
        NULL, 0u) == FVIZ_OK && result.matches == FVIZ_FALSE);
    CHECK(result.perceptual_error > 0.0 && isfinite(result.perceptual_error));
    return 0;
}
