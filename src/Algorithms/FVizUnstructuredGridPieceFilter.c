#include <stdint.h>
#include <string.h>

#include <FViz/Algorithms/FVizUnstructuredGridPieceFilter.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizHashMap.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Data/FVizGhost.h>
#include <FViz/Mesh/FVizCellAdjacency.h>
#include <FViz/Mesh/FVizCellLinks.h>
#include <FViz/Pipeline/FVizExecutive.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

struct FVizUnstructuredGridPieceFilter
{
    FVizObject base;
    FVizAlgorithm* algorithm;
    FVizCellArray* adjacency_cells;
    FVizCellAdjacency* adjacency_cache;
    FVizMTime adjacency_cells_mtime;
    FVizCellArray* links_cells;
    FVizCellLinks* links_cache;
    FVizMTime links_cells_mtime;
    uint32_t* point_owner_cache;
    FVizSize point_owner_count;
    uint32_t point_owner_piece_count;
    FVizMTime point_owner_cells_mtime;
};

static void fviz_unstructured_piece_destroy(FVizObject* object)
{
    FVizUnstructuredGridPieceFilter* filter = (FVizUnstructuredGridPieceFilter*)object;
    fviz_free(filter->point_owner_cache);
    fviz_release(filter->links_cache);
    fviz_release(filter->links_cells);
    fviz_release(filter->adjacency_cache);
    fviz_release(filter->adjacency_cells);
    fviz_release(filter->algorithm);
    filter->point_owner_cache = NULL;
    filter->point_owner_count = 0u;
    filter->point_owner_piece_count = 0u;
    filter->links_cache = NULL;
    filter->links_cells = NULL;
    filter->adjacency_cache = NULL;
    filter->adjacency_cells = NULL;
    filter->algorithm = NULL;
}

static const FVizObjectClass g_fviz_unstructured_piece_class = {FVIZ_TYPE_UNSTRUCTURED_GRID_PIECE_FILTER,
                                                                "FVizUnstructuredGridPieceFilter", &g_fviz_object_class,
                                                                fviz_unstructured_piece_destroy, NULL};

static FVizMTime fviz_unstructured_piece_state_mtime(const void* state)
{
    return fviz_object_mtime((const FVizObject*)state);
}

static FVizResult fviz_unstructured_piece_get_adjacency(FVizUnstructuredGridPieceFilter* filter, FVizCellArray* cells,
                                                        FVizSize point_count, FVizCellAdjacency** out_adjacency)
{
    const FVizMTime topology_mtime = fviz_object_mtime((const FVizObject*)cells);
    FVizCellAdjacency* built = NULL;
    if (out_adjacency == NULL || filter == NULL || cells == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_adjacency = NULL;
    if (filter->adjacency_cache != NULL && filter->adjacency_cells == cells &&
        filter->adjacency_cells_mtime == topology_mtime)
    {
        *out_adjacency = filter->adjacency_cache;
        return FVIZ_OK;
    }
    if (fviz_cell_adjacency_build(cells, point_count, &built) != FVIZ_OK) return fviz_last_error_code();
    fviz_release(filter->adjacency_cache);
    fviz_release(filter->adjacency_cells);
    filter->adjacency_cache = built;
    filter->adjacency_cells = (FVizCellArray*)fviz_retain(cells);
    filter->adjacency_cells_mtime = topology_mtime;
    *out_adjacency = built;
    return FVIZ_OK;
}

static FVizResult fviz_unstructured_piece_get_links(FVizUnstructuredGridPieceFilter* filter, FVizCellArray* cells,
                                                    FVizSize point_count, FVizCellLinks** out_links)
{
    const FVizMTime topology_mtime = fviz_object_mtime((const FVizObject*)cells);
    FVizCellLinks* built = NULL;
    if (out_links == NULL || filter == NULL || cells == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_links = NULL;
    if (filter->links_cache != NULL && filter->links_cells == cells && filter->links_cells_mtime == topology_mtime)
    {
        *out_links = filter->links_cache;
        return FVIZ_OK;
    }
    if (fviz_cell_links_build(cells, point_count, &built) != FVIZ_OK) return fviz_last_error_code();
    fviz_release(filter->links_cache);
    fviz_release(filter->links_cells);
    filter->links_cache = built;
    filter->links_cells = (FVizCellArray*)fviz_retain(cells);
    filter->links_cells_mtime = topology_mtime;
    *out_links = built;
    return FVIZ_OK;
}

static uint32_t fviz_unstructured_piece_owner_for_cell(FVizSize cell_count, FVizSize cell_id, uint32_t number_of_pieces)
{
    const FVizSize pieces = (FVizSize)number_of_pieces;
    const FVizSize quotient = cell_count / pieces;
    const FVizSize remainder = cell_count % pieces;
    const FVizSize large_count = quotient + 1u;
    const FVizSize large_cells = large_count * remainder;
    if (cell_id < large_cells) return (uint32_t)(cell_id / large_count);
    if (quotient == 0u) return (uint32_t)cell_id;
    return (uint32_t)(remainder + (cell_id - large_cells) / quotient);
}

static FVizResult fviz_unstructured_piece_get_point_owners(FVizUnstructuredGridPieceFilter* filter,
                                                           FVizCellArray* cells, FVizSize point_count,
                                                           FVizSize cell_count, uint32_t number_of_pieces,
                                                           const uint32_t** out_owners)
{
    const FVizMTime topology_mtime = fviz_object_mtime((const FVizObject*)cells);
    FVizCellLinks* links = NULL;
    uint32_t* owners = NULL;
    FVizSize bytes;
    FVizSize point_id;
    if (filter == NULL || cells == NULL || out_owners == NULL || number_of_pieces == 0u)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_owners = NULL;
    if (filter->point_owner_cache != NULL && filter->links_cells == cells &&
        filter->point_owner_cells_mtime == topology_mtime && filter->point_owner_count == point_count &&
        filter->point_owner_piece_count == number_of_pieces)
    {
        *out_owners = filter->point_owner_cache;
        return FVIZ_OK;
    }
    if (fviz_unstructured_piece_get_links(filter, cells, point_count, &links) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_size_multiply(point_count, sizeof(uint32_t), &bytes) != FVIZ_OK) return FVIZ_ERROR_OVERFLOW;
    owners = point_count != 0u ? (uint32_t*)fviz_alloc(bytes) : NULL;
    if (point_count != 0u && owners == NULL) return fviz_last_error_code();
    for (point_id = 0u; point_id < point_count; ++point_id)
    {
        FVizSize incident_count = 0u;
        FVizSize incident_index;
        const FVizId* incident = fviz_cell_links_cells_for_point(links, (FVizId)point_id, &incident_count);
        uint32_t owner = 0u;
        if (incident_count != 0u)
        {
            owner = number_of_pieces;
            for (incident_index = 0u; incident_index < incident_count; ++incident_index)
            {
                const uint32_t candidate = fviz_unstructured_piece_owner_for_cell(
                    cell_count, (FVizSize)incident[incident_index], number_of_pieces);
                if (candidate < owner) owner = candidate;
            }
            if (owner >= number_of_pieces) owner = 0u;
        }
        owners[point_id] = owner;
    }
    fviz_free(filter->point_owner_cache);
    filter->point_owner_cache = owners;
    filter->point_owner_count = point_count;
    filter->point_owner_piece_count = number_of_pieces;
    filter->point_owner_cells_mtime = topology_mtime;
    *out_owners = owners;
    return FVIZ_OK;
}

static FVizSize fviz_unstructured_piece_boundary(FVizSize count, uint32_t piece, uint32_t number_of_pieces)
{
    const FVizSize pieces = (FVizSize)number_of_pieces;
    const FVizSize p = (FVizSize)piece;
    const FVizSize quotient = count / pieces;
    const FVizSize remainder = count % pieces;
    return quotient * p + (p < remainder ? p : remainder);
}

static FVizResult fviz_unstructured_piece_map_request(FVizAlgorithm* algorithm, uint32_t input_port,
                                                      uint32_t connection, const FVizPipelineRequestInfo* downstream,
                                                      FVizPipelineRequestInfo* upstream, void* state)
{
    (void)algorithm;
    (void)connection;
    (void)state;
    if (input_port != 0u) return FVIZ_ERROR_INVALID_ARGUMENT;
    /* Piece materialization requires the whole upstream grid because ghost
       expansion needs topology on both sides of partition boundaries. */
    if (downstream->number_of_pieces != 1u || downstream->piece != 0u || downstream->ghost_levels != 0u)
        return fviz_pipeline_request_set_piece(upstream, 0u, 1u, 0u);
    return FVIZ_OK;
}

static FVizResult fviz_unstructured_piece_copy_field_attributes(const FVizAttributeSet* source,
                                                                FVizAttributeSet* destination)
{
    FVizSize i;
    for (i = 0u; i < fviz_attribute_set_count(source); ++i)
    {
        const char* name = fviz_attribute_set_name_at(source, i);
        const FVizDataArray* array = fviz_attribute_set_const_array_at(source, i);
        FVizDataArray* copy = NULL;
        FVizAttributeRole role;
        if (fviz_data_array_deep_copy(array, &copy) != FVIZ_OK ||
            fviz_attribute_set_add(destination, name, copy) != FVIZ_OK)
        {
            fviz_release(copy);
            return fviz_last_error_code();
        }
        fviz_release(copy);
        for (role = FVIZ_ATTRIBUTE_SCALARS; role < FVIZ_ATTRIBUTE_ROLE_COUNT; ++role)
        {
            const char* active = fviz_attribute_set_active_name(source, role);
            if (active != NULL && strcmp(active, name) == 0)
                (void)fviz_attribute_set_set_active(destination, role, name);
        }
    }
    return FVIZ_OK;
}

static FVizResult fviz_unstructured_piece_copy_indexed_attributes(const FVizAttributeSet* source,
                                                                  FVizAttributeSet* destination,
                                                                  const FVizId* source_ids, FVizSize output_count,
                                                                  FVizSize expected_source_count)
{
    FVizSize array_index;
    for (array_index = 0u; array_index < fviz_attribute_set_count(source); ++array_index)
    {
        const char* name = fviz_attribute_set_name_at(source, array_index);
        const FVizDataArray* in_array = fviz_attribute_set_const_array_at(source, array_index);
        FVizDataArray* out_array = NULL;
        FVizAttributeRole role;
        FVizSize i;
        if (fviz_data_array_tuple_count(in_array) != expected_source_count) continue;
        if (fviz_data_array_create(fviz_data_array_type(in_array), fviz_data_array_components(in_array), &out_array) !=
                FVIZ_OK ||
            fviz_data_array_resize(out_array, output_count) != FVIZ_OK)
            goto fail;
        for (i = 0u; i < output_count; ++i)
        {
            const FVizSize source_id = (FVizSize)source_ids[i];
            const void* in_tuple = fviz_data_array_const_tuple(in_array, source_id);
            void* out_tuple = fviz_data_array_tuple(out_array, i);
            if (in_tuple == NULL || out_tuple == NULL) goto fail;
            (void)memcpy(out_tuple, in_tuple, fviz_data_array_tuple_stride(in_array));
        }
        if (fviz_attribute_set_add(destination, name, out_array) != FVIZ_OK) goto fail;
        for (role = FVIZ_ATTRIBUTE_SCALARS; role < FVIZ_ATTRIBUTE_ROLE_COUNT; ++role)
        {
            const char* active = fviz_attribute_set_active_name(source, role);
            if (active != NULL && strcmp(active, name) == 0)
                (void)fviz_attribute_set_set_active(destination, role, name);
        }
        fviz_release(out_array);
        continue;
    fail:
        fviz_release(out_array);
        return fviz_last_error_code();
    }
    return FVIZ_OK;
}

static FVizResult fviz_unstructured_piece_add_provenance(FVizUnstructuredGrid* output, const FVizId* source_point_ids,
                                                         FVizSize point_count, const FVizId* source_cell_ids,
                                                         FVizSize cell_count)
{
    FVizDataArray* point_ids = NULL;
    FVizDataArray* cell_ids = NULL;
    FVizResult result = fviz_data_array_create(FVIZ_DATA_UINT64, 1u, &point_ids);
    if (result == FVIZ_OK) result = fviz_data_array_create(FVIZ_DATA_UINT64, 1u, &cell_ids);
    if (result == FVIZ_OK) result = fviz_data_array_resize(point_ids, point_count);
    if (result == FVIZ_OK) result = fviz_data_array_resize(cell_ids, cell_count);
    if (result == FVIZ_OK && point_count != 0u)
        result = fviz_data_array_set_tuples(point_ids, 0u, source_point_ids, point_count);
    if (result == FVIZ_OK && cell_count != 0u)
        result = fviz_data_array_set_tuples(cell_ids, 0u, source_cell_ids, cell_count);
    if (result == FVIZ_OK)
        result = fviz_attribute_set_add(fviz_unstructured_grid_point_data(output), "FVizOriginalPointIds", point_ids);
    if (result == FVIZ_OK)
        result = fviz_attribute_set_add(fviz_unstructured_grid_cell_data(output), "FVizOriginalCellIds", cell_ids);
    fviz_release(cell_ids);
    fviz_release(point_ids);
    return result;
}

static FVizResult fviz_unstructured_piece_add_ghost_arrays(FVizUnstructuredGrid* output, const FVizId* source_point_ids,
                                                           FVizSize point_count, const uint32_t* point_owners,
                                                           uint32_t piece, uint32_t number_of_pieces,
                                                           const uint16_t* cell_levels, FVizSize cell_count)
{
    FVizDataArray* point_ghosts = NULL;
    FVizDataArray* cell_ghosts = NULL;
    FVizDataArray* ghost_levels = NULL;
    uint8_t* point_values = NULL;
    uint8_t* cell_values = NULL;
    FVizSize i;
    FVizResult result = FVIZ_OK;
    if (cell_levels == NULL) return FVIZ_OK;
    if (number_of_pieces <= 1u)
    {
        FVizBool any_ghost = FVIZ_FALSE;
        for (i = 0u; i < cell_count; ++i)
        {
            if (cell_levels[i] != 0u)
            {
                any_ghost = FVIZ_TRUE;
                break;
            }
        }
        if (any_ghost == FVIZ_FALSE) return FVIZ_OK;
    }
    if (point_count != 0u)
    {
        if (point_owners == NULL) return FVIZ_ERROR_INVALID_STATE;
        point_values = (uint8_t*)fviz_alloc(point_count * sizeof(uint8_t));
        if (point_values == NULL) return fviz_last_error_code();
        for (i = 0u; i < point_count; ++i)
            point_values[i] = point_owners[(FVizSize)source_point_ids[i]] == piece ? (uint8_t)FVIZ_GHOST_NONE
                                                                                   : (uint8_t)FVIZ_GHOST_DUPLICATE;
    }
    if (cell_count != 0u)
    {
        cell_values = (uint8_t*)fviz_alloc(cell_count * sizeof(uint8_t));
        if (cell_values == NULL)
        {
            fviz_free(point_values);
            return fviz_last_error_code();
        }
        for (i = 0u; i < cell_count; ++i)
            cell_values[i] = cell_levels[i] == 0u ? (uint8_t)FVIZ_GHOST_NONE : (uint8_t)FVIZ_GHOST_DUPLICATE;
    }
    result = fviz_data_array_create(FVIZ_DATA_UINT8, 1u, &point_ghosts);
    if (result == FVIZ_OK) result = fviz_data_array_resize(point_ghosts, point_count);
    if (result == FVIZ_OK && point_count != 0u)
        result = fviz_data_array_set_tuples(point_ghosts, 0u, point_values, point_count);
    if (result == FVIZ_OK) result = fviz_data_array_create(FVIZ_DATA_UINT8, 1u, &cell_ghosts);
    if (result == FVIZ_OK) result = fviz_data_array_resize(cell_ghosts, cell_count);
    if (result == FVIZ_OK && cell_count != 0u)
        result = fviz_data_array_set_tuples(cell_ghosts, 0u, cell_values, cell_count);
    if (result == FVIZ_OK) result = fviz_data_array_create(FVIZ_DATA_UINT16, 1u, &ghost_levels);
    if (result == FVIZ_OK) result = fviz_data_array_resize(ghost_levels, cell_count);
    if (result == FVIZ_OK && cell_count != 0u)
        result = fviz_data_array_set_tuples(ghost_levels, 0u, cell_levels, cell_count);
    if (result == FVIZ_OK)
        result = fviz_attribute_set_add(fviz_unstructured_grid_point_data(output), FVIZ_GHOST_ARRAY_NAME, point_ghosts);
    if (result == FVIZ_OK)
        result = fviz_attribute_set_add(fviz_unstructured_grid_cell_data(output), FVIZ_GHOST_ARRAY_NAME, cell_ghosts);
    if (result == FVIZ_OK)
        result =
            fviz_attribute_set_add(fviz_unstructured_grid_cell_data(output), FVIZ_GHOST_LEVEL_ARRAY_NAME, ghost_levels);
    fviz_free(cell_values);
    fviz_free(point_values);
    fviz_release(ghost_levels);
    fviz_release(cell_ghosts);
    fviz_release(point_ghosts);
    return result;
}

static FVizResult fviz_unstructured_piece_process_request(FVizAlgorithm* algorithm,
                                                          const FVizPipelineRequestInfo* request, void* state)
{
    FVizUnstructuredGrid* input;
    FVizUnstructuredGrid* output = NULL;
    const FVizCellArray* input_cells;
    const FVizVec3* input_points;
    FVizHashMap* point_map = NULL;
    FVizCellAdjacency* adjacency = NULL;
    const uint32_t* point_owners = NULL;
    FVizId* source_point_ids = NULL;
    FVizId* source_cell_ids = NULL;
    FVizId* local_ids = NULL;
    uint16_t* selected_levels = NULL;
    uint16_t* source_levels = NULL;
    FVizSize source_point_count;
    FVizSize source_cell_count;
    FVizSize first_cell = 0u;
    FVizSize end_cell;
    FVizSize selected_cells = 0u;
    FVizSize connectivity_count = 0u;
    FVizSize max_cell_points = 0u;
    FVizSize unique_points = 0u;
    FVizSize selected_index;
    FVizResult result = FVIZ_OK;
    FVizUnstructuredGridPieceFilter* filter = (FVizUnstructuredGridPieceFilter*)state;

    if (request->type != FVIZ_PIPELINE_REQUEST_DATA) return FVIZ_OK;
    input = (FVizUnstructuredGrid*)fviz_algorithm_resolved_input(algorithm, 0u, 0u);
    if (input == NULL || fviz_unstructured_grid_validate(input) != FVIZ_OK)
    {
        if (input == NULL)
            fviz_internal_set_error(FVIZ_ERROR_INVALID_STATE, "piece filter has no UnstructuredGrid input");
        return input == NULL ? FVIZ_ERROR_INVALID_STATE : fviz_last_error_code();
    }
    if (request->ghost_levels > (uint32_t)UINT16_MAX)
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "ghost level exceeds UINT16 storage");
        return FVIZ_ERROR_OVERFLOW;
    }

    source_point_count = fviz_unstructured_grid_point_count(input);
    source_cell_count = fviz_unstructured_grid_cell_count(input);
    end_cell = source_cell_count;
    if (request->number_of_pieces != 1u || request->piece != 0u)
    {
        if (request->number_of_pieces == 0u || request->piece >= request->number_of_pieces)
            return FVIZ_ERROR_INVALID_ARGUMENT;
        first_cell = fviz_unstructured_piece_boundary(source_cell_count, request->piece, request->number_of_pieces);
        end_cell = fviz_unstructured_piece_boundary(source_cell_count, request->piece + 1u, request->number_of_pieces);
    }
    input_cells = fviz_unstructured_grid_cells(input);
    input_points = fviz_points_data(fviz_unstructured_grid_points(input));

    if (source_cell_count != 0u)
    {
        FVizSize id_bytes;
        FVizSize level_bytes;
        if (fviz_size_multiply(source_cell_count, sizeof(FVizId), &id_bytes) != FVIZ_OK ||
            fviz_size_multiply(source_cell_count, sizeof(uint16_t), &level_bytes) != FVIZ_OK)
            return FVIZ_ERROR_OVERFLOW;
        source_cell_ids = (FVizId*)fviz_alloc(id_bytes);
        selected_levels = (uint16_t*)fviz_alloc(level_bytes);
        source_levels = (uint16_t*)fviz_alloc(level_bytes);
        if (source_cell_ids == NULL || selected_levels == NULL || source_levels == NULL)
        {
            result = fviz_last_error_code();
            goto done;
        }
        (void)memset(source_levels, 0xff, level_bytes);
    }
    for (selected_index = first_cell; selected_index < end_cell; ++selected_index)
    {
        source_levels[selected_index] = 0u;
        source_cell_ids[selected_cells] = (FVizId)selected_index;
        selected_levels[selected_cells] = 0u;
        ++selected_cells;
    }

    if (request->ghost_levels != 0u && selected_cells != 0u)
    {
        FVizSize frontier_begin = 0u;
        FVizSize frontier_end = selected_cells;
        uint32_t level;
        if (fviz_unstructured_piece_get_adjacency(filter, (FVizCellArray*)input_cells, source_point_count,
                                                  &adjacency) != FVIZ_OK)
        {
            result = fviz_last_error_code();
            goto done;
        }
        for (level = 1u; level <= request->ghost_levels && frontier_begin < frontier_end; ++level)
        {
            FVizSize frontier_index;
            const FVizSize next_begin = frontier_end;
            for (frontier_index = frontier_begin; frontier_index < frontier_end; ++frontier_index)
            {
                FVizSize neighbor_count = 0u;
                FVizSize neighbor_index;
                const FVizId* neighbors =
                    fviz_cell_adjacency_neighbors(adjacency, source_cell_ids[frontier_index], &neighbor_count);
                for (neighbor_index = 0u; neighbor_index < neighbor_count; ++neighbor_index)
                {
                    const FVizSize source_id = (FVizSize)neighbors[neighbor_index];
                    if (source_levels[source_id] != UINT16_MAX) continue;
                    source_levels[source_id] = (uint16_t)level;
                    source_cell_ids[selected_cells] = (FVizId)source_id;
                    selected_levels[selected_cells] = (uint16_t)level;
                    ++selected_cells;
                }
            }
            frontier_begin = next_begin;
            frontier_end = selected_cells;
        }
    }

    for (selected_index = 0u; selected_index < selected_cells; ++selected_index)
    {
        FVizCellView view;
        if (fviz_cell_array_cell_view(input_cells, (FVizSize)source_cell_ids[selected_index], &view) != FVIZ_OK)
        {
            result = fviz_last_error_code();
            goto done;
        }
        if (view.point_count > (FVizSize)-1 - connectivity_count)
        {
            result = FVIZ_ERROR_OVERFLOW;
            goto done;
        }
        connectivity_count += view.point_count;
        if (view.point_count > max_cell_points) max_cell_points = view.point_count;
    }

    if (fviz_unstructured_grid_create(&output) != FVIZ_OK ||
        fviz_hash_map_create_reserve(connectivity_count, &point_map) != FVIZ_OK ||
        fviz_unstructured_grid_reserve(
            output, connectivity_count < source_point_count ? connectivity_count : source_point_count, selected_cells,
            connectivity_count) != FVIZ_OK)
    {
        result = fviz_last_error_code();
        goto done;
    }
    if (connectivity_count != 0u)
    {
        FVizSize bytes;
        const FVizSize max_unique = connectivity_count < source_point_count ? connectivity_count : source_point_count;
        if (fviz_size_multiply(max_unique, sizeof(FVizId), &bytes) != FVIZ_OK)
        {
            result = FVIZ_ERROR_OVERFLOW;
            goto done;
        }
        source_point_ids = (FVizId*)fviz_alloc(bytes);
        if (source_point_ids == NULL)
        {
            result = fviz_last_error_code();
            goto done;
        }
        if (fviz_size_multiply(max_cell_points, sizeof(FVizId), &bytes) != FVIZ_OK)
        {
            result = FVIZ_ERROR_OVERFLOW;
            goto done;
        }
        local_ids = (FVizId*)fviz_alloc(bytes);
        if (local_ids == NULL && max_cell_points != 0u)
        {
            result = fviz_last_error_code();
            goto done;
        }
    }
    for (selected_index = 0u; selected_index < selected_cells; ++selected_index)
    {
        FVizCellView view;
        FVizSize local_point;
        if (fviz_cell_array_cell_view(input_cells, (FVizSize)source_cell_ids[selected_index], &view) != FVIZ_OK)
        {
            result = fviz_last_error_code();
            goto done;
        }
        for (local_point = 0u; local_point < view.point_count; ++local_point)
        {
            const FVizId source_id = fviz_cell_view_point_id(&view, local_point);
            void* encoded = NULL;
            FVizId local_id;
            if ((FVizSize)source_id >= source_point_count)
            {
                result = FVIZ_ERROR_INVALID_STATE;
                goto done;
            }
            if (fviz_hash_map_get(point_map, source_id, &encoded) != FVIZ_FALSE)
            {
                local_id = (FVizId)((uintptr_t)encoded - (uintptr_t)1u);
            }
            else
            {
                if (unique_points >= (FVizSize)UINTPTR_MAX)
                {
                    result = FVIZ_ERROR_OVERFLOW;
                    goto done;
                }
                if (fviz_unstructured_grid_add_points_ids(output, &input_points[(FVizSize)source_id], 1u, &local_id) !=
                        FVIZ_OK ||
                    fviz_hash_map_set(point_map, source_id, (void*)(uintptr_t)(local_id + 1u)) != FVIZ_OK)
                {
                    result = fviz_last_error_code();
                    goto done;
                }
                source_point_ids[unique_points++] = source_id;
            }
            local_ids[local_point] = local_id;
        }
        if (fviz_unstructured_grid_add_cell_ids(output, view.type, view.point_count, local_ids) != FVIZ_OK)
        {
            result = fviz_last_error_code();
            goto done;
        }
    }

    if ((request->number_of_pieces > 1u || request->ghost_levels != 0u) && source_point_count != 0u &&
        fviz_unstructured_piece_get_point_owners(filter, (FVizCellArray*)input_cells, source_point_count,
                                                 source_cell_count, request->number_of_pieces,
                                                 &point_owners) != FVIZ_OK)
    {
        result = fviz_last_error_code();
        goto done;
    }

    if (fviz_unstructured_piece_copy_indexed_attributes(fviz_unstructured_grid_point_data(input),
                                                        fviz_unstructured_grid_point_data(output), source_point_ids,
                                                        unique_points, source_point_count) != FVIZ_OK ||
        fviz_unstructured_piece_copy_indexed_attributes(fviz_unstructured_grid_cell_data(input),
                                                        fviz_unstructured_grid_cell_data(output), source_cell_ids,
                                                        selected_cells, source_cell_count) != FVIZ_OK ||
        fviz_unstructured_piece_copy_field_attributes(fviz_unstructured_grid_field_data(input),
                                                      fviz_unstructured_grid_field_data(output)) != FVIZ_OK ||
        fviz_unstructured_piece_add_provenance(output, source_point_ids, unique_points, source_cell_ids,
                                               selected_cells) != FVIZ_OK ||
        fviz_unstructured_piece_add_ghost_arrays(output, source_point_ids, unique_points, point_owners, request->piece,
                                                 request->number_of_pieces, selected_levels,
                                                 selected_cells) != FVIZ_OK ||
        fviz_algorithm_set_output_data(algorithm, request->requested_output_port, (FVizDataObject*)output) != FVIZ_OK)
    {
        result = fviz_last_error_code();
        goto done;
    }

done:
    fviz_free(source_levels);
    fviz_free(selected_levels);
    fviz_free(local_ids);
    fviz_free(source_cell_ids);
    fviz_free(source_point_ids);
    fviz_release(point_map);
    fviz_release(output);
    return result;
}

FVizResult fviz_unstructured_grid_piece_filter_create(FVizUnstructuredGridPieceFilter** out_filter)
{
    FVizUnstructuredGridPieceFilter* filter;
    FVizAlgorithmCallbacks callbacks;
    if (out_filter == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_filter must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_filter = NULL;
    filter = (FVizUnstructuredGridPieceFilter*)fviz_internal_object_allocate(sizeof(*filter),
                                                                             &g_fviz_unstructured_piece_class, NULL);
    if (filter == NULL) return fviz_last_error_code();
    fviz_algorithm_callbacks_initialize(&callbacks);
    callbacks.process_request = fviz_unstructured_piece_process_request;
    callbacks.get_state_mtime = fviz_unstructured_piece_state_mtime;
    callbacks.state_object = (FVizObject*)filter;
    callbacks.map_input_request = fviz_unstructured_piece_map_request;
    if (fviz_algorithm_create(1u, 1u, &callbacks, filter, &filter->algorithm) != FVIZ_OK ||
        fviz_algorithm_configure_input_port(filter->algorithm, 0u, FVIZ_TYPE_UNSTRUCTURED_GRID, FVIZ_FALSE,
                                            FVIZ_FALSE) != FVIZ_OK ||
        fviz_algorithm_configure_output_port(filter->algorithm, 0u, FVIZ_TYPE_UNSTRUCTURED_GRID) != FVIZ_OK)
    {
        fviz_release(filter);
        return fviz_last_error_code();
    }
    *out_filter = filter;
    return FVIZ_OK;
}

FVizResult fviz_unstructured_grid_piece_filter_set_input_data(FVizUnstructuredGridPieceFilter* filter,
                                                              FVizUnstructuredGrid* input)
{
    return filter != NULL && input != NULL
               ? fviz_algorithm_set_input_data(filter->algorithm, 0u, (FVizDataObject*)input)
               : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizResult fviz_unstructured_grid_piece_filter_set_input_connection(FVizUnstructuredGridPieceFilter* filter,
                                                                    FVizAlgorithmOutput* input)
{
    return filter != NULL && input != NULL ? fviz_algorithm_set_input_connection(filter->algorithm, 0u, input)
                                           : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizAlgorithm* fviz_unstructured_grid_piece_filter_algorithm(FVizUnstructuredGridPieceFilter* filter)
{
    return filter != NULL ? filter->algorithm : NULL;
}

FVizAlgorithmOutput* fviz_unstructured_grid_piece_filter_output_port(FVizUnstructuredGridPieceFilter* filter)
{
    return filter != NULL ? fviz_algorithm_output_port(filter->algorithm, 0u) : NULL;
}

FVizUnstructuredGrid* fviz_unstructured_grid_piece_filter_output(FVizUnstructuredGridPieceFilter* filter)
{
    return filter != NULL ? (FVizUnstructuredGrid*)fviz_algorithm_output_data(filter->algorithm, 0u) : NULL;
}

FVizResult fviz_unstructured_grid_piece_filter_update(FVizUnstructuredGridPieceFilter* filter)
{
    return filter != NULL ? fviz_algorithm_update(filter->algorithm) : FVIZ_ERROR_INVALID_ARGUMENT;
}

FVizResult fviz_unstructured_grid_piece_filter_update_piece(FVizUnstructuredGridPieceFilter* filter, uint32_t piece,
                                                            uint32_t number_of_pieces, uint32_t ghost_levels)
{
    return filter != NULL ? fviz_executive_update_piece(fviz_algorithm_executive(filter->algorithm), 0u, piece,
                                                        number_of_pieces, ghost_levels)
                          : FVIZ_ERROR_INVALID_ARGUMENT;
}
