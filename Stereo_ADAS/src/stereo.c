#include "stereo.h"
#include <string.h>

#define RADIUS 2U
#define CENSUS_ROWS (2U * RADIUS + 1U)
#define FIRST_X (RADIUS + STEREO_DISPARITIES)
#define LAST_X (STEREO_WIDTH - RADIUS - 1U)

/* 5x5 Census, Hamming search, and 3x3 median adapted from
 * Stereo_Vision_RA6E2/ra6e2_sv/src/hal_entry.c (837b7a1). */
static uint8_t left_rows[CENSUS_ROWS][STEREO_WIDTH];
static uint8_t right_rows[CENSUS_ROWS][STEREO_WIDTH];
static uint32_t right_codes[STEREO_WIDTH];
static uint8_t match_rows[3][STEREO_WIDTH];
static uint8_t filtered[STEREO_WIDTH];
static const uint8_t border[STEREO_WIDTH];

static uint32_t census(const uint8_t *const rows[CENSUS_ROWS], unsigned x)
{
    uint8_t center = rows[RADIUS][x];
    uint32_t code = 0;
    for (unsigned y = 0; y < CENSUS_ROWS; ++y)
    {
        for (unsigned i = x - RADIUS; i <= x + RADIUS; ++i)
        {
            /* The center comparison contributes a constant zero bit. */
            code = (code << 1) | (rows[y][i] < center);
        }
    }
    return code;
}

static void match_row(unsigned y)
{
    const uint8_t *left[CENSUS_ROWS];
    const uint8_t *right[CENSUS_ROWS];
    for (unsigned i = 0; i < CENSUS_ROWS; ++i)
    {
        unsigned row = (y - RADIUS + i) % CENSUS_ROWS;
        left[i] = left_rows[row];
        right[i] = right_rows[row];
    }
    for (unsigned x = RADIUS; x <= LAST_X; ++x)
    {
        right_codes[x] = census(right, x);
    }
    uint8_t *out = match_rows[y % 3U];
    for (unsigned x = FIRST_X; x <= LAST_X; ++x)
    {
        uint32_t left_code = census(left, x);
        unsigned best_cost = 26U;
        unsigned best_d = 0;
        for (unsigned d = 0; d < STEREO_DISPARITIES; ++d)
        {
            unsigned cost = (unsigned) __builtin_popcount(left_code ^ right_codes[x - d]);
            if (cost < best_cost)
            {
                best_cost = cost;
                best_d = d;
            }
        }
        out[x] = (uint8_t) best_d;
    }
}

static const uint8_t *median_row(unsigned y)
{
    memcpy(filtered, match_rows[y % 3U], STEREO_WIDTH);
    const uint8_t *rows[] = {match_rows[(y - 1U) % 3U], match_rows[y % 3U],
                             match_rows[(y + 1U) % 3U]};
    for (unsigned x = FIRST_X + 1U; x < LAST_X; ++x)
    {
        uint8_t sorted[9];
        unsigned count = 0;
        for (unsigned i = 0; i < 3; ++i)
        {
            for (unsigned j = x - 1U; j <= x + 1U; ++j)
            {
                uint8_t value = rows[i][j];
                unsigned position = count++;
                while (position > 0 && sorted[position - 1U] > value)
                {
                    sorted[position] = sorted[position - 1U];
                    --position;
                }
                sorted[position] = value;
            }
        }
        filtered[x] = sorted[4];
    }
    return filtered;
}

bool stereo_process_pair(stereo_read_row_t read_row, stereo_write_row_t write_row)
{
    for (unsigned i = 0; i < CENSUS_ROWS - 1U; ++i)
    {
        if (!read_row(left_rows[i], right_rows[i]))
        {
            return false;
        }
    }
    if (!write_row(border) || !write_row(border))
    {
        return false;
    }

    for (unsigned y = RADIUS; y < STEREO_HEIGHT - RADIUS; ++y)
    {
        unsigned slot = (y + RADIUS) % CENSUS_ROWS;
        if (!read_row(left_rows[slot], right_rows[slot]))
        {
            return false;
        }
        match_row(y);
        if (y > RADIUS)
        {
            unsigned previous = y - 1U;
            const uint8_t *row = previous == RADIUS ? match_rows[previous % 3U] : median_row(previous);
            if (!write_row(row))
            {
                return false;
            }
        }
    }
    return write_row(match_rows[(STEREO_HEIGHT - RADIUS - 1U) % 3U]) &&
           write_row(border) && write_row(border);
}
