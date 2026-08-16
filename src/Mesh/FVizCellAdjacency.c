#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizHashMap.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Mesh/FVizCellAdjacency.h>
#include <FViz/Mesh/FVizCellTypeTraits.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

typedef struct FVizAdjacencyPair
{
    FVizId a;
    FVizId b;
} FVizAdjacencyPair;

typedef struct FVizBoundaryEntity
{
    uint32_t count;
    FVizId ids[9];
} FVizBoundaryEntity;

typedef struct FVizFacetRecord
{
    FVizBoundaryEntity entity;
    FVizId first_cell;
    FVizId second_cell;
    FVizSize next_record_plus_one;
    FVizArray* extra_cells;
} FVizFacetRecord;

struct FVizCellAdjacency
{
    FVizObject base;
    FVizSize cell_count;
    FVizSize edge_count;
    FVizSize* offsets;
    FVizId* neighbors;
};

static void fviz_cell_adjacency_destroy(FVizObject* object)
{
    FVizCellAdjacency* adjacency = (FVizCellAdjacency*)object;
    fviz_free(adjacency->neighbors);
    fviz_free(adjacency->offsets);
    adjacency->neighbors = NULL;
    adjacency->offsets = NULL;
}

static const FVizObjectClass g_fviz_cell_adjacency_class = {FVIZ_TYPE_CELL_ADJACENCY, "FVizCellAdjacency",
                                                            &g_fviz_object_class, fviz_cell_adjacency_destroy, NULL};

static void fviz_boundary_sort(FVizId* ids, uint32_t count)
{
    uint32_t i;
    for (i = 1u; i < count; ++i)
    {
        const FVizId value = ids[i];
        uint32_t j = i;
        while (j != 0u && ids[j - 1u] > value)
        {
            ids[j] = ids[j - 1u];
            --j;
        }
        ids[j] = value;
    }
}

static uint32_t fviz_boundary_entity_count(const FVizCellView* view)
{
    const FVizCellTypeTraits traits = fviz_cell_type_traits(view->type);
    if (traits.dimension == 0u) return 0u;
    if (traits.dimension == 1u) return 2u;
    if (traits.dimension == 2u)
    {
        if (view->type == FVIZ_CELL_POLYGON) return (uint32_t)view->point_count;
        if (view->type == FVIZ_CELL_TRIANGLE_STRIP) return UINT32_MAX;
        return traits.edge_count;
    }
    return traits.face_count;
}

static FVizResult fviz_boundary_entity(const FVizCellView* view, uint32_t entity_index, FVizBoundaryEntity* out_entity)
{
    const FVizCellTypeTraits traits = fviz_cell_type_traits(view->type);
    uint32_t local_ids[9];
    uint32_t count = 0u;
    uint32_t i;
    if (out_entity == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    (void)memset(out_entity, 0, sizeof(*out_entity));
    if (traits.dimension == 1u)
    {
        if (entity_index >= 2u) return FVIZ_ERROR_NOT_FOUND;
        out_entity->count = 1u;
        if (view->type == FVIZ_CELL_POLY_LINE)
            out_entity->ids[0] = fviz_cell_view_point_id(view, entity_index == 0u ? 0u : view->point_count - 1u);
        else
            out_entity->ids[0] = fviz_cell_view_point_id(view, entity_index == 0u ? 0u : 1u);
        return FVIZ_OK;
    }
    if (traits.dimension == 2u)
    {
        if (view->type == FVIZ_CELL_POLYGON)
        {
            if ((FVizSize)entity_index >= view->point_count) return FVIZ_ERROR_NOT_FOUND;
            out_entity->count = 2u;
            out_entity->ids[0] = fviz_cell_view_point_id(view, (FVizSize)entity_index);
            out_entity->ids[1] = fviz_cell_view_point_id(view, ((FVizSize)entity_index + 1u) % view->point_count);
            fviz_boundary_sort(out_entity->ids, out_entity->count);
            return FVIZ_OK;
        }
        if (view->type == FVIZ_CELL_TRIANGLE_STRIP)
        {
            fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "triangle-strip cell adjacency is not supported");
            return FVIZ_ERROR_NOT_SUPPORTED;
        }
        if (fviz_cell_type_edge(view->type, entity_index, local_ids) != FVIZ_OK) return fviz_last_error_code();
        count = 2u;
    }
    else if (traits.dimension == 3u)
    {
        if (fviz_cell_type_face(view->type, entity_index, local_ids, 9u, &count) != FVIZ_OK)
            return fviz_last_error_code();
    }
    else
    {
        return FVIZ_ERROR_NOT_FOUND;
    }
    if (count == 0u || count > 9u) return FVIZ_ERROR_NOT_SUPPORTED;
    out_entity->count = count;
    for (i = 0u; i < count; ++i)
    {
        if ((FVizSize)local_ids[i] >= view->point_count) return FVIZ_ERROR_INVALID_STATE;
        out_entity->ids[i] = fviz_cell_view_point_id(view, (FVizSize)local_ids[i]);
    }
    fviz_boundary_sort(out_entity->ids, count);
    return FVIZ_OK;
}

static FVizBool fviz_boundary_equal(const FVizBoundaryEntity* a, const FVizBoundaryEntity* b)
{
    return a->count == b->count && memcmp(a->ids, b->ids, (size_t)a->count * sizeof(FVizId)) == 0 ? FVIZ_TRUE
                                                                                                  : FVIZ_FALSE;
}

static uint64_t fviz_boundary_hash(const FVizBoundaryEntity* entity)
{
    uint64_t hash = UINT64_C(1469598103934665603) ^ (uint64_t)entity->count;
    uint32_t i;
    for (i = 0u; i < entity->count; ++i)
    {
        uint64_t x = (uint64_t)entity->ids[i];
        x ^= x >> 33u;
        x *= UINT64_C(0xff51afd7ed558ccd);
        x ^= x >> 33u;
        x *= UINT64_C(0xc4ceb9fe1a85ec53);
        x ^= x >> 33u;
        hash ^= x;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static int fviz_adjacency_pair_compare(const void* lhs, const void* rhs)
{
    const FVizAdjacencyPair* a = (const FVizAdjacencyPair*)lhs;
    const FVizAdjacencyPair* b = (const FVizAdjacencyPair*)rhs;
    if (a->a < b->a) return -1;
    if (a->a > b->a) return 1;
    if (a->b < b->b) return -1;
    if (a->b > b->b) return 1;
    return 0;
}

static int fviz_id_compare(const void* lhs, const void* rhs)
{
    const FVizId a = *(const FVizId*)lhs;
    const FVizId b = *(const FVizId*)rhs;
    return a < b ? -1 : (a > b ? 1 : 0);
}

static FVizResult fviz_adjacency_add_pair(FVizArray* pairs, FVizId a, FVizId b)
{
    FVizAdjacencyPair pair;
    if (a == b) return FVIZ_OK;
    if (a > b)
    {
        const FVizId tmp = a;
        a = b;
        b = tmp;
    }
    pair.a = a;
    pair.b = b;
    return fviz_array_push(pairs, &pair);
}

static void fviz_facet_records_release(FVizArray* records)
{
    FVizSize i;
    if (records == NULL) return;
    for (i = 0u; i < fviz_array_count(records); ++i)
    {
        FVizFacetRecord* record = (FVizFacetRecord*)fviz_array_at(records, i);
        fviz_release(record->extra_cells);
        record->extra_cells = NULL;
    }
}

static FVizResult fviz_facet_record_connect(FVizFacetRecord* record, FVizId cell_id, FVizArray* pairs)
{
    FVizSize i;
    if (fviz_adjacency_add_pair(pairs, record->first_cell, cell_id) != FVIZ_OK) return fviz_last_error_code();
    if (record->second_cell == FVIZ_INVALID_ID)
    {
        record->second_cell = cell_id;
        return FVIZ_OK;
    }
    if (fviz_adjacency_add_pair(pairs, record->second_cell, cell_id) != FVIZ_OK) return fviz_last_error_code();
    if (record->extra_cells != NULL)
    {
        const FVizId* extras = (const FVizId*)fviz_array_const_data(record->extra_cells);
        for (i = 0u; i < fviz_array_count(record->extra_cells); ++i)
            if (fviz_adjacency_add_pair(pairs, extras[i], cell_id) != FVIZ_OK) return fviz_last_error_code();
    }
    else if (fviz_array_create(sizeof(FVizId), &record->extra_cells) != FVIZ_OK)
    {
        return fviz_last_error_code();
    }
    return fviz_array_push(record->extra_cells, &cell_id);
}

FVizResult fviz_cell_adjacency_build(const FVizCellArray* cells, FVizSize point_count,
                                     FVizCellAdjacency** out_adjacency)
{
    FVizCellAdjacency* adjacency = NULL;
    FVizHashMap* facet_heads = NULL;
    FVizArray* records = NULL;
    FVizArray* pairs = NULL;
    FVizSize cell_count;
    FVizSize facet_count = 0u;
    FVizSize cell_id;
    FVizResult result = FVIZ_OK;
    if (out_adjacency == NULL || cells == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "cell adjacency requires cells and an output pointer");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_adjacency = NULL;
    if (fviz_cell_array_validate(cells, point_count) != FVIZ_OK) return fviz_last_error_code();
    cell_count = fviz_cell_array_count(cells);
    for (cell_id = 0u; cell_id < cell_count; ++cell_id)
    {
        FVizCellView view;
        uint32_t count;
        if (fviz_cell_array_cell_view(cells, cell_id, &view) != FVIZ_OK) return fviz_last_error_code();
        count = fviz_boundary_entity_count(&view);
        if (count == UINT32_MAX)
        {
            fviz_internal_set_error(FVIZ_ERROR_NOT_SUPPORTED, "triangle-strip cell adjacency is not supported");
            return FVIZ_ERROR_NOT_SUPPORTED;
        }
        if ((FVizSize)count > (FVizSize)-1 - facet_count) return FVIZ_ERROR_OVERFLOW;
        facet_count += (FVizSize)count;
    }
    if (fviz_hash_map_create_reserve(facet_count, &facet_heads) != FVIZ_OK ||
        fviz_array_create_reserve(sizeof(FVizFacetRecord), facet_count, &records) != FVIZ_OK ||
        fviz_array_create_reserve(sizeof(FVizAdjacencyPair), facet_count / 2u, &pairs) != FVIZ_OK)
    {
        result = fviz_last_error_code();
        goto done;
    }

    for (cell_id = 0u; cell_id < cell_count; ++cell_id)
    {
        FVizCellView view;
        uint32_t count;
        uint32_t entity_index;
        if (fviz_cell_array_cell_view(cells, cell_id, &view) != FVIZ_OK)
        {
            result = fviz_last_error_code();
            goto done;
        }
        count = fviz_boundary_entity_count(&view);
        for (entity_index = 0u; entity_index < count; ++entity_index)
        {
            FVizBoundaryEntity entity;
            const uint64_t hash_seed = UINT64_C(0x9e3779b97f4a7c15);
            uint64_t hash;
            void* encoded_head = NULL;
            FVizSize record_plus_one = 0u;
            FVizBool matched = FVIZ_FALSE;
            if (fviz_boundary_entity(&view, entity_index, &entity) != FVIZ_OK)
            {
                result = fviz_last_error_code();
                goto done;
            }
            hash = fviz_boundary_hash(&entity) ^ hash_seed;
            if (fviz_hash_map_get(facet_heads, (FVizId)hash, &encoded_head) != FVIZ_FALSE)
                record_plus_one = (FVizSize)(uintptr_t)encoded_head;
            while (record_plus_one != 0u)
            {
                FVizFacetRecord* record = (FVizFacetRecord*)fviz_array_at(records, record_plus_one - 1u);
                if (fviz_boundary_equal(&record->entity, &entity) != FVIZ_FALSE)
                {
                    result = fviz_facet_record_connect(record, (FVizId)cell_id, pairs);
                    if (result != FVIZ_OK) goto done;
                    matched = FVIZ_TRUE;
                    break;
                }
                record_plus_one = record->next_record_plus_one;
            }
            if (matched == FVIZ_FALSE)
            {
                FVizFacetRecord record;
                const FVizSize new_index = fviz_array_count(records);
                (void)memset(&record, 0, sizeof(record));
                record.entity = entity;
                record.first_cell = (FVizId)cell_id;
                record.second_cell = FVIZ_INVALID_ID;
                record.next_record_plus_one = encoded_head != NULL ? (FVizSize)(uintptr_t)encoded_head : 0u;
                if (new_index >= (FVizSize)UINTPTR_MAX)
                {
                    result = FVIZ_ERROR_OVERFLOW;
                    goto done;
                }
                if (fviz_array_push(records, &record) != FVIZ_OK ||
                    fviz_hash_map_set(facet_heads, (FVizId)hash, (void*)(uintptr_t)(new_index + 1u)) != FVIZ_OK)
                {
                    result = fviz_last_error_code();
                    goto done;
                }
            }
        }
    }

    if (fviz_array_count(pairs) > 1u)
        qsort(fviz_array_data(pairs), fviz_array_count(pairs), sizeof(FVizAdjacencyPair), fviz_adjacency_pair_compare);
    {
        FVizAdjacencyPair* data = (FVizAdjacencyPair*)fviz_array_data(pairs);
        FVizSize read_index;
        FVizSize write_index = 0u;
        const FVizSize pair_count = fviz_array_count(pairs);
        for (read_index = 0u; read_index < pair_count; ++read_index)
        {
            if (write_index != 0u && data[write_index - 1u].a == data[read_index].a &&
                data[write_index - 1u].b == data[read_index].b)
                continue;
            data[write_index++] = data[read_index];
        }
        if (fviz_array_resize(pairs, write_index) != FVIZ_OK)
        {
            result = fviz_last_error_code();
            goto done;
        }
    }

    adjacency =
        (FVizCellAdjacency*)fviz_internal_object_allocate(sizeof(*adjacency), &g_fviz_cell_adjacency_class, NULL);
    if (adjacency == NULL)
    {
        result = fviz_last_error_code();
        goto done;
    }
    adjacency->cell_count = cell_count;
    adjacency->edge_count = fviz_array_count(pairs);
    {
        FVizSize offset_bytes;
        if (cell_count == (FVizSize)-1 ||
            fviz_size_multiply(cell_count + 1u, sizeof(FVizSize), &offset_bytes) != FVIZ_OK)
        {
            result = FVIZ_ERROR_OVERFLOW;
            goto done;
        }
        adjacency->offsets = (FVizSize*)fviz_alloc(offset_bytes);
        if (adjacency->offsets == NULL)
        {
            result = fviz_last_error_code();
            goto done;
        }
        (void)memset(adjacency->offsets, 0, offset_bytes);
    }
    if (adjacency->edge_count != 0u)
    {
        FVizSize neighbor_bytes;
        FVizSize pair_index;
        FVizAdjacencyPair* data = (FVizAdjacencyPair*)fviz_array_data(pairs);
        if (adjacency->edge_count > (FVizSize)-1 / 2u ||
            fviz_size_multiply(adjacency->edge_count * 2u, sizeof(FVizId), &neighbor_bytes) != FVIZ_OK)
        {
            result = FVIZ_ERROR_OVERFLOW;
            goto done;
        }
        adjacency->neighbors = (FVizId*)fviz_alloc(neighbor_bytes);
        if (adjacency->neighbors == NULL)
        {
            result = fviz_last_error_code();
            goto done;
        }
        for (pair_index = 0u; pair_index < adjacency->edge_count; ++pair_index)
        {
            ++adjacency->offsets[(FVizSize)data[pair_index].a + 1u];
            ++adjacency->offsets[(FVizSize)data[pair_index].b + 1u];
        }
        {
            FVizSize i;
            FVizSize* cursor;
            FVizSize cursor_bytes;
            for (i = 1u; i <= cell_count; ++i)
                adjacency->offsets[i] += adjacency->offsets[i - 1u];
            if (fviz_size_multiply(cell_count, sizeof(FVizSize), &cursor_bytes) != FVIZ_OK)
            {
                result = FVIZ_ERROR_OVERFLOW;
                goto done;
            }
            cursor = cell_count != 0u ? (FVizSize*)fviz_alloc(cursor_bytes) : NULL;
            if (cell_count != 0u && cursor == NULL)
            {
                result = fviz_last_error_code();
                goto done;
            }
            if (cell_count != 0u) (void)memcpy(cursor, adjacency->offsets, cursor_bytes);
            for (pair_index = 0u; pair_index < adjacency->edge_count; ++pair_index)
            {
                const FVizSize a = (FVizSize)data[pair_index].a;
                const FVizSize b = (FVizSize)data[pair_index].b;
                adjacency->neighbors[cursor[a]++] = (FVizId)b;
                adjacency->neighbors[cursor[b]++] = (FVizId)a;
            }
            fviz_free(cursor);
            for (i = 0u; i < cell_count; ++i)
            {
                const FVizSize begin = adjacency->offsets[i];
                const FVizSize neighbor_count = adjacency->offsets[i + 1u] - begin;
                if (neighbor_count > 1u)
                    qsort(&adjacency->neighbors[begin], neighbor_count, sizeof(FVizId), fviz_id_compare);
            }
        }
    }
    *out_adjacency = adjacency;
    adjacency = NULL;

done:
    fviz_release(adjacency);
    fviz_facet_records_release(records);
    fviz_release(pairs);
    fviz_release(records);
    fviz_release(facet_heads);
    return result;
}

FVizSize fviz_cell_adjacency_cell_count(const FVizCellAdjacency* adjacency)
{
    return adjacency != NULL ? adjacency->cell_count : 0u;
}

FVizSize fviz_cell_adjacency_neighbor_count(const FVizCellAdjacency* adjacency, FVizId cell_id)
{
    if (adjacency == NULL || (FVizSize)cell_id >= adjacency->cell_count) return 0u;
    return adjacency->offsets[(FVizSize)cell_id + 1u] - adjacency->offsets[(FVizSize)cell_id];
}

const FVizId* fviz_cell_adjacency_neighbors(const FVizCellAdjacency* adjacency, FVizId cell_id, FVizSize* out_count)
{
    FVizSize count = 0u;
    if (out_count != NULL) *out_count = 0u;
    if (adjacency == NULL || (FVizSize)cell_id >= adjacency->cell_count) return NULL;
    count = adjacency->offsets[(FVizSize)cell_id + 1u] - adjacency->offsets[(FVizSize)cell_id];
    if (out_count != NULL) *out_count = count;
    return count != 0u ? &adjacency->neighbors[adjacency->offsets[(FVizSize)cell_id]] : NULL;
}

FVizSize fviz_cell_adjacency_edge_count(const FVizCellAdjacency* adjacency)
{
    return adjacency != NULL ? adjacency->edge_count : 0u;
}
