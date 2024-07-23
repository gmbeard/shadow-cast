#include "io/process.hpp"
#include "utils/contracts.hpp"
#include "utils/scope_guard.hpp"
#include <algorithm>
#include <cstring>
#include <errno.h>
#include <signal.h>
#include <stdexcept>
#include <sys/wait.h>
#include <utility>
#include <vector>

using namespace std::literals::string_literals;

namespace sc
{
Process::Process(pid_t pid) noexcept
    : pid_ { pid }
{
}

Process::Process(Process&& other) noexcept
    : pid_ { std::exchange(other.pid_, -1) }
{
}

Process::~Process() { static_cast<void>(terminate_and_wait()); }

auto Process::operator=(Process&& other) noexcept -> Process&
{
    using std::swap;

    auto tmp { std::move(other) };
    swap(*this, tmp);
    return *this;
}

auto swap(Process& lhs, Process& rhs) noexcept -> void
{
    using std::swap;
    swap(lhs.pid_, rhs.pid_);
}

auto Process::terminate() noexcept -> void
{
    if (pid_ <= 0)
        return;

    ::kill(pid_, SIGINT);
}

auto Process::wait() noexcept -> int
{
    SC_SCOPE_GUARD([&] { pid_ = -1; });
    if (pid_ <= 0)
        return 0;

    int wstatus;
    ::waitpid(pid_, &wstatus, WEXITED);
    if (!WIFEXITED(wstatus))
        return 1;

    return WEXITSTATUS(wstatus);
}

auto Process::terminate_and_wait() noexcept -> int
{
    terminate();
    return wait();
}

auto spawn_process(std::span<std::string const> args) -> Process
{
    /* TODO:
     */
    std::vector<char const*> child_args(args.size() + 1, nullptr);
    std::transform(args.begin(),
                   args.end(),
                   child_args.begin(),
                   [](auto const& str) { return str.c_str(); });

    auto pid = fork();
    if (pid < 0)
        throw std::runtime_error { "spawn_process failed: "s +
                                   std::strerror(errno) };

    if (pid == 0) {
        ::execvp(child_args.front(),
                 const_cast<char* const*>(child_args.data()));
    }

    return Process { pid };
}

} // namespace sc
