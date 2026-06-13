#include "pch.h"
#include "RtlTest.h"

int main()
{
    printf("=========================================\n");
    printf("  RtlLoadLibrary - Automated Test Suite\n");
    printf("=========================================\n\n");

    int nFailed = GetTestRunner().Run();

    if (nFailed > 0)
    {
        printf("\n=========================================\n");
        printf("  FAILED: %d test(s)\n", nFailed);
        printf("=========================================\n");
        return 1;
    }

    printf("\n=========================================\n");
    printf("  All tests passed.\n");
    printf("=========================================\n");
    return 0;
}
