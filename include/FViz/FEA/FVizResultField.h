#ifndef FVIZ_FEA_RESULT_FIELD_H
#define FVIZ_FEA_RESULT_FIELD_H

#include <stdint.h>

#include <FViz/FEA/FVizFEAApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Data/FVizDataArray.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizFEAField FVizFEAField;
#define FVIZ_TYPE_FEA_FIELD UINT64_C(0xE071725DE3D9A4C1)

typedef enum FVizFEAFieldType
{
    FVIZ_FEA_FIELD_SCALAR = 0,
    FVIZ_FEA_FIELD_VECTOR = 1,
    /* Abaqus-style symmetric tensor order: 11, 22, 33, 12, 13, 23. */
    FVIZ_FEA_FIELD_TENSOR_3D_SYMMETRIC = 2,
    /* Abaqus planar tensor order: 11, 22, 33, 12. */
    FVIZ_FEA_FIELD_TENSOR_2D_SYMMETRIC = 3,
    /* Row-major 3x3 tensor: 11,12,13,21,22,23,31,32,33. */
    FVIZ_FEA_FIELD_TENSOR_3D_FULL = 4
} FVizFEAFieldType;

typedef enum FVizFEAResultPosition
{
    FVIZ_FEA_POSITION_UNKNOWN = 0,
    FVIZ_FEA_POSITION_NODAL = 1,
    FVIZ_FEA_POSITION_ELEMENT_NODAL = 2,
    FVIZ_FEA_POSITION_INTEGRATION_POINT = 3,
    FVIZ_FEA_POSITION_CENTROID = 4,
    FVIZ_FEA_POSITION_ELEMENT_FACE = 5,
    FVIZ_FEA_POSITION_WHOLE_ELEMENT = 6,
    FVIZ_FEA_POSITION_WHOLE_REGION = 7
} FVizFEAResultPosition;

typedef enum FVizFEAInvariant
{
    FVIZ_FEA_INVARIANT_NONE = 0,
    FVIZ_FEA_INVARIANT_MAGNITUDE = 1,
    FVIZ_FEA_INVARIANT_MISES = 2,
    FVIZ_FEA_INVARIANT_TRESCA = 3,
    FVIZ_FEA_INVARIANT_PRESSURE = 4,
    FVIZ_FEA_INVARIANT_MAX_PRINCIPAL = 5,
    FVIZ_FEA_INVARIANT_MID_PRINCIPAL = 6,
    FVIZ_FEA_INVARIANT_MIN_PRINCIPAL = 7
} FVizFEAInvariant;

typedef uint64_t FVizFEAInvariantMask;
#define FVIZ_FEA_INVARIANT_BIT(invariant_value) (UINT64_C(1) << (uint32_t)(invariant_value))

typedef struct FVizFEAFieldBlockDescriptor
{
    uint32_t struct_size;
    const char* instance_name;
    FVizFEAResultPosition position;
    int32_t section_point_number; /* 0 when not applicable. */
    const char* section_point_label;
    /* Optional one-component integer arrays. entity_ids and local_ids must have
     * the same tuple count as values when supplied. entity_ids normally store
     * node/element labels; local_ids normally store integration-point, face,
     * or element-nodal local identifiers. */
    FVizDataArray* entity_ids;
    FVizDataArray* local_ids;
    FVizDataArray* values;
} FVizFEAFieldBlockDescriptor;

FVIZ_FEA_API void fviz_fea_field_block_descriptor_initialize(FVizFEAFieldBlockDescriptor* descriptor);
FVIZ_FEA_API FVizResult fviz_fea_field_create(const char* name, const char* description, FVizFEAFieldType field_type,
                                              FVizFEAField** out_field);
FVIZ_FEA_API const char* fviz_fea_field_name(const FVizFEAField* field);
FVIZ_FEA_API const char* fviz_fea_field_description(const FVizFEAField* field);
FVIZ_FEA_API FVizFEAFieldType fviz_fea_field_type(const FVizFEAField* field);
FVIZ_FEA_API FVizResult fviz_fea_field_set_component_labels(FVizFEAField* field, const char* const* labels,
                                                            FVizSize label_count);
FVIZ_FEA_API FVizSize fviz_fea_field_component_count(const FVizFEAField* field);
FVIZ_FEA_API const char* fviz_fea_field_component_label(const FVizFEAField* field, FVizSize component);
FVIZ_FEA_API FVizResult fviz_fea_field_find_component(const FVizFEAField* field, const char* label,
                                                      FVizSize* out_component);
FVIZ_FEA_API FVizFEAInvariantMask fviz_fea_field_valid_invariants(const FVizFEAField* field);
FVIZ_FEA_API FVizResult fviz_fea_field_set_valid_invariants(FVizFEAField* field, FVizFEAInvariantMask invariants);
FVIZ_FEA_API FVizSize fviz_fea_field_block_count(const FVizFEAField* field);
FVIZ_FEA_API FVizResult fviz_fea_field_add_block(FVizFEAField* field, const FVizFEAFieldBlockDescriptor* descriptor,
                                                 FVizSize* out_block_index);
FVIZ_FEA_API FVizResult fviz_fea_field_remove_block(FVizFEAField* field, FVizSize block_index);
FVIZ_FEA_API const char* fviz_fea_field_block_instance_name(const FVizFEAField* field, FVizSize block_index);
FVIZ_FEA_API FVizFEAResultPosition fviz_fea_field_block_position(const FVizFEAField* field, FVizSize block_index);
FVIZ_FEA_API int32_t fviz_fea_field_block_section_point_number(const FVizFEAField* field, FVizSize block_index);
FVIZ_FEA_API const char* fviz_fea_field_block_section_point_label(const FVizFEAField* field, FVizSize block_index);
FVIZ_FEA_API FVizDataArray* fviz_fea_field_block_values(FVizFEAField* field, FVizSize block_index);
FVIZ_FEA_API const FVizDataArray* fviz_fea_field_block_const_values(const FVizFEAField* field, FVizSize block_index);
FVIZ_FEA_API const FVizDataArray* fviz_fea_field_block_entity_ids(const FVizFEAField* field, FVizSize block_index);
FVIZ_FEA_API const FVizDataArray* fviz_fea_field_block_local_ids(const FVizFEAField* field, FVizSize block_index);
FVIZ_FEA_API FVizSize fviz_fea_field_total_tuple_count(const FVizFEAField* field);

FVIZ_FEA_API const char* fviz_fea_result_position_name(FVizFEAResultPosition position);
FVIZ_FEA_API const char* fviz_fea_invariant_name(FVizFEAInvariant invariant);

/* Creates a Float64 scalar array for one component or invariant. The returned
 * array is newly owned by the caller. These functions never modify the field. */
FVIZ_FEA_API FVizResult fviz_fea_field_evaluate_component(const FVizFEAField* field, FVizSize block_index,
                                                          FVizSize component, FVizDataArray** out_values);
FVIZ_FEA_API FVizResult fviz_fea_field_evaluate_invariant(const FVizFEAField* field, FVizSize block_index,
                                                          FVizFEAInvariant invariant, FVizDataArray** out_values);

FVIZ_EXTERN_C_END

#endif /* FVIZ_FEA_RESULT_FIELD_H */
