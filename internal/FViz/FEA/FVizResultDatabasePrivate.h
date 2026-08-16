#ifndef FVIZ_INTERNAL_FEA_RESULT_DATABASE_PRIVATE_H
#define FVIZ_INTERNAL_FEA_RESULT_DATABASE_PRIVATE_H

#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Core/FVizString.h>
#include <FViz/FEA/FVizResultDatabase.h>

typedef struct FVizFEAObservedField
{
    FVizFEAField* field;
    FVizObserverTag modified_tag;
} FVizFEAObservedField;

typedef struct FVizFEAObservedFrame
{
    FVizFEAFrame* frame;
    FVizObserverTag modified_tag;
} FVizFEAObservedFrame;

typedef struct FVizFEAObservedHistorySeries
{
    FVizFEAHistorySeries* series;
    FVizObserverTag modified_tag;
} FVizFEAObservedHistorySeries;

typedef struct FVizFEAObservedHistoryRegion
{
    FVizFEAHistoryRegion* region;
    FVizObserverTag modified_tag;
} FVizFEAObservedHistoryRegion;

typedef struct FVizFEAObservedStep
{
    FVizFEAStep* step;
    FVizObserverTag modified_tag;
} FVizFEAObservedStep;

struct FVizFEAHistorySeries
{
    FVizObject base;
    FVizString* name;
    FVizString* description;
    FVizArray* samples; /* FVizFEAHistorySample */
};

struct FVizFEAHistoryRegion
{
    FVizObject base;
    FVizString* name;
    FVizString* description;
    FVizArray* series; /* FVizFEAObservedHistorySeries */
};

struct FVizFEAFrame
{
    FVizObject base;
    int64_t frame_id;
    int64_t increment_number;
    double frame_value;
    double frequency;
    int64_t mode;
    FVizString* description;
    FVizArray* fields; /* FVizFEAObservedField */
};

struct FVizFEAStep
{
    FVizObject base;
    FVizString* name;
    FVizString* description;
    FVizFEAStepDomain domain;
    double time_period;
    FVizArray* frames;          /* FVizFEAObservedFrame */
    FVizArray* history_regions; /* FVizFEAObservedHistoryRegion */
};

struct FVizFEAResultDatabase
{
    FVizObject base;
    FVizArray* steps; /* FVizFEAObservedStep */
};

#endif /* FVIZ_INTERNAL_FEA_RESULT_DATABASE_PRIVATE_H */
