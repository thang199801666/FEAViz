#ifndef FVIZ_INTERNAL_CORE_HASH_MAP_PRIVATE_H
#define FVIZ_INTERNAL_CORE_HASH_MAP_PRIVATE_H

#include <stdint.h>

#include <FViz/Core/FVizHashMap.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Core/FVizTypes.h>

enum FVizHashMapState
{
    FVIZ_HASH_MAP_SLOT_EMPTY = 0,
    FVIZ_HASH_MAP_SLOT_OCCUPIED = 1,
    FVIZ_HASH_MAP_SLOT_TOMBSTONE = 2
};

struct FVizHashMap
{
    FVizObject base;
    FVizId* keys;
    void** values;
    uint8_t* states;
    FVizSize capacity;
    FVizSize count;
    FVizSize tombstones;
};

#endif /* FVIZ_INTERNAL_CORE_HASH_MAP_PRIVATE_H */
