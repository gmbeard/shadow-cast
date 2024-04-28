#include "./nvfbc.hpp"
#include "nvidia.hpp"

namespace sc
{

auto nvfbc() -> NvFBC const&
{
    static NvFBCLib lib = load_nvfbc();
    static NvFBC nvfbc = lib.NvFBCCreateInstance();

    return nvfbc;
}

auto make_nvfbc_session() -> NvFBCSessionHandlePtr
{
    return create_nvfbc_session(nvfbc());
}

} // namespace sc
