#include "epidemic.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#ifdef _WIN32
#define TEST_TMPFILE_NAME "test_tmp_scenario.txt"
#else
#define TEST_TMPFILE_NAME "/tmp/epidemic_test_XXXXXX"
#endif

static int passed;
static int failed;

static void check(int condition, const char *name)
{
    if (condition) {
        ++passed;
        printf("PASS: %s\n", name);
    } else {
        ++failed;
        printf("FAIL: %s\n", name);
    }
}

static FILE *temporary_file(void)
{
#ifdef _WIN32
    (void)remove(TEST_TMPFILE_NAME);
    return fopen(TEST_TMPFILE_NAME, "w+b");
#else
    char path[] = TEST_TMPFILE_NAME;
    int descriptor = mkstemp(path);
    FILE *file;
    if (descriptor < 0) {
        return NULL;
    }
    file = fdopen(descriptor, "w+b");
    (void)remove(path);
    return file;
#endif
}

static Scenario reference_scenario(int days)
{
    Scenario scenario = {days, 100.0, 90.0, 0.0, 10.0, 0.0, 0.0, 0.0,
                         1.11, 0.111, 0.200, 0.01, 0.001};
    return scenario;
}

static int numeric_row_count(FILE *file, int expected_columns, double *last_row)
{
    char line[1024];
    int rows = 0;
    rewind(file);
    while (fgets(line, sizeof(line), file) != NULL) {
        char *cursor = line;
        int columns = 0;
        while (*cursor != '\0' && *cursor != '#') {
            char *end;
            (void)strtod(cursor, &end);
            if (end == cursor) {
                break;
            }
            if (last_row != NULL && columns < 12) {
                last_row[columns] = strtod(cursor, NULL);
            }
            ++columns;
            cursor = end;
        }
        if (columns == expected_columns) {
            ++rows;
        }
    }
    return rows;
}

static int close_simulation(FILE *file, const Scenario *scenario,
                            ModelType model, SimulationOptions *options,
                            SimulationSummary *summary, double *last_row)
{
    char error[EPIDEMIC_MAX_ERROR] = {0};
    int result = simulate(scenario, 1U, model, options, file, summary,
                          error, sizeof(error));
    check(result, error[0] == '\0' ? "simulation succeeds" : error);
    if (result) {
        check(numeric_row_count(file, expected_output_columns(model, 1U), last_row) == scenario->days + 1,
              "day 0 through DAYS are written");
    }
    (void)fclose(file);
    return result;
}

static void test_parser(void)
{
    const char *valid = "# comments\n// legacy comments\nDAYS=2\nN=100\nS=90\nE=0\nI=10\nH=0\nR=0\nBETA=1\nGAMMA=.1\nSIGMA=.2\nh=.01\nt=.001\n";
    const char *duplicate = "DAYS=1\nDAYS=2\nN=10\nS=9\nE=0\nI=1\nH=0\nR=0\nBETA=1\nGAMMA=.1\nSIGMA=.2\nHOSPITALIZATION_RATE=.1\nMIGRATION_RATE=.001\n";
    FILE *file = temporary_file();
    Scenario scenario;
    char error[EPIDEMIC_MAX_ERROR] = {0};
    check(file != NULL, "temporary parser file is created");
    if (file == NULL) return;
    (void)fputs(valid, file);
    rewind(file);
    check(load_scenario(file, &scenario, error, sizeof(error)), "English and legacy fields parse");
    check(scenario.days == 2 && fabs(scenario.hospitalization_rate - .01) < 1e-12 &&
          fabs(scenario.migration_rate - .001) < 1e-12, "H/h and t mappings are distinct");
    (void)fclose(file);
    file = temporary_file();
    (void)fputs(duplicate, file);
    rewind(file);
    check(!load_scenario(file, &scenario, error, sizeof(error)) && strstr(error, "duplicate") != NULL,
          "duplicate fields are rejected");
    (void)fclose(file);
}

static void test_one_day_sir(void)
{
    Scenario scenario = reference_scenario(1);
    SimulationOptions options = default_simulation_options();
    SimulationSummary summary;
    double row[12] = {0.0};
    FILE *file = temporary_file();
    if (file == NULL) {
        check(0, "one-day SIR temporary file");
        return;
    }
    close_simulation(file, &scenario, MODEL_SIR, &options, &summary, row);
    check(fabs(row[0] - 1.0) < 1e-12 && fabs(row[2] - 18.88) < 1e-9,
          "one-day SIR gives I = 18.88");
    check(fabs(row[1] + row[2] + row[3] - scenario.N) < 1e-7,
          "SIR conserves population");
    check(fabs(row[1] - 80.01) < 1e-9 && fabs(row[3] - 1.11) < 1e-9,
          "one-day SIR reference S and R values match");
}

static void test_model_conservation(void)
{
    ModelType models[] = {MODEL_SIR, MODEL_SEIR, MODEL_SEIHRS};
    size_t index;
    for (index = 0; index < sizeof(models) / sizeof(models[0]); ++index) {
        Scenario scenario = reference_scenario(5);
        SimulationOptions options = default_simulation_options();
        SimulationSummary summary;
        double row[12] = {0.0};
        FILE *file = temporary_file();
        if (file == NULL) {
            check(0, "model conservation temporary file");
            continue;
        }
        if (models[index] == MODEL_SEIHRS) {
            scenario.H = 1.0;
            scenario.I = 9.0;
        }
        close_simulation(file, &scenario, models[index], &options, &summary, row);
        check(fabs(row[1] + row[2] + (models[index] == MODEL_SIR ? 0.0 : row[3]) +
                   (models[index] == MODEL_SEIHRS ? row[4] : 0.0) +
                   row[models[index] == MODEL_SIR ? 3 : (models[index] == MODEL_SEIR ? 4 : 5)] - scenario.N) < 1e-5,
              model_type_name(models[index]));
    }
}

static void test_sir_hidden_compartments_and_dt(void)
{
    Scenario scenario = reference_scenario(2);
    SimulationOptions options = default_simulation_options();
    SimulationSummary summary;
    double row[12] = {0.0};
    FILE *file = temporary_file();
    if (file == NULL) {
        check(0, "SIR dt temporary file");
        return;
    }
    scenario.E = 0.0;
    scenario.I = 10.0;
    scenario.H = 0.0;
    close_simulation(file, &scenario, MODEL_SIR, &options, &summary, row);
    check(fabs(row[1] + row[2] + row[3] - scenario.N) < 1e-7,
          "SIR output remains three-compartment and conservative");
    options.dt = 0.25;
    file = temporary_file();
    if (file != NULL) {
        close_simulation(file, &scenario, MODEL_SIR, &options, &summary, row);
        check(fabs(row[2] - 15.04) > 1e-6, "smaller dt performs full-day substeps");
    }
}

static void test_vaccine_and_seed(void)
{
    Scenario scenario = reference_scenario(1);
    SimulationOptions options = default_simulation_options();
    SimulationSummary summary;
    double row[12] = {0.0};
    FILE *file = temporary_file();
    if (file == NULL) {
        check(0, "intervention temporary file");
        return;
    }
    options.use_vaccine = 1;
    close_simulation(file, &scenario, MODEL_SIR, &options, &summary, row);
    check(fabs(summary.protected_population[0] - 18.0) < 1e-9,
          "exactly 20 percent of initial S is protected once");
    check(fabs(row[1] + row[2] + row[3] + summary.protected_population[0] - scenario.N) < 1e-7,
          "vaccination preserves total population");
    check(fabs(row[2] - (10.0 + 1.11 * 0.95 * 72.0 * 10.0 / 100.0 - 1.11)) < 1e-9,
          "vaccine transmission factor is applied once");
    file = temporary_file();
    if (file != NULL) {
        options.use_app = 1;
        close_simulation(file, &scenario, MODEL_SIR, &options, &summary, row);
        check(fabs(row[2] - (10.0 + 1.11 * 0.95 * 0.80 * 72.0 * 10.0 / 100.0 - 1.11)) < 1e-9,
              "app and vaccine factors are each applied once");
        (void)fclose(file);
    }
    options = default_simulation_options();
    options.stochastic = 1;
    options.seed_set = 1;
    options.seed = 42U;
    file = temporary_file();
    if (file != NULL) {
        close_simulation(file, &scenario, MODEL_SIR, &options, &summary, row);
    }
    check(file != NULL, "fixed seed stochastic run completes");
}

static void test_migration_and_duration(void)
{
    Scenario models[2] = {reference_scenario(3), reference_scenario(3)};
    SimulationOptions options = default_simulation_options();
    SimulationSummary summary;
    char error[EPIDEMIC_MAX_ERROR] = {0};
    double row[12] = {0.0};
    FILE *file = temporary_file();
    if (file == NULL) {
        check(0, "migration temporary file");
        return;
    }
    models[1].S = 100.0;
    models[1].I = 0.0;
    models[0].migration_rate = 0.001;
    models[1].migration_rate = 0.001;
    check(simulate(models, 2U, MODEL_SEIR, &options, file, &summary, error, sizeof(error)),
          "two-population migration succeeds");
    check(numeric_row_count(file, 9, row) == 4 && row[7] > 0.0,
          "migration transfers infection and writes two-population columns");
    check(fabs(row[1] + row[2] + row[3] + row[4] + row[5] + row[6] + row[7] + row[8] - 200.0) < 1e-5,
          "combined population is conserved with migration");
    (void)fclose(file);
    models[1].days = 2;
    file = temporary_file();
    check(!simulate(models, 2U, MODEL_SEIR, &options, file, &summary, error, sizeof(error)) &&
          strstr(error, "same DAYS") != NULL, "different scenario durations are rejected");
    (void)fclose(file);
}

static size_t read_script(const char *path, char *contents, size_t capacity)
{
    FILE *file = fopen(path, "r");
    size_t length;
    if (file == NULL || capacity == 0U) {
        if (file != NULL) (void)fclose(file);
        return 0U;
    }
    length = fread(contents, 1U, capacity - 1U, file);
    (void)fclose(file);
    contents[length] = '\0';
    return length;
}

static void test_plot_scripts(void)
{
    char error[EPIDEMIC_MAX_ERROR] = {0};
    char contents[8192];
    size_t length;
    check(write_plot_script("test_sir.gnu", "output/data.txt", MODEL_SIR, 1U, 1, 0,
                            error, sizeof(error)), "SIR plot script is generated");
    length = read_script("test_sir.gnu", contents, sizeof(contents));
    check(length > 0U && strstr(contents, "title 'S'") != NULL &&
          strstr(contents, "title 'I'") != NULL && strstr(contents, "title 'R'") != NULL &&
          strstr(contents, "title 'E'") == NULL && strstr(contents, "title 'H'") == NULL,
          "SIR uses only S, I, and R labels");
    check(strstr(contents, "set terminal qt size 1400,900") != NULL &&
          strstr(contents, "set key at screen 0.50,0.920") != NULL,
          "SIR uses a large window and compact key");

    check(write_plot_script("test_seir.gnu", "output/data.txt", MODEL_SEIR, 1U, 1, 0,
                            error, sizeof(error)), "SEIR plot script is generated");
    length = read_script("test_seir.gnu", contents, sizeof(contents));
    check(length > 0U && strstr(contents, "set multiplot") != NULL &&
          strstr(contents, "set origin 0.08,0.54") != NULL &&
          strstr(contents, "set origin 0.08,0.08") != NULL &&
          strstr(contents, "unset xlabel") != NULL && strstr(contents, "set xlabel 'Days'") != NULL,
          "SEIR uses explicit two-panel geometry and one x-axis label");
    check(strstr(contents, "title 'S'") != NULL && strstr(contents, "title 'E'") != NULL &&
          strstr(contents, "title 'I'") != NULL && strstr(contents, "title 'R'") != NULL &&
          strstr(contents, "title 'H'") == NULL,
          "SEIR uses S, E, I, and R without H");
    check(strstr(contents, "using 1:3") != NULL && strstr(contents, "using 1:4") != NULL,
          "SEIR active panel plots E and I");

    check(write_plot_script("test_seihrs.gnu", "output/data.txt", MODEL_SEIHRS, 1U, 1, 0,
                            error, sizeof(error)), "SEIHRS plot script is generated");
    length = read_script("test_seihrs.gnu", contents, sizeof(contents));
    check(length > 0U && strstr(contents, "title 'S'") != NULL &&
          strstr(contents, "title 'E'") != NULL && strstr(contents, "title 'I'") != NULL &&
          strstr(contents, "title 'H'") != NULL && strstr(contents, "title 'R'") != NULL,
          "SEIHRS uses S, E, I, H, and R");
    check(strstr(contents, "using 1:3") != NULL && strstr(contents, "using 1:4") != NULL &&
          strstr(contents, "using 1:5") != NULL,
          "SEIHRS active panel plots E, I, and H");
    check(strstr(contents, "unset multiplot") != NULL,
          "multiplot scripts clean up with unset multiplot");

    check(write_plot_script("test_two_populations.gnu", "output/data.txt", MODEL_SEIHRS, 2U, 1, 0,
                            error, sizeof(error)), "two-population plot script is generated");
    length = read_script("test_two_populations.gnu", contents, sizeof(contents));
    check(length > 0U && strstr(contents, "title 'S1'") != NULL &&
          strstr(contents, "title 'R1'") != NULL && strstr(contents, "title 'S2'") != NULL &&
          strstr(contents, "title 'R2'") != NULL &&
          strstr(contents, "Susceptible population") == NULL,
          "two-population plot uses compact labels and no long legend text");
    check(strstr(contents, "with lines dt 2 lw 2") != NULL &&
          strstr(contents, "set key maxrows 2") != NULL,
          "population two uses dashed lines and compact multi-row key");

    check(write_plot_script("test_replicates.gnu", "output/data.txt", MODEL_SEIHRS, 1U, 3, 0,
                            error, sizeof(error)), "replicate plot script is generated");
    length = read_script("test_replicates.gnu", contents, sizeof(contents));
    check(length > 0U && strstr(contents, "index 0") != NULL &&
          strstr(contents, "index rep") != NULL && strstr(contents, " notitle") != NULL &&
          strstr(contents, "lw 1") != NULL,
          "replicates use separate indexes, one legend, and thin lines");

    check(write_plot_script("test_png.gnu", "output/data.txt", MODEL_SEIR, 1U, 1, 1,
                            error, sizeof(error)), "PNG plot script is generated");
    length = read_script("test_png.gnu", contents, sizeof(contents));
    check(length > 0U && strstr(contents, "set terminal pngcairo size 1600,1000") != NULL &&
          strstr(contents, "set output 'output/plots/epidemic.png'") != NULL &&
          strstr(contents, "pause") == NULL,
          "PNG uses a large canvas without an interactive pause");

    (void)remove("test_seihrs.gnu");
    (void)remove("test_seir.gnu");
    (void)remove("test_sir.gnu");
    (void)remove("test_two_populations.gnu");
    (void)remove("test_replicates.gnu");
    (void)remove("test_png.gnu");
}

int main(void)
{
    test_parser();
    test_one_day_sir();
    test_model_conservation();
    test_sir_hidden_compartments_and_dt();
    test_vaccine_and_seed();
    test_migration_and_duration();
    test_plot_scripts();
    printf("\nPassed: %d\nFailed: %d\n", passed, failed);
    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
