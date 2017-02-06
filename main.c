/**
 * @file main.c
 */

/* Core library */
#include "core.h"
#include "core_general.h"
#define TRACE_MODULE _main_
#include "core_debug.h"
#include "core_cond.h"
#include "core_thread.h"
#include "core_signal.h"
#include "core_net.h"

#include "logger.h"
#include "symtbl.h"

/* Server */
#include "cellwire.h"

thread_id thr_sm;
#define THREAD_SM_STACK_SIZE
#define THREAD_SM_PRIORITY
extern void *THREAD_FUNC sm_main(void *data);

thread_id thr_dp;
#define THREAD_DP_STACK_SIZE
#define THREAD_DP_PRIORITY
extern void *THREAD_FUNC dp_main(void *data);

thread_id thr_test;

char *config_path = NULL;
extern char g_compile_time[];

static void show_version()
{
    printf("CellWire daemon v%s - %s\n", 
            PACKAGE_VERSION, g_compile_time);
}

static void show_help()
{
    show_version();

    printf("\n"
           "Usage: cellwired [arguments]\n"
           "\n"
           "Arguments:\n"
           "   -v                   Show version\n"
           "   -h                   Show help\n"
           "   -d                   Start as daemon\n"
           "   -f                   Set configuration file name\n"
           "   -l log_path          Fork log daemon with file path to be logged to\n"
           "\n"
           );
}

static void threads_start()
{
    status_t rv;

#if 0
    rv = thread_create(&thr_sm, NULL, sm_main, NULL);
    d_assert(rv == CORE_OK, return,
            "State machine thread creation failed");

    rv = thread_create(&thr_dp, NULL, dp_main, NULL);
    d_assert(rv == CORE_OK, return,
            "Control path thread creation failed");
#endif
}

static void threads_stop()
{
#if 0
    thread_delete(thr_sm);
    thread_delete(thr_dp);
#endif
}

static int check_signal(int signum)
{
    switch (signum)
    {
        case SIGTERM:
        case SIGINT:
        {
            d_info("%s received", 
                    signum == SIGTERM ? "SIGTERM" : "SIGINT");

            threads_stop();

            return 1;
        }
        case SIGHUP:
        {

            d_info("SIGHUP received");

            threads_stop();

            cellwire_terminate();

            core_terminate();

            core_initialize();

            cellwire_initialize(config_path);

            threads_start();

            break;
        }
        default:
        {
            d_error("Unknown Signal Number = %d\n", signum);
            break;
        }
            
    }
    return 0;
}

void logger_signal(int signum)
{
    switch (signum)
    {
        case SIGTERM:
        case SIGINT:
            logger_stop();
            break;
        case SIGHUP:
            break;
        default:
            break;
    }
}

int main(int argc, char *argv[])
{
    int opt;
    int opt_daemon = 0;
    int opt_logger = 0;

    char *log_path = NULL;

    /**************************************************************************
     * Starting up process.
     *
     * Keep the order of starting-up
     */

    while (1)
    {
        opt = getopt (argc, argv, "vhdf:l:");
        if (opt == -1)
            break;

        switch (opt)
        {
            case 'v':
                show_version();
                return EXIT_SUCCESS;
            case 'h':
                show_help();
                return EXIT_SUCCESS;
            case 'd':
                opt_daemon = 1;
                break;
            case 'f':
                config_path = optarg;
                break;
            case 'l':
                opt_logger = 1;
                log_path = optarg;
                break;
            default:
                show_help();
                return EXIT_FAILURE;
        }
    }

    if (opt_daemon)
    {
        pid_t pid;
        pid = fork();

        d_assert(pid != -1, return EXIT_FAILURE, "fork() failed");

        if (pid != 0)
        {
            /* Parent */
            return EXIT_SUCCESS;
        }
        /* Child */

        setsid();
        umask(027);
    }

    core_initialize();

    if (opt_logger)
    {
        pid_t pid;
        pid = fork();

        d_assert(pid != -1, return EXIT_FAILURE, "fork() failed");

        if (pid == 0)
        {
            /* Child */
            setsid();
            umask(027);

            core_signal(SIGINT, logger_signal);
            core_signal(SIGTERM, logger_signal);
            core_signal(SIGHUP, logger_signal);

            logger_start(log_path);

            return EXIT_SUCCESS;
        }
        /* Parent */
    }

    signal_init();

    if (cellwire_initialize(config_path) != CORE_OK)
    {
        d_fatal("CellWire initialization failed. Aborted");
        return EXIT_FAILURE;
    }

    
    d_info("CellWire daemon start");

    threads_start();

    signal_thread(check_signal);

    d_info("CellWire daemon terminating...");

    cellwire_terminate();

    core_terminate();

    return EXIT_SUCCESS;
}
