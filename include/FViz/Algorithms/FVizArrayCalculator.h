#ifndef FVIZ_ALGORITHMS_ARRAY_CALCULATOR_H
#define FVIZ_ALGORITHMS_ARRAY_CALCULATOR_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

FVIZ_EXTERN_C_BEGIN

typedef enum FVizArrayCalculation
{
    FVIZ_ARRAY_CALC_COMPONENT = 0,
    FVIZ_ARRAY_CALC_MAGNITUDE = 1,
    FVIZ_ARRAY_CALC_EQUIVALENT_DEVIATORIC = 2,
    FVIZ_ARRAY_CALC_SCALE_OFFSET = 3,
    FVIZ_ARRAY_CALC_TENSOR_MEAN = 4,
    FVIZ_ARRAY_CALC_PRINCIPAL_VALUES = 5,
    FVIZ_ARRAY_CALC_PRINCIPAL_MAX = 6,
    FVIZ_ARRAY_CALC_PRINCIPAL_MID = 7,
    FVIZ_ARRAY_CALC_PRINCIPAL_MIN = 8,
    FVIZ_ARRAY_CALC_HALF_PRINCIPAL_SPAN = 9,
    FVIZ_ARRAY_CALC_PRINCIPAL_SPAN = 10,
    FVIZ_ARRAY_CALC_DEVIATORIC_TENSOR = 11,
    FVIZ_ARRAY_CALC_PRINCIPAL_DIRECTIONS = 12
} FVizArrayCalculation;

/* 0.x source-compatibility aliases. Domain modules should expose mechanics
 * terminology such as Mises/Tresca rather than adding that policy to Core. */
#define FVIZ_ARRAY_CALC_VON_MISES FVIZ_ARRAY_CALC_EQUIVALENT_DEVIATORIC
#define FVIZ_ARRAY_CALC_MEAN_STRESS FVIZ_ARRAY_CALC_TENSOR_MEAN
#define FVIZ_ARRAY_CALC_MAX_SHEAR FVIZ_ARRAY_CALC_HALF_PRINCIPAL_SPAN
#define FVIZ_ARRAY_CALC_TRESCA FVIZ_ARRAY_CALC_PRINCIPAL_SPAN

typedef struct FVizArrayCalculatorOptions
{
    uint32_t struct_size;
    FVizArrayCalculation operation;
    uint32_t component;
    double scale;
    double offset;
    FVizSize parallel_threshold;
} FVizArrayCalculatorOptions;

FVIZ_API void fviz_array_calculator_options_initialize(FVizArrayCalculatorOptions* options);
/* Derived results are emitted as Float64. Tensor operations accept 6-component
 * symmetric tensors [xx,yy,zz,xy,yz,xz] or 9-component row-major tensors.
 * PRINCIPAL_VALUES emits [max,mid,min], PRINCIPAL_DIRECTIONS emits three row-major
 * direction vectors corresponding to those values, and DEVIATORIC_TENSOR emits
 * [xx,yy,zz,xy,yz,xz]. HALF_PRINCIPAL_SPAN is (max-min)/2 and
 * PRINCIPAL_SPAN is max-min. */
FVIZ_API FVizResult fviz_array_calculator_compute(const FVizDataArray* source,
                                                  const FVizArrayCalculatorOptions* options, FVizDataArray** out_array);

typedef enum FVizArrayCalculatorAssociation
{
    FVIZ_ARRAY_CALC_POINT_DATA = 0,
    FVIZ_ARRAY_CALC_CELL_DATA = 1
} FVizArrayCalculatorAssociation;

typedef struct FVizArrayCalculatorFilter FVizArrayCalculatorFilter;
#define FVIZ_TYPE_ARRAY_CALCULATOR_FILTER UINT64_C(0xB4A8E76D1392C5F0)

/* Pipeline form of the calculator for PolyData. The input dataset is deep-copied,
 * the named point/cell array is transformed, and the result is appended under
 * result_name. Single-input temporal metadata is inherited by the executive. */
FVIZ_API FVizResult fviz_array_calculator_filter_create(FVizArrayCalculatorFilter** out_filter);
FVIZ_API FVizResult fviz_array_calculator_filter_set_input_data(FVizArrayCalculatorFilter* filter, FVizPolyData* input);
FVIZ_API FVizResult fviz_array_calculator_filter_set_input_connection(FVizArrayCalculatorFilter* filter,
                                                                      FVizAlgorithmOutput* input);
FVIZ_API FVizResult fviz_array_calculator_filter_set_array(FVizArrayCalculatorFilter* filter,
                                                           FVizArrayCalculatorAssociation association,
                                                           const char* array_name);
FVIZ_API FVizResult fviz_array_calculator_filter_set_result_name(FVizArrayCalculatorFilter* filter,
                                                                 const char* result_name);
FVIZ_API FVizResult fviz_array_calculator_filter_set_options(FVizArrayCalculatorFilter* filter,
                                                             const FVizArrayCalculatorOptions* options);
FVIZ_API void fviz_array_calculator_filter_set_result_as_active_scalars(FVizArrayCalculatorFilter* filter,
                                                                        FVizBool enabled);
FVIZ_API FVizAlgorithm* fviz_array_calculator_filter_algorithm(FVizArrayCalculatorFilter* filter);
FVIZ_API FVizAlgorithmOutput* fviz_array_calculator_filter_output_port(FVizArrayCalculatorFilter* filter);
FVIZ_API FVizPolyData* fviz_array_calculator_filter_output(FVizArrayCalculatorFilter* filter);
FVIZ_API FVizResult fviz_array_calculator_filter_update(FVizArrayCalculatorFilter* filter);

FVIZ_EXTERN_C_END

#endif /* FVIZ_ALGORITHMS_ARRAY_CALCULATOR_H */
