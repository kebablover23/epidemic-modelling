#ifndef EPIDEMIC_H
#define EPIDEMIC_H

#include <stddef.h>
#include <stdio.h>

#define EPIDEMIC_MAX_ERROR 256
#define EPIDEMIC_MAX_PATH 512
#define EPIDEMIC_TOLERANCE 1.0e-7
#define EPIDEMIC_DEFAULT_DT 1.0
#define EPIDEMIC_VACCINE_COVERAGE 0.20
#define EPIDEMIC_VACCINE_TRANSMISSION_FACTOR 0.95
#define EPIDEMIC_APP_TRANSMISSION_FACTOR 0.80
#define EPIDEMIC_AGE_GROUPS 4

typedef enum {
    MODEL_SIR = 1,
    MODEL_SEIR = 2,
    MODEL_SEIHRS = 3
} ModelType;

typedef struct {
    int days;
    double N;
    double S;
    double E;
    double I;
    double H;
    double R;
    double protected_population;
    double beta;
    double gamma;
    double sigma;
    double hospitalization_rate;
    double migration_rate;
} Scenario;

typedef struct {
    int use_app;
    int use_vaccine;
    int stochastic;
    int seed_set;
    unsigned int seed;
    double dt;
} SimulationOptions;

typedef struct {
    double max_infected[2];
    double max_hospitalized[2];
    int max_infected_day[2];
    int max_hospitalized_day[2];
    double protected_population[2];
} SimulationSummary;

const char *model_type_name(ModelType model);
int parse_model_type(const char *text, ModelType *model);
int load_scenario(FILE *input, Scenario *scenario, char *error, size_t error_size);
int load_scenario_path(const char *path, Scenario *scenario, char *error, size_t error_size);
int validate_scenario(const Scenario *scenario, char *error, size_t error_size);

SimulationOptions default_simulation_options(void);
int simulate(const Scenario *models, size_t input_count, ModelType model,
             const SimulationOptions *options, FILE *output,
             SimulationSummary *summary, char *error, size_t error_size);
int run_replicates(const Scenario *models, size_t input_count, ModelType model,
                   const SimulationOptions *options, int replicates,
                   FILE *output, char *error, size_t error_size);

int expected_output_columns(ModelType model, size_t input_count);
int write_plot_script(const char *script_path, const char *data_path,
                      ModelType model, size_t input_count, int replicates,
                      int save_png, char *error, size_t error_size);
int gnuplot_available(void);
int launch_gnuplot(const char *script_path);

#endif
