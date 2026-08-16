#ifndef FVIZ_ALGORITHMS_FIELD_OPERATIONS_H
#define FVIZ_ALGORITHMS_FIELD_OPERATIONS_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Data/FVizDataArray.h>

FVIZ_EXTERN_C_BEGIN

typedef enum FVizMissingIdPolicy
{
    FVIZ_MISSING_ID_ERROR = 0,
    FVIZ_MISSING_ID_FILL = 1
} FVizMissingIdPolicy;

typedef struct FVizFieldGatherOptions
{
    uint32_t struct_size;
    FVizMissingIdPolicy missing_id_policy;
    const void* fill_tuple;
} FVizFieldGatherOptions;

typedef struct FVizIndexedAverageOptions
{
    uint32_t struct_size;
    FVizSize destination_tuple_count;
    FVizBool ignore_non_finite;
} FVizIndexedAverageOptions;

FVIZ_FILTERS_API void fviz_field_gather_options_initialize(FVizFieldGatherOptions* options);
FVIZ_FILTERS_API void fviz_indexed_average_options_initialize(FVizIndexedAverageOptions* options);
FVIZ_FILTERS_API FVizResult fviz_field_extract_component(const FVizDataArray* source, uint32_t component,
                                                 FVizDataArray** out_values);
FVIZ_FILTERS_API FVizResult fviz_field_compute_magnitude(const FVizDataArray* source, FVizDataArray** out_values);
FVIZ_FILTERS_API FVizResult fviz_field_compute_finite_mask(const FVizDataArray* source, FVizDataArray** out_mask);
/* Reorders source_values from source_ids into target_ids. IDs are one-component
 * integer arrays. Duplicate source IDs are rejected. out_valid_mask is optional. */
FVIZ_FILTERS_API FVizResult fviz_field_gather_by_ids(const FVizDataArray* source_values, const FVizDataArray* source_ids,
                                             const FVizDataArray* target_ids, const FVizFieldGatherOptions* options,
                                             FVizDataArray** out_values, FVizDataArray** out_valid_mask);
/* Deterministically scatters source tuples into destination indices and computes
 * a component-wise weighted average. weights may be NULL for unit weights. */
FVIZ_FILTERS_API FVizResult fviz_field_indexed_weighted_average(const FVizDataArray* source_values,
                                                        const FVizDataArray* destination_indices,
                                                        const FVizDataArray* weights,
                                                        const FVizIndexedAverageOptions* options,
                                                        FVizDataArray** out_values, FVizDataArray** out_valid_mask);
/* Applies a row-major output_tuple_count x input_tuple_count matrix to every
 * field component. Output is Float64 and is suitable for local interpolation
 * or extrapolation operators. */
FVIZ_FILTERS_API FVizResult fviz_field_apply_tuple_matrix(const FVizDataArray* source_values, const double* matrix,
                                                  FVizSize output_tuple_count, FVizDataArray** out_values);
/* Computes the least-squares operator (A^T A)^-1 A^T for a row-major
 * sample_count x coefficient_count basis matrix. */
FVIZ_FILTERS_API FVizResult fviz_field_least_squares_operator(const double* basis_matrix, FVizSize sample_count,
                                                      FVizSize coefficient_count, double* out_operator);
/* Flags destination IDs whose contributing tuple component spread exceeds the
 * relative threshold. A threshold of zero flags any unequal finite values. */
FVIZ_FILTERS_API FVizResult fviz_field_compute_indexed_discontinuity_mask(const FVizDataArray* source_values,
                                                                  const FVizDataArray* destination_indices,
                                                                  FVizSize destination_tuple_count,
                                                                  double relative_threshold, FVizDataArray** out_mask);

FVIZ_EXTERN_C_END

#endif /* FVIZ_ALGORITHMS_FIELD_OPERATIONS_H */
