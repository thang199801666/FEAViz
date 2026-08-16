#include <FViz/Core/FVizCommand.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizObject.h>

#include <FViz/Core/FVizCommandPrivate.h>
#include <FViz/Core/FVizErrorInternal.h>

static const FVizObjectClass g_fviz_command_class = {
    FVIZ_TYPE_COMMAND,
    "FVizCommand",
    &g_fviz_object_class,
    NULL,
    NULL
};

FVizResult fviz_command_create(
    FVizCommandExecuteFn execute,
    void* client_data,
    FVizCommand** out_command)
{
    FVizCommand* command;
    if (out_command == NULL || execute == NULL)
    {
        if (out_command != NULL) *out_command = NULL;
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT,
            "out_command and execute must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_command = NULL;
    command = (FVizCommand*)fviz_internal_object_allocate(
        sizeof(FVizCommand), &g_fviz_command_class, NULL);
    if (command == NULL) return fviz_last_error_code();
    command->execute = execute;
    command->client_data = client_data;
    command->abort_flag = FVIZ_FALSE;
    *out_command = command;
    return FVIZ_OK;
}

void fviz_command_set_execute(FVizCommand* command, FVizCommandExecuteFn execute)
{
    if (command == NULL || execute == NULL) return;
    if (command->execute == execute) return;
    command->execute = execute;
    fviz_object_modified((FVizObject*)command);
}

FVizCommandExecuteFn fviz_command_execute_function(const FVizCommand* command)
{
    return command != NULL ? command->execute : NULL;
}

void fviz_command_set_client_data(FVizCommand* command, void* client_data)
{
    if (command == NULL || command->client_data == client_data) return;
    command->client_data = client_data;
    fviz_object_modified((FVizObject*)command);
}

void* fviz_command_client_data(FVizCommand* command)
{
    return command != NULL ? command->client_data : NULL;
}

const void* fviz_command_const_client_data(const FVizCommand* command)
{
    return command != NULL ? command->client_data : NULL;
}

void fviz_command_set_abort_flag(FVizCommand* command, FVizBool abort_flag)
{
    if (command != NULL)
        command->abort_flag = abort_flag != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
}

FVizBool fviz_command_abort_flag(const FVizCommand* command)
{
    return command != NULL ? command->abort_flag : FVIZ_FALSE;
}

FVizBool fviz_command_execute(
    FVizCommand* command,
    FVizObject* caller,
    FVizEventId event_id,
    void* call_data)
{
    FVizBool callback_abort;
    if (command == NULL || caller == NULL || command->execute == NULL) return FVIZ_FALSE;
    command->abort_flag = FVIZ_FALSE;
    callback_abort = command->execute(
        command, caller, event_id, call_data, command->client_data);
    if (callback_abort != FVIZ_FALSE) command->abort_flag = FVIZ_TRUE;
    return command->abort_flag;
}
