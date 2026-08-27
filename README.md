# Epidemic Modelling

A command-line epidemic simulation application written in C using the C11 language standard.

The application explores epidemic spread through SIR, SEIR, and SEIHRS compartment models. It supports deterministic and stochastic simulations, configurable scenarios, migration between two populations, simplified age-based parameters, vaccination, contact-tracing interventions, automated regression tests, and visualisation with Gnuplot.

> [!IMPORTANT]
> This is an educational simulation project. It must not be used for medical, public-health, clinical, or policy decisions.

## Project Background

This project was originally developed collaboratively as part of a first-semester Software group project at Aalborg University.

Collaborator names are omitted from this public portfolio version for privacy. Publication of the project was approved by the project group.

After the course project was completed, I continued developing and manually validating the portfolio version of the product, using AI-assisted development tools as part of the review and improvement process. My goal was to preserve the original academic work while improving correctness, structure, test coverage, portability, documentation, and visual presentation.

The repository therefore contains two versions:

- `legacy/` contains the original submitted group implementation.
- `src/` and `include/` contain the actively maintained portfolio version.

This separation shows both the original first-semester project and the later improvements made during my continued learning.

## My Contributions

During the original group project, I contributed to the collaborative project process and the written discussion of ethical considerations related to vaccination.

After the course project, I continued working on the portfolio version by:

- Reviewing the original code and identifying potential defects
- Testing deterministic and stochastic scenarios
- Verifying population conservation across the supported models
- Improving input validation and error handling
- Testing vaccination, contact tracing, and migration behaviour
- Improving the repository and project structure
- Verifying reproducible stochastic simulations with fixed seeds
- Improving Gnuplot visualisation and PNG output
- Expanding the regression test suite
- Improving setup instructions and documentation
- Preserving the original group implementation for transparency

This is a learning project. It demonstrates my ability to revisit earlier work, identify weaknesses, test assumptions, and improve an existing codebase.

## AI-Assisted Development

AI-assisted development tools were used during the portfolio improvement process for code review, defect discovery, refactoring suggestions, test-case ideas, compiler-warning investigation, documentation, and Gnuplot layout improvements.

AI-generated suggestions were not accepted blindly. Changes were manually reviewed through strict compilation, automated tests, terminal testing, scenario comparisons, output inspection, population-conservation checks, fixed-seed comparisons, and visual validation on Windows.

The portfolio version was manually verified with:

```text
Passed: 57
Failed: 0
Compiler warnings: 0
Compiler errors: 0
```

Using AI became part of the learning process because every suggestion still required technical understanding, verification, and correction when it did not match the intended model.

## Features

- SIR, SEIR, and SEIHRS compartment models
- Deterministic forward-Euler simulation
- Configurable deterministic time steps
- Stochastic simulation using Poisson-distributed events
- Reproducible stochastic runs with fixed seeds
- Multiple stochastic replicates
- One or two simulated populations
- Migration between populations
- Four simplified age groups
- Vaccination and contact-tracing interventions
- Human-readable and validated scenario files
- Descriptive error messages
- Automatic Gnuplot script generation and launch
- Optional PNG plot export
- Automated regression tests
- Preservation of the original university implementation

## Model Compartments

- **S, Susceptible:** People who can become infected
- **E, Exposed:** People who are infected but not yet infectious
- **I, Infected:** People who are infectious
- **H, Hospitalised:** Infected people who require hospitalisation
- **R, Recovered:** People who have recovered from the disease
- **Protected:** Permanently vaccine-protected people tracked internally for population conservation

### SIR

```text
S -> I -> R
```

SIR transfers newly infected people directly from susceptible to infected.

### SEIR

```text
S -> E -> I -> R
```

SEIR adds an exposed compartment representing the incubation period before a person becomes infectious.

### SEIHRS

```text
S -> E -> I -> H -> R -> S
```

SEIHRS adds hospitalisation and loss of disease-acquired immunity. Recovered individuals may eventually return to the susceptible compartment. Permanently vaccine-protected individuals are tracked separately and do not return to susceptible.

## Mathematical Model

### SIR

```text
dS/dt = -beta * S * I / N
dI/dt =  beta * S * I / N - gamma * I
dR/dt =  gamma * I
```

### SEIR

```text
dS/dt = -beta * S * I / N
dE/dt =  beta * S * I / N - sigma * E
dI/dt =  sigma * E - gamma * I
dR/dt =  gamma * I
```

### SEIHRS additions

SEIHRS introduces hospitalisation from `I` to `H`, recovery from `H` to `R`, and loss of disease-acquired immunity from `R` to `S`.

The deterministic equations are simulated with forward-Euler integration.

## Educational Assumptions

- Default deterministic time step: `1.0` day
- Optional time steps satisfy `0 < dt <= 1`
- Vaccine coverage: `20%` of the initial susceptible population
- Vaccine transmission factor: `0.95`
- Contact-tracing transmission factor: `0.80`
- Intervention factors are applied once
- Four equally sized age groups are used
- Age-specific hospitalisation factors are `0.03`, `0.03`, `0.03`, and `0.10`
- A migration rate of `0.001` represents `0.1%` per day
- Susceptible, exposed, infected, and recovered people may migrate
- Hospitalised and permanently protected people do not migrate
- Disease-acquired immunity may decline in SEIHRS
- Vaccine protection remains active for the full simulation

These are educational assumptions, not validated medical estimates.

## Project Structure

```text
epidemic-modelling/
|-- docs/
|   `-- images/              Historical plots from the original project
|-- include/
|   `-- epidemic.h           Public types, constants, and function declarations
|-- legacy/                  Original submitted group implementation
|-- scenarios/               Example simulation scenarios
|-- src/
|   |-- input.c              Scenario loading and validation
|   |-- main.c               CLI and interactive user interface
|   |-- plotting.c           Gnuplot script generation and launch logic
|   `-- simulation.c         Deterministic and stochastic simulation engine
|-- tests/
|   `-- test_runner.c        Active regression test suite
|-- third_party/
|   `-- cutest/              Historical CuTest files from the original group project
|-- .gitignore
|-- Makefile
`-- README.md
```

Generated data, scripts, executables, and plots are excluded through `.gitignore`.

## Requirements

### Required

- A C11-compatible compiler such as GCC
- A terminal or command-line environment

### Optional

- Make
- Gnuplot for interactive graphs and PNG output

The simulation can run without Gnuplot by using `--no-plot`.

## Building on Windows

From the repository root, compile the application:

```powershell
gcc -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Iinclude src/input.c src/simulation.c src/plotting.c src/main.c -o epidemic-model.exe
```

Compile the tests:

```powershell
gcc -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Iinclude src/input.c src/simulation.c src/plotting.c tests/test_runner.c -o test-runner.exe
```

Run the tests:

```powershell
./test-runner.exe
```

Expected result:

```text
Passed: 57
Failed: 0
```

### Git Bash

Use:

```bash
./epidemic-model.exe
```

Do not use the PowerShell-style `.\epidemic-model.exe` inside Git Bash.

## Building with Make

If Make is installed:

```bash
make
make test
make run
make help
make clean
```

The Makefile creates:

```text
build/epidemic-model
build/test-runner
```

Some Windows installations do not include Make by default. In that case, use the direct GCC commands above.

## Running the Application

Running without arguments starts interactive mode:

```powershell
./epidemic-model.exe
```

The application asks for a model, scenario file, optional second scenario, simulation type, interventions, and replicate count when relevant.

## Command-Line Options

```text
--model SIR|SEIR|SEIHRS
--input PATH
--input2 PATH
--deterministic
--stochastic
--replicates NUMBER
--app
--vaccine
--seed NUMBER
--dt NUMBER
--no-plot
--save-plot
--help
```

When using two populations, both scenarios must contain the same `DAYS` value.

## Usage Examples

### One-day SIR reference

```powershell
./epidemic-model.exe --model SIR --input scenarios/sir_one_day.txt --deterministic --no-plot
```

Expected infected population after one day:

```text
I = 18.88
```

### Deterministic SIR

```powershell
./epidemic-model.exe --model SIR --input scenarios/copenhagen_scenario.txt --deterministic --dt 0.1
```

### Deterministic SEIR

```powershell
./epidemic-model.exe --model SEIR --input scenarios/copenhagen_influenza.txt --deterministic --dt 0.1
```

### Deterministic SEIHRS

```powershell
./epidemic-model.exe --model SEIHRS --input scenarios/copenhagen_sars.txt --deterministic --dt 0.1
```

### Compare two populations

```powershell
./epidemic-model.exe --model SEIHRS --input scenarios/copenhagen_scenario.txt --input2 scenarios/aau_initial_infections.txt --deterministic --dt 0.1
```

### Enable vaccination and contact tracing

```powershell
./epidemic-model.exe --model SEIHRS --input scenarios/copenhagen_sars.txt --deterministic --dt 0.1 --vaccine --app
```

### Reproducible stochastic simulation

```powershell
./epidemic-model.exe --model SEIHRS --input scenarios/copenhagen_sars.txt --stochastic --replicates 10 --seed 42
```

The same seed produces identical stochastic output. A different seed produces a different development.

### Run without Gnuplot

```powershell
./epidemic-model.exe --model SIR --input scenarios/copenhagen_scenario.txt --deterministic --dt 0.1 --no-plot
```

### Save a PNG

```powershell
./epidemic-model.exe --model SEIHRS --input scenarios/copenhagen_sars.txt --deterministic --dt 0.1 --save-plot
```

The image is saved to:

```text
output/plots/epidemic.png
```

## Scenario Format

Supported fields:

```text
DAYS
N
S
E
I
H
R
BETA
GAMMA
SIGMA
HOSPITALIZATION_RATE
MIGRATION_RATE
```

Example:

```text
# Simulation duration
DAYS = 365

# Initial population
N = 667100
S = 667099
E = 0
I = 1
H = 0
R = 0

# Model rates
BETA = 0.50
GAMMA = 0.142
SIGMA = 0.333
HOSPITALIZATION_RATE = 0.063
MIGRATION_RATE = 0.001
```

The parser supports blank lines, `#` comments, legacy `//` comments, scientific notation, and legacy keys.

For legacy compatibility:

- Uppercase `H` is the initial hospitalised population
- Lowercase `h` is the legacy hospitalisation rate
- Lowercase `t` is the legacy migration rate
- `TID I DAGE` is the legacy duration field

The parser rejects unknown, duplicate, missing, negative, non-finite, malformed, or inconsistent values.

Compartment values must satisfy:

```text
S + E + I + H + R = N
```

## Gnuplot Guide

### What is Gnuplot?

Gnuplot is a command-line plotting program used to visualise numerical data.

The project writes simulation results to:

```text
output/data_file.txt
```

It generates a plotting script at:

```text
output/plot.gnu
```

When Gnuplot is installed and available through `PATH`, the application opens the generated graph automatically.

### Why use Gnuplot?

Gnuplot keeps simulation and visualisation separate. This keeps numerical output available as plain text, avoids a large GUI dependency, allows plots to be regenerated, and supports Windows, Linux, and macOS.

### Install and verify Gnuplot

Download Gnuplot from its official website or install it through the operating system's package manager. Ensure the executable directory is included in `PATH`.

Check the installation:

```powershell
gnuplot --version
```

If the command is unavailable, use `--no-plot` or add Gnuplot to `PATH`.

### Plot layout

- SIR uses one large plot with `S`, `I`, and `R`
- SEIR uses a complete panel and a zoomed panel for `E` and `I`
- SEIHRS uses a complete panel and a zoomed panel for `E`, `I`, and `H`
- Population 1 uses solid lines
- Population 2 uses dashed lines
- Stochastic replicates use thinner lines
- Only the first replicate receives legend entries

Colors remain consistent:

- Green: susceptible
- Orange: exposed
- Red: infected
- Purple: hospitalised
- Blue: recovered

## Testing

The dependency-free regression runner was manually verified on Windows with:

```text
Passed: 57
Failed: 0
```

Coverage includes:

- English and legacy scenario parsing
- Distinction between `H` and legacy `h`
- Duplicate-field rejection
- One-day SIR reference calculation
- SIR, SEIR, and SEIHRS population conservation
- Day-zero and final-day output
- Deterministic substeps
- Vaccination and protected-population accounting
- App and vaccine transmission factors
- Reproducible stochastic seeds
- Migration and combined population conservation
- Scenario-duration validation
- Gnuplot labels, columns, layouts, replicate separation, and PNG configuration

## Verified Product Behaviour

Manual Windows testing confirmed:

- Strict compilation with zero warnings and zero errors
- Correct one-day SIR result
- Successful deterministic SIR, SEIR, and SEIHRS simulations
- Successful stochastic SEIHRS simulation with multiple replicates
- Reproducible fixed-seed stochastic output
- Clear rejection of invalid files, models, time steps, and templates
- Population conservation during disease transitions
- Combined population conservation during migration
- Lower infection and hospitalisation peaks when interventions are enabled
- Automatic Gnuplot launching
- Valid PNG generation
- Readable one-population, two-population, and stochastic plots

## Example Investigation Results

Manual educational scenarios showed that vaccination and contact tracing reduced the maximum infected population, their combination produced the largest reduction, interventions reduced maximum hospitalisation, and migration transferred infection into a population that initially contained no infected individuals.

These observations describe this educational model under configured assumptions. They are not predictions of real epidemics.

## Known Limitations

- Educational model, not a clinical forecasting system
- Four equally sized age groups
- Simplified age interactions and fixed parameters
- No dynamic human-behaviour changes
- No virus mutation
- No separate birth or death compartments
- Simplified vaccination and fixed-rate migration
- Standard C `rand()` generator, not scientific-grade randomness
- Forward Euler may be inaccurate with unsuitable rates or time steps
- Scenario values are educational examples, not verified forecasts
- No calibration against clinical datasets

## Historical Test Framework

The original group project used files named `CuTest.c` and `CuTest.h` together with group-authored test drivers, test suites, and test cases.

The available project material does not establish the original source or licence status of the CuTest files. They are preserved under `third_party/cutest/` for historical reference and are not used by the active portfolio test runner.

The active portfolio version uses `tests/test_runner.c`.

## Privacy and Publication

Personal and confidential information has been removed. University reports, process analyses, private communication, and personal group information are not included. Collaborator names are omitted for privacy.

## Licence

No licence has currently been granted for reuse.

The source code is publicly viewable for portfolio and educational review purposes.