#include "update_time.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    static const struct { int64_t timestamp; const char *expected; } cases[] = {
        {1774745940, "29.03.2026 01:59"},
        {1774746000, "29.03.2026 03:00"},
        {1792889940, "25.10.2026 02:59"},
        {1792890000, "25.10.2026 02:00"},
        {1793622840, "02.11.2026 13:34"},
        {1780317240, "01.06.2026 14:34"}
    };
    char text[32];
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        assert(update_time_format(cases[i].timestamp, text, sizeof(text)));
        assert(strcmp(text, cases[i].expected) == 0);
    }
    assert(!update_time_format(0, text, sizeof(text)));
    assert(!update_time_format(cases[0].timestamp, text, 2));
    assert(!update_time_format(cases[0].timestamp, NULL, 0));
    return 0;
}
