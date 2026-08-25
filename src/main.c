#include "epidemic.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif

static int make_output_directories(void)
{
#ifdef _WIN32
    return _mkdir("output") == 0 || errno == EEXIST ? (_mkdir("output/plots") == 0 || errno == EEXIST) : 0;
#else
    return mkdir("output", 0775) == 0 || errno == EEXIST ? (mkdir("output/plots", 0775) == 0 || errno == EEXIST) : 0;
#endif
}

static void usage(const char *program)
{
    printf("Usage: %s [options]\n\n", program);
    printf("Options:\n");
    printf("  --model SIR|SEIR|SEIHRS   Compartment model\n");
    printf("  --input PATH              First scenario\n");
    printf("  --input2 PATH             Optional second scenario\n");
    printf("  --deterministic           Use forward-Euler simulation (default)\n");
    printf("  --stochastic              Use stochastic competing events\n");
    printf("  --replicates NUMBER       Number of stochastic replicates\n");
    printf("  --app                     Apply contact-tracing transmission factor\n");
    printf("  --vaccine                 Vaccinate 20%% of susceptible people\n");
    printf("  --seed NUMBER             Reproducible stochastic seed\n");
    printf("  --dt NUMBER               Deterministic step size, 0 < dt <= 1 (default 1)\n");
    printf("  --no-plot                 Do not launch Gnuplot\n");
    printf("  --save-plot               Generate a PNG plot\n");
    printf("  --help                    Show this help\n\n");
    printf("Examples:\n");
    printf("  %s --model SEIHRS --input scenarios/copenhagen_scenario.txt --deterministic\n", program);
    printf("  %s --model SEIHRS --input scenarios/copenhagen_scenario.txt --input2 scenarios/aau_no_initial_infections.txt --stochastic --replicates 100 --seed 42\n", program);
}

static int read_line(const char *prompt, char *buffer, size_t size)
{
    printf("%s", prompt);
    if (size > (size_t)INT_MAX) {
        return 0;
    }
    if (fgets(buffer, (int)size, stdin) == NULL) {
        return 0;
    }
    if (strchr(buffer, '\n') == NULL && strchr(buffer, '\r') == NULL) {
        int character;
        while ((character = getchar()) != '\n' && character != '\r' && character != EOF) {
            /* Discard the remainder of an overlong input line. */
        }
    }
    buffer[strcspn(buffer, "\r\n")] = '\0';
    return 1;
}

static void lowercase(char *text)
{
    while (*text != '\0') {
        if (*text >= 'A' && *text <= 'Z') {
            *text = (char)(*text - 'A' + 'a');
        }
        ++text;
    }
}

static int parse_positive_int(const char *text, int *value)
{
    char *end;
    long parsed;
    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed <= 0 || parsed > 1000000L) {
        return 0;
    }
    *value = (int)parsed;
    return 1;
}

static int interactive_options(ModelType *model, char *path1, char *path2,
                               size_t *input_count, SimulationOptions *options,
                               int *replicates)
{
    char answer[64];
    if (!read_line("Model (SIR, SEIR, or SEIHRS): ", answer, sizeof(answer)) ||
        !parse_model_type(answer, model)) {
        fprintf(stderr, "Invalid model. Choose SIR, SEIR, or SEIHRS.\n");
        return 0;
    }
    if (!read_line("First scenario path: ", path1, EPIDEMIC_MAX_PATH) || path1[0] == '\0') {
        fprintf(stderr, "A first scenario path is required.\n");
        return 0;
    }
    if (!read_line("Use a second scenario? (yes/no): ", answer, sizeof(answer))) {
        return 0;
    }
    lowercase(answer);
    if (strcmp(answer, "yes") != 0 && strcmp(answer, "y") != 0 &&
        strcmp(answer, "no") != 0 && strcmp(answer, "n") != 0) {
        fprintf(stderr, "Please answer yes or no.\n");
        return 0;
    }
    *input_count = (strcmp(answer, "yes") == 0 || strcmp(answer, "y") == 0) ? 2U : 1U;
    if (*input_count == 2U && (!read_line("Second scenario path: ", path2, EPIDEMIC_MAX_PATH) || path2[0] == '\0')) {
        fprintf(stderr, "A second scenario path is required.\n");
        return 0;
    }
    if (!read_line("Simulation (deterministic/stochastic): ", answer, sizeof(answer))) {
        return 0;
    }
    lowercase(answer);
    options->stochastic = strcmp(answer, "stochastic") == 0 ? 1 : 0;
    if (strcmp(answer, "deterministic") != 0 && !options->stochastic) {
        fprintf(stderr, "Invalid simulation selection.\n");
        return 0;
    }
    if (options->stochastic) {
        if (!read_line("Replicates: ", answer, sizeof(answer)) || !parse_positive_int(answer, replicates)) {
            fprintf(stderr, "Replicate count must be a positive integer.\n");
            return 0;
        }
    }
    if (!read_line("Enable the app? (yes/no): ", answer, sizeof(answer))) {
        return 0;
    }
    lowercase(answer);
    options->use_app = strcmp(answer, "yes") == 0 || strcmp(answer, "y") == 0;
    if (!read_line("Use vaccination? (yes/no): ", answer, sizeof(answer))) {
        return 0;
    }
    lowercase(answer);
    options->use_vaccine = strcmp(answer, "yes") == 0 || strcmp(answer, "y") == 0;
    return 1;
}

int main(int argc, char **argv)
{
    ModelType model = MODEL_SIR;
    SimulationOptions options = default_simulation_options();
    const char *input1 = NULL;
    const char *input2 = NULL;
    char path1[EPIDEMIC_MAX_PATH] = {0};
    char path2[EPIDEMIC_MAX_PATH] = {0};
    size_t input_count = 1U;
    int replicates = 1;
    int no_plot = 0;
    int save_plot = 0;
    int index;
    Scenario scenarios[2];
    SimulationSummary summary;
    char error[EPIDEMIC_MAX_ERROR];
    FILE *output;
    const char *output_path = "output/data_file.txt";
    const char *script_path = "output/plot.gnu";

    for (index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "--help") == 0) {
            usage(argv[0]);
            return EXIT_SUCCESS;
        } else if (strcmp(argv[index], "--model") == 0 && index + 1 < argc) {
            if (!parse_model_type(argv[++index], &model)) {
                fprintf(stderr, "Invalid model. Choose SIR, SEIR, or SEIHRS.\n");
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[index], "--input") == 0 && index + 1 < argc) {
            input1 = argv[++index];
        } else if (strcmp(argv[index], "--input2") == 0 && index + 1 < argc) {
            input2 = argv[++index];
            input_count = 2U;
        } else if (strcmp(argv[index], "--stochastic") == 0) {
            options.stochastic = 1;
        } else if (strcmp(argv[index], "--deterministic") == 0) {
            options.stochastic = 0;
        } else if (strcmp(argv[index], "--replicates") == 0 && index + 1 < argc) {
            if (!parse_positive_int(argv[++index], &replicates)) {
                fprintf(stderr, "Replicate count must be a positive integer.\n");
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[index], "--app") == 0) {
            options.use_app = 1;
        } else if (strcmp(argv[index], "--vaccine") == 0) {
            options.use_vaccine = 1;
        } else if (strcmp(argv[index], "--seed") == 0 && index + 1 < argc) {
            char *end;
            unsigned long seed = strtoul(argv[++index], &end, 10);
            if (*argv[index] == '\0' || *end != '\0' || seed > 4294967295UL) {
                fprintf(stderr, "Seed must be an unsigned integer.\n");
                return EXIT_FAILURE;
            }
            options.seed_set = 1;
            options.seed = (unsigned int)seed;
        } else if (strcmp(argv[index], "--dt") == 0 && index + 1 < argc) {
            char *end;
            options.dt = strtod(argv[++index], &end);
            if (*argv[index] == '\0' || *end != '\0' || options.dt <= 0.0 || options.dt > 1.0) {
                fprintf(stderr, "--dt must be a number greater than 0 and no greater than 1.\n");
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[index], "--no-plot") == 0) {
            no_plot = 1;
        } else if (strcmp(argv[index], "--save-plot") == 0) {
            save_plot = 1;
        } else {
            fprintf(stderr, "Unknown or incomplete option: %s\n", argv[index]);
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }
    if (argc == 1) {
        if (!interactive_options(&model, path1, path2, &input_count, &options, &replicates)) {
            return EXIT_FAILURE;
        }
        input1 = path1;
        input2 = input_count == 2U ? path2 : NULL;
    }
    if (input1 == NULL) {
        fprintf(stderr, "--input is required when not using interactive mode.\n");
        return EXIT_FAILURE;
    }
    if (!load_scenario_path(input1, &scenarios[0], error, sizeof(error))) {
        fprintf(stderr, "%s\n", error);
        return EXIT_FAILURE;
    }
    if (input_count == 2U && (input2 == NULL || !load_scenario_path(input2, &scenarios[1], error, sizeof(error)))) {
        fprintf(stderr, "%s\n", input2 == NULL ? "A second scenario path is required" : error);
        return EXIT_FAILURE;
    }
    if (!make_output_directories()) {
        fprintf(stderr, "Cannot create output/plots directories.\n");
        return EXIT_FAILURE;
    }
    output = fopen(output_path, "w");
    if (output == NULL) {
        fprintf(stderr, "Cannot create %s.\n", output_path);
        return EXIT_FAILURE;
    }
    if (options.stochastic) {
        if (!run_replicates(scenarios, input_count, model, &options, replicates, output, error, sizeof(error))) {
            (void)fclose(output);
            fprintf(stderr, "%s\n", error);
            return EXIT_FAILURE;
        }
    } else if (!simulate(scenarios, input_count, model, &options, output, &summary, error, sizeof(error))) {
        (void)fclose(output);
        fprintf(stderr, "%s\n", error);
        return EXIT_FAILURE;
    }
    if (fclose(output) != 0 || !write_plot_script(script_path, output_path, model, input_count,
                                                   options.stochastic ? replicates : 1,
                                                   save_plot, error, sizeof(error))) {
        fprintf(stderr, "%s\n", error[0] == '\0' ? "Could not finish output files" : error);
        return EXIT_FAILURE;
    }
    printf("Simulation completed successfully.\nResults written to %s.\n", output_path);
    if (!no_plot) {
        if (gnuplot_available()) {
            printf("Opening plot with Gnuplot...\n");
            if (!launch_gnuplot(script_path)) {
                fprintf(stderr, "Gnuplot was found but could not launch the plot.\n");
            }
        } else {
            printf("Gnuplot was not found. Install Gnuplot and add it to PATH, or run with --no-plot.\n");
        }
    }
    return EXIT_SUCCESS;
}
