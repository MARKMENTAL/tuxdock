#include "operation_state.hpp"

#include <cassert>
#include <iostream>

int main() {
    OperationState operation;
    assert(!operation.busy());

    operation.begin("Stopping container", "Please wait...");
    assert(operation.busy());
    assert(operation.title() == "Stopping container");
    assert(operation.message() == "Please wait...");

    operation.complete("Container stopped.");
    assert(!operation.busy());
    assert(operation.result() == "Container stopped.");
    std::cout << "Operation state tests passed\n";
}
