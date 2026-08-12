#include <stdio.h>
#include <string.h>

#include <FViz/FViz.h>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <pthread.h>
#endif

#define THREAD_COUNT 4
#define ITERATION_COUNT 50000u

typedef struct WorkerContext
{
    FVizObject* object;
    int failed;
} WorkerContext;

static int run_worker(WorkerContext* context)
{
    unsigned int i;

    if (fviz_alloc_aligned(64u, 3u) != NULL)
    {
        context->failed = 1;
        return 0;
    }

    if (fviz_last_error_code() != FVIZ_ERROR_INVALID_ARGUMENT)
    {
        context->failed = 1;
        return 0;
    }

    for (i = 0u; i < ITERATION_COUNT; ++i)
    {
        if (fviz_retain(context->object) == NULL)
        {
            context->failed = 1;
            return 0;
        }
        fviz_release(context->object);
    }

    return 1;
}

#if defined(_WIN32)
static DWORD WINAPI worker_entry(LPVOID user_data)
{
    WorkerContext* context = (WorkerContext*)user_data;
    return run_worker(context) ? 0u : 1u;
}
#else
static void* worker_entry(void* user_data)
{
    WorkerContext* context = (WorkerContext*)user_data;
    (void)run_worker(context);
    return NULL;
}
#endif

static int require_true(int condition, const char* message)
{
    if (!condition)
    {
        fprintf(stderr, "FAILED: %s\n", message);
        return 0;
    }
    return 1;
}

int main(void)
{
    WorkerContext contexts[THREAD_COUNT];
    FVizObject* object = NULL;
    int i;

#if defined(_WIN32)
    HANDLE threads[THREAD_COUNT];
#else
    pthread_t threads[THREAD_COUNT];
#endif

    if (!require_true(fviz_object_create(&object) == FVIZ_OK, "failed to create shared object")) return 1;

    fviz_clear_last_error();
    if (!require_true(fviz_size_add(1u, 2u, NULL) == FVIZ_ERROR_INVALID_ARGUMENT, "failed to seed main-thread error")) return 1;
    if (!require_true(strcmp(fviz_last_error_message(), "out_value must not be NULL") == 0, "main-thread error message mismatch")) return 1;

    for (i = 0; i < THREAD_COUNT; ++i)
    {
        contexts[i].object = object;
        contexts[i].failed = 0;
#if defined(_WIN32)
        threads[i] = CreateThread(NULL, 0u, worker_entry, &contexts[i], 0u, NULL);
        if (!require_true(threads[i] != NULL, "CreateThread failed")) return 1;
#else
        if (!require_true(pthread_create(&threads[i], NULL, worker_entry, &contexts[i]) == 0, "pthread_create failed")) return 1;
#endif
    }

    for (i = 0; i < THREAD_COUNT; ++i)
    {
#if defined(_WIN32)
        if (!require_true(WaitForSingleObject(threads[i], INFINITE) == WAIT_OBJECT_0, "WaitForSingleObject failed")) return 1;
        (void)CloseHandle(threads[i]);
#else
        if (!require_true(pthread_join(threads[i], NULL) == 0, "pthread_join failed")) return 1;
#endif
        if (!require_true(contexts[i].failed == 0, "worker reported a runtime failure")) return 1;
    }

    if (!require_true(fviz_object_ref_count(object) == 1u, "threaded retain/release changed final ref count")) return 1;
    if (!require_true(fviz_last_error_code() == FVIZ_ERROR_INVALID_ARGUMENT, "worker error state leaked into main thread")) return 1;
    if (!require_true(strcmp(fviz_last_error_message(), "out_value must not be NULL") == 0, "TLS error message was overwritten by worker")) return 1;

    fviz_release(object);
    return 0;
}
