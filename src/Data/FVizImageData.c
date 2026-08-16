#include <math.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizImageData.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Data/FVizDataObjectPrivate.h>
#include <FViz/Data/FVizImageDataPrivate.h>

static void fviz_image_data_destroy(FVizObject* object);
static FVizMTime fviz_image_data_mtime(const FVizObject* object);
static const FVizObjectClass g_fviz_image_data_class = {
    FVIZ_TYPE_IMAGE_DATA, "FVizImageData", &g_fviz_data_object_class, fviz_image_data_destroy, fviz_image_data_mtime};

static FVizBool fviz_image_data_dependency_modified(FVizObject* caller, FVizEventId event_id, void* call_data,
                                                    void* client_data)
{
    FVizImageData* image = (FVizImageData*)client_data;
    (void)caller;
    (void)event_id;
    (void)call_data;
    if (image != NULL && image->dependency_suppression == 0u) fviz_object_modified((FVizObject*)image);
    return FVIZ_FALSE;
}

static FVizMTime fviz_image_data_mtime(const FVizObject* object)
{
    /* The owned dataset is observed, making aggregate MTime O(1). */
    return fviz_internal_object_local_mtime(object);
}

static void fviz_image_data_destroy(FVizObject* object)
{
    FVizImageData* image = (FVizImageData*)object;
    if (image->data_set != NULL && image->data_set_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)image->data_set, image->data_set_modified_tag);
    fviz_release(image->data_set);
    image->data_set = NULL;
}

static void fviz_image_identity_direction(double direction[9])
{
    (void)memset(direction, 0, 9u * sizeof(double));
    direction[0] = 1.0;
    direction[4] = 1.0;
    direction[8] = 1.0;
}

static FVizBool fviz_image_finite3(const double values[3])
{
    return values != NULL && isfinite(values[0]) && isfinite(values[1]) && isfinite(values[2]) ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizResult fviz_image_inverse3(const double m[9], double out[9])
{
    const double a = m[0], b = m[1], c = m[2];
    const double d = m[3], e = m[4], f = m[5];
    const double g = m[6], h = m[7], i = m[8];
    const double det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    if (!isfinite(det) || fabs(det) <= 1.0e-15)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "image direction matrix must be finite and invertible");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    {
        const double inv = 1.0 / det;
        out[0] = (e * i - f * h) * inv;
        out[1] = (c * h - b * i) * inv;
        out[2] = (b * f - c * e) * inv;
        out[3] = (f * g - d * i) * inv;
        out[4] = (a * i - c * g) * inv;
        out[5] = (c * d - a * f) * inv;
        out[6] = (d * h - e * g) * inv;
        out[7] = (b * g - a * h) * inv;
        out[8] = (a * e - b * d) * inv;
    }
    return FVIZ_OK;
}

static FVizResult fviz_image_extent_dimensions(const int64_t extent[6], FVizSize dims[3])
{
    uint32_t axis;
    for (axis = 0u; axis < 3u; ++axis)
    {
        const int64_t minimum = extent[axis * 2u];
        const int64_t maximum = extent[axis * 2u + 1u];
        uint64_t width;
        if (maximum < minimum)
        {
            dims[0] = dims[1] = dims[2] = 0u;
            return FVIZ_OK;
        }
        width = (uint64_t)maximum - (uint64_t)minimum + UINT64_C(1);
        if (width == 0u || width > (uint64_t)((FVizSize)-1))
        {
            fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "image extent dimension exceeds FVizSize");
            return FVIZ_ERROR_OVERFLOW;
        }
        dims[axis] = (FVizSize)width;
    }
    return FVIZ_OK;
}

static FVizResult fviz_image_counts(const int64_t extent[6], FVizSize* out_points, FVizSize* out_cells)
{
    FVizSize dims[3] = {0u, 0u, 0u};
    FVizSize points;
    FVizSize cells = 1u;
    uint32_t active_axes = 0u;
    uint32_t axis;
    if (fviz_image_extent_dimensions(extent, dims) != FVIZ_OK) return fviz_last_error_code();
    if (dims[0] == 0u || dims[1] == 0u || dims[2] == 0u)
    {
        *out_points = 0u;
        *out_cells = 0u;
        return FVIZ_OK;
    }
    if (fviz_size_multiply(dims[0], dims[1], &points) != FVIZ_OK ||
        fviz_size_multiply(points, dims[2], &points) != FVIZ_OK)
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "image point count overflow");
        return FVIZ_ERROR_OVERFLOW;
    }
    for (axis = 0u; axis < 3u; ++axis)
    {
        if (dims[axis] > 1u)
        {
            FVizSize next;
            ++active_axes;
            if (fviz_size_multiply(cells, dims[axis] - 1u, &next) != FVIZ_OK)
            {
                fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "image cell count overflow");
                return FVIZ_ERROR_OVERFLOW;
            }
            cells = next;
        }
    }
    if (active_axes == 0u) cells = 1u;
    *out_points = points;
    *out_cells = cells;
    return FVIZ_OK;
}

FVizResult fviz_image_data_create(FVizImageData** out_image)
{
    FVizImageData* image;
    if (out_image == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_image must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_image = NULL;
    image = (FVizImageData*)fviz_internal_object_allocate(sizeof(FVizImageData), &g_fviz_image_data_class, NULL);
    if (image == NULL) return fviz_last_error_code();
    if (fviz_data_set_create(&image->data_set) != FVIZ_OK)
    {
        fviz_release(image);
        return fviz_last_error_code();
    }
    image->dependency_suppression = 0u;
    if (fviz_object_add_observer((FVizObject*)image->data_set, FVIZ_EVENT_MODIFIED, 0.0f,
                                 fviz_image_data_dependency_modified, image, &image->data_set_modified_tag) != FVIZ_OK)
    {
        fviz_release(image);
        return fviz_last_error_code();
    }
    image->extent[0] = 0;
    image->extent[1] = -1;
    image->extent[2] = 0;
    image->extent[3] = -1;
    image->extent[4] = 0;
    image->extent[5] = -1;
    image->origin[0] = image->origin[1] = image->origin[2] = 0.0;
    image->spacing[0] = image->spacing[1] = image->spacing[2] = 1.0;
    fviz_image_identity_direction(image->direction);
    fviz_image_identity_direction(image->inverse_direction);
    *out_image = image;
    return FVIZ_OK;
}

void fviz_image_data_clear(FVizImageData* image)
{
    const int64_t empty[6] = {0, -1, 0, -1, 0, -1};
    FVizBool changed;
    if (image == NULL) return;
    changed = (memcmp(image->extent, empty, sizeof(empty)) != 0 ||
               fviz_attribute_set_count(fviz_data_set_point_data(image->data_set)) != 0u ||
               fviz_attribute_set_count(fviz_data_set_cell_data(image->data_set)) != 0u ||
               fviz_attribute_set_count(fviz_data_set_field_data(image->data_set)) != 0u)
                  ? FVIZ_TRUE
                  : FVIZ_FALSE;
    ++image->dependency_suppression;
    fviz_attribute_set_clear(fviz_data_set_point_data(image->data_set));
    fviz_attribute_set_clear(fviz_data_set_cell_data(image->data_set));
    fviz_attribute_set_clear(fviz_data_set_field_data(image->data_set));
    (void)memcpy(image->extent, empty, sizeof(empty));
    (void)fviz_data_set_set_point_count(image->data_set, 0u);
    (void)fviz_data_set_set_cell_count(image->data_set, 0u);
    --image->dependency_suppression;
    if (changed != FVIZ_FALSE) fviz_object_modified((FVizObject*)image);
}

static FVizBool fviz_image_attributes_accept_count(const FVizAttributeSet* attributes, FVizSize count)
{
    FVizSize i;
    for (i = 0u; i < fviz_attribute_set_count(attributes); ++i)
    {
        const FVizDataArray* array = fviz_attribute_set_const_array_at(attributes, i);
        const FVizSize tuples = fviz_data_array_tuple_count(array);
        if (tuples != 0u && tuples != count) return FVIZ_FALSE;
    }
    return FVIZ_TRUE;
}

FVizResult fviz_image_data_set_extent(FVizImageData* image, const int64_t extent[6])
{
    FVizSize points;
    FVizSize cells;
    if (image == NULL || extent == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "image and extent must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_image_counts(extent, &points, &cells) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_image_attributes_accept_count(fviz_data_set_point_data(image->data_set), points) == FVIZ_FALSE ||
        fviz_image_attributes_accept_count(fviz_data_set_cell_data(image->data_set), cells) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE,
                                "image extent conflicts with existing attribute tuple counts");
        return FVIZ_ERROR_INVALID_STATE;
    }
    ++image->dependency_suppression;
    if (fviz_data_set_set_point_count(image->data_set, points) != FVIZ_OK ||
        fviz_data_set_set_cell_count(image->data_set, cells) != FVIZ_OK)
    {
        --image->dependency_suppression;
        return fviz_last_error_code();
    }
    --image->dependency_suppression;
    if (memcmp(image->extent, extent, 6u * sizeof(int64_t)) != 0)
    {
        (void)memcpy(image->extent, extent, 6u * sizeof(int64_t));
        fviz_object_modified((FVizObject*)image);
    }
    return FVIZ_OK;
}

void fviz_image_data_extent(const FVizImageData* image, int64_t out_extent[6])
{
    if (out_extent == NULL) return;
    if (image == NULL)
    {
        const int64_t e[6] = {0, -1, 0, -1, 0, -1};
        (void)memcpy(out_extent, e, sizeof(e));
        return;
    }
    (void)memcpy(out_extent, image->extent, 6u * sizeof(int64_t));
}

void fviz_image_data_dimensions(const FVizImageData* image, FVizSize out_dimensions[3])
{
    if (out_dimensions == NULL) return;
    out_dimensions[0] = out_dimensions[1] = out_dimensions[2] = 0u;
    if (image != NULL) (void)fviz_image_extent_dimensions(image->extent, out_dimensions);
}

uint32_t fviz_image_data_dimension(const FVizImageData* image)
{
    FVizSize dims[3];
    uint32_t dimension = 0u;
    uint32_t axis;
    fviz_image_data_dimensions(image, dims);
    for (axis = 0u; axis < 3u; ++axis)
        if (dims[axis] > 1u) ++dimension;
    return dimension;
}

FVizResult fviz_image_data_set_origin(FVizImageData* image, const double origin[3])
{
    if (image == NULL || fviz_image_finite3(origin) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "image origin must be finite");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (memcmp(image->origin, origin, 3u * sizeof(double)) != 0)
    {
        (void)memcpy(image->origin, origin, 3u * sizeof(double));
        fviz_object_modified((FVizObject*)image);
    }
    return FVIZ_OK;
}

void fviz_image_data_origin(const FVizImageData* image, double out_origin[3])
{
    if (out_origin == NULL) return;
    if (image == NULL) out_origin[0] = out_origin[1] = out_origin[2] = 0.0;
    else
        (void)memcpy(out_origin, image->origin, 3u * sizeof(double));
}

FVizResult fviz_image_data_set_spacing(FVizImageData* image, const double spacing[3])
{
    if (image == NULL || fviz_image_finite3(spacing) == FVIZ_FALSE || spacing[0] == 0.0 || spacing[1] == 0.0 ||
        spacing[2] == 0.0)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "image spacing must be finite and non-zero");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (memcmp(image->spacing, spacing, 3u * sizeof(double)) != 0)
    {
        (void)memcpy(image->spacing, spacing, 3u * sizeof(double));
        fviz_object_modified((FVizObject*)image);
    }
    return FVIZ_OK;
}

void fviz_image_data_spacing(const FVizImageData* image, double out_spacing[3])
{
    if (out_spacing == NULL) return;
    if (image == NULL) out_spacing[0] = out_spacing[1] = out_spacing[2] = 1.0;
    else
        (void)memcpy(out_spacing, image->spacing, 3u * sizeof(double));
}

FVizResult fviz_image_data_set_direction(FVizImageData* image, const double direction[9])
{
    double inverse[9];
    uint32_t i;
    if (image == NULL || direction == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "image and direction must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    for (i = 0u; i < 9u; ++i)
        if (!isfinite(direction[i]))
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "image direction must be finite");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
    if (fviz_image_inverse3(direction, inverse) != FVIZ_OK) return fviz_last_error_code();
    if (memcmp(image->direction, direction, 9u * sizeof(double)) != 0)
    {
        (void)memcpy(image->direction, direction, 9u * sizeof(double));
        (void)memcpy(image->inverse_direction, inverse, 9u * sizeof(double));
        fviz_object_modified((FVizObject*)image);
    }
    return FVIZ_OK;
}

void fviz_image_data_direction(const FVizImageData* image, double out_direction[9])
{
    if (out_direction == NULL) return;
    if (image == NULL) fviz_image_identity_direction(out_direction);
    else
        (void)memcpy(out_direction, image->direction, 9u * sizeof(double));
}

FVizSize fviz_image_data_point_count(const FVizImageData* image)
{
    return image != NULL ? fviz_data_set_point_count(image->data_set) : 0u;
}

FVizSize fviz_image_data_cell_count(const FVizImageData* image)
{
    return image != NULL ? fviz_data_set_cell_count(image->data_set) : 0u;
}

FVizCellType fviz_image_data_cell_type(const FVizImageData* image)
{
    switch (fviz_image_data_dimension(image))
    {
        case 0u:
            return FVIZ_CELL_VERTEX;
        case 1u:
            return FVIZ_CELL_LINE;
        case 2u:
            return FVIZ_CELL_QUAD;
        case 3u:
            return FVIZ_CELL_HEXAHEDRON;
        default:
            return (FVizCellType)0;
    }
}

FVizResult fviz_image_data_index_to_physical(const FVizImageData* image, const double index[3], double out_physical[3])
{
    double scaled[3];
    if (image == NULL || index == NULL || out_physical == NULL || fviz_image_finite3(index) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "index-to-physical arguments are invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    scaled[0] = index[0] * image->spacing[0];
    scaled[1] = index[1] * image->spacing[1];
    scaled[2] = index[2] * image->spacing[2];
    out_physical[0] = image->origin[0] + image->direction[0] * scaled[0] + image->direction[1] * scaled[1] +
                      image->direction[2] * scaled[2];
    out_physical[1] = image->origin[1] + image->direction[3] * scaled[0] + image->direction[4] * scaled[1] +
                      image->direction[5] * scaled[2];
    out_physical[2] = image->origin[2] + image->direction[6] * scaled[0] + image->direction[7] * scaled[1] +
                      image->direction[8] * scaled[2];
    return FVIZ_OK;
}

FVizResult fviz_image_data_physical_to_continuous_index(const FVizImageData* image, const double physical[3],
                                                        double out_index[3])
{
    double delta[3];
    double local[3];
    if (image == NULL || physical == NULL || out_index == NULL || fviz_image_finite3(physical) == FVIZ_FALSE)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "physical-to-index arguments are invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    delta[0] = physical[0] - image->origin[0];
    delta[1] = physical[1] - image->origin[1];
    delta[2] = physical[2] - image->origin[2];
    local[0] = image->inverse_direction[0] * delta[0] + image->inverse_direction[1] * delta[1] +
               image->inverse_direction[2] * delta[2];
    local[1] = image->inverse_direction[3] * delta[0] + image->inverse_direction[4] * delta[1] +
               image->inverse_direction[5] * delta[2];
    local[2] = image->inverse_direction[6] * delta[0] + image->inverse_direction[7] * delta[1] +
               image->inverse_direction[8] * delta[2];
    out_index[0] = local[0] / image->spacing[0];
    out_index[1] = local[1] / image->spacing[1];
    out_index[2] = local[2] / image->spacing[2];
    return FVIZ_OK;
}

FVizResult fviz_image_data_point_id(const FVizImageData* image, int64_t i, int64_t j, int64_t k, FVizId* out_point_id)
{
    FVizSize dims[3];
    FVizSize x, y, z, plane, id;
    if (out_point_id != NULL) *out_point_id = FVIZ_INVALID_ID;
    if (image == NULL || out_point_id == NULL || i < image->extent[0] || i > image->extent[1] || j < image->extent[2] ||
        j > image->extent[3] || k < image->extent[4] || k > image->extent[5])
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "structured point index is outside image extent");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    fviz_image_data_dimensions(image, dims);
    x = (FVizSize)(i - image->extent[0]);
    y = (FVizSize)(j - image->extent[2]);
    z = (FVizSize)(k - image->extent[4]);
    if (fviz_size_multiply(dims[0], dims[1], &plane) != FVIZ_OK || fviz_size_multiply(z, plane, &id) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    id += y * dims[0] + x;
    *out_point_id = (FVizId)id;
    return FVIZ_OK;
}

FVizResult fviz_image_data_point_ijk(const FVizImageData* image, FVizId point_id, int64_t out_ijk[3])
{
    FVizSize dims[3];
    FVizSize plane;
    FVizSize id;
    if (image == NULL || out_ijk == NULL || point_id >= (FVizId)fviz_image_data_point_count(image))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "image point ID is out of range");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    fviz_image_data_dimensions(image, dims);
    if (fviz_size_multiply(dims[0], dims[1], &plane) != FVIZ_OK || plane == 0u) return FVIZ_ERROR_INVALID_STATE;
    id = (FVizSize)point_id;
    out_ijk[2] = image->extent[4] + (int64_t)(id / plane);
    id %= plane;
    out_ijk[1] = image->extent[2] + (int64_t)(id / dims[0]);
    out_ijk[0] = image->extent[0] + (int64_t)(id % dims[0]);
    return FVIZ_OK;
}

FVizResult fviz_image_data_point(const FVizImageData* image, FVizId point_id, FVizVec3* out_point)
{
    int64_t ijk[3];
    double index[3];
    double physical[3];
    if (out_point == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "point output must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_image_data_point_ijk(image, point_id, ijk) != FVIZ_OK) return fviz_last_error_code();
    index[0] = (double)ijk[0];
    index[1] = (double)ijk[1];
    index[2] = (double)ijk[2];
    if (fviz_image_data_index_to_physical(image, index, physical) != FVIZ_OK) return fviz_last_error_code();
    out_point->x = (float)physical[0];
    out_point->y = (float)physical[1];
    out_point->z = (float)physical[2];
    return FVIZ_OK;
}

static void fviz_image_cell_dimensions(const FVizImageData* image, FVizSize cell_dims[3])
{
    FVizSize dims[3];
    uint32_t axis;
    fviz_image_data_dimensions(image, dims);
    for (axis = 0u; axis < 3u; ++axis)
        cell_dims[axis] = dims[axis] > 1u ? dims[axis] - 1u : (dims[axis] == 1u ? 1u : 0u);
}

FVizResult fviz_image_data_cell_id(const FVizImageData* image, int64_t i, int64_t j, int64_t k, FVizId* out_cell_id)
{
    FVizSize dims[3];
    FVizSize cell_dims[3];
    const int64_t ijk[3] = {i, j, k};
    FVizSize local[3];
    FVizSize plane;
    FVizSize id;
    uint32_t axis;
    if (out_cell_id != NULL) *out_cell_id = FVIZ_INVALID_ID;
    if (image == NULL || out_cell_id == NULL || fviz_image_data_cell_count(image) == 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "image cell lookup is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    fviz_image_data_dimensions(image, dims);
    fviz_image_cell_dimensions(image, cell_dims);
    for (axis = 0u; axis < 3u; ++axis)
    {
        const int64_t minimum = image->extent[axis * 2u];
        const int64_t maximum_cell_index = dims[axis] > 1u ? image->extent[axis * 2u + 1u] - 1 : minimum;
        if (ijk[axis] < minimum || ijk[axis] > maximum_cell_index)
        {
            fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "structured cell index is outside image extent");
            return FVIZ_ERROR_INVALID_ARGUMENT;
        }
        local[axis] = (FVizSize)(ijk[axis] - minimum);
    }
    if (fviz_size_multiply(cell_dims[0], cell_dims[1], &plane) != FVIZ_OK ||
        fviz_size_multiply(local[2], plane, &id) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    id += local[1] * cell_dims[0] + local[0];
    *out_cell_id = (FVizId)id;
    return FVIZ_OK;
}

FVizResult fviz_image_data_cell_ijk(const FVizImageData* image, FVizId cell_id, int64_t out_ijk[3])
{
    FVizSize cell_dims[3];
    FVizSize plane;
    FVizSize id;
    if (image == NULL || out_ijk == NULL || cell_id >= (FVizId)fviz_image_data_cell_count(image))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "image cell ID is out of range");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    fviz_image_cell_dimensions(image, cell_dims);
    if (fviz_size_multiply(cell_dims[0], cell_dims[1], &plane) != FVIZ_OK || plane == 0u)
        return FVIZ_ERROR_INVALID_STATE;
    id = (FVizSize)cell_id;
    out_ijk[2] = image->extent[4] + (int64_t)(id / plane);
    id %= plane;
    out_ijk[1] = image->extent[2] + (int64_t)(id / cell_dims[0]);
    out_ijk[0] = image->extent[0] + (int64_t)(id % cell_dims[0]);
    return FVIZ_OK;
}

FVizResult fviz_image_data_cell_point_ids(const FVizImageData* image, FVizId cell_id, FVizId out_point_ids[8],
                                          uint32_t* out_point_count)
{
    int64_t base[3];
    FVizSize dims[3];
    uint32_t active[3];
    uint32_t active_count = 0u;
    uint32_t count;
    uint32_t corner;
    uint32_t axis;
    if (out_point_count != NULL) *out_point_count = 0u;
    if (image == NULL || out_point_ids == NULL || out_point_count == NULL ||
        fviz_image_data_cell_ijk(image, cell_id, base) != FVIZ_OK)
        return fviz_last_error_code();
    fviz_image_data_dimensions(image, dims);
    for (axis = 0u; axis < 3u; ++axis)
        if (dims[axis] > 1u) active[active_count++] = axis;
    count = active_count == 0u ? 1u : (1u << active_count);
    for (corner = 0u; corner < count; ++corner)
    {
        int64_t ijk[3] = {base[0], base[1], base[2]};
        uint32_t bit;
        /* VTK-compatible winding for 2D/3D cells: swap the last two binary corners. */
        uint32_t logical_corner = corner;
        if (active_count >= 2u && (corner & 3u) == 2u) logical_corner = corner + 1u;
        else if (active_count >= 2u && (corner & 3u) == 3u)
            logical_corner = corner - 1u;
        for (bit = 0u; bit < active_count; ++bit)
            if ((logical_corner & (1u << bit)) != 0u) ++ijk[active[bit]];
        if (fviz_image_data_point_id(image, ijk[0], ijk[1], ijk[2], &out_point_ids[corner]) != FVIZ_OK)
            return fviz_last_error_code();
    }
    *out_point_count = count;
    return FVIZ_OK;
}

FVizBounds fviz_image_data_bounds(const FVizImageData* image)
{
    FVizBounds bounds = fviz_bounds_empty();
    uint32_t corner;
    if (image == NULL || fviz_image_data_point_count(image) == 0u) return bounds;
    for (corner = 0u; corner < 8u; ++corner)
    {
        double index[3] = {(double)image->extent[(corner & 1u) ? 1u : 0u],
                           (double)image->extent[(corner & 2u) ? 3u : 2u],
                           (double)image->extent[(corner & 4u) ? 5u : 4u]};
        double physical[3];
        FVizVec3 p;
        if (fviz_image_data_index_to_physical(image, index, physical) != FVIZ_OK) return fviz_bounds_empty();
        p.x = (float)physical[0];
        p.y = (float)physical[1];
        p.z = (float)physical[2];
        fviz_bounds_include_point(&bounds, p);
    }
    return bounds;
}

static FVizResult fviz_image_sample_array_impl(const FVizImageData* image, const FVizDataArray* array,
                                               const double physical[3], uint32_t component, double* out_value)
{
    double index[3];
    FVizSize dims[3];
    int64_t lower[3];
    double t[3];
    uint32_t axis;
    uint32_t active[3];
    uint32_t active_count = 0u;
    uint32_t corner;
    double value = 0.0;
    if (out_value != NULL) *out_value = 0.0;
    if (image == NULL || array == NULL || physical == NULL || out_value == NULL ||
        component >= fviz_data_array_components(array) ||
        fviz_data_array_tuple_count(array) != fviz_image_data_point_count(image))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
                                "image sampling arguments or array association are invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_image_data_physical_to_continuous_index(image, physical, index) != FVIZ_OK) return fviz_last_error_code();
    fviz_image_data_dimensions(image, dims);
    for (axis = 0u; axis < 3u; ++axis)
    {
        const double minimum = (double)image->extent[axis * 2u];
        const double maximum = (double)image->extent[axis * 2u + 1u];
        const double epsilon = 1.0e-10 * (fabs(maximum - minimum) + 1.0);
        if (index[axis] < minimum - epsilon || index[axis] > maximum + epsilon)
        {
            fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "sample position lies outside image extent");
            return FVIZ_ERROR_NOT_FOUND;
        }
        if (dims[axis] <= 1u)
        {
            lower[axis] = image->extent[axis * 2u];
            t[axis] = 0.0;
        }
        else if (index[axis] >= maximum)
        {
            lower[axis] = image->extent[axis * 2u + 1u] - 1;
            t[axis] = 1.0;
            active[active_count++] = axis;
        }
        else
        {
            const double floored = floor(index[axis]);
            lower[axis] = (int64_t)floored;
            t[axis] = index[axis] - floored;
            active[active_count++] = axis;
        }
    }
    for (corner = 0u; corner < (1u << active_count); ++corner)
    {
        int64_t ijk[3] = {lower[0], lower[1], lower[2]};
        double weight = 1.0;
        FVizId point_id;
        double sample;
        uint32_t bit;
        for (bit = 0u; bit < active_count; ++bit)
        {
            const uint32_t a = active[bit];
            if ((corner & (1u << bit)) != 0u)
            {
                ++ijk[a];
                weight *= t[a];
            }
            else
                weight *= 1.0 - t[a];
        }
        if (weight == 0.0) continue;
        if (fviz_image_data_point_id(image, ijk[0], ijk[1], ijk[2], &point_id) != FVIZ_OK ||
            fviz_data_array_get_component(array, (FVizSize)point_id, component, &sample) != FVIZ_OK)
            return fviz_last_error_code();
        value += weight * sample;
    }
    *out_value = value;
    return FVIZ_OK;
}

FVizResult fviz_image_data_sample_point_array(const FVizImageData* image, const char* array_name,
                                              const double physical[3], uint32_t component, double* out_value)
{
    const FVizDataArray* array;
    if (image == NULL || array_name == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "image and point-array name must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    array = fviz_attribute_set_const_get(fviz_image_data_const_point_data(image), array_name);
    if (array == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "image point array was not found");
        return FVIZ_ERROR_NOT_FOUND;
    }
    return fviz_image_sample_array_impl(image, array, physical, component, out_value);
}

FVizResult fviz_image_data_sample_active_scalars(const FVizImageData* image, const double physical[3],
                                                 uint32_t component, double* out_value)
{
    const FVizDataArray* array =
        image != NULL ? fviz_attribute_set_const_active(fviz_image_data_const_point_data(image), FVIZ_ATTRIBUTE_SCALARS)
                      : NULL;
    if (array == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_NOT_FOUND, "image has no active point scalars");
        return FVIZ_ERROR_NOT_FOUND;
    }
    return fviz_image_sample_array_impl(image, array, physical, component, out_value);
}

FVizAttributeSet* fviz_image_data_point_data(FVizImageData* image)
{
    return image != NULL ? fviz_data_set_point_data(image->data_set) : NULL;
}

FVizAttributeSet* fviz_image_data_cell_data(FVizImageData* image)
{
    return image != NULL ? fviz_data_set_cell_data(image->data_set) : NULL;
}

FVizAttributeSet* fviz_image_data_field_data(FVizImageData* image)
{
    return image != NULL ? fviz_data_set_field_data(image->data_set) : NULL;
}

const FVizAttributeSet* fviz_image_data_const_point_data(const FVizImageData* image)
{
    return image != NULL ? fviz_data_set_point_data(image->data_set) : NULL;
}

const FVizAttributeSet* fviz_image_data_const_cell_data(const FVizImageData* image)
{
    return image != NULL ? fviz_data_set_cell_data(image->data_set) : NULL;
}

const FVizAttributeSet* fviz_image_data_const_field_data(const FVizImageData* image)
{
    return image != NULL ? fviz_data_set_field_data(image->data_set) : NULL;
}

static FVizResult fviz_image_allocate_scalars(FVizImageData* image, FVizAttributeSet* attributes, FVizSize tuples,
                                              const char* name, FVizDataType type, uint32_t components,
                                              FVizDataArray** out_array)
{
    FVizDataArray* array = NULL;
    if (out_array != NULL) *out_array = NULL;
    if (image == NULL || attributes == NULL || name == NULL || name[0] == '\0' || components == 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "image scalar allocation arguments are invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    ++image->dependency_suppression;
    if (fviz_data_array_create(type, components, &array) != FVIZ_OK ||
        fviz_data_array_resize(array, tuples) != FVIZ_OK ||
        fviz_attribute_set_add(attributes, name, array) != FVIZ_OK ||
        fviz_attribute_set_set_active(attributes, FVIZ_ATTRIBUTE_SCALARS, name) != FVIZ_OK)
    {
        --image->dependency_suppression;
        fviz_release(array);
        return fviz_last_error_code();
    }
    --image->dependency_suppression;
    if (out_array != NULL) *out_array = (FVizDataArray*)fviz_retain(array);
    fviz_release(array);
    fviz_object_modified((FVizObject*)image);
    return FVIZ_OK;
}

FVizResult fviz_image_data_allocate_point_scalars(FVizImageData* image, const char* name, FVizDataType type,
                                                  uint32_t components, FVizDataArray** out_array)
{
    return fviz_image_allocate_scalars(image, fviz_image_data_point_data(image), fviz_image_data_point_count(image),
                                       name, type, components, out_array);
}

FVizResult fviz_image_data_allocate_cell_scalars(FVizImageData* image, const char* name, FVizDataType type,
                                                 uint32_t components, FVizDataArray** out_array)
{
    return fviz_image_allocate_scalars(image, fviz_image_data_cell_data(image), fviz_image_data_cell_count(image), name,
                                       type, components, out_array);
}

FVizResult fviz_image_data_validate(const FVizImageData* image)
{
    FVizSize points, cells;
    double inverse[9];
    if (image == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "image must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_image_counts(image->extent, &points, &cells) != FVIZ_OK) return fviz_last_error_code();
    if (points != fviz_data_set_point_count(image->data_set) || cells != fviz_data_set_cell_count(image->data_set))
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "image extent and dataset counts differ");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (fviz_image_finite3(image->origin) == FVIZ_FALSE || fviz_image_finite3(image->spacing) == FVIZ_FALSE ||
        image->spacing[0] == 0.0 || image->spacing[1] == 0.0 || image->spacing[2] == 0.0)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "image origin/spacing is invalid");
        return FVIZ_ERROR_INVALID_STATE;
    }
    if (fviz_image_inverse3(image->direction, inverse) != FVIZ_OK) return fviz_last_error_code();
    return fviz_data_set_validate(image->data_set);
}
