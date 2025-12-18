#include "epid.h"
#include "CuTest.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>

// Brugermenu
void brugerInput()
{
    SEIHRS_model tekstfil[2];
    SEIHRS_model tekstfil_orig[2];

    printf("\nDette er et program, der simulerer smittespredning!");

    // Vælg model
    char valg[8];
    printf("\nVælg model (SIR, SEIR, SEIHRS): ");

    scanf(" %7s", valg);

    int model_type = 0;
    if (strcmp(valg, "SIR") == 0 || strcmp(valg, "sir") == 0)
    {
        model_type = 1;
    }
    else if (strcmp(valg, "SEIR") == 0 || strcmp(valg, "seir") == 0)
    {
        model_type = 2;
    }
    else if (strcmp(valg, "SEIHRS") == 0 || strcmp(valg, "seihrs") == 0)
    {
        model_type = 3;
    }
    else
    {
        printf("Ugyldigt valg.\n");
        return;
    }

    // Vælg antal filer (1 = én fil (én by) 2 = to filer (sammenligning))
    int valg_input = 0;

    printf("Tast 1 eller 2 for antal filer (2 = sammenligning): ");
    scanf(" %d", &valg_input);

    if (valg_input != 1 && valg_input != 2)
    {
        printf("Ugyldigt valg.\n");
        return;
    }

    // Hent filnavn i anden funktion
    char *filnavn = spoergOmFilnavn();

    // Åbn filen
    FILE *input_fil_1 = fopen(filnavn, "r");

    if (input_fil_1 == NULL)
    {
        printf("Kunne ikke åbne filen: %s\n", filnavn);
        printf("Tjek at filen findes i samme mappe som programmet.\n");
        exit(EXIT_FAILURE);
    }

    tekstfil[0] = indlaasFil(input_fil_1);
    tekstfil_orig[0] = tekstfil[0];
    fclose(input_fil_1);

    if (valg_input == 2)
    {
        char *filnavn2 = spoergOmFilnavn();
        FILE *input_fil_2 = fopen(filnavn2, "r");
        if (input_fil_2 == NULL)
        {
            printf("Kunne ikke åbne filen: %s\n", filnavn2);
            fclose(input_fil_1);
            return;
        }

        tekstfil[1] = indlaasFil(input_fil_2);
        tekstfil_orig[1] = tekstfil[1];

        fclose(input_fil_2);
    }
    // Vælg simuleringstype
    char sim_type;
    printf("Vælg simulerings type:\n");
    printf(" N) Normal (deterministisk)\n");
    printf(" S) Stokastisk (multiple replicates)\n");
    printf("Tast valg: ");
    scanf(" %c", &sim_type);

    if (sim_type == 'N' || sim_type == 'n')
    {
        // Normal deterministisk simulering
        int use_app = 0, use_vaccine = 0;
        appOgVaccine(&use_app, &use_vaccine);

        FILE *output_fil = fopen("data_file.txt", "w");
        if (!output_fil)
        {
            printf("Kan ikke åbne fil\n");
            return;
        }

        simulerEpidemi(tekstfil_orig, model_type, use_app, use_vaccine, valg_input, output_fil, 0, 0, 1);

        fclose(output_fil);

        // Lav gnuplot script for en enkelt simulering
        lavEnkeltGnuplotScript("plot_single.gnu", "data_file.txt", model_type, valg_input);
        printf("Gnuplot script gemt i plot_single.gnu\n");
        printf("Kør: gnuplot plot_single.gnu\n");

        printf("Simuleringen er færdig. Data er gemt i data_file.txt\n");
    }
    else if (sim_type == 'S' || sim_type == 's')
    {
        // Stokastisk simulering med flere kopier
        int numReplicates = 0;
        printf("Indtast antal replicates: ");

        if (scanf("%d", &numReplicates) != 1 || numReplicates <= 0)
        {
            printf("Ugyldigt antal replicates.\n");
            return;
        }

        int use_app = 0, use_vaccine = 0;
        appOgVaccine(&use_app, &use_vaccine);

        koerFlereKopier(tekstfil_orig, model_type, use_app, use_vaccine, numReplicates, valg_input);

        printf("Simuleringen er færdig. Data er gemt i stochastic_replicates.txt\n");
    }
    else
    {
        printf("Ugyldigt valg.\n");
    }
}

char *spoergOmFilnavn(void)
{
    static char filnavn[250];

    // Bed brugeren om filnavn
    printf("Upload en tekstfil (se evt skabelon: skabelon.seihr.txt)\n");
    printf("Indtast navnet på din fil (f.eks. SEIHR.txt): ");

    scanf("%249s", filnavn); // Læser ét ord, undgår buffer overflow
    return filnavn;
}

// Funktion der læser filens indhold
SEIHRS_model indlaasFil(FILE *input_fil)
{

    SEIHRS_model seihr;

    fscanf(input_fil, " %*[^0-9]%d", &seihr.dage);

    fscanf(input_fil, " %*[^0-9]%f", &seihr.N);
    fscanf(input_fil, " %*[^0-9]%f", &seihr.S);
    fscanf(input_fil, " %*[^0-9]%f", &seihr.E);
    fscanf(input_fil, " %*[^0-9]%f", &seihr.I);
    fscanf(input_fil, " %*[^0-9]%f", &seihr.H);
    fscanf(input_fil, " %*[^0-9]%f", &seihr.R);

    fscanf(input_fil, " %*[^0-9]%f", &seihr.beta);
    fscanf(input_fil, " %*[^0-9]%f", &seihr.gamma);
    fscanf(input_fil, " %*[^0-9]%f", &seihr.sigma);
    fscanf(input_fil, " %*[^0-9]%f", &seihr.h);
    fscanf(input_fil, " %*[^0-9]%f", &seihr.t);

    printf("Din fil indeholder følgende værdier:\n");
    printf(" Tid i dage = %d\n\n N = %.3f\n S = %.3f\n E = %.3f\n I = %.3f\n H = %.3f\n R = %.3f\n\n Beta = %.3f\n Gamma = %.3f\n Sigma = %.3f\n h = %.3f\n t = %.3f\n", seihr.dage, seihr.N, seihr.S, seihr.E, seihr.I, seihr.H, seihr.R, seihr.beta, seihr.gamma, seihr.sigma, seihr.h, seihr.t);

    return seihr;
}

// Vaccine og app
void appOgVaccine(int *use_app, int *use_vaccine)
{
    char app, vaccine;
    printf("Er smittestop-app aktiv (j/n)? ");
    scanf(" %c", &app);
    printf("Er vaccine udrullet (j/n)? ");
    scanf(" %c", &vaccine);

    *use_app = (app == 'j' || app == 'J') ? 1 : 0;
    *use_vaccine = (vaccine == 'j' || vaccine == 'J') ? 1 : 0;
}

// Kører flere kopier
void koerFlereKopier(SEIHRS_model *org, int model_type, int use_app, int use_vaccine, int numReplicates, int valg_input)
{
    FILE *file = fopen("stochastic_replicates.txt", "w");
    if (!file)
    {
        printf("Kan ikke åbne fil\n");
        return;
    }

    printf("Kører %d replicates...\n", numReplicates);

    for (int rep = 0; rep < numReplicates; rep++)
    {
        // reset sker allerede i simulerEpidemi
        printf("Replicate %d/%d\n", rep + 1, numReplicates);
        simulerEpidemi(org, model_type, use_app, use_vaccine, valg_input, file, rep, 1, 1);
    }

    fclose(file);
    printf("Alle replicates færdige. Data gemt i stochastic_replicates.txt\n");

    // Lav gnuplot script
    lavGnuplotScript("plot_replicates.gnu", "stochastic_replicates.txt", numReplicates, model_type, valg_input);
    printf("Gnuplot script gemt i plot_replicates.gnu\n");
    printf("Kør: gnuplot plot_replicates.gnu\n");
}

void lavGnuplotScript(const char *scriptFile, const char *dataFile, int numReplicates, int model_type, int valg_input)
{
    FILE *fp = fopen(scriptFile, "w");
    if (fp == NULL)
    {
        fprintf(stderr, "Error opening file %s\n", scriptFile);
        return;
    }

    fprintf(fp, "set xlabel 'Dage'\n");
    fprintf(fp, "set ylabel 'Antal individer'\n");

    if (model_type == 1)
    {
        fprintf(fp, "set title 'Stochastic SIR Model - %d Replicates (Input 1)'\n", numReplicates);
    }
    else if (model_type == 2)
    {
        fprintf(fp, "set title 'Stochastic SEIR Model - %d Replicates (Input 1)'\n", numReplicates);
    }
    else if (model_type == 3)
    {
        fprintf(fp, "set title 'Stochastic SEIHRS Model - %d Replicates (Input 1)'\n", numReplicates);
    }

    fprintf(fp, "set grid\n");
    fprintf(fp, "set key outside top center\n");
    fprintf(fp, "set key maxcols 5\n");

    // Definer farver
    fprintf(fp, "set style line 1 lc rgb '#00FF00' lt 1 lw 1\n"); // Grøn for S
    fprintf(fp, "set style line 2 lc rgb '#FFA500' lt 1 lw 1\n"); // Orange for E
    fprintf(fp, "set style line 3 lc rgb '#FF0000' lt 1 lw 1\n"); // Rød for I
    fprintf(fp, "set style line 4 lc rgb '#800080' lt 1 lw 1\n"); // Lilla for H
    fprintf(fp, "set style line 5 lc rgb '#0000FF' lt 1 lw 1\n"); // Blå for R
    fprintf(fp, "set style line 6 lc rgb '#009900' dt 3 lw 1\n"); // Grøn for S til input 2
    fprintf(fp, "set style line 7 lc rgb '#996300' dt 3 lw 1\n"); // Orange for E til input 2
    fprintf(fp, "set style line 8 lc rgb '#990000' dt 3 lw 1\n"); // Rød for I til input 2
    fprintf(fp, "set style line 9 lc rgb '#4D004D' dt 3 lw 1\n"); // Lilla for H til input 2
    fprintf(fp, "set style line 10 lc rgb '#000099' dt 3 lw 1\n"); // Blå for R til input 2

    // Plot baseret på valgte model

    int baseA = 1; // Input 1 kolonner start forskydning baseA -> S = baseA+1 -> kolonne 2
    int baseK = 4; // Input 2 kolonner start forskydning baseK -> S = baseK+1 -> kolonne 7

    // Byg en eksplicit plot-liste per replicate for at undgå gnuplot 'for' kompatibilitetsproblemer
    int first = 1;
    fprintf(fp, "plot ");
    for (int rep = 0; rep < numReplicates; rep++)
    {
        if (model_type == 1)
        {
            // SIR
            if (valg_input == 1 || valg_input == 2)
            {
                int colS = baseA + 1;
                int colI = baseA + 2;
                int colR = baseA + 3;
                if (!first)
                    fprintf(fp, ", \\\n");
                if (rep == 0)
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 1 title 'S (Input 1)'", dataFile, rep, colS);
                else
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 1 notitle", dataFile, rep, colS);
                fprintf(fp, ", \\\n");
                if (rep == 0)
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 3 title 'I (Input 1)'", dataFile, rep, colI);
                else
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 3 notitle", dataFile, rep, colI);
                fprintf(fp, ", \\\n");
                if (rep == 0)
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 5 title 'R (Input 1)'", dataFile, rep, colR);
                else
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 5 notitle", dataFile, rep, colR);
                first = 0;
            }
            if (valg_input == 2)
            {
                int colS = baseK + 1;
                int colI = baseK + 2;
                int colR = baseK + 3;
                if (!first)
                    fprintf(fp, ", \\\n");
                if (rep == 0)
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 6 title 'S (Input 2)'", dataFile, rep, colS);
                else
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 6 notitle", dataFile, rep, colS);
                fprintf(fp, ", \\\n");
                if (rep == 0)
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 8 title 'I (Input 2)'", dataFile, rep, colI);
                else
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 8 notitle", dataFile, rep, colI);
                fprintf(fp, ", \\\n");
                if (rep == 0)
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 10 title 'R (Input 2)'", dataFile, rep, colR);
                else
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 10 notitle", dataFile, rep, colR);
                first = 0;
            }
        }
        else if (model_type == 2)
        {
            // SEIR
            if (valg_input == 1 || valg_input == 2)
            {
                int colS = baseA + 1;
                int colE = baseA + 2;
                int colI = baseA + 3;
                int colR = baseA + 4;
                if (!first)
                    fprintf(fp, ", \\\n");
                if (rep == 0)
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 1 title 'S (Input 1)'", dataFile, rep, colS);
                else
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 1 notitle", dataFile, rep, colS);
                fprintf(fp, ", \\\n");
                if (rep == 0)
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 2 title 'E (Input 1)'", dataFile, rep, colE);
                else
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 2 notitle", dataFile, rep, colE);
                fprintf(fp, ", \\\n");
                if (rep == 0)
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 3 title 'I (Input 1)'", dataFile, rep, colI);
                else
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 3 notitle", dataFile, rep, colI);
                fprintf(fp, ", \\\n");
                if (rep == 0)
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 5 title 'R (Input 1)'", dataFile, rep, colR);
                else
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 5 notitle", dataFile, rep, colR);
                first = 0;
            }
            if (valg_input == 2)
            {
                int baseK = 5;
                int colS = baseK + 1;
                int colE = baseK + 2;
                int colI = baseK + 3;
                int colR = baseK + 4;
                if (!first)
                    fprintf(fp, ", \\\n");
                if (rep == 0)
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 6 title 'S (Input 2)'", dataFile, rep, colS);
                else
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 6 notitle", dataFile, rep, colS);
                fprintf(fp, ", \\\n");
                if (rep == 0)
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 7 title 'E (Input 2)'", dataFile, rep, colE);
                else
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 7 notitle", dataFile, rep, colE);
                fprintf(fp, ", \\\n");
                if (rep == 0)
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 8 title 'I (Input 2)'", dataFile, rep, colI);
                else
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 8 notitle", dataFile, rep, colI);
                fprintf(fp, ", \\\n");
                if (rep == 0)
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 10 title 'R (Input 2)'", dataFile, rep, colR);
                else
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 10 notitle", dataFile, rep, colR);
                first = 0;
            }
        }
        else if (model_type == 3)
        {
            // SEIHRS
            if (valg_input == 1 || valg_input == 2)
            {
                int colS = baseA + 1;
                int colE = baseA + 2;
                int colI = baseA + 3;
                int colH = baseA + 4;
                int colR = baseA + 5;
                if (!first)
                    fprintf(fp, ", \\\n");
                if (rep == 0)
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 1 title 'S (Input 1)'", dataFile, rep, colS);
                else
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 1 notitle", dataFile, rep, colS);
                fprintf(fp, ", \\\n");
                if (rep == 0)
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 2 title 'E (Input 1)'", dataFile, rep, colE);
                else
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 2 notitle", dataFile, rep, colE);
                fprintf(fp, ", \\\n");
                if (rep == 0)
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 3 title 'I (Input 1)'", dataFile, rep, colI);
                else
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 3 notitle", dataFile, rep, colI);
                fprintf(fp, ", \\\n");
                if (rep == 0)
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 4 title 'H (Input 1)'", dataFile, rep, colH);
                else
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 4 notitle", dataFile, rep, colH);
                fprintf(fp, ", \\\n");
                if (rep == 0)
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 5 title 'R (Input 1)'", dataFile, rep, colR);
                else
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 5 notitle", dataFile, rep, colR);
                first = 0;
            }
            if (valg_input == 2)
            {
                int baseK = 6;
                int colS = baseK + 1;
                int colE = baseK + 2;
                int colI = baseK + 3;
                int colH = baseK + 4;
                int colR = baseK + 5;
                if (!first)
                    fprintf(fp, ", \\\n");
                if (rep == 0)
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 6 title 'S (Input 2)'", dataFile, rep, colS);
                else
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 6 notitle", dataFile, rep, colS);
                fprintf(fp, ", \\\n");
                if (rep == 0)
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 7 title 'E (Input 2)'", dataFile, rep, colE);
                else
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 7 notitle", dataFile, rep, colE);
                fprintf(fp, ", \\\n");
                if (rep == 0)
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 8 title 'I (Input 2)'", dataFile, rep, colI);
                else
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 8 notitle", dataFile, rep, colI);
                fprintf(fp, ", \\\n");
                if (rep == 0)
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 9 title 'H (Input 2)'", dataFile, rep, colH);
                else
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 9 notitle", dataFile, rep, colH);
                fprintf(fp, ", \\\n");
                if (rep == 0)
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 10 title 'R (Input 2)'", dataFile, rep, colR);
                else
                    fprintf(fp, "'%s' index %d using 1:%d with lines ls 10 notitle", dataFile, rep, colR);
                first = 0;
            }
        }
    }
    fprintf(fp, "\n");

    fprintf(fp, "pause -1 'Tast enter for at lukke'\n");

    fclose(fp);
}

// Opret Gnuplot-script for enkelt simulering (normal/deterministisk)
void lavEnkeltGnuplotScript(const char *scriptFile, const char *dataFile, int model_type, int valg_input)
{
    FILE *fp = fopen(scriptFile, "w");
    if (fp == NULL)
    {
        fprintf(stderr, "Error opening file %s\n", scriptFile);
        return;
    }

    fprintf(fp, "set xlabel 'Dage'\n");
    fprintf(fp, "set ylabel 'Antal individer'\n");

    if (model_type == 1)
    {
        fprintf(fp, "set title 'SIR Model - Deterministisk Simulering'\n");
    }
    else if (model_type == 2)
    {
        fprintf(fp, "set title 'SEIR Model - Deterministisk Simulering'\n");
    }
    else if (model_type == 3)
    {
        fprintf(fp, "set title 'SEIHRS Model - Deterministisk Simulering'\n");
    }

    fprintf(fp, "set grid\n");
    fprintf(fp, "set key outside top center\n");
    fprintf(fp, "set key maxcols 5\n");

    // Definer farver (solid for enkelt simulering)
    fprintf(fp, "set style line 1 lc rgb '#00FF00' lt 1 lw 2\n"); // Grøn for S
    fprintf(fp, "set style line 2 lc rgb '#FFA500' lt 1 lw 2\n"); // Orange for E
    fprintf(fp, "set style line 3 lc rgb '#FF0000' lt 1 lw 2\n"); // Rød for I
    fprintf(fp, "set style line 4 lc rgb '#800080' lt 1 lw 2\n"); // Lilla for H
    fprintf(fp, "set style line 5 lc rgb '#0000FF' lt 1 lw 2\n"); // Blå for R
    fprintf(fp, "set style line 6 lc rgb '#009900' dt 3 lw 1\n"); // Grøn for S til input 2
    fprintf(fp, "set style line 7 lc rgb '#996300' dt 3 lw 1\n"); // Orange for E til input 2
    fprintf(fp, "set style line 8 lc rgb '#990000' dt 3 lw 1\n"); // Rød for I til input 2
    fprintf(fp, "set style line 9 lc rgb '#4D004D' dt 3 lw 1\n"); // Lilla for H til input 2
    fprintf(fp, "set style line 10 lc rgb '#000099' dt 3 lw 1\n"); // Blå for R til input 2

    int baseA = 1; // Input 1 offset
    int baseK = 4;
    // Input 2 offset, afhængig af model_type

    fprintf(fp, "plot \\\n");

    // INPUT 1
    if (model_type == 1) // SIR
    {
        fprintf(fp, "    '%s' using 1:%d with lines ls 1 title 'S (Input 1)', \\\n", dataFile, baseA + 1);
        fprintf(fp, "    '%s' using 1:%d with lines ls 3 title 'I (Input 1)', \\\n", dataFile, baseA + 2);
        fprintf(fp, "    '%s' using 1:%d with lines ls 5 title 'R (Input 1)'", dataFile, baseA + 3);
    }
    else if (model_type == 2) // SEIR
    {
        fprintf(fp, "    '%s' using 1:%d with lines ls 1 title 'S (Input 1)', \\\n", dataFile, baseA + 1);
        fprintf(fp, "    '%s' using 1:%d with lines ls 2 title 'E (Input 1)', \\\n", dataFile, baseA + 2);
        fprintf(fp, "    '%s' using 1:%d with lines ls 3 title 'I (Input 1)', \\\n", dataFile, baseA + 3);
        fprintf(fp, "    '%s' using 1:%d with lines ls 5 title 'R (Input 1)'", dataFile, baseA + 4);
    }
    else if (model_type == 3) // SEIHRS
    {
        fprintf(fp, "    '%s' using 1:%d with lines ls 1 title 'S (Input 1)', \\\n", dataFile, baseA + 1);
        fprintf(fp, "    '%s' using 1:%d with lines ls 2 title 'E (Input 1)', \\\n", dataFile, baseA + 2);
        fprintf(fp, "    '%s' using 1:%d with lines ls 3 title 'I (Input 1)', \\\n", dataFile, baseA + 3);
        fprintf(fp, "    '%s' using 1:%d with lines ls 4 title 'H (Input 1)', \\\n", dataFile, baseA + 4);
        fprintf(fp, "    '%s' using 1:%d with lines ls 5 title 'R (Input 1)'", dataFile, baseA + 5);
    }

    // INPUT 2
    if (valg_input == 2)
    {
        fprintf(fp, ", \\\n");

        if (model_type == 1) // SIR
        {
            fprintf(fp, "    '%s' using 1:%d with lines ls 6 title 'S (Input 2)', \\\n", dataFile, baseK + 1);
            fprintf(fp, "    '%s' using 1:%d with lines ls 8 title 'I (Input 2)', \\\n", dataFile, baseK + 3);
            fprintf(fp, "    '%s' using 1:%d with lines ls 10 title 'R (Input 2)'", dataFile, baseK + 5);
        }
        else if (model_type == 2) // SEIR
        {
            int baseK = 5;
            fprintf(fp, "    '%s' using 1:%d with lines ls 6 title 'S (Input 2)', \\\n", dataFile, baseK + 1);
            fprintf(fp, "    '%s' using 1:%d with lines ls 7 title 'E (Input 2)', \\\n", dataFile, baseK + 2);
            fprintf(fp, "    '%s' using 1:%d with lines ls 8 title 'I (Input 2)', \\\n", dataFile, baseK + 3);
            fprintf(fp, "    '%s' using 1:%d with lines ls 10 title 'R (Input 2)'", dataFile, baseK + 4);
        }
        else if (model_type == 3) // SEIHRS
        {
            int baseK = 6;
            fprintf(fp, "    '%s' using 1:%d with lines ls 6 title 'S (Input 2)', \\\n", dataFile, baseK + 1);
            fprintf(fp, "    '%s' using 1:%d with lines ls 7 title 'E (Input 2)', \\\n", dataFile, baseK + 2);
            fprintf(fp, "    '%s' using 1:%d with lines ls 8 title 'I (Input 2)', \\\n", dataFile, baseK + 3);
            fprintf(fp, "    '%s' using 1:%d with lines ls 9 title 'H (Input 2)', \\\n", dataFile, baseK + 4);
            fprintf(fp, "    '%s' using 1:%d with lines ls 10 title 'R (Input 2)'", dataFile, baseK + 5);
        }
    }

    fprintf(fp, "\n");
    fprintf(fp, "pause -1 'Tast enter for at lukke'\n");

    fclose(fp);
}
