#include "selftest.h"

#include <cstdio>

int main(int argc, char** argv) {
    const int result = efs::runSelfTest(argc, argv);
    if (result == 0) {
        std::printf("\nTodos os testes passaram.\n");
    } else {
        std::printf("\nAlguns testes falharam.\n");
    }
    return result;
}
