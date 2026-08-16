#ifndef FVIZ_FEA_RESULT_DATABASE_H
#define FVIZ_FEA_RESULT_DATABASE_H

#include <stdint.h>

#include <FViz/FEA/FVizFEAApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/FEA/FVizResultField.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizFEAFrame FVizFEAFrame;
typedef struct FVizFEAStep FVizFEAStep;
typedef struct FVizFEAResultDatabase FVizFEAResultDatabase;
typedef struct FVizFEAHistorySeries FVizFEAHistorySeries;
typedef struct FVizFEAHistoryRegion FVizFEAHistoryRegion;

#define FVIZ_TYPE_FEA_FRAME UINT64_C(0x3763055A9722792D)
#define FVIZ_TYPE_FEA_STEP UINT64_C(0xD533E70F6B0F2678)
#define FVIZ_TYPE_FEA_RESULT_DATABASE UINT64_C(0xA6D6CEB87B76AAE4)
#define FVIZ_TYPE_FEA_HISTORY_SERIES UINT64_C(0x3EA3268CB90B7604)
#define FVIZ_TYPE_FEA_HISTORY_REGION UINT64_C(0x710C659C0D4AEF11)

typedef struct FVizFEAHistorySample
{
    double frame_value;
    double value;
} FVizFEAHistorySample;

FVIZ_FEA_API FVizResult fviz_fea_history_series_create(const char* name, const char* description,
                                                       FVizFEAHistorySeries** out_series);
FVIZ_FEA_API const char* fviz_fea_history_series_name(const FVizFEAHistorySeries* series);
FVIZ_FEA_API const char* fviz_fea_history_series_description(const FVizFEAHistorySeries* series);
FVIZ_FEA_API FVizSize fviz_fea_history_series_count(const FVizFEAHistorySeries* series);
FVIZ_FEA_API FVizResult fviz_fea_history_series_reserve(FVizFEAHistorySeries* series, FVizSize count);
FVIZ_FEA_API FVizResult fviz_fea_history_series_append(FVizFEAHistorySeries* series, double frame_value, double value);
FVIZ_FEA_API FVizResult fviz_fea_history_series_append_samples(FVizFEAHistorySeries* series,
                                                               const FVizFEAHistorySample* samples, FVizSize count);
FVIZ_FEA_API FVizResult fviz_fea_history_series_sample(const FVizFEAHistorySeries* series, FVizSize index,
                                                       FVizFEAHistorySample* out_sample);
FVIZ_FEA_API FVizResult fviz_fea_history_series_interpolate(const FVizFEAHistorySeries* series, double frame_value,
                                                            double* out_value);

FVIZ_FEA_API FVizResult fviz_fea_history_region_create(const char* name, const char* description,
                                                       FVizFEAHistoryRegion** out_region);
FVIZ_FEA_API const char* fviz_fea_history_region_name(const FVizFEAHistoryRegion* region);
FVIZ_FEA_API const char* fviz_fea_history_region_description(const FVizFEAHistoryRegion* region);
FVIZ_FEA_API FVizSize fviz_fea_history_region_series_count(const FVizFEAHistoryRegion* region);
FVIZ_FEA_API FVizResult fviz_fea_history_region_add_series(FVizFEAHistoryRegion* region, FVizFEAHistorySeries* series);
FVIZ_FEA_API FVizFEAHistorySeries* fviz_fea_history_region_series(FVizFEAHistoryRegion* region, const char* name);
FVIZ_FEA_API const FVizFEAHistorySeries* fviz_fea_history_region_const_series(const FVizFEAHistoryRegion* region,
                                                                              const char* name);
FVIZ_FEA_API FVizFEAHistorySeries* fviz_fea_history_region_series_at(FVizFEAHistoryRegion* region, FVizSize index);
FVIZ_FEA_API const FVizFEAHistorySeries* fviz_fea_history_region_const_series_at(const FVizFEAHistoryRegion* region,
                                                                                 FVizSize index);

typedef enum FVizFEAStepDomain
{
    FVIZ_FEA_STEP_TIME = 0,
    FVIZ_FEA_STEP_FREQUENCY = 1,
    FVIZ_FEA_STEP_MODAL = 2,
    FVIZ_FEA_STEP_ARC_LENGTH = 3
} FVizFEAStepDomain;

typedef struct FVizFEAFrameInfo
{
    uint32_t struct_size;
    int64_t frame_id;
    int64_t increment_number;
    double frame_value;
    double frequency;
    int64_t mode;
    const char* description;
} FVizFEAFrameInfo;

FVIZ_FEA_API void fviz_fea_frame_info_initialize(FVizFEAFrameInfo* info);
FVIZ_FEA_API FVizResult fviz_fea_frame_create(const FVizFEAFrameInfo* info, FVizFEAFrame** out_frame);
FVIZ_FEA_API int64_t fviz_fea_frame_id(const FVizFEAFrame* frame);
FVIZ_FEA_API int64_t fviz_fea_frame_increment_number(const FVizFEAFrame* frame);
FVIZ_FEA_API double fviz_fea_frame_value(const FVizFEAFrame* frame);
FVIZ_FEA_API double fviz_fea_frame_frequency(const FVizFEAFrame* frame);
FVIZ_FEA_API int64_t fviz_fea_frame_mode(const FVizFEAFrame* frame);
FVIZ_FEA_API const char* fviz_fea_frame_description(const FVizFEAFrame* frame);
FVIZ_FEA_API FVizSize fviz_fea_frame_field_count(const FVizFEAFrame* frame);
FVIZ_FEA_API FVizResult fviz_fea_frame_add_field(FVizFEAFrame* frame, FVizFEAField* field);
FVIZ_FEA_API FVizResult fviz_fea_frame_remove_field(FVizFEAFrame* frame, const char* name);
FVIZ_FEA_API FVizFEAField* fviz_fea_frame_field(FVizFEAFrame* frame, const char* name);
FVIZ_FEA_API const FVizFEAField* fviz_fea_frame_const_field(const FVizFEAFrame* frame, const char* name);
FVIZ_FEA_API FVizFEAField* fviz_fea_frame_field_at(FVizFEAFrame* frame, FVizSize index);
FVIZ_FEA_API const FVizFEAField* fviz_fea_frame_const_field_at(const FVizFEAFrame* frame, FVizSize index);

FVIZ_FEA_API FVizResult fviz_fea_step_create(const char* name, const char* description, FVizFEAStepDomain domain,
                                             double time_period, FVizFEAStep** out_step);
FVIZ_FEA_API const char* fviz_fea_step_name(const FVizFEAStep* step);
FVIZ_FEA_API const char* fviz_fea_step_description(const FVizFEAStep* step);
FVIZ_FEA_API FVizFEAStepDomain fviz_fea_step_domain(const FVizFEAStep* step);
FVIZ_FEA_API double fviz_fea_step_time_period(const FVizFEAStep* step);
FVIZ_FEA_API FVizSize fviz_fea_step_frame_count(const FVizFEAStep* step);
FVIZ_FEA_API FVizResult fviz_fea_step_reserve_frames(FVizFEAStep* step, FVizSize count);
FVIZ_FEA_API FVizResult fviz_fea_step_add_frame(FVizFEAStep* step, FVizFEAFrame* frame, FVizSize* out_index);
FVIZ_FEA_API FVizResult fviz_fea_step_remove_frame(FVizFEAStep* step, FVizSize index);
FVIZ_FEA_API FVizFEAFrame* fviz_fea_step_frame(FVizFEAStep* step, FVizSize index);
FVIZ_FEA_API const FVizFEAFrame* fviz_fea_step_const_frame(const FVizFEAStep* step, FVizSize index);
FVIZ_FEA_API FVizSize fviz_fea_step_history_region_count(const FVizFEAStep* step);
FVIZ_FEA_API FVizResult fviz_fea_step_add_history_region(FVizFEAStep* step, FVizFEAHistoryRegion* region);
FVIZ_FEA_API FVizFEAHistoryRegion* fviz_fea_step_history_region(FVizFEAStep* step, const char* name);
FVIZ_FEA_API const FVizFEAHistoryRegion* fviz_fea_step_const_history_region(const FVizFEAStep* step, const char* name);
FVIZ_FEA_API FVizFEAHistoryRegion* fviz_fea_step_history_region_at(FVizFEAStep* step, FVizSize index);
FVIZ_FEA_API const FVizFEAHistoryRegion* fviz_fea_step_const_history_region_at(const FVizFEAStep* step, FVizSize index);

FVIZ_FEA_API FVizResult fviz_fea_step_find_frame_value(const FVizFEAStep* step, double value, FVizSize* out_lower,
                                                       FVizSize* out_upper, double* out_alpha);

FVIZ_FEA_API FVizResult fviz_fea_result_database_create(FVizFEAResultDatabase** out_database);
FVIZ_FEA_API FVizSize fviz_fea_result_database_step_count(const FVizFEAResultDatabase* database);
FVIZ_FEA_API FVizResult fviz_fea_result_database_add_step(FVizFEAResultDatabase* database, FVizFEAStep* step,
                                                          FVizSize* out_index);
FVIZ_FEA_API FVizResult fviz_fea_result_database_remove_step(FVizFEAResultDatabase* database, const char* name);
FVIZ_FEA_API FVizFEAStep* fviz_fea_result_database_step(FVizFEAResultDatabase* database, const char* name);
FVIZ_FEA_API const FVizFEAStep* fviz_fea_result_database_const_step(const FVizFEAResultDatabase* database,
                                                                    const char* name);
FVIZ_FEA_API FVizFEAStep* fviz_fea_result_database_step_at(FVizFEAResultDatabase* database, FVizSize index);
FVIZ_FEA_API const FVizFEAStep* fviz_fea_result_database_const_step_at(const FVizFEAResultDatabase* database,
                                                                       FVizSize index);

FVIZ_EXTERN_C_END

#endif /* FVIZ_FEA_RESULT_DATABASE_H */
