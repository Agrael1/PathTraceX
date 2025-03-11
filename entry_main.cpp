#include "App.h"

int main(int argc, char* argv[])
{
    int entry_main(std::span<std::string_view> args);
    // Better main function
    auto args = reinterpret_cast<std::string_view*>(alloca(argc * sizeof(std::string_view)));
    for (int i = 0; i < argc; ++i) {
        args[i] = argv[i];
    }

    return entry_main(std::span(args, argc));
}

int entry_main(std::span<std::string_view> args)
try {
    return w::App{}.run();
} catch (const std::exception& e) {
    // Handle exceptions
    return 1;
} catch (...) {
    // Handle unknown exceptions
    return 2;
}