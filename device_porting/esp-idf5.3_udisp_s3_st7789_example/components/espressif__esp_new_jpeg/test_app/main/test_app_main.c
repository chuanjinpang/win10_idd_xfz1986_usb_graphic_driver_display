// Copyright 2024 Espressif Systems (Shanghai) CO., LTD.
// All rights reserved.

#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"

#define TEST_MEMORY_LEAK_THRESHOLD (500)

void setUp(void)
{
    unity_utils_record_free_mem();
}

void tearDown(void)
{
    unity_utils_evaluate_leaks_direct(TEST_MEMORY_LEAK_THRESHOLD);
}

void app_main()
{
    //   _____ _____ ____ _____       _ ____  _____ ____ 
    //  |_   _| ____/ ___|_   _|     | |  _ \| ____/ ___|
    //    | | |  _| \___ \ | |    _  | | |_) |  _|| |  _ 
    //    | | | |___ ___) || |   | |_| |  __/| |__| |_| |
    //    |_| |_____|____/ |_|    \___/|_|   |_____\____|

    printf(" _____ _____ ____ _____       _ ____  _____ ____ \n");
    printf("|_   _| ____/ ___|_   _|     | |  _ \\| ____/ ___|\n");
    printf("  | | |  _| \\___ \\ | |    _  | | |_) |  _|| |  _ \n");
    printf("  | | | |___ ___) || |   | |_| |  __/| |__| |_| |\n");
    printf("  |_| |_____|____/ |_|    \\___/|_|   |_____\\____|\n");

    unity_run_menu();
}
