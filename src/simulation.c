#include "epidemic.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#define EPIDEMIC_ISFINITE(value) __builtin_isfinite(value)
#else
#define EPIDEMIC_ISFINITE(value) isfinite(value)
#endif

#define VACCINE_COVERAGE EPIDEMIC_VACCINE_COVERAGE
#define VACCINE_TRANSMISSION_FACTOR EPIDEMIC_VACCINE_TRANSMISSION_FACTOR
#define APP_TRANSMISSION_FACTOR EPIDEMIC_APP_TRANSMISSION_FACTOR
#define STEPS_PER_DAY 10

static const double age_beta[EPIDEMIC_AGE_GROUPS] = {1.0, 1.0, 1.0, 1.0};
static const double age_hospitalization[EPIDEMIC_AGE_GROUPS] = {0.03, 0.03, 0.03, 0.10};
static const double immunity_loss_rate[EPIDEMIC_AGE_GROUPS] = {
    1.0 / 365.0, 1.0 / 300.0, 1.0 / 250.0, 1.0 / 180.0
};

typedef struct {
    double s;
    double e;
    double i;
    double h;
    double r;
    double protected_population;
} State;

typedef struct {
    State state[2][EPIDEMIC_AGE_GROUPS];
    size_t input_count;
} SimulationState;

typedef struct {
    double s;
    double e;
    double i;
    double h;
    double r;
} Derivative;

static void set_error(char *error, size_t error_size, const char *message)
{
    if (error != NULL && error_size > 0) {
        (void)snprintf(error, error_size, "%s", message);
    }
}

static double clamp_nonnegative(double value)
{
    return value < 0.0 ? 0.0 : value;
}

static void apply_interventions(Scenario *scenario, int use_app, int use_vaccine)
{
    scenario->protected_population = 0.0;
    if (use_vaccine) {
        scenario->protected_population = VACCINE_COVERAGE * scenario->S;
        scenario->S -= scenario->protected_population;
        scenario->beta *= VACCINE_TRANSMISSION_FACTOR;
    }
    if (use_app) {
        scenario->beta *= APP_TRANSMISSION_FACTOR;
    }
}

static void initialize_state(const Scenario *models, size_t input_count,
                             ModelType model, SimulationState *state)
{
    size_t population;
    size_t age;
    (void)memset(state, 0, sizeof(*state));
    state->input_count = input_count;
    for (population = 0; population < input_count; ++population) {
        const Scenario *adjusted = &models[population];
        for (age = 0; age < EPIDEMIC_AGE_GROUPS; ++age) {
            state->state[population][age].s = adjusted->S / EPIDEMIC_AGE_GROUPS;
            state->state[population][age].e = model == MODEL_SIR ? 0.0 : adjusted->E / EPIDEMIC_AGE_GROUPS;
            state->state[population][age].i = adjusted->I / EPIDEMIC_AGE_GROUPS;
            state->state[population][age].h = model == MODEL_SEIHRS ? adjusted->H / EPIDEMIC_AGE_GROUPS : 0.0;
            state->state[population][age].r = adjusted->R / EPIDEMIC_AGE_GROUPS;
            state->state[population][age].protected_population = adjusted->protected_population / EPIDEMIC_AGE_GROUPS;
        }
    }
}

static State sum_population(const SimulationState *state, size_t population)
{
    State total = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    size_t age;
    for (age = 0; age < EPIDEMIC_AGE_GROUPS; ++age) {
        total.s += state->state[population][age].s;
        total.e += state->state[population][age].e;
        total.i += state->state[population][age].i;
        total.h += state->state[population][age].h;
        total.r += state->state[population][age].r;
        total.protected_population += state->state[population][age].protected_population;
    }
    return total;
}

static double positive_rate(double rate)
{
    return EPIDEMIC_ISFINITE(rate) && rate > 0.0 ? rate : 0.0;
}

static long sample_poisson(double lambda)
{
    double product;
    long result;
    if (!EPIDEMIC_ISFINITE(lambda) || lambda <= 0.0) {
        return 0L;
    }
    if (lambda > (double)LONG_MAX - 1.0) {
        return LONG_MAX;
    }
    if (lambda > 30.0) {
        double u1 = ((double)rand() + 1.0) / ((double)RAND_MAX + 2.0);
        double u2 = (double)rand() / ((double)RAND_MAX + 1.0);
        double normal = sqrt(-2.0 * log(u1)) * cos(6.28318530717958647692 * u2);
        double approximation = lambda + sqrt(lambda) * normal + 0.5;
        if (approximation <= 0.0) {
            return 0L;
        }
        if (approximation >= (double)LONG_MAX) {
            return LONG_MAX;
        }
        return (long)approximation;
    }
    product = 1.0;
    result = 0L;
    do {
        double uniform = ((double)rand() + 1.0) / ((double)RAND_MAX + 2.0);
        product *= uniform;
        ++result;
    } while (product > exp(-lambda) && result < LONG_MAX);
    return result - 1L;
}

static long bounded_event(double rate, double dt, double available)
{
    long sampled;
    if (available <= 0.0) {
        return 0L;
    }
    sampled = sample_poisson(positive_rate(rate) * dt);
    if ((double)sampled > available) {
        return available >= (double)LONG_MAX ? LONG_MAX : (long)floor(available);
    }
    return sampled;
}

static void deterministic_derivative(const Scenario *models, const SimulationState *current,
                                     ModelType model, size_t population, size_t age,
                                     Derivative *derivative)
{
    State totals = sum_population(current, population);
    const Scenario *scenario = &models[population];
    double incidence = scenario->beta * age_beta[age] * current->state[population][age].s
                       * totals.i / scenario->N;
    double migration_out = scenario->migration_rate;
    double migration_in = 0.0;
    size_t other = population == 0U ? 1U : 0U;
    if (current->input_count == 2U) {
        migration_in = models[other].migration_rate;
    }
    derivative->s = -incidence;
    derivative->e = model == MODEL_SIR ? 0.0 : incidence;
    derivative->i = 0.0;
    derivative->h = 0.0;
    derivative->r = 0.0;
    if (model == MODEL_SIR) {
        derivative->i = incidence - scenario->gamma * current->state[population][age].i;
        derivative->r = scenario->gamma * current->state[population][age].i;
    } else if (model == MODEL_SEIR) {
        derivative->e = incidence - scenario->sigma * current->state[population][age].e;
        derivative->i = scenario->sigma * current->state[population][age].e
                      - scenario->gamma * current->state[population][age].i;
        derivative->r = scenario->gamma * current->state[population][age].i;
    } else {
        double hospitalization = scenario->hospitalization_rate * age_hospitalization[age]
                               * current->state[population][age].i;
        double immunity_loss = immunity_loss_rate[age] * current->state[population][age].r;
        derivative->e = incidence - scenario->sigma * current->state[population][age].e;
        derivative->i = scenario->sigma * current->state[population][age].e
                      - scenario->gamma * current->state[population][age].i - hospitalization;
        derivative->h = hospitalization - scenario->gamma * current->state[population][age].h;
        derivative->r = scenario->gamma * current->state[population][age].i
                      + scenario->gamma * current->state[population][age].h - immunity_loss;
        derivative->s += immunity_loss;
    }
    if (current->input_count == 2U) {
        State other_state = current->state[other][age];
        derivative->s += migration_in * other_state.s - migration_out * current->state[population][age].s;
        derivative->e += migration_in * other_state.e - migration_out * current->state[population][age].e;
        derivative->i += migration_in * other_state.i - migration_out * current->state[population][age].i;
        derivative->r += migration_in * other_state.r - migration_out * current->state[population][age].r;
    }
}

static void apply_deterministic_step(const Scenario *models, SimulationState *state,
                                     ModelType model, double dt)
{
    Derivative derivatives[2][EPIDEMIC_AGE_GROUPS];
    size_t population;
    size_t age;
    (void)memset(derivatives, 0, sizeof(derivatives));
    for (population = 0; population < state->input_count; ++population) {
        for (age = 0; age < EPIDEMIC_AGE_GROUPS; ++age) {
            deterministic_derivative(models, state, model, population, age,
                                     &derivatives[population][age]);
        }
    }
    for (population = 0; population < state->input_count; ++population) {
        for (age = 0; age < EPIDEMIC_AGE_GROUPS; ++age) {
            State *value = &state->state[population][age];
            const Derivative *change = &derivatives[population][age];
            value->s = clamp_nonnegative(value->s + dt * change->s);
            value->e = clamp_nonnegative(value->e + dt * change->e);
            value->i = clamp_nonnegative(value->i + dt * change->i);
            value->h = clamp_nonnegative(value->h + dt * change->h);
            value->r = clamp_nonnegative(value->r + dt * change->r);
        }
    }
}

static void stochastic_transition(State *value, const Scenario *scenario,
                                  ModelType model, size_t age, double dt,
                                  double infectious_total, double population)
{
    long infections = bounded_event(scenario->beta * age_beta[age] * value->s
                                    * infectious_total / population, dt, value->s);
    long progression = 0L;
    long recoveries = 0L;
    long hospitalizations = 0L;
    long hospital_recoveries = 0L;
    long immunity_losses = 0L;
    long available_infected;
    if (model != MODEL_SIR) {
        progression = bounded_event(scenario->sigma * value->e, dt, value->e);
    }
    available_infected = value->i >= (double)LONG_MAX ? LONG_MAX : (long)floor(value->i);
    if (model == MODEL_SEIHRS) {
        hospitalizations = bounded_event(scenario->hospitalization_rate * age_hospitalization[age]
                                          * value->i, dt, value->i);
    }
    if (hospitalizations > available_infected) {
        hospitalizations = available_infected;
    }
    recoveries = bounded_event(scenario->gamma * (double)(available_infected - hospitalizations),
                                dt, (double)(available_infected - hospitalizations));
    if (model == MODEL_SEIHRS) {
        hospital_recoveries = bounded_event(scenario->gamma * value->h, dt, value->h);
        immunity_losses = bounded_event(immunity_loss_rate[age] * value->r, dt, value->r);
    }
    if (model == MODEL_SIR) {
        value->s -= (double)infections;
        value->i += (double)infections - (double)recoveries;
        value->r += (double)recoveries;
    } else if (model == MODEL_SEIR) {
        value->s -= (double)infections;
        value->e += (double)infections - (double)progression;
        value->i += (double)progression - (double)recoveries;
        value->r += (double)recoveries;
    } else {
        value->s += (double)immunity_losses - (double)infections;
        value->e += (double)infections - (double)progression;
        value->i += (double)progression - (double)recoveries - (double)hospitalizations;
        value->h += (double)hospitalizations - (double)hospital_recoveries;
        value->r += (double)recoveries + (double)hospital_recoveries - (double)immunity_losses;
    }
    value->s = clamp_nonnegative(value->s);
    value->e = clamp_nonnegative(value->e);
    value->i = clamp_nonnegative(value->i);
    value->h = clamp_nonnegative(value->h);
    value->r = clamp_nonnegative(value->r);
}

static void apply_stochastic_step(const Scenario *models, SimulationState *state,
                                  ModelType model, double dt)
{
    size_t population;
    size_t age;
    for (population = 0; population < state->input_count; ++population) {
        State total = sum_population(state, population);
        for (age = 0; age < EPIDEMIC_AGE_GROUPS; ++age) {
            stochastic_transition(&state->state[population][age], &models[population], model,
                                  age, dt, total.i, models[population].N);
        }
    }
    if (state->input_count == 2U) {
        for (age = 0; age < EPIDEMIC_AGE_GROUPS; ++age) {
            State *first = &state->state[0][age];
            State *second = &state->state[1][age];
            double first_s = models[0].migration_rate * dt * first->s;
            double first_e = models[0].migration_rate * dt * first->e;
            double first_i = models[0].migration_rate * dt * first->i;
            double first_r = models[0].migration_rate * dt * first->r;
            double second_s = models[1].migration_rate * dt * second->s;
            double second_e = models[1].migration_rate * dt * second->e;
            double second_i = models[1].migration_rate * dt * second->i;
            double second_r = models[1].migration_rate * dt * second->r;
            first->s += second_s - first_s;
            first->e += second_e - first_e;
            first->i += second_i - first_i;
            first->r += second_r - first_r;
            second->s += first_s - second_s;
            second->e += first_e - second_e;
            second->i += first_i - second_i;
            second->r += first_r - second_r;
        }
    }
}

static void update_summary(const SimulationState *state, int day, SimulationSummary *summary)
{
    size_t population;
    for (population = 0; population < state->input_count; ++population) {
        State total = sum_population(state, population);
        if (total.i > summary->max_infected[population]) {
            summary->max_infected[population] = total.i;
            summary->max_infected_day[population] = day;
        }
        summary->protected_population[population] = total.protected_population;
        if (total.h > summary->max_hospitalized[population]) {
            summary->max_hospitalized[population] = total.h;
            summary->max_hospitalized_day[population] = day;
        }
    }
}

static int write_state(FILE *output, const SimulationState *state, int day, ModelType model)
{
    size_t population;
    if (output == NULL) {
        return 0;
    }
    (void)fprintf(output, "%d", day);
    for (population = 0; population < state->input_count; ++population) {
        State total = sum_population(state, population);
        (void)fprintf(output, " %.10f", total.s);
        if (model != MODEL_SIR) {
            (void)fprintf(output, " %.10f", total.e);
        }
        (void)fprintf(output, " %.10f", total.i);
        if (model == MODEL_SEIHRS) {
            (void)fprintf(output, " %.10f", total.h);
        }
        (void)fprintf(output, " %.10f", total.r);
    }
    (void)fputc('\n', output);
    return ferror(output) == 0;
}

static int run_single(const Scenario *models, size_t input_count, ModelType model,
                      const SimulationOptions *options, FILE *output,
                      SimulationSummary *summary, int replicate, char *error,
                      size_t error_size)
{
    SimulationState state;
    int day;
    int substep;
    (void)replicate;
    initialize_state(models, input_count, model, &state);
    (void)memset(summary, 0, sizeof(*summary));
    if (!write_state(output, &state, 0, model)) {
        set_error(error, error_size, "Could not write simulation output");
        return 0;
    }
    update_summary(&state, 0, summary);
    for (day = 1; day <= models[0].days; ++day) {
        if (options->stochastic) {
            for (substep = 0; substep < STEPS_PER_DAY; ++substep) {
                apply_stochastic_step(models, &state, model, 1.0 / STEPS_PER_DAY);
            }
        } else {
            {
                double remaining = 1.0;
                while (remaining > EPIDEMIC_TOLERANCE) {
                    double step = options->dt < remaining ? options->dt : remaining;
                    apply_deterministic_step(models, &state, model, step);
                    remaining -= step;
                }
            }
        }
        if (!write_state(output, &state, day, model)) {
            set_error(error, error_size, "Could not write simulation output");
            return 0;
        }
        update_summary(&state, day, summary);
    }
    return 1;
}

int expected_output_columns(ModelType model, size_t input_count)
{
    int compartments;
    if (input_count < 1U || input_count > 2U) {
        return 0;
    }
    compartments = model == MODEL_SIR ? 3 : (model == MODEL_SEIR ? 4 : 5);
    return 1 + compartments * (int)input_count;
}

int simulate(const Scenario *models, size_t input_count, ModelType model,
             const SimulationOptions *options, FILE *output,
             SimulationSummary *summary, char *error, size_t error_size)
{
    Scenario adjusted[2];
    size_t i;
    if (models == NULL || options == NULL || summary == NULL || output == NULL) {
        set_error(error, error_size, "Simulation requires models, options, summary, and output");
        return 0;
    }
    if (input_count < 1U || input_count > 2U || model < MODEL_SIR || model > MODEL_SEIHRS) {
        set_error(error, error_size, "Simulation model or input count is unsupported");
        return 0;
    }
    if (options->dt <= 0.0 || options->dt > 1.0 || !EPIDEMIC_ISFINITE(options->dt)) {
        set_error(error, error_size, "Simulation time step must be finite and greater than zero");
        return 0;
    }
    for (i = 0; i < input_count; ++i) {
        adjusted[i] = models[i];
        if (!validate_scenario(&adjusted[i], error, error_size)) {
            return 0;
        }
        if (input_count == 2U && i == 1U && adjusted[i].days != adjusted[0].days) {
            set_error(error, error_size, "Both scenarios must use the same DAYS value");
            return 0;
        }
        apply_interventions(&adjusted[i], options->use_app, options->use_vaccine);
    }
    if (options->seed_set) {
        srand(options->seed);
    }
    if (!run_single(adjusted, input_count, model, options, output, summary, 0, error, error_size)) {
        return 0;
    }
    if (fprintf(output, "# MAX_INFECTED_1 %.10f DAY %d\n", summary->max_infected[0], summary->max_infected_day[0]) < 0) {
        set_error(error, error_size, "Could not write simulation summary");
        return 0;
    }
    if (input_count == 2U) {
        (void)fprintf(output, "# MAX_INFECTED_2 %.10f DAY %d\n", summary->max_infected[1], summary->max_infected_day[1]);
    }
    (void)fprintf(output, "# MAX_HOSPITALIZED_1 %.10f DAY %d\n", summary->max_hospitalized[0], summary->max_hospitalized_day[0]);
    if (input_count == 2U) {
        (void)fprintf(output, "# MAX_HOSPITALIZED_2 %.10f DAY %d\n", summary->max_hospitalized[1], summary->max_hospitalized_day[1]);
    }
    return ferror(output) == 0;
}

int run_replicates(const Scenario *models, size_t input_count, ModelType model,
                   const SimulationOptions *options, int replicates,
                   FILE *output, char *error, size_t error_size)
{
    Scenario adjusted[2];
    SimulationOptions replicate_options;
    int replicate;
    size_t i;
    if (models == NULL || options == NULL || output == NULL || replicates <= 0) {
        set_error(error, error_size, "Replicates, models, options, and output are required");
        return 0;
    }
    if (input_count < 1U || input_count > 2U || model < MODEL_SIR || model > MODEL_SEIHRS) {
        set_error(error, error_size, "Simulation model or input count is unsupported");
        return 0;
    }
    for (i = 0; i < input_count; ++i) {
        adjusted[i] = models[i];
        if (!validate_scenario(&adjusted[i], error, error_size)) {
            return 0;
        }
        if (input_count == 2U && i == 1U && adjusted[i].days != adjusted[0].days) {
            set_error(error, error_size, "Both scenarios must use the same DAYS value");
            return 0;
        }
        apply_interventions(&adjusted[i], options->use_app, options->use_vaccine);
    }
    replicate_options = *options;
    for (replicate = 0; replicate < replicates; ++replicate) {
        SimulationSummary summary;
        (void)fprintf(output, "# REPLICATE %d\n", replicate + 1);
        if (options->seed_set) {
            replicate_options.seed_set = 1;
            replicate_options.seed = options->seed + (unsigned int)replicate;
            srand(replicate_options.seed);
        }
        if (!run_single(adjusted, input_count, model, &replicate_options, output,
                        &summary, replicate, error, error_size)) {
            return 0;
        }
        if (replicate + 1 < replicates && fputs("\n\n", output) == EOF) {
            set_error(error, error_size, "Could not separate replicate output blocks");
            return 0;
        }
    }
    return ferror(output) == 0;
}
