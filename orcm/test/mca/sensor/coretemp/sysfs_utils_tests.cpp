/*
 * Copyright (c) 2017      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include <ftw.h>
#include <regex.h>

#include "sysfs_utils_tests.hpp"

static bool enable_nftw_mock = false;
static bool expect_nftw_to_find = false;
static bool enable_regcomp_mock = false;

static int find_temp_files_return_value = 0;

extern "C"
{
    #include "orcm/mca/sensor/coretemp/sysfs_utils.h"

    int __real_nftw(const char *dirpath,
                    int (*fn) (const char *fpath, const struct sat *sb,
                               int flag, struct FTW *ftw),
                    int nopenfd, int flags);

    int __wrap_nftw(const char *dirpath,
                    int (*fn) (const char *fpath, const struct sat *sb,
                               int flag, struct FTW *ftw),
                    int nopenfd, int flags)
    {
        if (!enable_nftw_mock)
            return __real_nftw(dirpath, fn, nopenfd, flags);
        if (expect_nftw_to_find)
        {
            struct FTW pathinfo;
            pathinfo.base = 512; // maximum path size
            find_temp_files_return_value = fn(dirpath, NULL, 0, &pathinfo);
        }
        return 0;
    }

    int __real_regcomp(regex_t *preg, const char *regex, int cflags);

    int __wrap_regcomp(regex_t *preg, const char *regex, int cflags)
    {
        if (!enable_regcomp_mock)
            return __real_regcomp(preg, regex, cflags);
        return 1;
    }
}

using namespace std;

void ut_sysfs_utils_tests::SetUpTestCase()
{
    enable_nftw_mock = false;
    expect_nftw_to_find = false;
    enable_regcomp_mock = false;
    find_temp_files_return_value = -1;
}

void ut_sysfs_utils_tests::TearDownTestCase()
{
    enable_nftw_mock = false;
    expect_nftw_to_find = false;
    enable_regcomp_mock = false;
}

TEST_F(ut_sysfs_utils_tests, get_temp_files_null_current_dir)
{
    ASSERT_TRUE(NULL == get_temperature_files_path(NULL));
}

TEST_F(ut_sysfs_utils_tests, get_temp_files_not_found)
{
    enable_nftw_mock = true;
    ASSERT_TRUE(NULL == get_temperature_files_path("/this/does/not/exist"));
}

TEST_F(ut_sysfs_utils_tests, get_temp_files_no_regex_match)
{
    enable_nftw_mock = true;
    expect_nftw_to_find = true;
    ASSERT_TRUE(NULL == get_temperature_files_path("coretemp.0/temp_input"));
    ASSERT_FALSE(find_temp_files_return_value);
}

TEST_F(ut_sysfs_utils_tests, get_temp_files_found_01)
{
    enable_nftw_mock = true;
    expect_nftw_to_find = true;

    char* test_str = get_temperature_files_path("coretemp.0/temp12_input");
    ASSERT_FALSE(strcmp(test_str, "coretemp.0/temp12_input"));
    ASSERT_TRUE(find_temp_files_return_value);
    free(test_str);
}

TEST_F(ut_sysfs_utils_tests, get_temp_files_found_02)
{
    enable_nftw_mock = true;
    expect_nftw_to_find = true;

    char* test_str = get_temperature_files_path("coretemp.0/temp12_label");
    ASSERT_FALSE(strcmp(test_str, "coretemp.0/temp12_label"));
    ASSERT_TRUE(find_temp_files_return_value);
    free(test_str);
}

TEST_F(ut_sysfs_utils_tests, get_temp_files_failed_regcomp)
{
    enable_nftw_mock = true;
    expect_nftw_to_find = true;
    enable_regcomp_mock = true;
    char* test_str = get_temperature_files_path("coretemp.0/temp12_input");
    ASSERT_FALSE(find_temp_files_return_value);
    free(test_str);
}
