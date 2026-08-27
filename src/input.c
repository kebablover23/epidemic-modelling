#include "epidemic.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#define EPIDEMIC_ISFINITE(value) __builtin_isfinite(value)
#else
#define EPIDEMIC_ISFINITE(value) isfinite(value)
#endif

static void set_error(char *error, size_t error_size, const char *message)
{
    if (error != NULL && error_size > 0) {
        (void)snprintf(error, error_size, "%s", message);
    }
}

static void set_field_error(char *error, size_t error_size, const char *field,
                            const char *reason)
{
    if (error != NULL && error_size > 0) {
        (void)snprintf(error, error_size, "Invalid %s: %s", field, reason);
    }
}

static char *trim(char *text)
{
    char *end;
    while (isspace((unsigned char)*text) != 0) {
        text++;
    }
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1]) != 0) {
        --end;
    }
    *end = '\0';
    return text;
}

static void uppercase(char *text)
{
    while (*text != '\0') {
        *text = (char)toupper((unsigned char)*text);
        ++text;
    }
}

static int parse_double_value(const char *text, double *value)
{
    char *end;
    double parsed;
    errno = 0;
    parsed = strtod(text, &end);
    if (text == end || errno == ERANGE || !EPIDEMIC_ISFINITE(parsed)) {
        return 0;
    }
    while (isspace((unsigned char)*end) != 0) {
        ++end;
    }
    if (*end != '\0') {
        return 0;
    }
    *value = parsed;
    return 1;
}

const char *model_type_name(ModelType model)
{
    switch (model) {
    case MODEL_SIR:
        return "SIR";
    case MODEL_SEIR:
        return "SEIR";
    case MODEL_SEIHRS:
        return "SEIHRS";
    default:
        return "unknown";
    }
}

int parse_model_type(const char *text, ModelType *model)
{
    char normalized[16];
    size_t length;
    if (text == NULL || model == NULL) {
        return 0;
    }
    length = strlen(text);
    if (length >= sizeof(normalized)) {
        return 0;
    }
    (void)memcpy(normalized, text, length + 1);
    uppercase(normalized);
    if (strcmp(normalized, "SIR") == 0) {
        *model = MODEL_SIR;
    } else if (strcmp(normalized, "SEIR") == 0) {
        *model = MODEL_SEIR;
    } else if (strcmp(normalized, "SEIHRS") == 0) {
        *model = MODEL_SEIHRS;
    } else {
        return 0;
    }
    return 1;
}

int validate_scenario(const Scenario *scenario, char *error, size_t error_size)
{
    double total;
    if (scenario == NULL) {
        set_error(error, error_size, "Scenario is null");
        return 0;
    }
    if (scenario->days <= 0) {
        set_field_error(error, error_size, "DAYS", "must be greater than zero");
        return 0;
    }
    if (!EPIDEMIC_ISFINITE(scenario->N) || scenario->N <= 0.0) {
        set_field_error(error, error_size, "N", "must be finite and greater than zero");
        return 0;
    }
    if (!EPIDEMIC_ISFINITE(scenario->S) || scenario->S < 0.0) {
        set_field_error(error, error_size, "S", "must be finite and non-negative");
        return 0;
    }
    if (!EPIDEMIC_ISFINITE(scenario->E) || scenario->E < 0.0) {
        set_field_error(error, error_size, "E", "must be finite and non-negative");
        return 0;
    }
    if (!EPIDEMIC_ISFINITE(scenario->I) || scenario->I < 0.0) {
        set_field_error(error, error_size, "I", "must be finite and non-negative");
        return 0;
    }
    if (!EPIDEMIC_ISFINITE(scenario->H) || scenario->H < 0.0) {
        set_field_error(error, error_size, "H", "must be finite and non-negative");
        return 0;
    }
    if (!EPIDEMIC_ISFINITE(scenario->R) || scenario->R < 0.0) {
        set_field_error(error, error_size, "R", "must be finite and non-negative");
        return 0;
    }
    total = scenario->S + scenario->E + scenario->I + scenario->H + scenario->R;
    if (fabs(total - scenario->N) > EPIDEMIC_TOLERANCE * fmax(1.0, scenario->N)) {
        set_field_error(error, error_size, "compartments", "S + E + I + H + R must equal N");
        return 0;
    }
    if (!EPIDEMIC_ISFINITE(scenario->beta) || scenario->beta < 0.0) {
        set_field_error(error, error_size, "BETA", "must be finite and non-negative");
        return 0;
    }
    if (!EPIDEMIC_ISFINITE(scenario->gamma) || scenario->gamma < 0.0) {
        set_field_error(error, error_size, "GAMMA", "must be finite and non-negative");
        return 0;
    }
    if (!EPIDEMIC_ISFINITE(scenario->sigma) || scenario->sigma < 0.0) {
        set_field_error(error, error_size, "SIGMA", "must be finite and non-negative");
        return 0;
    }
    if (!EPIDEMIC_ISFINITE(scenario->hospitalization_rate) || scenario->hospitalization_rate < 0.0) {
        set_field_error(error, error_size, "HOSPITALIZATION_RATE", "must be finite and non-negative");
        return 0;
    }
    if (!EPIDEMIC_ISFINITE(scenario->migration_rate) || scenario->migration_rate < 0.0 || scenario->migration_rate > 1.0) {
        set_field_error(error, error_size, "MIGRATION_RATE", "must be between 0 and 1");
        return 0;
    }
    return 1;
}

static int field_index(const char *key)
{
    static const char *const names[] = {
        "DAYS", "N", "S", "E", "I", "H", "R",
        "BETA", "GAMMA", "SIGMA", "HOSPITALIZATION_RATE",
        "MIGRATION_RATE"
    };
    size_t i;
    for (i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (strcmp(key, names[i]) == 0) {
            return (int)i;
        }
    }
    return -1;
}

int load_scenario(FILE *input, Scenario *scenario, char *error, size_t error_size)
{
    char line[512];
    unsigned int seen = 0U;
    Scenario parsed = {0};
    unsigned int duplicate_mask = 0U;
    if (input == NULL) {
        set_error(error, error_size, "Cannot read a null scenario stream");
        return 0;
    }
    if (scenario == NULL) {
        set_error(error, error_size, "Scenario destination is null");
        return 0;
    }
    while (fgets(line, sizeof(line), input) != NULL) {
        char *text = trim(line);
        char *equals;
        char key[64];
        char *value_text;
        double value;
        size_t key_length;
        if (*text == '\0' || *text == '#') {
            continue;
        }
        if (strncmp(text, "//", 2) == 0) {
            continue;
        }
        equals = strchr(text, '=');
        if (equals == NULL) {
            set_error(error, error_size, "Malformed scenario line: expected KEY = VALUE");
            return 0;
        }
        *equals = '\0';
        value_text = trim(equals + 1);
        text = trim(text);
        if (*text == '#') {
            continue;
        }
        key_length = strlen(text);
        if (key_length == 0 || key_length >= sizeof(key)) {
            set_error(error, error_size, "Malformed scenario key");
            return 0;
        }
        (void)memcpy(key, text, key_length + 1);
        if (strcmp(key, "h") == 0) {
            (void)snprintf(key, sizeof(key), "%s", "HOSPITALIZATION_RATE");
        } else if (strcmp(key, "t") == 0) {
            (void)snprintf(key, sizeof(key), "%s", "MIGRATION_RATE");
        } else {
            uppercase(key);
        }
        if (strcmp(key, "TID I DAGE") == 0) {
            (void)snprintf(key, sizeof(key), "%s", "DAYS");
        }
        if (!parse_double_value(value_text, &value)) {
            set_field_error(error, error_size, key, "expected a finite number");
            return 0;
        }
        {
            int field = field_index(key);
            unsigned int bit;
            if (field < 0) {
                set_field_error(error, error_size, key, "unknown field");
                return 0;
            }
            bit = (unsigned int)field;
            if ((duplicate_mask & (1U << bit)) != 0U) {
                set_field_error(error, error_size, key, "duplicate field");
                return 0;
            }
            duplicate_mask |= 1U << bit;
        }
        switch (field_index(key)) {
        case 0:
            if (value < 0.0 || value > (double)INT_MAX || floor(value) != value) {
                set_field_error(error, error_size, "DAYS", "must be a positive integer");
                return 0;
            }
            parsed.days = (int)value;
            seen |= 1U << 0;
            break;
        case 1: parsed.N = value; seen |= 1U << 1; break;
        case 2: parsed.S = value; seen |= 1U << 2; break;
        case 3: parsed.E = value; seen |= 1U << 3; break;
        case 4: parsed.I = value; seen |= 1U << 4; break;
        case 5: parsed.H = value; seen |= 1U << 5; break;
        case 6: parsed.R = value; seen |= 1U << 6; break;
        case 7: parsed.beta = value; seen |= 1U << 7; break;
        case 8: parsed.gamma = value; seen |= 1U << 8; break;
        case 9: parsed.sigma = value; seen |= 1U << 9; break;
        case 10: parsed.hospitalization_rate = value; seen |= 1U << 10; break;
        case 11: parsed.migration_rate = value; seen |= 1U << 11; break;
        default:
            set_field_error(error, error_size, key, "unknown field");
            return 0;
        }
    }
    if (ferror(input) != 0) {
        set_error(error, error_size, "Error while reading scenario");
        return 0;
    }
    {
        static const char *const required[] = {
            "DAYS", "N", "S", "E", "I", "H", "R", "BETA",
            "GAMMA", "SIGMA", "HOSPITALIZATION_RATE", "MIGRATION_RATE"
        };
        size_t i;
        for (i = 0; i < sizeof(required) / sizeof(required[0]); ++i) {
            if ((seen & (1U << i)) == 0U) {
                char missing[EPIDEMIC_MAX_ERROR];
                (void)snprintf(missing, sizeof(missing), "Missing scenario field: %s", required[i]);
                set_error(error, error_size, missing);
                return 0;
            }
        }
    }
    if (!validate_scenario(&parsed, error, error_size)) {
        return 0;
    }
    *scenario = parsed;
    return 1;
}

int load_scenario_path(const char *path, Scenario *scenario, char *error, size_t error_size)
{
    FILE *input;
    int result;
    if (path == NULL || scenario == NULL) {
        set_error(error, error_size, "Scenario path and destination are required");
        return 0;
    }
    input = fopen(path, "r");
    if (input == NULL) {
        if (error != NULL && error_size > 0) {
            (void)snprintf(error, error_size, "Cannot open scenario '%s'. Run from the project root or check the path.", path);
        }
        return 0;
    }
    result = load_scenario(input, scenario, error, error_size);
    (void)fclose(input);
    return result;
}

SimulationOptions default_simulation_options(void)
{
    SimulationOptions options = {0, 0, 0, 0, 0U, EPIDEMIC_DEFAULT_DT};
    return options;
}
