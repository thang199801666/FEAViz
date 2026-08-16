#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Core/FVizString.h>
#include <FViz/IO/FVizPVD.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizObjectPrivate.h>

typedef struct FVizPVDEntry
{
    double time;
    uint32_t part;
    FVizString* group;
    FVizString* file;
} FVizPVDEntry;

struct FVizPVDCollection
{
    FVizObject base;
    FVizArray* entries;
};

static void fviz_pvd_collection_destroy(FVizObject* object);
static FVizMTime fviz_pvd_collection_mtime(const FVizObject* object);
static const FVizObjectClass g_fviz_pvd_collection_class = {
    FVIZ_TYPE_PVD_COLLECTION, "FVizPVDCollection", &g_fviz_object_class,
    fviz_pvd_collection_destroy, fviz_pvd_collection_mtime
};

static void fviz_pvd_entry_release(FVizPVDEntry* entry)
{
    if (entry == NULL) return;
    fviz_release(entry->group);
    fviz_release(entry->file);
    entry->group = NULL;
    entry->file = NULL;
}

static FVizMTime fviz_pvd_collection_mtime(const FVizObject* object)
{
    const FVizPVDCollection* collection = (const FVizPVDCollection*)object;
    FVizMTime mtime = fviz_internal_object_local_mtime(object);
    FVizSize i;
    if (collection == NULL || collection->entries == NULL) return mtime;
    for (i = 0u; i < fviz_array_count(collection->entries); ++i)
    {
        const FVizPVDEntry* entry = (const FVizPVDEntry*)fviz_array_const_at(collection->entries, i);
        FVizMTime child;
        if (entry->group != NULL)
        {
            child = fviz_object_mtime((const FVizObject*)entry->group);
            if (child > mtime) mtime = child;
        }
        if (entry->file != NULL)
        {
            child = fviz_object_mtime((const FVizObject*)entry->file);
            if (child > mtime) mtime = child;
        }
    }
    return mtime;
}

static void fviz_pvd_collection_destroy(FVizObject* object)
{
    FVizPVDCollection* collection = (FVizPVDCollection*)object;
    fviz_pvd_collection_clear(collection);
    fviz_release(collection->entries);
    collection->entries = NULL;
}

FVizResult fviz_pvd_collection_create(FVizPVDCollection** out_collection)
{
    FVizPVDCollection* collection;
    if (out_collection == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_collection must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_collection = NULL;
    collection = (FVizPVDCollection*)fviz_internal_object_allocate(
        sizeof(*collection), &g_fviz_pvd_collection_class, NULL);
    if (collection == NULL) return fviz_last_error_code();
    if (fviz_array_create(sizeof(FVizPVDEntry), &collection->entries) != FVIZ_OK)
    {
        fviz_release(collection);
        return fviz_last_error_code();
    }
    *out_collection = collection;
    return FVIZ_OK;
}

FVizSize fviz_pvd_collection_count(const FVizPVDCollection* collection)
{
    return collection != NULL && collection->entries != NULL ? fviz_array_count(collection->entries) : 0u;
}

static FVizSize fviz_pvd_lower_bound(const FVizPVDCollection* collection, double time)
{
    FVizSize first = 0u;
    FVizSize count = fviz_pvd_collection_count(collection);
    while (count > 0u)
    {
        const FVizSize half = count / 2u;
        const FVizSize index = first + half;
        const FVizPVDEntry* entry = (const FVizPVDEntry*)fviz_array_const_at(collection->entries, index);
        if (entry->time < time)
        {
            first = index + 1u;
            count -= half + 1u;
        }
        else count = half;
    }
    return first;
}

FVizResult fviz_pvd_collection_add(
    FVizPVDCollection* collection,
    double time,
    uint32_t part,
    const char* group,
    const char* file,
    FVizSize* out_index)
{
    FVizPVDEntry entry;
    FVizPVDEntry* entries;
    FVizSize index;
    FVizSize count;
    if (out_index != NULL) *out_index = 0u;
    if (collection == NULL || !isfinite(time) || file == NULL || file[0] == '\0')
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "PVD entry requires finite time and file path");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    (void)memset(&entry, 0, sizeof(entry));
    entry.time = time;
    entry.part = part;
    if (fviz_string_create_from(group != NULL ? group : "", &entry.group) != FVIZ_OK ||
        fviz_string_create_from(file, &entry.file) != FVIZ_OK)
    {
        fviz_pvd_entry_release(&entry);
        return fviz_last_error_code();
    }
    index = fviz_pvd_lower_bound(collection, time);
    count = fviz_pvd_collection_count(collection);
    /* Multiple parts/groups may share a timestep. Keep equal times adjacent and stable. */
    while (index < count)
    {
        const FVizPVDEntry* current = (const FVizPVDEntry*)fviz_array_const_at(collection->entries, index);
        if (current->time != time) break;
        ++index;
    }
    if (fviz_array_resize(collection->entries, count + 1u) != FVIZ_OK)
    {
        fviz_pvd_entry_release(&entry);
        return fviz_last_error_code();
    }
    entries = (FVizPVDEntry*)fviz_array_data(collection->entries);
    if (index < count)
        (void)memmove(&entries[index + 1u], &entries[index], (size_t)(count - index) * sizeof(*entries));
    entries[index] = entry;
    if (out_index != NULL) *out_index = index;
    fviz_object_modified((FVizObject*)collection);
    return FVIZ_OK;
}

double fviz_pvd_collection_time(const FVizPVDCollection* collection, FVizSize index)
{
    const FVizPVDEntry* entry;
    if (collection == NULL || index >= fviz_pvd_collection_count(collection)) return 0.0;
    entry = (const FVizPVDEntry*)fviz_array_const_at(collection->entries, index);
    return entry != NULL ? entry->time : 0.0;
}

uint32_t fviz_pvd_collection_part(const FVizPVDCollection* collection, FVizSize index)
{
    const FVizPVDEntry* entry;
    if (collection == NULL || index >= fviz_pvd_collection_count(collection)) return 0u;
    entry = (const FVizPVDEntry*)fviz_array_const_at(collection->entries, index);
    return entry != NULL ? entry->part : 0u;
}

const char* fviz_pvd_collection_group(const FVizPVDCollection* collection, FVizSize index)
{
    const FVizPVDEntry* entry;
    if (collection == NULL || index >= fviz_pvd_collection_count(collection)) return NULL;
    entry = (const FVizPVDEntry*)fviz_array_const_at(collection->entries, index);
    return entry != NULL && entry->group != NULL ? fviz_string_c_str(entry->group) : NULL;
}

const char* fviz_pvd_collection_file(const FVizPVDCollection* collection, FVizSize index)
{
    const FVizPVDEntry* entry;
    if (collection == NULL || index >= fviz_pvd_collection_count(collection)) return NULL;
    entry = (const FVizPVDEntry*)fviz_array_const_at(collection->entries, index);
    return entry != NULL && entry->file != NULL ? fviz_string_c_str(entry->file) : NULL;
}

FVizResult fviz_pvd_collection_time_range(const FVizPVDCollection* collection, double* out_minimum, double* out_maximum)
{
    const FVizSize count = fviz_pvd_collection_count(collection);
    if (out_minimum != NULL) *out_minimum = 0.0;
    if (out_maximum != NULL) *out_maximum = 0.0;
    if (collection == NULL || out_minimum == NULL || out_maximum == NULL || count == 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "PVD time range requires a non-empty collection");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_minimum = fviz_pvd_collection_time(collection, 0u);
    *out_maximum = fviz_pvd_collection_time(collection, count - 1u);
    return FVIZ_OK;
}

FVizResult fviz_pvd_collection_find_nearest(const FVizPVDCollection* collection, double time, FVizSize* out_index)
{
    FVizSize index;
    FVizSize count;
    if (out_index != NULL) *out_index = 0u;
    if (collection == NULL || out_index == NULL || !isfinite(time) ||
        (count = fviz_pvd_collection_count(collection)) == 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "PVD nearest-time query is invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    index = fviz_pvd_lower_bound(collection, time);
    if (index == 0u) *out_index = 0u;
    else if (index >= count) *out_index = count - 1u;
    else
    {
        const double before = fviz_pvd_collection_time(collection, index - 1u);
        const double after = fviz_pvd_collection_time(collection, index);
        *out_index = fabs(time - before) <= fabs(after - time) ? index - 1u : index;
    }
    /* Return the first entry for that timestep, so grouped/partitioned frames are deterministic. */
    while (*out_index > 0u && fviz_pvd_collection_time(collection, *out_index - 1u) ==
        fviz_pvd_collection_time(collection, *out_index)) --(*out_index);
    return FVIZ_OK;
}

void fviz_pvd_collection_clear(FVizPVDCollection* collection)
{
    FVizSize i;
    FVizSize count;
    if (collection == NULL || collection->entries == NULL) return;
    count = fviz_array_count(collection->entries);
    for (i = 0u; i < count; ++i)
        fviz_pvd_entry_release((FVizPVDEntry*)fviz_array_at(collection->entries, i));
    if (count > 0u)
    {
        fviz_array_clear(collection->entries);
        fviz_object_modified((FVizObject*)collection);
    }
}

static FVizBool fviz_pvd_attr(const char* tag, const char* name, char* out, FVizSize capacity)
{
    char pattern[64];
    const char* start;
    const char* end;
    FVizSize written=0u;
    if (tag==NULL || name==NULL || out==NULL || capacity==0u) return FVIZ_FALSE;
    (void)snprintf(pattern, sizeof(pattern), "%s=\"", name);
    {
        const char* tag_end = strchr(tag, '>');
        start = strstr(tag, pattern);
        if (start == NULL || tag_end == NULL || start >= tag_end) return FVIZ_FALSE;
    }
    start += strlen(pattern);
    end = strchr(start, '"');
    if (end == NULL) return FVIZ_FALSE;
    while (start<end && written+1u<capacity)
    {
        char decoded=*start;
        FVizSize consumed=1u;
        if (*start=='&')
        {
            if ((FVizSize)(end-start)>=5u && strncmp(start,"&amp;",5u)==0) { decoded='&'; consumed=5u; }
            else if ((FVizSize)(end-start)>=4u && strncmp(start,"&lt;",4u)==0) { decoded='<'; consumed=4u; }
            else if ((FVizSize)(end-start)>=4u && strncmp(start,"&gt;",4u)==0) { decoded='>'; consumed=4u; }
            else if ((FVizSize)(end-start)>=6u && strncmp(start,"&quot;",6u)==0) { decoded='"'; consumed=6u; }
            else if ((FVizSize)(end-start)>=6u && strncmp(start,"&apos;",6u)==0) { decoded='\''; consumed=6u; }
        }
        out[written++]=decoded;
        start += consumed;
    }
    out[written]='\0';
    return FVIZ_TRUE;
}

FVizResult fviz_pvd_read(const char* file_path, FVizPVDCollection** out_collection)
{
    FILE* file = NULL;
    long size_long;
    FVizSize size;
    char* text = NULL;
    char* cursor;
    FVizPVDCollection* collection = NULL;
    FVizResult result = FVIZ_OK;
    if (out_collection != NULL) *out_collection = NULL;
    if (file_path == NULL || out_collection == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "PVD reader arguments are invalid");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    file = fopen(file_path, "rb");
    if (file == NULL) { fviz_internal_set_error(FVIZ_ERROR_IO, "failed to open PVD file"); return FVIZ_ERROR_IO; }
    if (fseek(file, 0, SEEK_END) != 0 || (size_long = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) != 0)
    { result = FVIZ_ERROR_IO; goto cleanup; }
    size = (FVizSize)size_long;
    if (size > 64u * 1024u * 1024u) { result=FVIZ_ERROR_OVERFLOW; goto cleanup; }
    text = (char*)fviz_alloc(size + 1u);
    if (text == NULL) { result=fviz_last_error_code(); goto cleanup; }
    if (size > 0u && fread(text,1u,size,file)!=size) { result=FVIZ_ERROR_IO; goto cleanup; }
    text[size]='\0';
    if (strstr(text,"type=\"Collection\"")==NULL || fviz_pvd_collection_create(&collection)!=FVIZ_OK)
    { result=fviz_last_error_code()==FVIZ_OK?FVIZ_ERROR_PARSE:fviz_last_error_code(); goto cleanup; }
    cursor = text;
    while ((cursor = strstr(cursor, "<DataSet")) != NULL)
    {
        const char* tag_end = strchr(cursor, '>');
        char time_text[64], part_text[64], group[256], path[2048];
        char* parse_end = NULL;
        double time;
        unsigned long part = 0u;
        if (tag_end == NULL) { result=FVIZ_ERROR_PARSE; goto cleanup; }
        if (!fviz_pvd_attr(cursor,"timestep",time_text,sizeof(time_text)) ||
            !fviz_pvd_attr(cursor,"file",path,sizeof(path)))
        { result=FVIZ_ERROR_PARSE; goto cleanup; }
        time = strtod(time_text,&parse_end);
        if (parse_end==time_text || !isfinite(time)) { result=FVIZ_ERROR_PARSE; goto cleanup; }
        group[0]='\0'; part_text[0]='\0';
        (void)fviz_pvd_attr(cursor,"group",group,sizeof(group));
        if (fviz_pvd_attr(cursor,"part",part_text,sizeof(part_text))) part=strtoul(part_text,NULL,10);
        if (part > UINT32_MAX || fviz_pvd_collection_add(collection,time,(uint32_t)part,group,path,NULL)!=FVIZ_OK)
        { result=fviz_last_error_code()==FVIZ_OK?FVIZ_ERROR_PARSE:fviz_last_error_code(); goto cleanup; }
        cursor = (char*)tag_end + 1;
    }
    if (fviz_pvd_collection_count(collection)==0u)
    { result=FVIZ_ERROR_PARSE; fviz_internal_set_error(FVIZ_ERROR_PARSE,"PVD collection contains no datasets"); goto cleanup; }
    *out_collection=collection; collection=NULL;
cleanup:
    if (file != NULL) (void)fclose(file);
    fviz_free(text);
    fviz_release(collection);
    if (result!=FVIZ_OK && fviz_last_error_code()==FVIZ_OK)
        fviz_internal_set_error(result, result==FVIZ_ERROR_IO?"failed to read PVD file":"invalid PVD collection");
    return result;
}

static FVizBool fviz_pvd_write_escaped(FILE* file, const char* value)
{
    const unsigned char* cursor=(const unsigned char*)(value!=NULL?value:"");
    while (*cursor!=0u)
    {
        const char* replacement=NULL;
        if (*cursor=='&') replacement="&amp;"; else if (*cursor=='<') replacement="&lt;";
        else if (*cursor=='>') replacement="&gt;"; else if (*cursor=='"') replacement="&quot;";
        else if (*cursor=='\'') replacement="&apos;";
        if (replacement!=NULL) { if (fputs(replacement,file)==EOF) return FVIZ_FALSE; }
        else if (fputc((int)*cursor,file)==EOF) return FVIZ_FALSE;
        ++cursor;
    }
    return FVIZ_TRUE;
}

FVizResult fviz_pvd_write(const char* file_path, const FVizPVDCollection* collection)
{
    FILE* file;
    FVizSize i;
    if (file_path==NULL || collection==NULL || fviz_pvd_collection_count(collection)==0u)
    { fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,"PVD writer requires a non-empty collection"); return FVIZ_ERROR_INVALID_ARGUMENT; }
    file=fopen(file_path,"wb");
    if (file==NULL) { fviz_internal_set_error(FVIZ_ERROR_IO,"failed to open PVD output"); return FVIZ_ERROR_IO; }
    if (fputs("<?xml version=\"1.0\"?>\n<VTKFile type=\"Collection\" version=\"0.1\" byte_order=\"LittleEndian\">\n  <Collection>\n",file)==EOF) goto fail;
    for (i=0u;i<fviz_pvd_collection_count(collection);++i)
    {
        if (fprintf(file,"    <DataSet timestep=\"%.17g\" group=\"",fviz_pvd_collection_time(collection,i))<0 ||
            fviz_pvd_write_escaped(file,fviz_pvd_collection_group(collection,i))==FVIZ_FALSE ||
            fprintf(file,"\" part=\"%u\" file=\"",fviz_pvd_collection_part(collection,i))<0 ||
            fviz_pvd_write_escaped(file,fviz_pvd_collection_file(collection,i))==FVIZ_FALSE ||
            fputs("\"/>\n",file)==EOF) goto fail;
    }
    if (fputs("  </Collection>\n</VTKFile>\n",file)==EOF || fclose(file)!=0)
    { file=NULL; goto fail; }
    return FVIZ_OK;
fail:
    if (file!=NULL) (void)fclose(file);
    fviz_internal_set_error(FVIZ_ERROR_IO,"failed while writing PVD file");
    return FVIZ_ERROR_IO;
}
