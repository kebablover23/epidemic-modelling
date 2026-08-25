#include <stdio.h>
#include <math.h>
#include "CuTest.h"
#include "epid.h"

// testcase 1 af vaccine funktion (vaccine til sammenlignet med vaccine fra)
void vaccine_effekt_1(CuTest *tc)
{
    // Arrange - sætter testen op med det den skal bruge

    SEIHRS_model tekstfil[2];
    SEIHRS_model tekstfil_orig[2];

    // Indlæser KBH.txt én gang
    FILE *input = fopen("KBH.txt", "r");
    CuAssertPtrNotNull(tc, input);

    tekstfil[0] = indlaasFil(input);
    tekstfil_orig[0] = tekstfil[0];
    fclose(input);

    // Kopiér data til sammenligning (uden vaccine)
    tekstfil[1] = tekstfil[0];
    tekstfil_orig[1] = tekstfil[0];

    FILE *testfile1 = fopen("test_output1.txt", "w");
    FILE *testfile2 = fopen("test_output2.txt", "w");

    CuAssertPtrNotNull(tc, testfile1);
    CuAssertPtrNotNull(tc, testfile2);

    // Act - kalder den funktion der skal testet
    simulerEpidemi(
        &tekstfil[0],
        1, // SIR
        0, // ingen app
        1, // vaccine til
        1, // inputfil 1
        testfile1,
        1, // 1 simulering
        0, // deterministisk
        0  // ingen terminalprint
    );
    simulerEpidemi(
        &tekstfil[1],
        1, // SIR
        0, // ingen app
        0, // vaccine fra
        1, // inputfil 1
        testfile2,
        1, // 1 simulering
        0, // deterministisk
        0  // ingen terminalprint
    );

    fclose(testfile1);
    fclose(testfile2);

    // Assert - Nu åbner vi filen igen og tjekker resultatet
    testfile1 = fopen("test_output1.txt", "r");
    testfile2 = fopen("test_output2.txt", "r");

    CuAssertPtrNotNull(tc, testfile1);
    CuAssertPtrNotNull(tc, testfile2);

    double Imax_med = 0.0;
    double Imax_uden = 0.0;

    char line[256];
    Imax_med = -1;

    while (fgets(line, sizeof(line), testfile1))
    {
        if (sscanf(line, "Imax1: %lf", &Imax_med) == 1)
            break;
    }

    Imax_uden = -1;

    while (fgets(line, sizeof(line), testfile2))
    {
        if (sscanf(line, "Imax1: %lf", &Imax_uden) == 1)
            break;
    }

    fclose(testfile1);
    fclose(testfile2);

    // forventning
    CuAssertTrue(tc, Imax_med < Imax_uden); // forventer at Imax_uden er større end Imax_med
}
// testsuite af testcases af vaccineeffekt
CuSuite *vaccine_udrulning_suite(void)
{
    CuSuite *suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, vaccine_effekt_1);

    return suite;
}

// testcases af migrations effekt (lukket grænse = 0) (1 by uden smittede, får overførst smitte fra anden by)
void migrations_effekt_1(CuTest *tc)
{
    // Arrange - sætter testen op med det den skal bruge

    SEIHRS_model tekstfil[2];

    // Indlæser KBH.txt én gang
    FILE *input = fopen("KBH.txt", "r");
    CuAssertPtrNotNull(tc, input);
    tekstfil[0] = indlaasFil(input);
    fclose(input);

    // Indlæser AAU_ingen_smittede.txt én gang
    FILE *input2 = fopen("AAU_ingen_smittede.txt", "r");
    CuAssertPtrNotNull(tc, input2);
    tekstfil[1] = indlaasFil(input2);
    fclose(input2);

    FILE *testfile3 = fopen("test_output3.txt", "w");
    CuAssertPtrNotNull(tc, testfile3);

    // Act - kalder den funktion der skal testet
    simulerEpidemi(
        tekstfil,
        1, // SIR
        0, // ingen app
        0, // vaccine fra
        2, // 2 inputfiler
        testfile3,
        1, // 1 simulering
        0, // deterministisk
        0  // ingen terminalprint
    );

    fclose(testfile3);

    // Assert - Nu åbner vi filen igen og tjekker resultatet
    testfile3 = fopen("test_output3.txt", "r");
    CuAssertPtrNotNull(tc, testfile3);

    char line[256];
    int day;
    double S1, I1, R1, S2, I2, R2;

    int smitteOpstaar = 0;

    while (fgets(line, sizeof(line), testfile3))
    {
        // Matcher linjer med 7 tal (som SIR-output skriver)
        if (sscanf(line, "%d %lf %lf %lf %lf %lf %lf",
                   &day, &S1, &I1, &R1, &S2, &I2, &R2) == 7)
        {
            if (I2 > 0.0)
            {
                smitteOpstaar = 1;
                break;
            }
        }
    }
    fclose(testfile3);

    // forventning
    CuAssertTrue(tc, smitteOpstaar == 1); // forventer at der bliver overført inficerede folk til Aalborg og at I derfor vokser
}
// testsuite af testcases af migrationseffekt
CuSuite *migration_suite(void)
{
    CuSuite *suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, migrations_effekt_1);

    return suite;
}

// testcases af deterministisk SIR (simulerEpidemi)
void sir_en_dag(CuTest *tc)
{
    // Arrange - sætter testen op med det den skal bruge
    SEIHRS_model tekstfil[2];

    // Indlæser KBH.txt én gang
    FILE *input = fopen("sir_en_dag.txt", "r");
    CuAssertPtrNotNull(tc, input);
    tekstfil[0] = indlaasFil(input);
    fclose(input);

    FILE *testfile4 = fopen("test_output4.txt", "w");
    CuAssertPtrNotNull(tc, testfile4);

    // Act - kalder den funktion der skal testes
    simulerEpidemi(
        tekstfil,
        1, // SIR
        0, // ingen app
        0, // vaccine fra
        1, // 1 inputfil
        testfile4,
        1, // 1 simulering
        0, // deterministisk
        0  // ingen terminalprint
    );

    fclose(testfile4);

    // Assert - Nu åbner vi filen igen og tjekker resultatet
    testfile4 = fopen("test_output4.txt", "r");
    CuAssertPtrNotNull(tc, testfile4);

    char line[256];
    double Imax1;
    int korrekt_output = 0;

    while (fgets(line, sizeof(line), testfile4))
    {
        if (sscanf(line, "Imax1: %lf", &Imax1) == 1)
        {
            if (Imax1 > 18 && Imax1 < 20) // vi har beregnet at det korrekt output burde være 18.88
            {
                korrekt_output = 1;
                break;
            }
            break;
        }
    }
    fclose(testfile4);

    // forventning
    CuAssertTrue(tc, korrekt_output == 1); // forventer at der efter 1 dag, kommer korrekt antal mennesker over i I-gruppen
}

// testsuite af testcases af sir
CuSuite *sir_suite(void)
{
    CuSuite *suite = CuSuiteNew();
    SUITE_ADD_TEST(suite, sir_en_dag);

    return suite;
}