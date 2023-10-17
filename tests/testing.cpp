#include "./testing.hpp"
#include <cstddef>
#include <iostream>

namespace testing
{

TestFailure::TestFailure(char const* msg)
    : std::logic_error { msg }
{
}

TestIgnored::TestIgnored(char const* msg)
    : std::logic_error { msg }
{
}

auto run(std::initializer_list<Test> tests) -> int
{
    std::size_t passed = 0;
    for (auto const& test : tests) {
        try {
            std::get<1>(test)();
            ++passed;
        }
        catch (TestIgnored const& e) {
            std::cerr << std::get<0>(test) << " ignored: " << e.what() << '\n';
        }
        catch (std::exception const& e) {
            std::cerr << std::get<0>(test) << " failed: " << e.what() << '\n';
        }
    }

    std::cerr << (tests.size() - passed) << "/" << tests.size()
              << " tests failed\n";

    return passed == tests.size() ? 0 : 1;
}

auto run_with_context(int argc,
                      char const** argv,
                      std::initializer_list<TestWithContext> tests) -> int
{
    std::size_t passed = 0;
    for (auto const& test : tests) {
        try {
            std::get<1>(test)(TestContext { argc > 0 ? argc - 1 : 0,
                                            argc > 0 ? argv + 1 : argv });
            ++passed;
        }
        catch (TestIgnored const& e) {
            std::cerr << std::get<0>(test) << " ignored: " << e.what() << '\n';
        }
        catch (std::exception const& e) {
            std::cerr << std::get<0>(test) << " failed: " << e.what() << '\n';
        }
    }

    std::cerr << (tests.size() - passed) << "/" << tests.size()
              << " tests failed\n";

    return passed == tests.size() ? 0 : 1;
}

} // namespace testing
