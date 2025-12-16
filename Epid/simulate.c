
#include "epid.h"

#include <math.h>
#include <stdlib.h>

// Befolkningen er opdelt i 4 aldersgrupper
#define ALDERS_GRUPPER 4

// Antal sub-steps pr. dag for den stokastiske simulering
#define STEPS_PER_DAY 10

// extern SEIHRS_model tekstfil[2];
// extern SEIHRS_model tekstfil_orig[2];

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Poisson RNG baseret på Knuth for små lambda og normalapproximation for store lambda
long poisson(double lambda)
{
    if (lambda <= 0)
        return 0;
    if (lambda > 30.0)
    {
        // Normalapproksimation
        double u1 = (double)rand() / RAND_MAX;
        double u2 = (double)rand() / RAND_MAX;
        if (u1 < 1e-12)
            u1 = 1e-12;
        double z = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
        long val = (long)(lambda + sqrt(lambda) * z + 0.5);
        return val < 0 ? 0 : val;
    }
    double L = exp(-lambda);
    double p = 1.0;
    long k = 0;
    do
    {
        k++;
        p *= (double)rand() / RAND_MAX;
    } while (p > L && k < 100000);
    return k - 1;
}

// Hovedsimulering for én eller begge filer
void simulerEpidemi(SEIHRS_model *tekstfil_orig, int model_type, int use_app, int use_vaccine, int valg_input, FILE *file, int replicate_num, int is_stochastic, int print_to_terminal)
{
    SEIHRS_model tekstfil[2];

    tekstfil[0] = tekstfil_orig[0];
    if (valg_input == 2)
        tekstfil[1] = tekstfil_orig[1];

    /* Vaccine-effekt og app-effekt anvendes kun én gang og
      efterfølgende simuleringer starter altid fra samme udgangspunkt. */
    if (use_vaccine)
    {
        // Reducérer modtagelige med 20%
        tekstfil[0].S *= 0.8;
        if (valg_input == 2)
        {
            tekstfil[1].S *= 0.8;
        }

        // Reducérer smitteevne (beta) med 5%
        tekstfil[0].beta *= 0.95;
        if (valg_input == 2)
        {
            tekstfil[1].beta *= 0.59;
        }
    }

    printf("beta(after)=%f S(after)=%f\n", tekstfil[0].beta, tekstfil[0].S);
    // App effekt bliver brugt én gang
    if (use_app)
    {
        tekstfil[0].beta *= 0.7; // Reducerer kontakter
        if (valg_input == 2)
        {
            tekstfil[1].beta *= 0.7;
        }
    }

    // Fordeler på aldersgrupper
    float S_input_1[ALDERS_GRUPPER], E_input_1[ALDERS_GRUPPER], I_input_1[ALDERS_GRUPPER], R_input_1[ALDERS_GRUPPER], H_input_1[ALDERS_GRUPPER];
    float S_input_2[ALDERS_GRUPPER], E_input_2[ALDERS_GRUPPER], I_input_2[ALDERS_GRUPPER], R_input_2[ALDERS_GRUPPER], H_input_2[ALDERS_GRUPPER];

    for (int i = 0; i < ALDERS_GRUPPER; i++)
    {
        S_input_1[i] = tekstfil[0].S / ALDERS_GRUPPER;
        E_input_1[i] = tekstfil[0].E / ALDERS_GRUPPER;
        I_input_1[i] = tekstfil[0].I / ALDERS_GRUPPER;
        R_input_1[i] = tekstfil[0].R / ALDERS_GRUPPER;
        H_input_1[i] = tekstfil[0].H / ALDERS_GRUPPER;

        // Input 2 kun hvis den er valgt (ellers sat til 0)
        if (valg_input == 2)
        {
            S_input_2[i] = tekstfil[1].S / ALDERS_GRUPPER;
            E_input_2[i] = tekstfil[1].E / ALDERS_GRUPPER;
            I_input_2[i] = tekstfil[1].I / ALDERS_GRUPPER;
            R_input_2[i] = tekstfil[1].R / ALDERS_GRUPPER;
            H_input_2[i] = tekstfil[1].H / ALDERS_GRUPPER;
        }
        else
        {
            S_input_2[i] = 0;
            E_input_2[i] = 0;
            I_input_2[i] = 0;
            R_input_2[i] = 0;
            H_input_2[i] = 0;
        }
    }

    // Aldersspecifikke parametre
    float age_h_1[ALDERS_GRUPPER] = {0.09, 0.09, 0.09, 0.50}; // Sandsynlighed for at ende på hospitalet
    float age_beta_1[ALDERS_GRUPPER] = {1, 1, 1, 1};          // Antal kontakter

    float age_h_2[ALDERS_GRUPPER] = {0.3, 0.3, 0.3, 0.7}; // Sandsynlighed for at ende på hospitalet
    float age_beta_2[ALDERS_GRUPPER] = {1, 1, 1, 1};      // Antal kontakter

    float age_modtagelig_igen[ALDERS_GRUPPER] = {1.0f / 365, 1.0f / 300, 1.0f / 250, 1.0f / 180}; // jo yngre, jo længere er man immun

    // Header til datafil
    if (file != NULL)
    {
        if (valg_input == 1)
            fprintf(file, "# Replicate %d - Input 1\n", replicate_num);
        else if (valg_input == 2)
            fprintf(file, "# Replicate %d - Both\n", replicate_num);
    }

    double Imax1 = 0.0;
    double Imax2 = 0.0;

    double Hmax1 = 0.0;
    double Hmax2 = 0.0;

    // Simulering (tids-loop)
    for (int n = 1; n < tekstfil[0].dage; n++)
    {
        // Transfer rate pr dag
        float t1 = tekstfil[0].t;

        // Stokastisk del
        if (is_stochastic)
        {
            double dt = 1.0 / (double)STEPS_PER_DAY;

            // Udfører STEPS_PER_DAY sub-steps per dag
            for (int step = 0; step < STEPS_PER_DAY; step++)
            {
                // Udregn infectious totals hver sub-step
                float total_I_input_1 = 0, total_I_input_2 = 0;

                for (int ii = 0; ii < ALDERS_GRUPPER; ii++)
                {
                    total_I_input_1 += I_input_1[ii];
                    if (valg_input == 2)
                    {
                        total_I_input_2 += I_input_2[ii];
                    }
                }

                for (int i = 0; i < ALDERS_GRUPPER; i++)
                {

                    float S_out_1 = 0, E_out_1 = 0, I_out_1 = 0, R_out_1 = 0;
                    float S_out_2 = 0, E_out_2 = 0, I_out_2 = 0, R_out_2 = 0;

                    if (valg_input == 2) // Migration mellem byer sker kun hvis der er to filer (ellers sat til 0)
                    {
                        // Migration (skaleret med dt) - hospitaliserede kan ikke flytte sig
                        S_out_1 = t1 * dt * S_input_1[i];
                        E_out_1 = t1 * dt * E_input_1[i];
                        I_out_1 = t1 * dt * I_input_1[i];
                        R_out_1 = t1 * dt * R_input_1[i];

                        S_out_2 = t1 * dt * S_input_2[i];
                        E_out_2 = t1 * dt * E_input_2[i];
                        I_out_2 = t1 * dt * I_input_2[i];
                        R_out_2 = t1 * dt * R_input_2[i];
                    }

                    //  Input 1 - rate skaleret med dt
                    double rate_inf_1 = tekstfil[0].beta * S_input_1[i] * total_I_input_1 / tekstfil[0].N;
                    long n_inf_1 = poisson(rate_inf_1 * dt);
                    long n_prog_1 = 0;
                    long n_rec_1 = poisson(tekstfil[0].gamma * I_input_1[i] * dt);
                    long n_hosp_1 = 0;
                    long n_h_rec_1 = 0;
                    long n_R_to_S_1 = 0;

                    if (model_type == 2)
                    {
                        n_prog_1 = poisson(tekstfil[0].sigma * E_input_1[i] * dt);
                    }
                    else if (model_type == 3)
                    {
                        n_prog_1 = poisson(tekstfil[0].sigma * E_input_1[i] * dt);
                        n_hosp_1 = poisson(tekstfil[0].h * I_input_1[i] * dt);
                        n_h_rec_1 = poisson((tekstfil[0].gamma / 2.0) * H_input_1[i] * dt);
                        n_R_to_S_1 = poisson(age_modtagelig_igen[i] * R_input_1[i] * dt);
                    }
                    if (n_inf_1 > (long)S_input_1[i])
                        n_inf_1 = (long)S_input_1[i];
                    if (n_prog_1 > (long)E_input_1[i])
                        n_prog_1 = (long)E_input_1[i];
                    if (n_rec_1 > (long)I_input_1[i])
                        n_rec_1 = (long)I_input_1[i];
                    if (n_hosp_1 > (long)I_input_1[i])
                        n_hosp_1 = (long)I_input_1[i];
                    if (n_h_rec_1 > (long)H_input_1[i])
                        n_h_rec_1 = (long)H_input_1[i];
                    if (n_R_to_S_1 > (long)R_input_1[i])
                        n_R_to_S_1 = (long)R_input_1[i];

                    if (model_type == 1)
                    {
                        S_input_1[i] -= n_inf_1;
                        I_input_1[i] += n_inf_1 - n_rec_1;
                        R_input_1[i] += n_rec_1;
                    }
                    else if (model_type == 2)
                    {
                        S_input_1[i] -= n_inf_1;
                        E_input_1[i] += n_inf_1 - n_prog_1;
                        I_input_1[i] += n_prog_1 - n_rec_1;
                        R_input_1[i] += n_rec_1;
                    }
                    else if (model_type == 3)
                    {
                        S_input_1[i] += n_R_to_S_1 - n_inf_1;
                        E_input_1[i] += n_inf_1 - n_prog_1;
                        I_input_1[i] += n_prog_1 - n_rec_1 - n_hosp_1;
                        H_input_1[i] += n_hosp_1 - n_h_rec_1;
                        R_input_1[i] += n_rec_1 + n_h_rec_1 - n_R_to_S_1;
                    }

                    // Input 2 - rates skaleret med dt (kun hvis valg_input == 2)
                    if (valg_input == 2)
                    {
                        double rate_inf_2 = tekstfil[1].beta * S_input_2[i] * total_I_input_2 / tekstfil[1].N;
                        long n_inf_2 = poisson(rate_inf_2 * dt);
                        long n_prog_2 = 0;
                        long n_rec_2 = poisson(tekstfil[1].gamma * I_input_2[i] * dt);
                        long n_hosp_2 = 0;
                        long n_h_rec_2 = 0;
                        long n_R_to_S_2 = 0;

                        if (model_type == 2)
                        {
                            n_prog_2 = poisson(tekstfil[1].sigma * E_input_2[i] * dt);
                        }
                        else if (model_type == 3)
                        {
                            n_prog_2 = poisson(tekstfil[1].sigma * E_input_2[i] * dt);
                            n_hosp_2 = poisson(tekstfil[1].h * I_input_2[i] * dt);
                            n_h_rec_2 = poisson((tekstfil[1].gamma / 2.0) * H_input_2[i] * dt);
                            n_R_to_S_2 = poisson(age_modtagelig_igen[i] * R_input_2[i] * dt); // tab af immunitet (R til S)
                        }
                        if (n_inf_2 > (long)S_input_2[i])
                            n_inf_2 = (long)S_input_2[i];
                        if (n_prog_2 > (long)E_input_2[i])
                            n_prog_2 = (long)E_input_2[i];
                        if (n_rec_2 > (long)I_input_2[i])
                            n_rec_2 = (long)I_input_2[i];
                        if (n_hosp_2 > (long)I_input_2[i])
                            n_hosp_2 = (long)I_input_2[i];
                        if (n_h_rec_2 > (long)H_input_2[i])
                            n_h_rec_2 = (long)H_input_2[i];
                        if (n_R_to_S_2 > (long)R_input_2[i])
                            n_R_to_S_2 = (long)R_input_2[i];

                        if (model_type == 1)
                        {
                            S_input_2[i] -= n_inf_2;
                            I_input_2[i] += n_inf_2 - n_rec_2;
                            R_input_2[i] += n_rec_2;
                        }
                        else if (model_type == 2)
                        {
                            S_input_2[i] -= n_inf_2;
                            E_input_2[i] += n_inf_2 - n_prog_2;
                            I_input_2[i] += n_prog_2 - n_rec_2;
                            R_input_2[i] += n_rec_2;
                        }
                        else if (model_type == 3)
                        {
                            S_input_2[i] += n_R_to_S_2 - n_inf_2;
                            E_input_2[i] += n_inf_2 - n_prog_2;
                            I_input_2[i] += n_prog_2 - n_rec_2 - n_hosp_2;
                            H_input_2[i] += n_hosp_2 - n_h_rec_2;
                            R_input_2[i] += n_rec_2 + n_h_rec_2 - n_R_to_S_2;
                        }
                    }

                    if (valg_input == 2)
                    {
                        // Migration mellem byerne (kun hvis valg_input == 2)
                        S_input_1[i] += -S_out_1 + S_out_2;
                        E_input_1[i] += -E_out_1 + E_out_2;
                        I_input_1[i] += -I_out_1 + I_out_2;
                        R_input_1[i] += -R_out_1 + R_out_2;

                        S_input_2[i] += -S_out_2 + S_out_1;
                        E_input_2[i] += -E_out_2 + E_out_1;
                        I_input_2[i] += -I_out_2 + I_out_1;
                        R_input_2[i] += -R_out_2 + R_out_1;
                    }

                    // Beskyt mod negative værdier
                    S_input_1[i] = fmaxf(S_input_1[i], 0.0f);
                    E_input_1[i] = fmaxf(E_input_1[i], 0.0f);
                    I_input_1[i] = fmaxf(I_input_1[i], 0.0f);
                    R_input_1[i] = fmaxf(R_input_1[i], 0.0f);
                    H_input_1[i] = fmaxf(H_input_1[i], 0.0f);

                    S_input_2[i] = fmaxf(S_input_2[i], 0.0f);
                    E_input_2[i] = fmaxf(E_input_2[i], 0.0f);
                    I_input_2[i] = fmaxf(I_input_2[i], 0.0f);
                    R_input_2[i] = fmaxf(R_input_2[i], 0.0f);
                    H_input_2[i] = fmaxf(H_input_2[i], 0.0f);
                }
            }
        }
        else
        {
            // Deterministisk del

            float total_input_1 = 0;
            float total_input_2 = 0;

            for (int i = 0; i < ALDERS_GRUPPER; i++)
            {
                total_input_1 += I_input_1[i];
                if (valg_input == 2)
                {
                    total_input_2 += I_input_2[i];
                }
            }

            float dS_input_1[ALDERS_GRUPPER], dE_input_1[ALDERS_GRUPPER], dI_input_1[ALDERS_GRUPPER], dR_input_1[ALDERS_GRUPPER], dH_input_1[ALDERS_GRUPPER];
            float dS_input_2[ALDERS_GRUPPER], dE_input_2[ALDERS_GRUPPER], dI_input_2[ALDERS_GRUPPER], dR_input_2[ALDERS_GRUPPER], dH_input_2[ALDERS_GRUPPER];

            float sum_S_input_1 = 0, sum_E_input_1 = 0, sum_I_input_1 = 0, sum_R_input_1 = 0, sum_H_input_1 = 0;
            float sum_S_input_2 = 0, sum_E_input_2 = 0, sum_I_input_2 = 0, sum_R_input_2 = 0, sum_H_input_2 = 0;

            for (int i = 0; i < ALDERS_GRUPPER; i++)
            {
                sum_S_input_1 += S_input_1[i];
                sum_E_input_1 += E_input_1[i];
                sum_I_input_1 += I_input_1[i];
                sum_R_input_1 += R_input_1[i];
                sum_H_input_1 += H_input_1[i];

                sum_S_input_2 += S_input_2[i];
                sum_E_input_2 += E_input_2[i];
                sum_I_input_2 += I_input_2[i];
                sum_R_input_2 += R_input_2[i];
                sum_H_input_2 += H_input_2[i];

                // Default =  (ingen migration if valg_input == 1)
                float S_ud_1 = 0, E_ud_1 = 0, I_ud_1 = 0, R_ud_1 = 0;
                float S_ud_2 = 0, E_ud_2 = 0, I_ud_2 = 0, R_ud_2 = 0;

                if (valg_input == 2)
                {
                    // Transfer rate per aldersgruppe (H rejser ikke)
                    float t1 = tekstfil[0].t;

                    S_ud_1 = t1 * S_input_1[i];
                    E_ud_1 = t1 * E_input_1[i];
                    I_ud_1 = t1 * I_input_1[i];
                    R_ud_1 = t1 * R_input_1[i];

                    S_ud_2 = t1 * S_input_2[i];
                    E_ud_2 = t1 * E_input_2[i];
                    I_ud_2 = t1 * I_input_2[i];
                    R_ud_2 = t1 * R_input_2[i];
                }

                // Aldersspecifikke parametre
                float beta_i_1 = tekstfil[0].beta * age_beta_1[i];
                float gamma_i_1 = tekstfil[0].gamma;
                float sigma_i_1 = tekstfil[0].sigma;
                float h_i_1 = age_h_1[i];

                if (valg_input == 2)
                {
                    float beta_i_2 = tekstfil[1].beta * age_beta_2[i];
                    float gamma_i_2 = tekstfil[1].gamma;
                    float sigma_i_2 = tekstfil[1].sigma;
                    float h_i_2 = age_h_2[i];
                }

                if (model_type == 1)
                {                                                                                                         // SIR
                    dS_input_1[i] = (-beta_i_1 * S_input_1[i] * total_input_1) / tekstfil[0].N;                           // - S_ud_1 + S_ud_2;
                    dI_input_1[i] = (beta_i_1 * S_input_1[i] * total_input_1) / tekstfil[0].N - gamma_i_1 * I_input_1[i]; // - I_ud_1 + I_ud_2;

                    float Itest = dI_input_1[i];
                    printf("%lf", Itest);

                    double Itest1 = tekstfil[0].N;
                    printf("%lf", Itest1);

                    printf("%lf", total_input_1);
                    printf("%lf", beta_i_1);

                    dR_input_1[i] = gamma_i_1 * I_input_1[i]; // - R_ud_1 + R_ud_2;
                    dE_input_1[i] = 0;
                    dH_input_1[i] = 0;
                }
                else if (model_type == 2)
                { // SEIR
                    dS_input_1[i] = -beta_i_1 * S_input_1[i] * total_input_1 / tekstfil[0].N - S_ud_1 + S_ud_2;
                    dE_input_1[i] = beta_i_1 * S_input_1[i] * total_input_1 / tekstfil[0].N - sigma_i_1 * E_input_1[i] - E_ud_1 + E_ud_2;
                    dI_input_1[i] = sigma_i_1 * E_input_1[i] - gamma_i_1 * I_input_1[i] - I_ud_1 + I_ud_2;
                    dR_input_1[i] = gamma_i_1 * I_input_1[i] - R_ud_1 + R_ud_2;
                    dH_input_1[i] = 0;
                }
                else if (model_type == 3)
                { // SEIHRS
                    dS_input_1[i] = -beta_i_1 * S_input_1[i] * total_input_1 / tekstfil[0].N + age_modtagelig_igen[i] * R_input_1[i] - S_ud_1 + S_ud_2;
                    dE_input_1[i] = beta_i_1 * S_input_1[i] * total_input_1 / tekstfil[0].N - sigma_i_1 * E_input_1[i] - E_ud_1 + E_ud_2;
                    dI_input_1[i] = sigma_i_1 * E_input_1[i] - gamma_i_1 * I_input_1[i] - h_i_1 * I_input_1[i] - I_ud_1 + I_ud_2;
                    dH_input_1[i] = h_i_1 * I_input_1[i] - (gamma_i_1 / 2) * H_input_1[i];
                    dR_input_1[i] = gamma_i_1 * I_input_1[i] + (gamma_i_1 / 2) * H_input_1[i] - age_modtagelig_igen[i] * R_input_1[i] - R_ud_1 + R_ud_2;
                }
                // By 2 – kun hvis valg_input == 2
                if (valg_input == 2)
                {

                    if (model_type == 1)
                    {
                        dS_input_2[i] = -tekstfil[1].beta * S_input_2[i] * total_input_2 / tekstfil[1].N - S_ud_2 + S_ud_1;
                        dI_input_2[i] = tekstfil[1].beta * S_input_2[i] * total_input_2 / tekstfil[1].N - tekstfil[1].gamma * I_input_2[i] - I_ud_2 + I_ud_1;
                        dR_input_2[i] = tekstfil[1].gamma * I_input_2[i] - R_ud_2 + R_ud_1;
                        dE_input_2[i] = 0;
                        dH_input_2[i] = 0;
                    }
                    else if (model_type == 2)
                    {
                        dS_input_2[i] = -tekstfil[1].beta * S_input_2[i] * total_input_2 / tekstfil[1].N - S_ud_2 + S_ud_1;
                        dE_input_2[i] = tekstfil[1].beta * S_input_2[i] * total_input_2 / tekstfil[1].N - tekstfil[1].gamma * E_input_2[i] - E_ud_2 + E_ud_1;
                        dI_input_2[i] = tekstfil[1].sigma * E_input_2[i] - tekstfil[1].gamma * I_input_2[i] - I_ud_2 + I_ud_1;
                        dR_input_2[i] = tekstfil[1].gamma * I_input_2[i] - R_ud_2 + R_ud_1;
                        dH_input_2[i] = 0;
                    }
                    else if (model_type == 3)
                    {
                        dS_input_2[i] = -tekstfil[1].beta * S_input_2[i] * total_input_2 / tekstfil[1].N + age_modtagelig_igen[i] * R_input_2[i] - S_ud_2 + S_ud_1;
                        dE_input_2[i] = tekstfil[1].beta * S_input_2[i] * total_input_2 / tekstfil[1].N - tekstfil[1].sigma * E_input_2[i] - E_ud_2 + E_ud_1;
                        dI_input_2[i] = tekstfil[1].sigma * E_input_2[i] - tekstfil[1].gamma * I_input_2[i] - tekstfil[1].h * I_input_2[i] - I_ud_2 + I_ud_1;
                        dH_input_2[i] = tekstfil[1].h * I_input_2[i] - (tekstfil[1].gamma / 2) * H_input_2[i];
                        dR_input_2[i] = tekstfil[1].gamma * I_input_2[i] + (tekstfil[1].gamma / 2) * H_input_2[i] - age_modtagelig_igen[i] * R_input_2[i] - R_ud_2 + R_ud_1;
                    }
                }
                else
                {
                    // Hvis kun én by, sæt ændringer for by 2 til 0
                    dS_input_2[i] = dE_input_2[i] = dI_input_2[i] = dR_input_2[i] = dH_input_2[i] = 0;
                }

                // Opdater værdier (deterministisk)
                S_input_1[i] += dS_input_1[i];
                E_input_1[i] += dE_input_1[i];
                I_input_1[i] += dI_input_1[i];
                R_input_1[i] += dR_input_1[i];
                H_input_1[i] += dH_input_1[i];

                S_input_2[i] += dS_input_2[i];
                E_input_2[i] += dE_input_2[i];
                I_input_2[i] += dI_input_2[i];
                R_input_2[i] += dR_input_2[i];
                H_input_2[i] += dH_input_2[i];

                // Negativ populationsbeskyttelse
                S_input_1[i] = fmaxf(S_input_1[i], 0);
                E_input_1[i] = fmaxf(E_input_1[i], 0);
                I_input_1[i] = fmaxf(I_input_1[i], 0);
                R_input_1[i] = fmaxf(R_input_1[i], 0);
                H_input_1[i] = fmaxf(H_input_1[i], 0);

                S_input_2[i] = fmaxf(S_input_2[i], 0);
                E_input_2[i] = fmaxf(E_input_2[i], 0);
                I_input_2[i] = fmaxf(I_input_2[i], 0);
                R_input_2[i] = fmaxf(R_input_2[i], 0);
                H_input_2[i] = fmaxf(H_input_2[i], 0);
            }
        }

        // Summér alle aldersgrupper til output
        float sum_S_input_1 = 0, sum_E_input_1 = 0, sum_I_input_1 = 0, sum_R_input_1 = 0, sum_H_input_1 = 0;
        float sum_S_input_2 = 0, sum_E_input_2 = 0, sum_I_input_2 = 0, sum_R_input_2 = 0, sum_H_input_2 = 0;

        for (int i = 0; i <= ALDERS_GRUPPER; i++)
        {
            sum_S_input_1 += S_input_1[i];
            sum_E_input_1 += E_input_1[i];
            sum_I_input_1 += I_input_1[i];
            sum_R_input_1 += R_input_1[i];
            sum_H_input_1 += H_input_1[i];

            sum_S_input_2 += S_input_2[i];
            sum_E_input_2 += E_input_2[i];
            sum_I_input_2 += I_input_2[i];
            sum_R_input_2 += R_input_2[i];
            sum_H_input_2 += H_input_2[i];
        }

        if (sum_I_input_1 > Imax1)
            Imax1 = sum_I_input_1;

        if (valg_input == 2 && sum_I_input_2 > Imax2)
            Imax2 = sum_I_input_2;

        if (sum_H_input_1 > Hmax1)
            Hmax1 = sum_H_input_1;

        if (valg_input == 2 && sum_H_input_2 > Hmax2)
            Hmax2 = sum_H_input_2;

        // Udskriv værdier (dag for dag)
        if (model_type == 1) // sir
        {
            if (print_to_terminal)
            {
                if (valg_input == 1)
                    printf("Day %d | Input 1:(S=%.0f,I=%.0f,R=%.0f, Imax1 = %.2f)\n", n, sum_S_input_1, sum_I_input_1, sum_R_input_1, Imax1);
                else if (valg_input == 2)
                    printf("Day %d | Input 1:(S=%.0f, I=%.0f, R=%.0f, Imax1 = %.2f) Input 2: (S=%.0f, I=%.0f, R=%.0f, Imax2 = %.2f)\n", n, sum_S_input_1, sum_I_input_1, sum_R_input_1, Imax1, sum_S_input_2, sum_I_input_2, sum_R_input_2, Imax2);
            }

            if (file != NULL)
            {
                fprintf(file, "%d %.6f %.6f %.6f %.6f %.6f %.6f\n",
                        n, sum_S_input_1, sum_I_input_1, sum_R_input_1,
                        sum_S_input_2, sum_I_input_2, sum_R_input_2);
            }
        }
        else if (model_type == 2) // SEIR
        {
            if (print_to_terminal)
            {
                if (valg_input == 1)
                    printf("Day %d | Input 1:(S=%.0f, E=%.0f, I=%.0f,R=%.0f, Imax1 = %.2f)\n", n, sum_S_input_1, sum_E_input_1, sum_I_input_1, sum_R_input_1, Imax1);
                else if (valg_input == 2)
                    printf("Day %d | Input 1:(S=%.0f, E=%.0f, I=%.0f, R=%.0f, Imax1 = %.2f) Input 2: (S=%.0f, E=%.0f, I=%.0f, R=%.0f, Imax2 = %.2f)\n", n, sum_S_input_1, sum_E_input_1, sum_I_input_1, sum_R_input_1, Imax1, sum_S_input_2, sum_E_input_2, sum_I_input_2, sum_R_input_2, Imax2);
            }

            if (file != NULL)
            {
                fprintf(file, "%d %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f\n",
                        n, sum_S_input_1, sum_E_input_1, sum_I_input_1, sum_R_input_1,
                        sum_S_input_2, sum_E_input_2, sum_I_input_2, sum_R_input_2);
            }
        }
        else if (model_type == 3) // SEIHRS
        {
            if (print_to_terminal)
            {
                if (valg_input == 1)
                    printf("Day %d | Input 1:(S=%.0f, E=%.0f, I=%.0f, H=%.0f, R=%.0f, Hmax1 = %.2f )\n", n, sum_S_input_1, sum_E_input_1, sum_I_input_1, sum_H_input_1, sum_R_input_1, Hmax1);
                else if (valg_input == 2)
                    printf("Day %d | Input 1:(S=%.0f, E=%.0f, I=%.0f, H=%.0f, R=%.0f, Hmax1 = %.2f) Input 2: (S=%.0f, E=%.0f, I=%.0f, H=%.0f, R=%.0f, Hmax2 = %.2f)\n", n, sum_S_input_1, sum_E_input_1, sum_I_input_1, sum_H_input_1, sum_R_input_1, Hmax1, sum_S_input_2, sum_E_input_2, sum_I_input_2, sum_H_input_2, sum_R_input_2, Hmax2);
            }

            if (file != NULL)
            {
                fprintf(file, "%d %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f\n",
                        n, sum_S_input_1, sum_E_input_1, sum_I_input_1, sum_H_input_1, sum_R_input_1,
                        sum_S_input_2, sum_E_input_2, sum_I_input_2, sum_H_input_2, sum_R_input_2);
            }
        }
    }
    fprintf(file, "Imax1: %.6f\n", Imax1);

    fprintf(file, "Hmax1: %.6f\n", Hmax1);

        if (file != NULL)
    {
        fprintf(file, "\n\n");
    }
}
