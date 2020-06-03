/*
 * (c) Copyright, Real-Time Innovations, 2020.  All rights reserved.
 * RTI grants Licensee a license to use, modify, compile, and create derivative
 * works of the software solely for use with RTI Connext DDS. Licensee may
 * redistribute copies of the software provided that all such copies are subject
 * to this license. The software is provided "as is", with no warranty of any
 * type, including any warranty for fitness for any purpose. RTI is under no
 * obligation to maintain or support the software. RTI shall not be liable for
 * any incidental or consequential damages arising out of the use or inability
 * to use the software.
 */

#ifndef APPLICATION_H
#define APPLICATION_H

#include <iostream>
#include <csignal>
#include <limits>
#include "ndds/ndds_cpp.h"

namespace application {


#ifdef RTI_WIN32
  /* strtok, fopen warnings */
  #pragma warning( disable : 4996 )
#endif

#ifdef RTI_WIN32
  #define DllExport __declspec( dllexport )
  #include <Winsock2.h>
  #include <process.h>
#else
  #define DllExport
  #include <sys/select.h>
  #include <semaphore.h>
  #include <pthread.h>
#endif


// A function that takes a void pointer, and is passed to the thread creation
// function.
typedef void* (*ThreadFunction)(void *);

class OSThread
{

public:
    OSThread(ThreadFunction function, void *function_param):
            function(function),
            function_param(function_param)
    {
    }

    // Run the thread
    void run()
    {
#ifdef RTI_WIN32
        thread = (HANDLE) _beginthread(
            (void(__cdecl*)(void*))function,
            0, (void*)function_param);
#else
        pthread_attr_t thread_attr;
        pthread_attr_init(&thread_attr);
        pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);
        int error = pthread_create(
                    &thread,
                    &thread_attr,
                    function,
                    (void *)function_param);
        pthread_attr_destroy(&thread_attr);
#endif
    }
    // Join the thread
    void join()
    {
#ifdef RTI_WIN32
        WaitForSingleObject(thread, INFINITE);
#else
        void *ret_val;
        int error = pthread_join(thread, &ret_val);
#endif
    }


private:
    // --- Private members ---

    // OS-specific thread definition
#ifdef RTI_WIN32
    HANDLE thread;
#else
    pthread_t thread;
#endif
    // Function called by OS-specific thread
    ThreadFunction function;

    // Parameter to the function
    void *function_param;
};



// Catch control-C and tell application to shut down
bool shutdown_requested = false;

inline void stop_handler(int)
{
    shutdown_requested = true;
    std::cout << "preparing to shut down..." << std::endl;
}

inline void setup_signal_handlers()
{
    signal(SIGINT, stop_handler);
    signal(SIGTERM, stop_handler);
}

enum ParseReturn { PARSE_RETURN_OK, PARSE_RETURN_FAILURE, PARSE_RETURN_EXIT };

struct ApplicationArguments {
    ParseReturn parse_result;
    unsigned int domain_id;
    unsigned int sample_count;
    char sensor_id[256];
    NDDS_Config_LogVerbosity verbosity;
};


// Parses application arguments for example.  Returns whether to exit.
inline void parse_arguments(
        ApplicationArguments& arguments,
        int argc,
        char *argv[])
{
    int arg_processing = 1;
    bool show_usage = false;
    arguments.domain_id = 0;
    arguments.sample_count = (std::numeric_limits<unsigned int>::max)();
    arguments.verbosity = NDDS_CONFIG_LOG_VERBOSITY_ERROR;
    arguments.parse_result = PARSE_RETURN_OK;

    while (arg_processing < argc) {
        if (strcmp(argv[arg_processing], "-d") == 0
                || strcmp(argv[arg_processing], "--domain") == 0) {
            arguments.domain_id = atoi(argv[arg_processing + 1]);
            arg_processing += 2;
        } else if (strcmp(argv[arg_processing], "-s") == 0
                || strcmp(argv[arg_processing], "--sample-count") == 0) {
            arguments.sample_count = atoi(argv[arg_processing + 1]);
            arg_processing += 2;
        } else if (strcmp(argv[arg_processing], "-id") == 0
                || strcmp(argv[arg_processing], "--sensor-id") == 0) {
            snprintf(arguments.sensor_id, 255, "%s", argv[arg_processing + 1]);
            arg_processing += 2;
        } else if (strcmp(argv[arg_processing], "-v") == 0
                || strcmp(argv[arg_processing], "--verbosity") == 0) {
            arguments.verbosity =
                    (NDDS_Config_LogVerbosity) atoi(argv[arg_processing + 1]);
            arg_processing += 2;
        } else if (strcmp(argv[arg_processing], "-h") == 0
                || strcmp(argv[arg_processing], "--help") == 0) {
            std::cout << "Example application." << std::endl;
            show_usage = true;
            arguments.parse_result = PARSE_RETURN_EXIT;
            break;
        } else {
            std::cout << "Bad parameter." << std::endl;
            show_usage = true;
            arguments.parse_result = PARSE_RETURN_FAILURE;
            break;
        }
    }
    if (show_usage) {
        std::cout << "Usage:\n"\
                    "    -d, --domain       <int>   Domain ID this application will\n" \
                    "                               subscribe in.  \n"
                    "                               Default: 0\n"\
                    "    -s, --sample_count <int>   Number of samples to receive before\n"\
                    "                               cleanly shutting down. \n"
                    "                               Default: infinite\n"
                    "    -v, --verbosity    <int>   How much debugging output to show.\n"\
                    "                               Range: 0-5 \n"
                    "                               Default: 0"
                << std::endl;
    }
}

}  // namespace application

#endif  // APPLICATION_H
