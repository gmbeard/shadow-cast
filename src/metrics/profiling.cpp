#include "metrics/profiling.hpp"
#include "utils/contracts.hpp"
#include <unordered_map>

namespace
{
auto profile_table() -> std::unordered_map<sc::metrics::ProfileSectionId,
                                           sc::metrics::ProfileSection>&
{
    static std::unordered_map<sc::metrics::ProfileSectionId,
                              sc::metrics::ProfileSection>
        table;
    return table;
}
} // namespace

namespace sc::metrics
{
auto get_profile_section_name(ProfileSectionId id) noexcept -> char const*
{
    switch (id) {
    case ProfileSectionId::cuda_copy_frame:
        return "cuda copy frame";
    case ProfileSectionId::wayland_fetch_drm_data:
        return "fetch DRM data";
    case ProfileSectionId::opengl_color_conversion:
        return "OpenGL color conversion";
    case ProfileSectionId::END_OF_PROFILE_SECTION_IDS:
        SC_EXPECT(false);
    }
    SC_EXPECT(false);
}

auto get_profile_table() noexcept
    -> std::unordered_map<ProfileSectionId, ProfileSection> const&
{
    return profile_table();
}

auto add_profile_sample(ProfileSectionId id, std::uint64_t time) -> void
{
    auto& table = profile_table();
    auto [pos, inserted] = table.try_emplace(id,
                                             ProfileSection {
                                                 .sample_count = 1,
                                                 .average_duration = time,
                                                 .highest_duration = time,
                                                 .lowest_duration = time,
                                             });
    if (!inserted) {
        auto& item = pos->second;
        item.sample_count += 1;
        item.average_duration -= item.average_duration / item.sample_count;
        item.average_duration += time / item.sample_count;
        item.highest_duration = std::max(time, item.highest_duration);
        item.lowest_duration = std::min(time, item.lowest_duration);
    }
}

} // namespace sc::metrics
