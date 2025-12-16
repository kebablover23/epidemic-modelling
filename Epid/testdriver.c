#include "epid.h"
#include <stdio.h>
#include "CuTest.h"

void RunAllTests(void);

int main(void)
{
    RunAllTests();

    return 0;
}

void RunAllTests(void)
{
    CuString *output = CuStringNew();
    CuSuite *suite = CuSuiteNew();

    // Adding test suites:
    CuSuiteAddSuite(suite, (CuSuite *)vaccine_udrulning_suite());
    CuSuiteAddSuite(suite, (CuSuite *)migration_suite());
    CuSuiteAddSuite(suite, (CuSuite *)sir_suite());

    CuSuiteRun(suite);
    CuSuiteSummary(suite, output);
    CuSuiteDetails(suite, output);
    printf("%s\n", output->buffer);
}
