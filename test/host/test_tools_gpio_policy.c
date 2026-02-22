/*
 * Host tests for GPIO policy helpers (range and allowlist).
 */

#include <stdio.h>
#include <string.h>
#include <cjson/cJSON.h>
#include "config.h"
#include "tools_handlers.h"

#define TEST(name) static int test_##name(void)
#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", #cond, __LINE__); \
        return 1; \
    } \
} while(0)
#define ASSERT_STR_EQ(a, b) do { \
    if (strcmp((a), (b)) != 0) { \
        printf("  FAIL: '%s' != '%s' (line %d)\n", (a), (b), __LINE__); \
        return 1; \
    } \
} while(0)

bool tools_gpio_test_pin_is_allowed(int pin, const char *csv, int min_pin, int max_pin);

static void build_expected_range_read_all(char *buf, size_t buf_len)
{
    char *cursor = buf;
    size_t remaining = buf_len;
    int written;

    written = snprintf(cursor, remaining, "GPIO states: ");
    if (written < 0 || (size_t)written >= remaining) {
        if (buf_len > 0) {
            buf[0] = '\0';
        }
        return;
    }
    cursor += (size_t)written;
    remaining -= (size_t)written;

    for (int pin = GPIO_MIN_PIN; pin <= GPIO_MAX_PIN; pin++) {
        written = snprintf(cursor, remaining, "%s%d=LOW",
                           pin == GPIO_MIN_PIN ? "" : ", ", pin);
        if (written < 0 || (size_t)written >= remaining) {
            if (buf_len > 0) {
                buf[0] = '\0';
            }
            return;
        }
        cursor += (size_t)written;
        remaining -= (size_t)written;
    }
}

TEST(range_policy)
{
    ASSERT(!tools_gpio_test_pin_is_allowed(1, "", 2, 10));
    ASSERT(tools_gpio_test_pin_is_allowed(2, "", 2, 10));
    ASSERT(tools_gpio_test_pin_is_allowed(10, "", 2, 10));
    ASSERT(!tools_gpio_test_pin_is_allowed(11, "", 2, 10));
    return 0;
}

TEST(allowlist_policy_non_contiguous)
{
    const char *pins = "1,2,3,4,5,6,7,8,9,43,44";

    ASSERT(tools_gpio_test_pin_is_allowed(1, pins, 2, 10));
    ASSERT(tools_gpio_test_pin_is_allowed(43, pins, 2, 10));
    ASSERT(tools_gpio_test_pin_is_allowed(44, pins, 2, 10));
    ASSERT(!tools_gpio_test_pin_is_allowed(10, pins, 2, 10));
    ASSERT(!tools_gpio_test_pin_is_allowed(42, pins, 2, 10));
    return 0;
}

TEST(allowlist_policy_tolerates_spaces_and_invalid_tokens)
{
    const char *pins = " 1, two, 3 , , 44";

    ASSERT(tools_gpio_test_pin_is_allowed(1, pins, 0, 0));
    ASSERT(tools_gpio_test_pin_is_allowed(3, pins, 0, 0));
    ASSERT(tools_gpio_test_pin_is_allowed(44, pins, 0, 0));
    ASSERT(!tools_gpio_test_pin_is_allowed(2, pins, 0, 0));
    return 0;
}

TEST(read_all_default_range)
{
    cJSON *input = cJSON_CreateObject();
    char result[512] = {0};
    char expected[512] = {0};

    ASSERT(input != NULL);
    build_expected_range_read_all(expected, sizeof(expected));
    ASSERT(tools_gpio_read_all_handler(input, result, sizeof(result)));
    ASSERT_STR_EQ(result, expected);

    cJSON_Delete(input);
    return 0;
}

TEST(read_all_does_not_require_pin_argument)
{
    cJSON *input = cJSON_Parse("{\"pin\":999}");
    char result[512] = {0};
    char expected[512] = {0};

    ASSERT(input != NULL);
    build_expected_range_read_all(expected, sizeof(expected));
    ASSERT(tools_gpio_read_all_handler(input, result, sizeof(result)));
    ASSERT_STR_EQ(result, expected);

    cJSON_Delete(input);
    return 0;
}

int test_tools_gpio_policy_all(void)
{
    int failures = 0;

    printf("\nGPIO Policy Tests:\n");

    printf("  range_policy... ");
    if (test_range_policy() == 0) {
        printf("OK\n");
    } else {
        failures++;
    }

    printf("  allowlist_policy_non_contiguous... ");
    if (test_allowlist_policy_non_contiguous() == 0) {
        printf("OK\n");
    } else {
        failures++;
    }

    printf("  allowlist_policy_tolerates_spaces_and_invalid_tokens... ");
    if (test_allowlist_policy_tolerates_spaces_and_invalid_tokens() == 0) {
        printf("OK\n");
    } else {
        failures++;
    }

    printf("  read_all_default_range... ");
    if (test_read_all_default_range() == 0) {
        printf("OK\n");
    } else {
        failures++;
    }

    printf("  read_all_does_not_require_pin_argument... ");
    if (test_read_all_does_not_require_pin_argument() == 0) {
        printf("OK\n");
    } else {
        failures++;
    }

    return failures;
}
