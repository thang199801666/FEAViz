#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Mesh/FVizCellLinks.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

struct FVizCellLinks
{
    FVizObject base;
    FVizSize point_count;
    FVizSize cell_count;
    FVizSize incidence_count;
    FVizSize* offsets;
    FVizId* cell_ids;
};

static void fviz_cell_links_destroy(FVizObject* object)
{
    FVizCellLinks* links = (FVizCellLinks*)object;
    fviz_free(links->cell_ids);
    fviz_free(links->offsets);
    links->cell_ids = NULL;
    links->offsets = NULL;
}

static const FVizObjectClass g_fviz_cell_links_class = {
    FVIZ_TYPE_CELL_LINKS,
    "FVizCellLinks",
    &g_fviz_object_class,
    fviz_cell_links_destroy,
    NULL
};

FVizResult fviz_cell_links_build(
    const FVizCellArray* cells,
    FVizSize point_count,
    FVizCellLinks** out_links)
{
    FVizCellLinks* links = NULL;
    FVizSize* cursor = NULL;
    FVizSize offset_bytes;
    FVizSize id_bytes;
    FVizSize cell_id;
    FVizSize total = 0u;
    if (out_links == NULL || cells == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "cell links require cells and an output pointer");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_links = NULL;
    if (fviz_cell_array_validate(cells, point_count) != FVIZ_OK) return fviz_last_error_code();
    if (point_count == (FVizSize)-1 ||
        fviz_size_multiply(point_count + 1u, sizeof(FVizSize), &offset_bytes) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;

    links = (FVizCellLinks*)fviz_internal_object_allocate(
        sizeof(*links), &g_fviz_cell_links_class, NULL);
    if (links == NULL) return fviz_last_error_code();
    links->point_count = point_count;
    links->cell_count = fviz_cell_array_count(cells);
    links->offsets = (FVizSize*)fviz_alloc(offset_bytes);
    if (links->offsets == NULL)
    {
        fviz_release(links);
        return fviz_last_error_code();
    }
    (void)memset(links->offsets, 0, offset_bytes);

    for (cell_id = 0u; cell_id < links->cell_count; ++cell_id)
    {
        FVizCellView view;
        FVizSize i;
        if (fviz_cell_array_cell_view(cells, cell_id, &view) != FVIZ_OK)
        {
            fviz_release(links);
            return fviz_last_error_code();
        }
        if (view.point_count > (FVizSize)-1 - total)
        {
            fviz_release(links);
            return FVIZ_ERROR_OVERFLOW;
        }
        total += view.point_count;
        for (i = 0u; i < view.point_count; ++i)
        {
            const FVizId point_id = fviz_cell_view_point_id(&view, i);
            ++links->offsets[(FVizSize)point_id + 1u];
        }
    }
    {
        FVizSize i;
        for (i = 1u; i <= point_count; ++i)
            links->offsets[i] += links->offsets[i - 1u];
    }
    links->incidence_count = total;
    if (total != 0u)
    {
        if (fviz_size_multiply(total, sizeof(FVizId), &id_bytes) != FVIZ_OK)
        {
            fviz_release(links);
            return FVIZ_ERROR_OVERFLOW;
        }
        links->cell_ids = (FVizId*)fviz_alloc(id_bytes);
        if (links->cell_ids == NULL)
        {
            fviz_release(links);
            return fviz_last_error_code();
        }
    }
    if (point_count != 0u)
    {
        FVizSize cursor_bytes;
        if (fviz_size_multiply(point_count, sizeof(FVizSize), &cursor_bytes) != FVIZ_OK)
        {
            fviz_release(links);
            return FVIZ_ERROR_OVERFLOW;
        }
        cursor = (FVizSize*)fviz_alloc(cursor_bytes);
        if (cursor == NULL)
        {
            fviz_release(links);
            return fviz_last_error_code();
        }
        (void)memcpy(cursor, links->offsets, cursor_bytes);
    }
    for (cell_id = 0u; cell_id < links->cell_count; ++cell_id)
    {
        FVizCellView view;
        FVizSize i;
        (void)fviz_cell_array_cell_view(cells, cell_id, &view);
        for (i = 0u; i < view.point_count; ++i)
        {
            const FVizSize point_id = (FVizSize)fviz_cell_view_point_id(&view, i);
            links->cell_ids[cursor[point_id]++] = (FVizId)cell_id;
        }
    }
    fviz_free(cursor);
    *out_links = links;
    return FVIZ_OK;
}

FVizSize fviz_cell_links_point_count(const FVizCellLinks* links)
{
    return links != NULL ? links->point_count : 0u;
}

FVizSize fviz_cell_links_cell_count(const FVizCellLinks* links)
{
    return links != NULL ? links->cell_count : 0u;
}

FVizSize fviz_cell_links_incidence_count(const FVizCellLinks* links)
{
    return links != NULL ? links->incidence_count : 0u;
}

FVizSize fviz_cell_links_cell_count_for_point(const FVizCellLinks* links, FVizId point_id)
{
    if (links == NULL || (FVizSize)point_id >= links->point_count) return 0u;
    return links->offsets[(FVizSize)point_id + 1u] - links->offsets[(FVizSize)point_id];
}

const FVizId* fviz_cell_links_cells_for_point(
    const FVizCellLinks* links,
    FVizId point_id,
    FVizSize* out_count)
{
    FVizSize count = 0u;
    if (out_count != NULL) *out_count = 0u;
    if (links == NULL || (FVizSize)point_id >= links->point_count) return NULL;
    count = links->offsets[(FVizSize)point_id + 1u] - links->offsets[(FVizSize)point_id];
    if (out_count != NULL) *out_count = count;
    return count != 0u ? &links->cell_ids[links->offsets[(FVizSize)point_id]] : NULL;
}
