#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizHashMap.h>
#include <FViz/Core/FVizMemory.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Core/FVizHashMapPrivate.h>

#define FVIZ_HASH_MAP_MIN_CAPACITY 8u
#define FVIZ_HASH_MAP_MAX_LOAD 7u
#define FVIZ_HASH_MAP_LOAD_DENOM 10u

static void fviz_hash_map_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_hash_map_class = {
    FVIZ_TYPE_HASH_MAP,
    "FVizHashMap",
    &g_fviz_object_class,
    fviz_hash_map_destroy,
    NULL
};

static uint64_t fviz_hash_map_hash(FVizId key)
{
    uint64_t x = (uint64_t)key;
    x += UINT64_C(0x9E3779B97F4A7C15);
    x = (x ^ (x >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94D049BB133111EB);
    return x ^ (x >> 31);
}

static FVizResult fviz_hash_map_allocate_slots(
    const FVizAllocator* allocator,
    FVizSize capacity,
    FVizId** out_keys,
    void*** out_values,
    uint8_t** out_states)
{
    FVizSize keys_bytes;
    FVizSize values_bytes;
    FVizSize states_bytes;
    void* keys = NULL;
    void* values = NULL;
    void* states = NULL;
    if (fviz_size_multiply(capacity, sizeof(FVizId), &keys_bytes) != FVIZ_OK ||
        fviz_size_multiply(capacity, sizeof(void*), &values_bytes) != FVIZ_OK ||
        fviz_size_multiply(capacity, sizeof(uint8_t), &states_bytes) != FVIZ_OK)
    {
        return FVIZ_ERROR_OVERFLOW;
    }
    keys = fviz_allocator_allocate(allocator, keys_bytes, 0u);
    if (keys == NULL) return fviz_last_error_code();
    values = fviz_allocator_allocate(allocator, values_bytes, 0u);
    if (values == NULL)
    {
        fviz_allocator_deallocate(allocator, keys, keys_bytes, 0u);
        return fviz_last_error_code();
    }
    states = fviz_allocator_allocate(allocator, states_bytes, 0u);
    if (states == NULL)
    {
        fviz_allocator_deallocate(allocator, keys, keys_bytes, 0u);
        fviz_allocator_deallocate(allocator, values, values_bytes, 0u);
        return fviz_last_error_code();
    }
    (void)memset(keys, 0, keys_bytes);
    (void)memset(values, 0, values_bytes);
    (void)memset(states, FVIZ_HASH_MAP_SLOT_EMPTY, states_bytes);
    *out_keys = (FVizId*)keys;
    *out_values = (void**)values;
    *out_states = (uint8_t*)states;
    return FVIZ_OK;
}

static void fviz_hash_map_free_slots(
    const FVizAllocator* allocator,
    FVizId* keys,
    void** values,
    uint8_t* states,
    FVizSize capacity)
{
    FVizSize keys_bytes;
    FVizSize values_bytes;
    FVizSize states_bytes;
    (void)fviz_size_multiply(capacity, sizeof(FVizId), &keys_bytes);
    (void)fviz_size_multiply(capacity, sizeof(void*), &values_bytes);
    (void)fviz_size_multiply(capacity, sizeof(uint8_t), &states_bytes);
    if (keys != NULL) fviz_allocator_deallocate(allocator, keys, keys_bytes, 0u);
    if (values != NULL) fviz_allocator_deallocate(allocator, values, values_bytes, 0u);
    if (states != NULL) fviz_allocator_deallocate(allocator, states, states_bytes, 0u);
}

static void fviz_hash_map_destroy(FVizObject* object)
{
    FVizHashMap* map = (FVizHashMap*)object;
    fviz_hash_map_free_slots(
        &map->base.allocator, map->keys, map->values, map->states, map->capacity);
    map->keys = NULL;
    map->values = NULL;
    map->states = NULL;
    map->capacity = 0u;
    map->count = 0u;
    map->tombstones = 0u;
}

static FVizSize fviz_hash_map_round_up_pow2(FVizSize value)
{
    FVizSize result = FVIZ_HASH_MAP_MIN_CAPACITY;
    if (value > result)
    {
        while (result < value)
        {
            if (result > ((FVizSize)-1) / 2u)
            {
                return value;
            }
            result *= 2u;
        }
    }
    return result;
}

FVizResult fviz_hash_map_create_reserve(FVizSize initial_capacity, FVizHashMap** out_map)
{
    FVizHashMap* map;
    FVizSize capacity;
    FVizResult result;
    if (out_map == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_map must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_map = NULL;
    map = (FVizHashMap*)fviz_internal_object_allocate(sizeof(FVizHashMap), &g_fviz_hash_map_class, NULL);
    if (map == NULL)
    {
        return fviz_last_error_code();
    }
    capacity = fviz_hash_map_round_up_pow2(initial_capacity);
    result = fviz_hash_map_allocate_slots(&map->base.allocator, capacity, &map->keys, &map->values, &map->states);
    if (result != FVIZ_OK)
    {
        fviz_release(map);
        return result;
    }
    map->capacity = capacity;
    *out_map = map;
    return FVIZ_OK;
}

FVizResult fviz_hash_map_create(FVizHashMap** out_map)
{
    return fviz_hash_map_create_reserve(0u, out_map);
}

FVizSize fviz_hash_map_count(const FVizHashMap* map) { return map != NULL ? map->count : 0u; }
FVizSize fviz_hash_map_capacity(const FVizHashMap* map) { return map != NULL ? map->capacity : 0u; }

static FVizSize fviz_hash_map_find_slot(const FVizHashMap* map, FVizId key)
{
    FVizSize slot;
    FVizSize mask;
    if (map->capacity == 0u) return (FVizSize)-1;
    mask = map->capacity - 1u;
    slot = (FVizSize)fviz_hash_map_hash(key) & mask;
    while (map->states[slot] != FVIZ_HASH_MAP_SLOT_EMPTY)
    {
        if (map->states[slot] == FVIZ_HASH_MAP_SLOT_OCCUPIED && map->keys[slot] == key)
        {
            return slot;
        }
        slot = (slot + 1u) & mask;
    }
    return slot;
}

static FVizResult fviz_hash_map_grow(FVizHashMap* map)
{
    FVizSize new_capacity;
    FVizId* new_keys;
    void** new_values;
    uint8_t* new_states;
    FVizSize i;
    FVizResult result;
    new_capacity = map->capacity * 2u;
    result = fviz_hash_map_allocate_slots(&map->base.allocator, new_capacity, &new_keys, &new_values, &new_states);
    if (result != FVIZ_OK)
    {
        return result;
    }
    for (i = 0u; i < map->capacity; ++i)
    {
        if (map->states[i] == FVIZ_HASH_MAP_SLOT_OCCUPIED)
        {
            FVizSize slot = (FVizSize)fviz_hash_map_hash(map->keys[i]) & (new_capacity - 1u);
            while (new_states[slot] != FVIZ_HASH_MAP_SLOT_EMPTY)
            {
                slot = (slot + 1u) & (new_capacity - 1u);
            }
            new_keys[slot] = map->keys[i];
            new_values[slot] = map->values[i];
            new_states[slot] = FVIZ_HASH_MAP_SLOT_OCCUPIED;
        }
    }
    fviz_hash_map_free_slots(&map->base.allocator, map->keys, map->values, map->states, map->capacity);
    map->keys = new_keys;
    map->values = new_values;
    map->states = new_states;
    map->capacity = new_capacity;
    map->tombstones = 0u;
    return FVIZ_OK;
}

FVizResult fviz_hash_map_set(FVizHashMap* map, FVizId key, void* value)
{
    FVizSize slot;
    if (map == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "map must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    slot = fviz_hash_map_find_slot(map, key);
    if (slot != (FVizSize)-1 && map->states[slot] == FVIZ_HASH_MAP_SLOT_OCCUPIED)
    {
        map->values[slot] = value;
        fviz_object_modified((FVizObject*)map);
        return FVIZ_OK;
    }
    if ((map->count + map->tombstones + 1u) * FVIZ_HASH_MAP_LOAD_DENOM >= map->capacity * FVIZ_HASH_MAP_MAX_LOAD)
    {
        FVizResult result = fviz_hash_map_grow(map);
        if (result != FVIZ_OK) return result;
        slot = fviz_hash_map_find_slot(map, key);
    }
    if (slot == (FVizSize)-1)
    {
        slot = (FVizSize)fviz_hash_map_hash(key) & (map->capacity - 1u);
        while (map->states[slot] == FVIZ_HASH_MAP_SLOT_OCCUPIED)
        {
            slot = (slot + 1u) & (map->capacity - 1u);
        }
    }
    map->keys[slot] = key;
    map->values[slot] = value;
    map->states[slot] = FVIZ_HASH_MAP_SLOT_OCCUPIED;
    map->count += 1u;
    fviz_object_modified((FVizObject*)map);
    return FVIZ_OK;
}

FVizBool fviz_hash_map_get(const FVizHashMap* map, FVizId key, void** out_value)
{
    FVizSize slot;
    if (out_value != NULL) *out_value = NULL;
    if (map == NULL || map->capacity == 0u) return FVIZ_FALSE;
    slot = fviz_hash_map_find_slot(map, key);
    if (slot == (FVizSize)-1 || map->states[slot] != FVIZ_HASH_MAP_SLOT_OCCUPIED)
    {
        return FVIZ_FALSE;
    }
    if (out_value != NULL) *out_value = map->values[slot];
    return FVIZ_TRUE;
}

FVizBool fviz_hash_map_contains(const FVizHashMap* map, FVizId key)
{
    return fviz_hash_map_get(map, key, NULL);
}

FVizBool fviz_hash_map_erase(FVizHashMap* map, FVizId key)
{
    FVizSize slot;
    if (map == NULL || map->capacity == 0u) return FVIZ_FALSE;
    slot = fviz_hash_map_find_slot(map, key);
    if (slot == (FVizSize)-1 || map->states[slot] != FVIZ_HASH_MAP_SLOT_OCCUPIED)
    {
        return FVIZ_FALSE;
    }
    map->states[slot] = FVIZ_HASH_MAP_SLOT_TOMBSTONE;
    map->values[slot] = NULL;
    map->count -= 1u;
    map->tombstones += 1u;
    fviz_object_modified((FVizObject*)map);
    return FVIZ_TRUE;
}

void fviz_hash_map_clear(FVizHashMap* map)
{
    if (map == NULL || map->capacity == 0u || map->count == 0u) return;
    (void)memset(map->states, FVIZ_HASH_MAP_SLOT_EMPTY, map->capacity);
    map->count = 0u;
    map->tombstones = 0u;
    fviz_object_modified((FVizObject*)map);
}

FVizBool fviz_hash_map_iterate(FVizHashMap* map, FVizSize* cursor, FVizId* out_key, void** out_value)
{
    FVizSize i;
    if (map == NULL || cursor == NULL || map->capacity == 0u) return FVIZ_FALSE;
    for (i = *cursor; i < map->capacity; ++i)
    {
        if (map->states[i] == FVIZ_HASH_MAP_SLOT_OCCUPIED)
        {
            if (out_key != NULL) *out_key = map->keys[i];
            if (out_value != NULL) *out_value = map->values[i];
            *cursor = i + 1u;
            return FVIZ_TRUE;
        }
    }
    return FVIZ_FALSE;
}
