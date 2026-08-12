#include <stdio.h>

#include <FViz/FViz.h>

static void example_log_callback(
    FVizLogLevel level,
    const char* category,
    const char* message,
    void* user_data)
{
    FILE* stream = (FILE*)user_data;
    (void)fprintf(
        stream,
        "[%s][%s] %s\n",
        fviz_log_level_string(level),
        category != NULL ? category : "General",
        message);
}

int main(void)
{
    FVizObject* object = NULL;
    void* memory;

    fviz_log_set_callback(example_log_callback, stdout);
    (void)fviz_log_set_level(FVIZ_LOG_INFO);
    fviz_log_message(FVIZ_LOG_INFO, "Core", "FEAViz core runtime example started");

    memory = fviz_alloc_aligned(1024u, 64u);
    if (memory == NULL)
    {
        (void)fprintf(stderr, "Allocation failed: %s\n", fviz_last_error_message());
        return 1;
    }

    if (fviz_object_create(&object) != FVIZ_OK)
    {
        (void)fprintf(stderr, "Object creation failed: %s\n", fviz_last_error_message());
        fviz_free(memory);
        return 1;
    }

    (void)printf(
        "Created %s, type=0x%016llx, refs=%u\n",
        fviz_object_type_name(object),
        (unsigned long long)fviz_object_type_id(object),
        (unsigned int)fviz_object_ref_count(object));

    (void)fviz_retain(object);
    (void)printf("After retain: refs=%u\n", (unsigned int)fviz_object_ref_count(object));
    fviz_release(object);
    fviz_release(object);
    fviz_free(memory);

    fviz_log_message(FVIZ_LOG_INFO, "Core", "FEAViz core runtime example completed");
    fviz_log_reset_callback();
    return 0;
}
