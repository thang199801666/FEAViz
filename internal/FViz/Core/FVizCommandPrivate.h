#ifndef FVIZ_INTERNAL_CORE_COMMAND_PRIVATE_H
#define FVIZ_INTERNAL_CORE_COMMAND_PRIVATE_H

#include <FViz/Core/FVizCommand.h>
#include <FViz/Core/FVizObjectPrivate.h>

struct FVizCommand
{
    FVizObject base;
    FVizCommandExecuteFn execute;
    void* client_data;
    FVizBool abort_flag;
};

#endif /* FVIZ_INTERNAL_CORE_COMMAND_PRIVATE_H */
