// Copyright 2017-2018 Leland Lucius
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#if !defined(_UNITY_H_)
#define _UNITY_H_ 1

// ============================================================================
// Simulate unity test macros
// ============================================================================

#define TST(c,m)                                                              \
    {                                                                         \
        if (!(c))                                                             \
        {                                                                     \
            printf("%s(%d) - \"%s\" - %s\n", __func__, __LINE__, #c, m);      \
            abort();                                                          \
        }                                                                     \
    }

#define TEST_ASSERT_EQUAL_INT8_ARRAY(expected,actual,num_elements)            \
    {                                                                         \
        for (int test_i = 0; test_i < (num_elements); ++test_i)               \
        {                                                                     \
            int8_t test_e = ((expected))[test_i];                             \
            int8_t test_a = ((actual))[test_i];                               \
            if (test_e != test_a)                                             \
            {                                                                 \
                printf("%s(%d) - arrays \"%s\" and \"%s\" not equal at %d",   \
                       __func__, __LINE__, #expected, #actual, test_e);       \
                abort();                                                      \
            }                                                                 \
        }                                                                     \
    }

#define TEST_ASSERT(condition) \
    TST((condition), "evaluated FALSE");

#define TEST_ASSERT_FALSE(condition) \
    TST(!(condition), "expected FALSE got TRUE");

#define TEST_ASSERT_TRUE(condition) \
    TST((condition), "expected TRUE got FALSE");

#define TEST_ASSERT_NULL(condition) \
    TST((condition) == NULL, "expected NULL");

#define TEST_ASSERT_NOT_NULL(condition) \
    TST((condition) != NULL, "expected non-NULL");

#define TEST_ASSERT_EQUAL(expected, actual) \
    TST(((expected) == (actual)), "expected to be equal but wasn't");

#define TEST_ASSERT_NOT_EQUAL(expected, actual) \
    TST(((expected) != (actual)), "expected to be not equal but was");

#define TEST_ASSERT_EQUAL_STRING_LEN(expected, actual, len) \
    TST((strncmp((expected), (actual), (len)) == 0), "expected to be the same");

#define TEST_FAIL() \
    TST((1), "forced failure");

#define TEST_FAIL_MESSAGE(m) \
    TST((1), (m));

#define TEST_CASE(n, d, c) \
void test_ ## n(); \
void n() \
{   \
    printf("TEST: %s\n", d); \
    test_ ## n(); \
} \
void test_ ## n()

#endif
