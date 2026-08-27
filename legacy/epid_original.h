
#include <stdio.h>
#include "CuTest.h"

#ifndef _EPID__
#define _EPID__

typedef struct
{
    int dage;
    float N, S, E, I, H, R;
    float beta, gamma, sigma, h, t;
} SEIHRS_model;

// Funktionsprototyper
void brugerInput();
char *spoergOmFilnavn(void);
SEIHRS_model indlaasFil(FILE *f);
void appOgVaccine(int *use_app, int *use_vaccine);
void simulerEpidemi(SEIHRS_model *, int model_type, int use_app, int use_vaccine, int valg_input, FILE *output_fil, int replicate_num, int is_stochastic, int print_to_terminal);
void koerFlereKopier(SEIHRS_model *, int model_type, int use_app, int use_vaccine, int numReplicates, int valg_input);
long poisson(double lambda);
void lavGnuplotScript(const char *scriptFile, const char *dataFile, int numReplicates, int model_type, int valg_input);
void lavEnkeltGnuplotScript(const char *scriptFile, const char *dataFile, int model_type, int valg_input);

CuSuite *vaccine_udrulning_suite(void);
CuSuite *migration_suite(void);
CuSuite *sir_suite(void);

#endif
