/**
 * @file    main.c
 * @brief   Autograder for ECE 350
 * 
 * @copyright Copyright (C) 2024 John Jekel and contributors
 * See the LICENSE file at the root of the project for licensing info.
 * 
 * Replace your `main.c` file with this and you're off to the races!
 *
*/

/* ------------------------------------------------------------------------------------------------
 * Constants/Defines
 * --------------------------------------------------------------------------------------------- */

//Change this to the lab you want to test for
#define LAB_NUMBER 1
//#define LAB_NUMBER 2
//#define LAB_NUMBER 3

#define NUM_TEST_FUNCTIONS 12

//X macros are magical! :)
//Order: function name, stack size, minimum lab number required, description string, author string
#define TEST_FUNCTIONS \
    X(sanity,                       STACK_SIZE, 1,  "Basic sanity test",                                            "JZJ") \
    X(eternalprintf,                STACK_SIZE, 1,  "Group 13's first testcase. No idea why that's the name...",    "JZJ") \
    X(reject_bad_tcbs,              STACK_SIZE, 1,  "You shouldn't create tasks from bad TCBs, it's not healthy!",  "JZJ") \
    X(stack_reuse,                  STACK_SIZE, 1,  "Basic stack reuse test",                                       "JZJ") \
    X(square_batman,                STACK_SIZE, 1,  "Round robin test",                                             "JZJ") \
    X(odds_are_stacked_against_you, STACK_SIZE, 1,  "Stack integrity test across osYield()",                        "JZJ") \
    X(i_prefer_latches,             STACK_SIZE, 1,  "Register integrity test across osYield()",                     "JZJ") \
    X(tid_limits,                   STACK_SIZE, 1,  "Maximum number of TIDs test",                                  "JZJ") \
    X(tid_uniqueness,               STACK_SIZE, 1,  "Ensure the same TID isn't used for two tasks",                 "JZJ") \
    X(reincarnation,                STACK_SIZE, 1,  "A task whose last act is to recreate itself",                  "JZJ") \
    X(insanity,                     0x400,      1,  "This is a tough one, but you can do it!",                      "JZJ") \
    X(greedy,                       STACK_SIZE, 1,  "Stack exhaustion test. This test should come last.",            "JZJ")
//TODO We can always use more testcases!

//Bonus tests (not required to support these)!
//X(task_wrapper_test,            STACK_SIZE,     "What happens if a task's function returns?",                   "JZJ")

#define NUM_PRIVILEGED_TESTS 18

#define NUM_SIDEKICKS   5
#define EVIL_ROBIN      NUM_SIDEKICKS / 2

#define INSANITY_LEVEL 50
#define MANDELBROT_RMIN  -1.8f
#define MANDELBROT_RMAX  0.8f
#define MANDELBROT_RPIX  60
#define MANDELBROT_IPIX  20
#define MANDELBROT_IMIN  -1.1f
#define MANDELBROT_IMAX  1.1f
#define MANDELBROT_DIVERGE_THRESHOLD 2.0f

//Making it to the manager is also a point!
#define NUM_TESTS (NUM_TEST_FUNCTIONS + NUM_PRIVILEGED_TESTS + 1)

#define FN_MANAGER_STACK_SIZE 0x400

#define fancy_printf_primitive(prefix, ...) do { \
    printf("%s", prefix); \
    printf(__VA_ARGS__); \
    printf("\x1b[0m\r\n"); \
} while (0)

//Colours!
#define rprintf(...) fancy_printf_primitive("\x1b[1m\x1b[91m", __VA_ARGS__)
#define gprintf(...) fancy_printf_primitive("\x1b[1m\x1b[92m", __VA_ARGS__)
#define yprintf(...) fancy_printf_primitive("\x1b[1m\x1b[93m", __VA_ARGS__)
#define bprintf(...) fancy_printf_primitive("\x1b[1m\x1b[94m", __VA_ARGS__)
#define mprintf(...) fancy_printf_primitive("\x1b[1m\x1b[95m", __VA_ARGS__)
#define cprintf(...) fancy_printf_primitive("\x1b[1m\x1b[96m", __VA_ARGS__)
#define wprintf(...) fancy_printf_primitive("\x1b[1m\x1b[97m", __VA_ARGS__)

//Test functions that print should use this ONLY
#define tprintf(...) fancy_printf_primitive("    \x1b[90m", __VA_ARGS__)

//Convenience macro for exiting
#define treturn(status) do { \
    function_complete = true; \
    function_status   = status; \
    osTaskExit(); \
} while (0)

/* ------------------------------------------------------------------------------------------------
 * Includes
 * --------------------------------------------------------------------------------------------- */

//These are your headers
#include "common.h"
#include "k_mem.h"
#include "k_task.h"

//These are headers that the autograder needs
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "main.h"

/* ------------------------------------------------------------------------------------------------
 * Type Declarations
 * --------------------------------------------------------------------------------------------- */

typedef struct {
    void              (*ptr)(void* args);
    const char* const   name;
    uint16_t            stack_size;
    uint8_t             min_labn;
    const char* const   description;
    const char* const   author;
} test_function_info_s;

/* ------------------------------------------------------------------------------------------------
 * Static Function Declarations
 * --------------------------------------------------------------------------------------------- */

static void print_score_so_far(void);

static void test_function_manager(void*);

//Spinning helper task infrastructure that's useful for several tests
static void     spinner(void*);//Spins while osYield()ing until it "topples". Used by a few tests
static void     topple_spinners(void);//Waits for spinners to exit
static task_t   beyblade_let_it_rip(void);//Does anyone remember this show? Kinda just a marketing stunt to sell spinning tops...

static void square_batman_helper(void*);
static void insanity_helper(void*);

//Too bad these couldn't be part of insanity
static uint32_t mandelbrot_iterations(float creal, float cimag);
static void     mandelbrot_forever(void);

//Test function definitions
#define X(name, stack_size, min_labn, desc, author) static void name(void*);
TEST_FUNCTIONS
#undef X

/* ------------------------------------------------------------------------------------------------
 * Static Variables
 * --------------------------------------------------------------------------------------------- */

//Static constants

static const char* const LOGO = "\r\n\r\n\x1b[95m"
"             _                            _                 ____\r\n"
"  __ _ _   _| |_ ___   __ _ _ __ __ _  __| | ___ _ ____   _|___ \\\r\n"
" / _` | | | | __/ _ \\ / _` | '__/ _` |/ _` |/ _ \\ '__\\ \\ / / __) |\r\n"
"| (_| | |_| | || (_) | (_| | | | (_| | (_| |  __/ |   \\ V / / __/\r\n"
" \\__,_|\\__,_|\\__\\___/ \\__, |_|  \\__,_|\\__,_|\\___|_|    \\_/ |_____|\r\n"
"                      |___/\x1b[0m\r\n"
"\x1b[1m\"We're doing a sequel!\"\x1b[0m\r\n"
"\x1b[1mCopyright (C) 2024 \x1b[95mJohn Jekel\x1b[0m\x1b[1m and contributors\x1b[0m\r\n"
"\x1b[1mRepo: \x1b[96mhttps://github.com/JZJisawesome/autograderv2\x1b[0m\r\n\r\n";

static const test_function_info_s test_functions[NUM_TEST_FUNCTIONS] = {
    //These should set function_complete to true when they finish so we can move onto the next one
    //This synchronization mechanism works only if there's one test function running at once and
    //they only write true (while the test_function_manager reads it/writes false)
#define X(name, stack_size, min_labn, desc, author) {name, #name, stack_size, min_labn, desc, author},
    TEST_FUNCTIONS
#undef X
};

//Mutable statics

//Autograder state
static volatile bool function_complete  = false;//Causes things to move onto the next test
static volatile bool function_status    = false;//False if failed, true if passed
static volatile size_t num_passed  = 0;//Number of tests passed
static volatile size_t num_skipped = 0;//Number of tests skipped

//Used by spinner() and friends
static volatile size_t  spin_count = 0;
static volatile bool    topple     = false;

//Testcase-specific statics
static volatile int     square_batman_counters[NUM_SIDEKICKS] = {0, 0, 0};
static volatile size_t  insanity_counter = 0;

/* ------------------------------------------------------------------------------------------------
 * Function Implementations
 * --------------------------------------------------------------------------------------------- */

int main(void) {
    //Do things that the STM32CubeIDE does
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    //Logo!
    printf("%s", LOGO);//Corresponds to Lab 1 evaluation outline #0
    wprintf("Testing for Lab \x1b[96m%d\r\n", LAB_NUMBER);
    wprintf("Note that a base level of functionality is required in order to run the autograder");
    wprintf("to completion without crashing. Even if you can't get that far right away,");
    wprintf("as you make progress you'll get further and further through the autograder");
    wprintf("code, which can be a great way to gauge your progress in and of itself!");
    wprintf("Cheers and best of luck. Let's get into it! - \x1b[95mJZJ\r\n");

    //Getting into it...
    wprintf("Initializing the kernel and doing some pre-osKernelStart() tests...");

    //Privileged test #1
    if (osTaskExit() != RTX_ERR) {
        rprintf("    osTaskExit() should return RTX_ERR when called from a privileged context!");
    } else {
        gprintf("    Awesome, you passed the first osTaskExit() test!");
        ++num_passed;
    }

    //Privileged test #2
    if (osKernelStart() != RTX_ERR) {
        rprintf("    osKernelStart() should return RTX_ERR when called before the kernel was initialized!");
    } else {
        gprintf("    Nice work on the pre-init osKernelStart() behavior!");
        ++num_passed;
    }

    //Privileged test #3
    if (osTaskExit() != RTX_ERR) {
        rprintf("    osTaskExit() should return RTX_ERR when called from a privileged context!");
    } else {
        gprintf("    Good pre-osKernelStart() osTaskExit() behavior!");
        ++num_passed;
    }

    //Privileged test #4
    if (osGetTID() != 0) {
        rprintf("    osGetTID() should return 0 when called from a privileged context!");
    } else {
        gprintf("    Good pre-start osGetTID() behavior!");
        ++num_passed;
    }

    //Privileged test #5
    osYield();
    ++num_passed;
    gprintf("    You survived an osYield before the kernel started!");

    //Privileged test #6
    bool task_info_passed = true;
    for (task_t ii = 0; ii < MAX_TASKS; ++ii) {
        TCB task_info;
        memset(&task_info, 0, sizeof(TCB));
        if (osTaskInfo((task_t)ii, &task_info) != RTX_ERR) {
            rprintf("    osTaskInfo() should return RTX_ERR since no tasks exist yet!");
            task_info_passed = false;
            break;
        }

        TCB zeroed_task_info;
        memset(&zeroed_task_info, 0, sizeof(TCB));
        if (memcmp(&task_info, &zeroed_task_info, sizeof(TCB)) != 0) {
            rprintf("    osTaskInfo() should not modify the task_info struct when it fails!");
            task_info_passed = false;
            break;
        }
    }
    if (task_info_passed) {
        gprintf("    osTaskInfo() is behaving as expected before the kernel starts!");
        ++num_passed;
    }

    //Privileged test #7
#if LAB_NUMBER >= 2
    if (k_mem_alloc(1) != NULL) {
        rprintf("    k_mem_alloc() should fail before k_mem_init()!");
    } else {
        gprintf("    k_mem_alloc() is behaving as expected before k_mem_init()!");
        ++num_passed;
    }
#else
    bprintf("    Skipping k_mem_alloc() test since it's not required for Lab 1...");
    ++num_skipped;
#endif

    //Privileged test #8
#if LAB_NUMBER >= 2
    if (k_mem_init() == RTX_OK) {
        rprintf("    k_mem_init() should fail before kernel init!");
    } else {
        gprintf("    k_mem_init() is behaving as expected before the kernel starts!");
        ++num_passed;
    }
#else
    bprintf("    Skipping k_mem_init() test since it's not required for Lab 1...");
    ++num_skipped;
#endif

    //Privileged test #9
#if LAB_NUMBER >= 2
    if (k_mem_alloc(1) != NULL) {
        rprintf("    k_mem_alloc() should fail before (a successful) k_mem_init()!");
    } else {
        gprintf("    k_mem_alloc() is behaving as expected before k_mem_init()!");
        ++num_passed;
    }
#else
    bprintf("    Skipping another k_mem_alloc() test since it's not required for Lab 1...");
    ++num_skipped;
#endif
    
    //TODO more tests pre-init

    //Privileged test #10
    wprintf("Initializing the kernel...");
    osKernelInit();//Corresponds to Lab 1 evaluation outline #0
    ++num_passed;
    gprintf("Alrighty, the kernel is initialized!\x1b[0m\x1b[1m Let's see how you're doing so far...");
    print_score_so_far();
    
    //Privileged test #11
    if (osGetTID() != 0) {
        rprintf("    osGetTID() should return 0 when called from a privileged context!");
    } else {
        gprintf("    osGetTID() still behaving as expected after init!");
        ++num_passed;
    }

    //Privileged test #12
    if (osTaskExit() != RTX_ERR) {
        rprintf("    osTaskExit() should return RTX_ERR when called from a privileged context!");
    } else {
        gprintf("    osTaskExit() still behaving as expected after init!");
        ++num_passed;
    }

    //Privileged test #13
    osYield();
    ++num_passed;
    gprintf("    You survived ANOTHER osYield before the kernel started!");

    //Privileged test #14
    task_info_passed = true;
    for (task_t ii = 0; ii < MAX_TASKS; ++ii) {
        TCB task_info;
        memset(&task_info, 0, sizeof(TCB));
        if (osTaskInfo((task_t)ii, &task_info) != RTX_ERR) {
            if (ii == TID_NULL) {//NULL task special case, see Piazza #101
                if (!task_info.ptask) {
                    yprintf("    (osTaskInfo() reporting NULL ptask for the NULL task, maybe this is bad?)");
                }

                if (task_info.stack_size < STACK_SIZE) {
                    yprintf("    (osTaskInfo() weird stack size for the NULL task, maybe this is bad?)");
                }

                continue;
            }

            rprintf("    osTaskInfo() should return RTX_ERR since no tasks exist yet!");
            task_info_passed = false;
            break;
        }

        TCB zeroed_task_info;
        memset(&zeroed_task_info, 0, sizeof(TCB));
        if (memcmp(&task_info, &zeroed_task_info, sizeof(TCB)) != 0) {
            rprintf("    osTaskInfo() should not modify the task_info struct when it fails!");
            task_info_passed = false;
            break;
        }
    }
    if (task_info_passed) {
        gprintf("    osTaskInfo() is behaving as expected before the kernel starts!");
        ++num_passed;
    }

    //Privileged test #15
    TCB test_function_manager_task;
    memset(&test_function_manager_task, 0, sizeof(TCB));
    test_function_manager_task.ptask      = test_function_manager;
    test_function_manager_task.stack_size = FN_MANAGER_STACK_SIZE;
    if (osCreateTask(&test_function_manager_task) == RTX_ERR) {//Corresponds to Lab 1 evaluation outline #1
        rprintf("    osCreateTask() failed to create the test function manager task!");
        rprintf("    Sadly this means we can't really continue, but don't give up! :)");
        while(true);
    } else if (test_function_manager_task.tid == 0) {
        rprintf("    osCreateTask() succeeded but didn't set TID in the task it was passed!");
    } else {
        gprintf("    Successfully created the test function manager task!");
        ++num_passed;
    }

    //Privileged test #16
    task_info_passed = true;
    for (task_t ii = 0; ii < MAX_TASKS; ++ii) {
        TCB task_info;
        memset(&task_info, 0, sizeof(TCB));
        if (osTaskInfo(ii, &task_info) != RTX_ERR) {
            if (ii == TID_NULL) {//NULL task special case, see Piazza #101
                if (!task_info.ptask) {
                    yprintf("    (osTaskInfo() reporting NULL ptask for the NULL task, maybe this is bad?)");
                }

                if (task_info.stack_size < STACK_SIZE) {
                    yprintf("    (osTaskInfo() weird stack size for the NULL task, maybe this is bad?)");
                }

                continue;
            }

            if (ii == test_function_manager_task.tid) {//The task we created
                //Corresponds to Lab 1 evaluation outline #1
                if (task_info.ptask != test_function_manager) {
                    rprintf("    osTaskInfo() reporting incorrect ptask, or bad TCB initialization!");
                    task_info_passed = false;
                    break;
                }

                if (task_info.stack_size < FN_MANAGER_STACK_SIZE) {
                    rprintf("    osTaskInfo() reporting incorrect stack size, or bad TCB initialization!");
                    task_info_passed = false;
                    break;
                }

                continue;
            }

            rprintf("    osTaskInfo() should return RTX_ERR since no tasks exist yet!");
            task_info_passed = false;
            break;
        }

        TCB zeroed_task_info;
        memset(&zeroed_task_info, 0, sizeof(TCB));
        if (memcmp(&task_info, &zeroed_task_info, sizeof(TCB)) != 0) {
            rprintf("    osTaskInfo() should not modify the task_info struct when it fails!");
            task_info_passed = false;
            break;
        }
    }
    if (task_info_passed) {
        gprintf("    osTaskInfo() is behaving as expected before the kernel starts!");
        ++num_passed;
    }

    //Privileged test #17
#if LAB_NUMBER >= 2
    if (k_mem_init() != RTX_OK) {//FIXME what about testing if k_mem_init() works if called from a user task?
        rprintf("    k_mem_init() failed!");
    } else {
        gprintf("    k_mem_init() was successful!");
        ++num_passed;
    }
#else
    bprintf("    Skipping k_mem_init() test since it's not required for Lab 1...");
    ++num_skipped;
#endif

    //Privileged test #18
#if LAB_NUMBER >= 2
    if (k_mem_init() == RTX_OK) {
        rprintf("    k_mem_init() should fail if called twice!");
    } else {
        gprintf("    k_mem_init() refused to be called twice as expected!");
        ++num_passed;
    }
#else
    bprintf("    Skipping another k_mem_init() test since it's not required for Lab 1...");
    ++num_skipped;
#endif

    //TODO try to allocate memory in privileged mode!

    //And off we go!
    wprintf("Okay, I'm calling osKernelStart() now. This is a big step, don't get disheartened");
    wprintf("if it doesn't work on your first try, it certainly didn't for our group :)");
    wprintf("Before we leave though, here's your score so far:");
    print_score_so_far();
    printf("\r\n\r\n");
    osKernelStart();
    assert(false && "\x1b[1m\x1b[92mosKernelStart() should never exit when called from a privileged context!\x1b[0m");
}

/* ------------------------------------------------------------------------------------------------
 * Static Function Implementations (Non-Test Functions)
 * --------------------------------------------------------------------------------------------- */

static void print_score_so_far(void) {
    //Can't use any floating point here due to extended processor state
    /*
    double jekelscore_ratio = (double)num_passed / (double)NUM_TESTS;
    uint32_t jekelscore = (uint32_t)(jekelscore_ratio * 100);
    */
    //Do (very incorrectly rounded and unoptimized) fixed point math instead
    uint32_t total_num              = num_passed + num_skipped;
    uint32_t jekelscore_times_100   = (total_num * 10000) / NUM_TESTS;
    uint32_t jekelscore_whole       = (total_num == NUM_TESTS) ? 100 : (jekelscore_times_100 / 100);
    uint32_t jekelscore_fraction    = (total_num == NUM_TESTS) ? 0 : (jekelscore_times_100 % 100);
    wprintf(
        "Your \x1b[95mJekelScore\x1b[0m\x1b[1m is \x1b[96m%lu.%02lu%%\x1b[0m\x1b[1m so far"
        " (\x1b[96m%d/%d\x1b[0m\x1b[1m passed, \x1b[94m%d/%d\x1b[0m\x1b[1m skipped)!",
        jekelscore_whole,
        jekelscore_fraction,
        num_passed,
        NUM_TESTS,
        num_skipped,
        NUM_TESTS
    );
}

static void test_function_manager(void*) {
    gprintf("Haha, awesome you made it! This is being printed from a user task!");
    ++num_passed;//Corresponds to Lab 1 evaluation outline #2
    print_score_so_far();

    for (size_t ii = 0; ii < NUM_TEST_FUNCTIONS; ++ii) {
        function_complete   = false;
        function_status     = false;

        TCB task;
        memset(&task, 0, sizeof(TCB));
        task.ptask      = test_functions[ii].ptr;
        task.stack_size = test_functions[ii].stack_size;

        if (LAB_NUMBER < test_functions[ii].min_labn) {
            bprintf(
                "\r\nSkipping test function \x1b[96m#%u\x1b[91m, \x1b[96m%s()\x1b[91m,"
                 " since it's a Lab %u test but we're in Lab %u testing mode!",
                ii + 1,
                test_functions[ii].name,
                test_functions[ii].min_labn,
                LAB_NUMBER
            );
            ++num_skipped;
            continue;
        }
        
        wprintf(
            "\r\nRunning test function \x1b[96m#%u\x1b[0m\x1b[1m, \x1b[96m%s()\x1b[0m\x1b[1m, "
            "by \x1b[96m%s\x1b[0m\x1b[1m, with a stack size of \x1b[96m%u\x1b[0m\x1b[1m bytes!",
            ii + 1,
            test_functions[ii].name,
            test_functions[ii].author,
            test_functions[ii].stack_size
        );
        wprintf("Description: \x1b[96m%s", test_functions[ii].description);

        int result = osCreateTask(&task);
        if (result != RTX_OK) {
            rprintf("Failed to create a task for the function! Moving on...");
            continue;
        }

        if (task.tid == 0) {
            yprintf("Warning: TID value wasn't set correctly!\x1b[0m\r\n");
            yprintf("You were likely already docked for this so I won't dock you again...\r\n");
        }

        while (!function_complete) {
            osYield();
        }

        if (function_status) {
            gprintf(
                "Test function \x1b[96m#%u\x1b[92m, \x1b[96m%s()\x1b[92m, passed!",
                ii + 1,
                test_functions[ii].name
            );
            ++num_passed;
        } else {
            rprintf(
                "Test function \x1b[96m#%u\x1b[91m, \x1b[96m%s()\x1b[91m, failed!",
                ii + 1,
                test_functions[ii].name
            );
        }

        if ((ii + 1) != NUM_TEST_FUNCTIONS) {
            print_score_so_far();
        }
    }

    gprintf("\r\nYou made it to the end! :)");

    print_score_so_far();

    uint32_t total_num = num_passed + num_skipped;
    if (total_num == NUM_TESTS) {
        gprintf("You passed all the tests with flying colours! Good stuff! :)");
    } else {
        yprintf("You didn't quite get them all, but don't give up! :)");
    }

    wprintf("Have an idea for a test? Submit a PR at \x1b[96mhttps://github.com/JZJisawesome/autograderv2\x1b[0m\x1b[1m !");
    wprintf("Cheers and best of luck! - \x1b[95mJZJ");

    mandelbrot_forever();
}

static void spinner(void*) {
    while (!topple) {
        //Around and around we go!
        osYield();
    }

    //We toppled over!
    --spin_count;
    osTaskExit();
}

static void topple_spinners(void) {
    topple = true;
    while (spin_count) {
        osYield();
    }
}

static task_t beyblade_let_it_rip(void) {
    //Does anyone remember this show? Kinda just a marketing stunt to sell spinning tops...
    topple = false;
    TCB spinner_task;
    memset(&spinner_task, 0, sizeof(TCB));
    spinner_task.ptask      = spinner;
    spinner_task.stack_size = STACK_SIZE;
    int result = osCreateTask(&spinner_task);
    if (result == RTX_OK) {
        ++spin_count;
        return spinner_task.tid;
    } else {
        return TID_NULL;
    }
}

//This mandelbrot stuff was going to be part of insanity(), but there's no easy way to force
//GCC to use soft floating point without control of compiler flags. This is a problem since
//no ones kernels are set up to save extended processor state in addition to integer state.
//So just do this at the end for fun!
static uint32_t mandelbrot_iterations(float creal, float cimag) {
    float zreal = 0.0f;
    float zimag = 0.0f;

    uint32_t ii = 0;

    while ((ii < INSANITY_LEVEL) && (((zreal * zreal) + (zimag * zimag)) < MANDELBROT_DIVERGE_THRESHOLD)) {
        float next_zreal = (zreal * zreal) - (zimag * zimag) + creal;
        float next_zimag = (2.0f * zreal * zimag) + cimag;
        zimag = next_zimag;
        zreal = next_zreal;
        ++ii;
    }

    return ii;
}

static void mandelbrot_forever(void) {//Can't return due to the FP issue, we don't ever want this task to exit
    wprintf("\r\nI'll spin forever now, reset or reprogram the board to go again! :)");


    wprintf("Until then, here's the Mandelbrot set I promised!");
    float real_step = (MANDELBROT_RMAX - MANDELBROT_RMIN) / MANDELBROT_RPIX;
    float imag_step = (MANDELBROT_IMAX - MANDELBROT_IMIN) / MANDELBROT_IPIX;
    float cimag = MANDELBROT_IMIN;
    for (int ii = 0; ii < MANDELBROT_IPIX; ++ii) {
        float creal = MANDELBROT_RMIN;
        for (int jj = 0; jj < MANDELBROT_RPIX; ++jj) {
            uint32_t iterations = mandelbrot_iterations(creal, cimag);
            printf("%s", iterations > 25 ? " " : "*");
            creal += real_step;
        }
        printf("\r\n");
        cimag += imag_step;
    }

    while (true);
}

/* ------------------------------------------------------------------------------------------------
 * Static Function Implementations (Test Functions)
 * --------------------------------------------------------------------------------------------- */

static void sanity(void*) {
    //Do nothing!
    treturn(true);
}

static void eternalprintf(void*) {
    //Is the task environment robust enough to support calling printf()?
    //Almost certainly it is at this point if you're successfully running the test_manager_function(),
    //but hey, can you call printf from more than one task?
    for (int ii = 0; ii < 10; ++ii) {
        tprintf("Test task executing!");
    }

    osYield();//For kicks

    treturn(true);
}

static void reject_bad_tcbs(void*) {
    TCB task;

    //First, a task with a less-than-minimum stack size
    memset(&task, 0, sizeof(TCB));
    task.ptask      = sanity;
    task.stack_size = STACK_SIZE / 2;
    if (osCreateTask(&task) != RTX_ERR) {
        tprintf("A task with a stack size less than the minimum was created!");
        treturn(false);
    }

    //Next, a task with a null ptask function pointer
    memset(&task, 0, sizeof(TCB));
    task.ptask      = NULL;
    task.stack_size = STACK_SIZE;
    if (osCreateTask(&task) != RTX_ERR) {
        tprintf("A task with a NULL ptask function pointer was created!");
        treturn(false);
    }

    //Next, what about a null TCB pointer itself?
    if (osCreateTask(NULL) != RTX_ERR) {
        tprintf("A task with a NULL TCB pointer was created!");
        treturn(false);
    }

    //We made it!
    treturn(true);
}

static void stack_reuse(void*) {//PARTIALLY corresponds to Lab 1 evaluation outline #10 (less intense, insanity takes care of more)
    //Setup a spinner and get info about it
    task_t spinner1_tid = beyblade_let_it_rip();
    TCB spinner1_info;
    osTaskInfo(spinner1_tid, &spinner1_info);
    topple_spinners();

    //Now that the first spinner is gone, do another
    task_t spinner2_tid = beyblade_let_it_rip();
    TCB spinner2_info;
    osTaskInfo(spinner2_tid, &spinner2_info);
    topple_spinners();

    tprintf("stack_high for spinner 1: 0x%lX", spinner1_info.stack_high);
    tprintf("stack_high for spinner 2: 0x%lX", spinner2_info.stack_high);
    tprintf("You passed if those are the same (and both spinners were actually created)!");

    //We were successful if spinner 2 reused spinner 1's stack
    treturn(spinner1_tid && spinner2_tid && (spinner1_info.stack_high == spinner2_info.stack_high));
}

static void square_batman_helper(void*) {
    //Choose a counter for the test
    int my_counter = 0;
    for (int ii = 0; ii < NUM_SIDEKICKS; ++ii) {
        if (square_batman_counters[ii] == 0) {
            my_counter                  = ii;
            square_batman_counters[ii]  = 1;
            break;
        }
    }
    tprintf("I am Robin #%d!", my_counter);

    //Wait for all Robins to pick their counter
    while (square_batman_counters[NUM_SIDEKICKS - 1] == 0) {
        osYield();
    }

    //Let's see how round these Robins are!
    for (int ii = 1; ii < 10; ++ii) {
        tprintf(
            "Incrementing counter %d from %d to %d",
            my_counter,
            square_batman_counters[my_counter],
            square_batman_counters[my_counter] + 1
        );
        ++square_batman_counters[my_counter];

        if ((ii == 5) && (my_counter == EVIL_ROBIN)) {
            tprintf("I AM EVIL #%d! I'm going to exit early and throw the other Robins off!", my_counter);
            osTaskExit();
        }

        osYield();
    }

    osTaskExit();
}

static void square_batman(void*) {//Corresponds to Lab 1 evaluation outline #3 and #4
    //Setup robins
    TCB helper_task;
    memset(&helper_task, 0, sizeof(TCB));
    helper_task.ptask      = square_batman_helper;
    helper_task.stack_size = STACK_SIZE;

    for (int ii = 0; ii < NUM_SIDEKICKS; ++ii) {
        if (osCreateTask(&helper_task) != RTX_OK) {
            tprintf("we live in a society...");
            treturn(false);
        }
    }

    //Wait for all Robins to pick their counter
    while (square_batman_counters[NUM_SIDEKICKS - 1] == 0) {
        osYield();
    }

    tprintf("I'M BATMAN!");

    //The entire round robin test is complete when all counters are 10
    bool all_counters_are_10 = false;
    while (!all_counters_are_10) {
        all_counters_are_10 = true;

        int minimum = 11;
        int maximum = 0;
        for (int ii = 0; ii < NUM_SIDEKICKS; ++ii) {
            if (ii == EVIL_ROBIN) {//Ignore the evil Robin
                continue;
            }

            if (square_batman_counters[ii] != 10) {
                all_counters_are_10 = false;
            }
            
            if (square_batman_counters[ii] < minimum) {
                minimum = square_batman_counters[ii];
            }

            if (square_batman_counters[ii] > maximum) {
                maximum = square_batman_counters[ii];
            }
        }

        int difference = maximum - minimum;
        if (difference > 1) {
            tprintf("Your Robins aren't round enough!");
            tprintf("The difference between the highest and lowest Robin counter is %d", difference);
            for (int ii = 0; ii < NUM_SIDEKICKS; ++ii) {
                tprintf("    Robin #%d: %d", ii, square_batman_counters[ii]);
            }
            treturn(false);
        }

        osYield();
    }

    //Success! Yield a few times just to ensure the Robins exit
    tprintf("Your Robins are perfectly round!");
    osYield();
    osYield();
    osYield();
    treturn(true);
}

static void odds_are_stacked_against_you(void*) {
    volatile uint8_t stack_data[STACK_SIZE/2];
    for (size_t ii = 0; ii < INSANITY_LEVEL; ++ii) {//Do the check a few times for good measure
        for (size_t jj = 0; jj < (STACK_SIZE/2); ++jj) {
            stack_data[jj] = jj & 0xFF;
        }
        osYield();
        for (size_t jj = 0; jj < (STACK_SIZE/2); ++jj) {
            if (stack_data[jj] != (jj & 0xFF)) {//Stack corruption!
                treturn(false);
            }
        }
    }

    treturn(true);
}

static void i_prefer_latches(void*) {//Corresponds to Lab 1 evaluation outline #6
    //Only check callee-saved registers since when we call osYield() it's allowed to clobber the others
    register uint32_t r4  asm("r4");
    register uint32_t r5  asm("r5");
    register uint32_t r6  asm("r6");
    //register uint32_t r7  asm("r7");//Nor r7 since it is used as the frame pointer for debugging
    register uint32_t r8  asm("r8");
    register uint32_t r9  asm("r9");
    register uint32_t r10 asm("r10");
    register uint32_t r11 asm("r11");

    r4  = 0x44444444;
    r5  = 0x55555555;
    r6  = 0x66666666;
    //r7  = 0x77777777;
    r8  = 0x88888888;
    r9  = 0x99999999;
    r10 = 0xAAAAAAAA;
    r11 = 0xBBBBBBBB;

    osYield();

    bool passed = true;

    passed = passed && (r4  == 0x44444444);
    passed = passed && (r5  == 0x55555555);
    passed = passed && (r6  == 0x66666666);
    //passed = passed && (r7  == 0x77777777);
    passed = passed && (r8  == 0x88888888);
    passed = passed && (r9  == 0x99999999);
    passed = passed && (r10 == 0xAAAAAAAA);
    passed = passed && (r11 == 0xBBBBBBBB);

    treturn(passed);
}

static void tid_limits(void*) {//Corresponds to Lab 1 evaluation outline #7 and 8
    //Try to create as many spinner tasks as possible
    int ii = 0;//This will contain how much we actually successfully created in the end
    for (ii = 0; ii < (MAX_TASKS * 2); ++ii) {
        if (beyblade_let_it_rip() == TID_NULL) {
            break;
        }
    }

    //Wait for them all to finish
    topple_spinners();

    //We were successful if we could create EXACTLY MAX_TASKS - 3 tasks, no more, no less
    //(Since the null task, the test_function_manager task, and this task take up 3 TIDs)
    treturn(ii == (MAX_TASKS - 3));
}

static void tid_uniqueness(void*) {
    //Create the maximum amount of spinner tasks we possibly can
    int tid_counters[MAX_TASKS];
    memset(tid_counters, 0, sizeof(tid_counters));
    for (int ii = 0; ii < (MAX_TASKS - 3); ++ii) {
        task_t tid = beyblade_let_it_rip();

        //Avoid writing to tid_counters out of bounds
        if (tid >= MAX_TASKS) {
            tprintf("TID %d was assigned to a task, which is >= MAX_TASKS!", tid);
            treturn(false);
        }

        ++tid_counters[tid];
    }

    //Wait for them all to finish
    //No TIDs should get reused since we don't `topple` the spinners until this call
    topple_spinners();

    //Ensure none of them got TID 0
    if (tid_counters[0]) {
        tprintf("One or more of the tasks were assigned TID 0, or the stack allocation failed!");
        treturn(false);
    }

    //Ensure all TIDs were unique
    for (int ii = 1; ii < MAX_TASKS; ++ii) {
        if (tid_counters[ii] > 1) {
            tprintf("TID %d was assigned to more than one task!", ii);
            treturn(false);
        }
    }

    //If we made it here, we're good!
    treturn(true);
}

static void reincarnation(void*) {//Corresponds to Lab 1 evaluation outline #11
    static volatile size_t number_of_lives = 9;//Lol
    tprintf("I'm alive! I have %u lives left!", number_of_lives);

    if (number_of_lives == 0) {
        tprintf("I can't afford life insurance anymore! NOOOOO!!!");
        tprintf("(Test passed!)");
        treturn(true);
    }

    tprintf("Let me just make sure I have life insurance...");
    TCB task;
    memset(&task, 0, sizeof(TCB));
    task.ptask      = reincarnation;
    task.stack_size = STACK_SIZE;
    
    if (osCreateTask(&task) != RTX_OK) {
        tprintf("The premiums are way to high! I can't afford this!");
        tprintf("(Failed to create a new task!)");
        treturn(false);
    }

    --number_of_lives;

    tprintf("I feel myself slipping away, good thing I'm insured, that's how this works right?");
    osTaskExit();
}

static void insanity_helper(void*) {
    task_t tid = osGetTID();
    tprintf("    Hello there from TID %u!", tid);

    if (tid == 0) {
        tprintf("    Uh, why is my TID equal to 0?");
        function_status = false;
    }

    ++insanity_counter;
    osTaskExit();
}

static void insanity(void*) {//Corresponds to Lab 1 evaluation outline #10, and also just a really hard test
    tprintf("I have a bunch of friends who are going to say hello!");

    function_status = true;//The helpers may set this to false if the bad things happen

    TCB task;
    memset(&task, 0, sizeof(TCB));
    task.ptask      = insanity_helper;
    task.stack_size = STACK_SIZE;
    for (int ii = 0; ii < INSANITY_LEVEL; ++ii) {
        while (osCreateTask(&task) != RTX_OK) {
            osYield();
        }
    }

    while (insanity_counter < INSANITY_LEVEL) {
        osYield();
    }

    if (!function_status) {
        treturn(false);
    }

    tprintf("And goodbye!");
    tprintf("(There would have been a cool mandelbrot stress test here, but there were issues");
    tprintf("I couldn't resolve to due with saving and restoring floating point state.");
    tprintf("So it will be at the end instead, where we don't need to save anything!)");
    treturn(true);
}

static void greedy(void*) {//Corresponds to Lab 1 evaluation outline #9
    tprintf("GIVE ME ALL OF THE STACK SPACE!");

    TCB task;
    memset(&task, 0, sizeof(TCB));
    task.ptask      = sanity;
    task.stack_size = 0x4000;//EVIL! MWAHAHAHAHAHAHAH
    if (osCreateTask(&task) == RTX_OK) {
        tprintf("Well I didn't expect that to work...");
        tprintf("(Test failed!)");
        treturn(false);
    }

    tprintf("FOILED AGAIN! I need to work on my money (stack?) laundering skills...");
    tprintf("I've read it's much easier to get away with this if I take in smaller amounts!");

    for (size_t ii = 0; ii < 0x4000; ii += 0x800) {
        TCB task;
        memset(&task, 0, sizeof(TCB));
        task.ptask      = sanity;
        task.stack_size = 0x800;//Much more sneaky!
        if (osCreateTask(&task) != RTX_OK) {
            tprintf("Nope! Whelp, guess I'm off to federal prison!");
            tprintf("(Test passed!)");
            treturn(true);
        }
    }

    tprintf("ALL OF YOUR STACK, NO, ALL OF YOUR MEMORY IS MINE! MWAHAHAHAHA!!!!!!");
    tprintf("(Test failed!)");
    treturn(false);
}

/*
static void task_wrapper_test(void*) {
    function_complete   = true;
    function_status     = true;
    //NOT calling osTaskExit()
}
*/
