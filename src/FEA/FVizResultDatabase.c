#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizString.h>
#include <FViz/FEA/FVizResultDatabase.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/FEA/FVizResultDatabasePrivate.h>

static void fviz_fea_history_series_destroy(FVizObject* object);
static void fviz_fea_history_region_destroy(FVizObject* object);
static void fviz_fea_frame_destroy(FVizObject* object);
static void fviz_fea_step_destroy(FVizObject* object);
static void fviz_fea_result_database_destroy(FVizObject* object);
static FVizMTime fviz_fea_local_mtime(const FVizObject* object)
{
    return fviz_internal_object_local_mtime(object);
}

static const FVizObjectClass g_fviz_fea_history_series_class = {
    FVIZ_TYPE_FEA_HISTORY_SERIES,
    "FVizFEAHistorySeries",
    NULL,
    fviz_fea_history_series_destroy,
    fviz_fea_local_mtime
};

static const FVizObjectClass g_fviz_fea_history_region_class = {
    FVIZ_TYPE_FEA_HISTORY_REGION,
    "FVizFEAHistoryRegion",
    NULL,
    fviz_fea_history_region_destroy,
    fviz_fea_local_mtime
};

static const FVizObjectClass g_fviz_fea_frame_class = {
    FVIZ_TYPE_FEA_FRAME,
    "FVizFEAFrame",
    NULL,
    fviz_fea_frame_destroy,
    fviz_fea_local_mtime
};

static const FVizObjectClass g_fviz_fea_step_class = {
    FVIZ_TYPE_FEA_STEP,
    "FVizFEAStep",
    NULL,
    fviz_fea_step_destroy,
    fviz_fea_local_mtime
};

static const FVizObjectClass g_fviz_fea_result_database_class = {
    FVIZ_TYPE_FEA_RESULT_DATABASE,
    "FVizFEAResultDatabase",
    NULL,
    fviz_fea_result_database_destroy,
    fviz_fea_local_mtime
};

static FVizBool fviz_fea_parent_child_modified(
    FVizObject* caller, FVizEventId event_id, void* call_data, void* client_data)
{
    FVizObject* parent = (FVizObject*)client_data;
    (void)caller;
    (void)event_id;
    (void)call_data;
    if (parent != NULL) fviz_object_modified(parent);
    return FVIZ_FALSE;
}

static void fviz_fea_observed_field_release(FVizFEAObservedField* entry)
{
    if (entry == NULL) return;
    if (entry->field != NULL && entry->modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)entry->field, entry->modified_tag);
    fviz_release(entry->field);
    entry->field = NULL;
    entry->modified_tag = FVIZ_OBSERVER_TAG_INVALID;
}

static void fviz_fea_observed_frame_release(FVizFEAObservedFrame* entry)
{
    if (entry == NULL) return;
    if (entry->frame != NULL && entry->modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)entry->frame, entry->modified_tag);
    fviz_release(entry->frame);
    entry->frame = NULL;
    entry->modified_tag = FVIZ_OBSERVER_TAG_INVALID;
}

static void fviz_fea_observed_history_series_release(FVizFEAObservedHistorySeries* entry)
{
    if (entry == NULL) return;
    if (entry->series != NULL && entry->modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)entry->series, entry->modified_tag);
    fviz_release(entry->series);
    entry->series = NULL;
    entry->modified_tag = FVIZ_OBSERVER_TAG_INVALID;
}

static void fviz_fea_observed_history_region_release(FVizFEAObservedHistoryRegion* entry)
{
    if (entry == NULL) return;
    if (entry->region != NULL && entry->modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)entry->region, entry->modified_tag);
    fviz_release(entry->region);
    entry->region = NULL;
    entry->modified_tag = FVIZ_OBSERVER_TAG_INVALID;
}

static void fviz_fea_observed_step_release(FVizFEAObservedStep* entry)
{
    if (entry == NULL) return;
    if (entry->step != NULL && entry->modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)entry->step, entry->modified_tag);
    fviz_release(entry->step);
    entry->step = NULL;
    entry->modified_tag = FVIZ_OBSERVER_TAG_INVALID;
}

static void fviz_fea_history_series_destroy(FVizObject* object)
{
    FVizFEAHistorySeries* series = (FVizFEAHistorySeries*)object;
    fviz_release(series->samples);
    fviz_release(series->name);
    fviz_release(series->description);
    series->samples = NULL;
    series->name = NULL;
    series->description = NULL;
}

static void fviz_fea_history_region_destroy(FVizObject* object)
{
    FVizFEAHistoryRegion* region = (FVizFEAHistoryRegion*)object;
    FVizSize i;
    if (region->series != NULL)
        for (i = 0u; i < fviz_array_count(region->series); ++i)
            fviz_fea_observed_history_series_release((FVizFEAObservedHistorySeries*)fviz_array_at(region->series, i));
    fviz_release(region->series);
    fviz_release(region->name);
    fviz_release(region->description);
    region->series = NULL;
    region->name = NULL;
    region->description = NULL;
}

static void fviz_fea_frame_destroy(FVizObject* object)
{
    FVizFEAFrame* frame = (FVizFEAFrame*)object;
    FVizSize i;
    if (frame->fields != NULL)
        for (i = 0u; i < fviz_array_count(frame->fields); ++i)
            fviz_fea_observed_field_release((FVizFEAObservedField*)fviz_array_at(frame->fields, i));
    fviz_release(frame->fields);
    fviz_release(frame->description);
    frame->fields = NULL;
    frame->description = NULL;
}

static void fviz_fea_step_destroy(FVizObject* object)
{
    FVizFEAStep* step = (FVizFEAStep*)object;
    FVizSize i;
    if (step->frames != NULL)
        for (i = 0u; i < fviz_array_count(step->frames); ++i)
            fviz_fea_observed_frame_release((FVizFEAObservedFrame*)fviz_array_at(step->frames, i));
    if (step->history_regions != NULL)
        for (i = 0u; i < fviz_array_count(step->history_regions); ++i)
            fviz_fea_observed_history_region_release((FVizFEAObservedHistoryRegion*)fviz_array_at(step->history_regions, i));
    fviz_release(step->history_regions);
    fviz_release(step->frames);
    fviz_release(step->name);
    fviz_release(step->description);
    step->frames = NULL;
    step->history_regions = NULL;
    step->name = NULL;
    step->description = NULL;
}

static void fviz_fea_result_database_destroy(FVizObject* object)
{
    FVizFEAResultDatabase* database = (FVizFEAResultDatabase*)object;
    FVizSize i;
    if (database->steps != NULL)
        for (i = 0u; i < fviz_array_count(database->steps); ++i)
            fviz_fea_observed_step_release((FVizFEAObservedStep*)fviz_array_at(database->steps, i));
    fviz_release(database->steps);
    database->steps = NULL;
}

FVizResult fviz_fea_history_series_create(
    const char* name,
    const char* description,
    FVizFEAHistorySeries** out_series)
{
    FVizFEAHistorySeries* series;
    if (out_series == NULL || name == NULL || name[0] == '\0') return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_series = NULL;
    series = (FVizFEAHistorySeries*)fviz_internal_object_allocate(
        sizeof(*series), &g_fviz_fea_history_series_class, NULL);
    if (series == NULL) return fviz_last_error_code();
    if (fviz_string_create_from(name, &series->name) != FVIZ_OK ||
        fviz_string_create_from(description != NULL ? description : "", &series->description) != FVIZ_OK ||
        fviz_array_create(sizeof(FVizFEAHistorySample), &series->samples) != FVIZ_OK)
    {
        fviz_release(series);
        return fviz_last_error_code();
    }
    *out_series = series;
    return FVIZ_OK;
}

const char* fviz_fea_history_series_name(const FVizFEAHistorySeries* series)
{
    return series != NULL && series->name != NULL ? fviz_string_c_str(series->name) : "";
}

const char* fviz_fea_history_series_description(const FVizFEAHistorySeries* series)
{
    return series != NULL && series->description != NULL ? fviz_string_c_str(series->description) : "";
}

FVizSize fviz_fea_history_series_count(const FVizFEAHistorySeries* series)
{
    return series != NULL && series->samples != NULL ? fviz_array_count(series->samples) : 0u;
}

FVizResult fviz_fea_history_series_reserve(FVizFEAHistorySeries* series, FVizSize count)
{
    return series != NULL ? fviz_array_reserve(series->samples, count) : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizResult fviz_fea_history_series_append(
    FVizFEAHistorySeries* series, double frame_value, double value)
{
    FVizFEAHistorySample sample;
    FVizSize count;
    if (series == NULL || !isfinite(frame_value) || !isfinite(value)) return FVIZ_ERROR_INVALID_ARGUMENT;
    count = fviz_fea_history_series_count(series);
    if (count != 0u)
    {
        const FVizFEAHistorySample* last = (const FVizFEAHistorySample*)fviz_array_const_at(series->samples, count - 1u);
        if (last != NULL && frame_value < last->frame_value)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "FEA history samples must be appended in nondecreasing frame-value order");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
    }
    sample.frame_value = frame_value;
    sample.value = value;
    if (fviz_array_push(series->samples, &sample) != FVIZ_OK) return fviz_last_error_code();
    fviz_object_modified((FVizObject*)series);
    return FVIZ_OK;
}

FVizResult fviz_fea_history_series_append_samples(
    FVizFEAHistorySeries* series, const FVizFEAHistorySample* samples, FVizSize count)
{
    FVizSize i;
    FVizSize old_count;
    if (series == NULL || (count != 0u && samples == NULL)) return FVIZ_ERROR_INVALID_ARGUMENT;
    old_count = fviz_fea_history_series_count(series);
    for (i = 0u; i < count; ++i)
    {
        if (!isfinite(samples[i].frame_value) || !isfinite(samples[i].value) ||
            (i > 0u && samples[i].frame_value < samples[i - 1u].frame_value) ||
            (i == 0u && old_count != 0u && samples[i].frame_value <
                ((const FVizFEAHistorySample*)fviz_array_const_at(series->samples, old_count - 1u))->frame_value))
            return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (count == 0u) return FVIZ_OK;
    if (fviz_array_append(series->samples, samples, count) != FVIZ_OK) return fviz_last_error_code();
    fviz_object_modified((FVizObject*)series);
    return FVIZ_OK;
}

FVizResult fviz_fea_history_series_sample(
    const FVizFEAHistorySeries* series, FVizSize index, FVizFEAHistorySample* out_sample)
{
    const FVizFEAHistorySample* sample;
    if (series == NULL || out_sample == NULL || index >= fviz_fea_history_series_count(series))
        return FVIZ_ERROR_INVALID_ARGUMENT;
    sample = (const FVizFEAHistorySample*)fviz_array_const_at(series->samples, index);
    *out_sample = *sample;
    return FVIZ_OK;
}

FVizResult fviz_fea_history_series_interpolate(
    const FVizFEAHistorySeries* series, double frame_value, double* out_value)
{
    FVizSize count;
    FVizSize lo;
    FVizSize hi;
    if (series == NULL || out_value == NULL || !isfinite(frame_value)) return FVIZ_ERROR_INVALID_ARGUMENT;
    count = fviz_fea_history_series_count(series);
    if (count == 0u) return FVIZ_ERROR_NOT_FOUND;
    {
        const FVizFEAHistorySample* first = (const FVizFEAHistorySample*)fviz_array_const_at(series->samples, 0u);
        const FVizFEAHistorySample* last = (const FVizFEAHistorySample*)fviz_array_const_at(series->samples, count - 1u);
        if (frame_value <= first->frame_value) { *out_value = first->value; return FVIZ_OK; }
        if (frame_value >= last->frame_value) { *out_value = last->value; return FVIZ_OK; }
    }
    lo = 0u; hi = count - 1u;
    while (hi - lo > 1u)
    {
        const FVizSize mid = lo + (hi - lo) / 2u;
        const FVizFEAHistorySample* sample = (const FVizFEAHistorySample*)fviz_array_const_at(series->samples, mid);
        if (sample->frame_value <= frame_value) lo = mid; else hi = mid;
    }
    {
        const FVizFEAHistorySample* a = (const FVizFEAHistorySample*)fviz_array_const_at(series->samples, lo);
        const FVizFEAHistorySample* b = (const FVizFEAHistorySample*)fviz_array_const_at(series->samples, hi);
        const double alpha = b->frame_value != a->frame_value ?
            (frame_value - a->frame_value) / (b->frame_value - a->frame_value) : 0.0;
        *out_value = a->value + alpha * (b->value - a->value);
    }
    return FVIZ_OK;
}

FVizResult fviz_fea_history_region_create(
    const char* name,
    const char* description,
    FVizFEAHistoryRegion** out_region)
{
    FVizFEAHistoryRegion* region;
    if (out_region == NULL || name == NULL || name[0] == '\0') return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_region = NULL;
    region = (FVizFEAHistoryRegion*)fviz_internal_object_allocate(
        sizeof(*region), &g_fviz_fea_history_region_class, NULL);
    if (region == NULL) return fviz_last_error_code();
    if (fviz_string_create_from(name, &region->name) != FVIZ_OK ||
        fviz_string_create_from(description != NULL ? description : "", &region->description) != FVIZ_OK ||
        fviz_array_create(sizeof(FVizFEAObservedHistorySeries), &region->series) != FVIZ_OK)
    {
        fviz_release(region);
        return fviz_last_error_code();
    }
    *out_region = region;
    return FVIZ_OK;
}

const char* fviz_fea_history_region_name(const FVizFEAHistoryRegion* region)
{
    return region != NULL && region->name != NULL ? fviz_string_c_str(region->name) : "";
}

const char* fviz_fea_history_region_description(const FVizFEAHistoryRegion* region)
{
    return region != NULL && region->description != NULL ? fviz_string_c_str(region->description) : "";
}

FVizSize fviz_fea_history_region_series_count(const FVizFEAHistoryRegion* region)
{
    return region != NULL && region->series != NULL ? fviz_array_count(region->series) : 0u;
}

static FVizSize fviz_fea_history_region_find_series_index(const FVizFEAHistoryRegion* region, const char* name)
{
    FVizSize i;
    if (region == NULL || name == NULL) return (FVizSize)-1;
    for (i = 0u; i < fviz_fea_history_region_series_count(region); ++i)
    {
        const FVizFEAObservedHistorySeries* entry = (const FVizFEAObservedHistorySeries*)fviz_array_const_at(region->series, i);
        if (entry != NULL && entry->series != NULL && strcmp(fviz_fea_history_series_name(entry->series), name) == 0)
            return i;
    }
    return (FVizSize)-1;
}

FVizResult fviz_fea_history_region_add_series(FVizFEAHistoryRegion* region, FVizFEAHistorySeries* series)
{
    FVizFEAObservedHistorySeries entry;
    if (region == NULL || series == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (fviz_fea_history_region_find_series_index(region, fviz_fea_history_series_name(series)) != (FVizSize)-1)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    (void)memset(&entry, 0, sizeof(entry));
    entry.modified_tag = FVIZ_OBSERVER_TAG_INVALID;
    entry.series = (FVizFEAHistorySeries*)fviz_retain(series);
    if (entry.series == NULL) return fviz_last_error_code();
    if (fviz_object_add_observer((FVizObject*)entry.series, FVIZ_EVENT_MODIFIED, 0.0f,
        fviz_fea_parent_child_modified, region, &entry.modified_tag) != FVIZ_OK ||
        fviz_array_push(region->series, &entry) != FVIZ_OK)
    {
        fviz_fea_observed_history_series_release(&entry);
        return fviz_last_error_code();
    }
    fviz_object_modified((FVizObject*)region);
    return FVIZ_OK;
}

FVizFEAHistorySeries* fviz_fea_history_region_series(FVizFEAHistoryRegion* region, const char* name)
{
    const FVizSize index = fviz_fea_history_region_find_series_index(region, name);
    if (index == (FVizSize)-1) return NULL;
    return ((FVizFEAObservedHistorySeries*)fviz_array_at(region->series, index))->series;
}

const FVizFEAHistorySeries* fviz_fea_history_region_const_series(const FVizFEAHistoryRegion* region, const char* name)
{
    const FVizSize index = fviz_fea_history_region_find_series_index(region, name);
    if (index == (FVizSize)-1) return NULL;
    return ((const FVizFEAObservedHistorySeries*)fviz_array_const_at(region->series, index))->series;
}

FVizFEAHistorySeries* fviz_fea_history_region_series_at(FVizFEAHistoryRegion* region, FVizSize index)
{
    FVizFEAObservedHistorySeries* entry = region != NULL && index < fviz_fea_history_region_series_count(region)
        ? (FVizFEAObservedHistorySeries*)fviz_array_at(region->series, index) : NULL;
    return entry != NULL ? entry->series : NULL;
}

const FVizFEAHistorySeries* fviz_fea_history_region_const_series_at(const FVizFEAHistoryRegion* region, FVizSize index)
{
    const FVizFEAObservedHistorySeries* entry = region != NULL && index < fviz_fea_history_region_series_count(region)
        ? (const FVizFEAObservedHistorySeries*)fviz_array_const_at(region->series, index) : NULL;
    return entry != NULL ? entry->series : NULL;
}

void fviz_fea_frame_info_initialize(FVizFEAFrameInfo* info)
{
    if (info == NULL) return;
    (void)memset(info, 0, sizeof(*info));
    info->struct_size = (uint32_t)sizeof(*info);
}

FVizResult fviz_fea_frame_create(const FVizFEAFrameInfo* info, FVizFEAFrame** out_frame)
{
    FVizFEAFrame* frame;
    if (out_frame == NULL || info == NULL || info->struct_size < sizeof(FVizFEAFrameInfo) ||
        !isfinite(info->frame_value) || !isfinite(info->frequency))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "FEA frame metadata or output is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_frame = NULL;
    frame = (FVizFEAFrame*)fviz_internal_object_allocate(sizeof(*frame), &g_fviz_fea_frame_class, NULL);
    if (frame == NULL) return fviz_last_error_code();
    frame->frame_id = info->frame_id;
    frame->increment_number = info->increment_number;
    frame->frame_value = info->frame_value;
    frame->frequency = info->frequency;
    frame->mode = info->mode;
    if (fviz_string_create_from(info->description != NULL ? info->description : "", &frame->description) != FVIZ_OK ||
        fviz_array_create(sizeof(FVizFEAObservedField), &frame->fields) != FVIZ_OK)
    {
        fviz_release(frame);
        return fviz_last_error_code();
    }
    *out_frame = frame;
    return FVIZ_OK;
}

int64_t fviz_fea_frame_id(const FVizFEAFrame* frame) { return frame != NULL ? frame->frame_id : 0; }
int64_t fviz_fea_frame_increment_number(const FVizFEAFrame* frame) { return frame != NULL ? frame->increment_number : 0; }
double fviz_fea_frame_value(const FVizFEAFrame* frame) { return frame != NULL ? frame->frame_value : 0.0; }
double fviz_fea_frame_frequency(const FVizFEAFrame* frame) { return frame != NULL ? frame->frequency : 0.0; }
int64_t fviz_fea_frame_mode(const FVizFEAFrame* frame) { return frame != NULL ? frame->mode : 0; }
const char* fviz_fea_frame_description(const FVizFEAFrame* frame)
{
    return frame != NULL && frame->description != NULL ? fviz_string_c_str(frame->description) : "";
}
FVizSize fviz_fea_frame_field_count(const FVizFEAFrame* frame)
{
    return frame != NULL && frame->fields != NULL ? fviz_array_count(frame->fields) : 0u;
}

static FVizSize fviz_fea_frame_find_field_index(const FVizFEAFrame* frame, const char* name)
{
    FVizSize i;
    if (frame == NULL || name == NULL) return (FVizSize)-1;
    for (i = 0u; i < fviz_fea_frame_field_count(frame); ++i)
    {
        const FVizFEAObservedField* entry = (const FVizFEAObservedField*)fviz_array_const_at(frame->fields, i);
        if (entry != NULL && entry->field != NULL && strcmp(fviz_fea_field_name(entry->field), name) == 0)
            return i;
    }
    return (FVizSize)-1;
}

FVizResult fviz_fea_frame_add_field(FVizFEAFrame* frame, FVizFEAField* field)
{
    FVizFEAObservedField entry;
    if (frame == NULL || field == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "FEA frame and field must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_fea_frame_find_field_index(frame, fviz_fea_field_name(field)) != (FVizSize)-1)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "FEA frame already contains a field with this name");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    (void)memset(&entry, 0, sizeof(entry));
    entry.modified_tag = FVIZ_OBSERVER_TAG_INVALID;
    entry.field = (FVizFEAField*)fviz_retain(field);
    if (entry.field == NULL) return fviz_last_error_code();
    if (fviz_object_add_observer((FVizObject*)entry.field, FVIZ_EVENT_MODIFIED, 0.0f,
        fviz_fea_parent_child_modified, frame, &entry.modified_tag) != FVIZ_OK ||
        fviz_array_push(frame->fields, &entry) != FVIZ_OK)
    {
        fviz_fea_observed_field_release(&entry);
        return fviz_last_error_code();
    }
    fviz_object_modified((FVizObject*)frame);
    return FVIZ_OK;
}

FVizResult fviz_fea_frame_remove_field(FVizFEAFrame* frame, const char* name)
{
    FVizSize index;
    FVizSize count;
    FVizFEAObservedField* entries;
    if (frame == NULL || name == NULL || name[0] == '\0') return FVIZ_ERROR_INVALID_ARGUMENT;
    index = fviz_fea_frame_find_field_index(frame, name);
    if (index == (FVizSize)-1) return FVIZ_ERROR_NOT_FOUND;
    count = fviz_array_count(frame->fields);
    entries = (FVizFEAObservedField*)fviz_array_data(frame->fields);
    fviz_fea_observed_field_release(&entries[index]);
    if (index + 1u < count)
        (void)memmove(&entries[index], &entries[index + 1u], (size_t)(count - index - 1u) * sizeof(*entries));
    if (fviz_array_resize(frame->fields, count - 1u) != FVIZ_OK) return fviz_last_error_code();
    fviz_object_modified((FVizObject*)frame);
    return FVIZ_OK;
}

FVizFEAField* fviz_fea_frame_field(FVizFEAFrame* frame, const char* name)
{
    const FVizSize index = fviz_fea_frame_find_field_index(frame, name);
    if (index == (FVizSize)-1) return NULL;
    return ((FVizFEAObservedField*)fviz_array_at(frame->fields, index))->field;
}

const FVizFEAField* fviz_fea_frame_const_field(const FVizFEAFrame* frame, const char* name)
{
    const FVizSize index = fviz_fea_frame_find_field_index(frame, name);
    if (index == (FVizSize)-1) return NULL;
    return ((const FVizFEAObservedField*)fviz_array_const_at(frame->fields, index))->field;
}

FVizFEAField* fviz_fea_frame_field_at(FVizFEAFrame* frame, FVizSize index)
{
    FVizFEAObservedField* entry = frame != NULL && index < fviz_fea_frame_field_count(frame)
        ? (FVizFEAObservedField*)fviz_array_at(frame->fields, index) : NULL;
    return entry != NULL ? entry->field : NULL;
}

const FVizFEAField* fviz_fea_frame_const_field_at(const FVizFEAFrame* frame, FVizSize index)
{
    const FVizFEAObservedField* entry = frame != NULL && index < fviz_fea_frame_field_count(frame)
        ? (const FVizFEAObservedField*)fviz_array_const_at(frame->fields, index) : NULL;
    return entry != NULL ? entry->field : NULL;
}

FVizResult fviz_fea_step_create(
    const char* name,
    const char* description,
    FVizFEAStepDomain domain,
    double time_period,
    FVizFEAStep** out_step)
{
    FVizFEAStep* step;
    if (out_step == NULL || name == NULL || name[0] == '\0' || !isfinite(time_period) || time_period < 0.0 ||
        domain < FVIZ_FEA_STEP_TIME || domain > FVIZ_FEA_STEP_ARC_LENGTH)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "FEA step metadata or output is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_step = NULL;
    step = (FVizFEAStep*)fviz_internal_object_allocate(sizeof(*step), &g_fviz_fea_step_class, NULL);
    if (step == NULL) return fviz_last_error_code();
    if (fviz_string_create_from(name, &step->name) != FVIZ_OK ||
        fviz_string_create_from(description != NULL ? description : "", &step->description) != FVIZ_OK ||
        fviz_array_create(sizeof(FVizFEAObservedFrame), &step->frames) != FVIZ_OK ||
        fviz_array_create(sizeof(FVizFEAObservedHistoryRegion), &step->history_regions) != FVIZ_OK)
    {
        fviz_release(step);
        return fviz_last_error_code();
    }
    step->domain = domain;
    step->time_period = time_period;
    *out_step = step;
    return FVIZ_OK;
}

const char* fviz_fea_step_name(const FVizFEAStep* step)
{
    return step != NULL && step->name != NULL ? fviz_string_c_str(step->name) : "";
}
const char* fviz_fea_step_description(const FVizFEAStep* step)
{
    return step != NULL && step->description != NULL ? fviz_string_c_str(step->description) : "";
}
FVizFEAStepDomain fviz_fea_step_domain(const FVizFEAStep* step) { return step != NULL ? step->domain : FVIZ_FEA_STEP_TIME; }
double fviz_fea_step_time_period(const FVizFEAStep* step) { return step != NULL ? step->time_period : 0.0; }
FVizSize fviz_fea_step_frame_count(const FVizFEAStep* step)
{
    return step != NULL && step->frames != NULL ? fviz_array_count(step->frames) : 0u;
}
FVizResult fviz_fea_step_reserve_frames(FVizFEAStep* step, FVizSize count)
{
    return step != NULL ? fviz_array_reserve(step->frames, count) : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizResult fviz_fea_step_add_frame(FVizFEAStep* step, FVizFEAFrame* frame, FVizSize* out_index)
{
    FVizFEAObservedFrame entry;
    FVizSize count;
    if (out_index != NULL) *out_index = 0u;
    if (step == NULL || frame == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    count = fviz_fea_step_frame_count(step);
    if (count != 0u)
    {
        const FVizFEAFrame* last = fviz_fea_step_const_frame(step, count - 1u);
        if (last != NULL && fviz_fea_frame_value(frame) < fviz_fea_frame_value(last))
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "FEA frames must be appended in nondecreasing frame-value order");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
    }
    (void)memset(&entry, 0, sizeof(entry));
    entry.modified_tag = FVIZ_OBSERVER_TAG_INVALID;
    entry.frame = (FVizFEAFrame*)fviz_retain(frame);
    if (entry.frame == NULL) return fviz_last_error_code();
    if (fviz_object_add_observer((FVizObject*)entry.frame, FVIZ_EVENT_MODIFIED, 0.0f,
        fviz_fea_parent_child_modified, step, &entry.modified_tag) != FVIZ_OK ||
        fviz_array_push(step->frames, &entry) != FVIZ_OK)
    {
        fviz_fea_observed_frame_release(&entry);
        return fviz_last_error_code();
    }
    if (out_index != NULL) *out_index = count;
    fviz_object_modified((FVizObject*)step);
    return FVIZ_OK;
}

FVizResult fviz_fea_step_remove_frame(FVizFEAStep* step, FVizSize index)
{
    FVizSize count;
    FVizFEAObservedFrame* entries;
    if (step == NULL || index >= fviz_fea_step_frame_count(step)) return FVIZ_ERROR_INVALID_ARGUMENT;
    count = fviz_array_count(step->frames);
    entries = (FVizFEAObservedFrame*)fviz_array_data(step->frames);
    fviz_fea_observed_frame_release(&entries[index]);
    if (index + 1u < count)
        (void)memmove(&entries[index], &entries[index + 1u], (size_t)(count - index - 1u) * sizeof(*entries));
    if (fviz_array_resize(step->frames, count - 1u) != FVIZ_OK) return fviz_last_error_code();
    fviz_object_modified((FVizObject*)step);
    return FVIZ_OK;
}

FVizFEAFrame* fviz_fea_step_frame(FVizFEAStep* step, FVizSize index)
{
    FVizFEAObservedFrame* entry = step != NULL && index < fviz_fea_step_frame_count(step)
        ? (FVizFEAObservedFrame*)fviz_array_at(step->frames, index) : NULL;
    return entry != NULL ? entry->frame : NULL;
}

const FVizFEAFrame* fviz_fea_step_const_frame(const FVizFEAStep* step, FVizSize index)
{
    const FVizFEAObservedFrame* entry = step != NULL && index < fviz_fea_step_frame_count(step)
        ? (const FVizFEAObservedFrame*)fviz_array_const_at(step->frames, index) : NULL;
    return entry != NULL ? entry->frame : NULL;
}

FVizSize fviz_fea_step_history_region_count(const FVizFEAStep* step)
{
    return step != NULL && step->history_regions != NULL ? fviz_array_count(step->history_regions) : 0u;
}

static FVizSize fviz_fea_step_find_history_region_index(const FVizFEAStep* step, const char* name)
{
    FVizSize i;
    if (step == NULL || name == NULL) return (FVizSize)-1;
    for (i = 0u; i < fviz_fea_step_history_region_count(step); ++i)
    {
        const FVizFEAObservedHistoryRegion* entry = (const FVizFEAObservedHistoryRegion*)fviz_array_const_at(step->history_regions, i);
        if (entry != NULL && entry->region != NULL && strcmp(fviz_fea_history_region_name(entry->region), name) == 0)
            return i;
    }
    return (FVizSize)-1;
}

FVizResult fviz_fea_step_add_history_region(FVizFEAStep* step, FVizFEAHistoryRegion* region)
{
    FVizFEAObservedHistoryRegion entry;
    if (step == NULL || region == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (fviz_fea_step_find_history_region_index(step, fviz_fea_history_region_name(region)) != (FVizSize)-1)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    (void)memset(&entry, 0, sizeof(entry));
    entry.modified_tag = FVIZ_OBSERVER_TAG_INVALID;
    entry.region = (FVizFEAHistoryRegion*)fviz_retain(region);
    if (entry.region == NULL) return fviz_last_error_code();
    if (fviz_object_add_observer((FVizObject*)entry.region, FVIZ_EVENT_MODIFIED, 0.0f,
        fviz_fea_parent_child_modified, step, &entry.modified_tag) != FVIZ_OK ||
        fviz_array_push(step->history_regions, &entry) != FVIZ_OK)
    {
        fviz_fea_observed_history_region_release(&entry);
        return fviz_last_error_code();
    }
    fviz_object_modified((FVizObject*)step);
    return FVIZ_OK;
}

FVizFEAHistoryRegion* fviz_fea_step_history_region(FVizFEAStep* step, const char* name)
{
    const FVizSize index = fviz_fea_step_find_history_region_index(step, name);
    if (index == (FVizSize)-1) return NULL;
    return ((FVizFEAObservedHistoryRegion*)fviz_array_at(step->history_regions, index))->region;
}

const FVizFEAHistoryRegion* fviz_fea_step_const_history_region(const FVizFEAStep* step, const char* name)
{
    const FVizSize index = fviz_fea_step_find_history_region_index(step, name);
    if (index == (FVizSize)-1) return NULL;
    return ((const FVizFEAObservedHistoryRegion*)fviz_array_const_at(step->history_regions, index))->region;
}

FVizFEAHistoryRegion* fviz_fea_step_history_region_at(FVizFEAStep* step, FVizSize index)
{
    FVizFEAObservedHistoryRegion* entry = step != NULL && index < fviz_fea_step_history_region_count(step)
        ? (FVizFEAObservedHistoryRegion*)fviz_array_at(step->history_regions, index) : NULL;
    return entry != NULL ? entry->region : NULL;
}

const FVizFEAHistoryRegion* fviz_fea_step_const_history_region_at(const FVizFEAStep* step, FVizSize index)
{
    const FVizFEAObservedHistoryRegion* entry = step != NULL && index < fviz_fea_step_history_region_count(step)
        ? (const FVizFEAObservedHistoryRegion*)fviz_array_const_at(step->history_regions, index) : NULL;
    return entry != NULL ? entry->region : NULL;
}

FVizResult fviz_fea_step_find_frame_value(
    const FVizFEAStep* step,
    double value,
    FVizSize* out_lower,
    FVizSize* out_upper,
    double* out_alpha)
{
    FVizSize count;
    FVizSize lo;
    FVizSize hi;
    if (out_lower != NULL) *out_lower = 0u;
    if (out_upper != NULL) *out_upper = 0u;
    if (out_alpha != NULL) *out_alpha = 0.0;
    if (step == NULL || out_lower == NULL || out_upper == NULL || out_alpha == NULL || !isfinite(value))
        return FVIZ_ERROR_INVALID_ARGUMENT;
    count = fviz_fea_step_frame_count(step);
    if (count == 0u) return FVIZ_ERROR_NOT_FOUND;
    if (value <= fviz_fea_frame_value(fviz_fea_step_const_frame(step, 0u)))
    {
        *out_lower = 0u; *out_upper = 0u; *out_alpha = 0.0; return FVIZ_OK;
    }
    if (value >= fviz_fea_frame_value(fviz_fea_step_const_frame(step, count - 1u)))
    {
        *out_lower = count - 1u; *out_upper = count - 1u; *out_alpha = 0.0; return FVIZ_OK;
    }
    lo = 0u;
    hi = count - 1u;
    while (hi - lo > 1u)
    {
        const FVizSize mid = lo + (hi - lo) / 2u;
        if (fviz_fea_frame_value(fviz_fea_step_const_frame(step, mid)) <= value) lo = mid;
        else hi = mid;
    }
    {
        const double a = fviz_fea_frame_value(fviz_fea_step_const_frame(step, lo));
        const double b = fviz_fea_frame_value(fviz_fea_step_const_frame(step, hi));
        *out_lower = lo;
        *out_upper = hi;
        *out_alpha = b != a ? (value - a) / (b - a) : 0.0;
    }
    return FVIZ_OK;
}

FVizResult fviz_fea_result_database_create(FVizFEAResultDatabase** out_database)
{
    FVizFEAResultDatabase* database;
    if (out_database == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_database = NULL;
    database = (FVizFEAResultDatabase*)fviz_internal_object_allocate(
        sizeof(*database), &g_fviz_fea_result_database_class, NULL);
    if (database == NULL) return fviz_last_error_code();
    if (fviz_array_create(sizeof(FVizFEAObservedStep), &database->steps) != FVIZ_OK)
    {
        fviz_release(database);
        return fviz_last_error_code();
    }
    *out_database = database;
    return FVIZ_OK;
}

FVizSize fviz_fea_result_database_step_count(const FVizFEAResultDatabase* database)
{
    return database != NULL && database->steps != NULL ? fviz_array_count(database->steps) : 0u;
}

static FVizSize fviz_fea_result_database_find_step_index(const FVizFEAResultDatabase* database, const char* name)
{
    FVizSize i;
    if (database == NULL || name == NULL) return (FVizSize)-1;
    for (i = 0u; i < fviz_fea_result_database_step_count(database); ++i)
    {
        const FVizFEAObservedStep* entry = (const FVizFEAObservedStep*)fviz_array_const_at(database->steps, i);
        if (entry != NULL && entry->step != NULL && strcmp(fviz_fea_step_name(entry->step), name) == 0)
            return i;
    }
    return (FVizSize)-1;
}

FVizResult fviz_fea_result_database_add_step(
    FVizFEAResultDatabase* database, FVizFEAStep* step, FVizSize* out_index)
{
    FVizFEAObservedStep entry;
    FVizSize count;
    if (out_index != NULL) *out_index = 0u;
    if (database == NULL || step == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (fviz_fea_result_database_find_step_index(database, fviz_fea_step_name(step)) != (FVizSize)-1)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "FEA result database already contains this step name");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    count = fviz_fea_result_database_step_count(database);
    (void)memset(&entry, 0, sizeof(entry));
    entry.modified_tag = FVIZ_OBSERVER_TAG_INVALID;
    entry.step = (FVizFEAStep*)fviz_retain(step);
    if (entry.step == NULL) return fviz_last_error_code();
    if (fviz_object_add_observer((FVizObject*)entry.step, FVIZ_EVENT_MODIFIED, 0.0f,
        fviz_fea_parent_child_modified, database, &entry.modified_tag) != FVIZ_OK ||
        fviz_array_push(database->steps, &entry) != FVIZ_OK)
    {
        fviz_fea_observed_step_release(&entry);
        return fviz_last_error_code();
    }
    if (out_index != NULL) *out_index = count;
    fviz_object_modified((FVizObject*)database);
    return FVIZ_OK;
}

FVizResult fviz_fea_result_database_remove_step(FVizFEAResultDatabase* database, const char* name)
{
    FVizSize index;
    FVizSize count;
    FVizFEAObservedStep* entries;
    if (database == NULL || name == NULL || name[0] == '\0') return FVIZ_ERROR_INVALID_ARGUMENT;
    index = fviz_fea_result_database_find_step_index(database, name);
    if (index == (FVizSize)-1) return FVIZ_ERROR_NOT_FOUND;
    count = fviz_array_count(database->steps);
    entries = (FVizFEAObservedStep*)fviz_array_data(database->steps);
    fviz_fea_observed_step_release(&entries[index]);
    if (index + 1u < count)
        (void)memmove(&entries[index], &entries[index + 1u], (size_t)(count - index - 1u) * sizeof(*entries));
    if (fviz_array_resize(database->steps, count - 1u) != FVIZ_OK) return fviz_last_error_code();
    fviz_object_modified((FVizObject*)database);
    return FVIZ_OK;
}

FVizFEAStep* fviz_fea_result_database_step(FVizFEAResultDatabase* database, const char* name)
{
    const FVizSize index = fviz_fea_result_database_find_step_index(database, name);
    if (index == (FVizSize)-1) return NULL;
    return ((FVizFEAObservedStep*)fviz_array_at(database->steps, index))->step;
}

const FVizFEAStep* fviz_fea_result_database_const_step(const FVizFEAResultDatabase* database, const char* name)
{
    const FVizSize index = fviz_fea_result_database_find_step_index(database, name);
    if (index == (FVizSize)-1) return NULL;
    return ((const FVizFEAObservedStep*)fviz_array_const_at(database->steps, index))->step;
}

FVizFEAStep* fviz_fea_result_database_step_at(FVizFEAResultDatabase* database, FVizSize index)
{
    FVizFEAObservedStep* entry = database != NULL && index < fviz_fea_result_database_step_count(database)
        ? (FVizFEAObservedStep*)fviz_array_at(database->steps, index) : NULL;
    return entry != NULL ? entry->step : NULL;
}

const FVizFEAStep* fviz_fea_result_database_const_step_at(const FVizFEAResultDatabase* database, FVizSize index)
{
    const FVizFEAObservedStep* entry = database != NULL && index < fviz_fea_result_database_step_count(database)
        ? (const FVizFEAObservedStep*)fviz_array_const_at(database->steps, index) : NULL;
    return entry != NULL ? entry->step : NULL;
}
