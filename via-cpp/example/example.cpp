#include <via-cpp/Via.h>
#include <utility>

int main(int argc, char** argv)
{
    via::Vm vm;

    // Registers a C++ function as the Via procedure "test".
    vm.registerProcedure("test", "test-proc",
            [&](via_int x, via_int y) { return std::pair(x, y); });

    // Run a Via program that calls the "test" procedure.
    auto result = vm.run("(test 12 34)"); 

    // Decode the result from a Via pair back into a C++ pair.
    auto resultPair = via::decode<std::pair<via_int, via_int>>(vm, result);

    // Will return 46.
    return resultPair.first + resultPair.second;
}
