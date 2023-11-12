#ifndef SHADOW_CAST_UTILS_PROCESS_HPP_INCLUDED
#define SHADOW_CAST_UTILS_PROCESS_HPP_INCLUDED

#include <span>
#include <string>
#include <unistd.h>

namespace sc
{

struct Process
{
    friend auto spawn_process(std::span<std::string const> /*args*/) -> Process;
    friend auto swap(Process&, Process&) noexcept -> void;

    Process() noexcept = default;
    Process(Process&&) noexcept;
    ~Process();

    auto operator=(Process&&) noexcept -> Process&;

    auto terminate() noexcept -> void;
    [[nodiscard]] auto wait() noexcept -> int;
    [[nodiscard]] auto terminate_and_wait() noexcept -> int;

private:
    Process(pid_t /*pid*/) noexcept;
    pid_t pid_ { -1 };
};

auto spawn_process(std::span<std::string const> /*args*/) -> Process;

} // namespace sc

#endif // SHADOW_CAST_UTILS_PROCESS_HPP_INCLUDED
