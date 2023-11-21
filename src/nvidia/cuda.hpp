#ifndef SHADOW_CAST_NVIDIA_CUDA_HPP_INCLUDED
#define SHADOW_CAST_NVIDIA_CUDA_HPP_INCLUDED

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <type_traits>

#define CUDA_VERSION 11070
#define CUDA_SUCCESS 0
#define CU_CTX_SCHED_AUTO 0

#if defined(_WIN64) || defined(__LP64__)
using CUdeviceptr_v2 = unsigned long long;
#else
using CUdeviceptr_v2 = unsigned int;
#endif
using CUdeviceptr = CUdeviceptr_v2;
using CUresult = int;
using CUdevice_v1 = int;
using CUdevice = CUdevice_v1;
using CUcontext = struct CUctx_st*;
using CUstream = struct CUstream_st*;
using CUarray = struct CUarray_st*;

enum CUgraphicsMapResourceFlags
{
    CU_GRAPHICS_MAP_RESOURCE_FLAGS_NONE = 0x00,
    CU_GRAPHICS_MAP_RESOURCE_FLAGS_READ_ONLY = 0x01,
    CU_GRAPHICS_MAP_RESOURCE_FLAGS_WRITE_DISCARD = 0x02
};

enum CUgraphicsRegisterFlags
{
    CU_GRAPHICS_REGISTER_FLAGS_NONE = 0x00,
    CU_GRAPHICS_REGISTER_FLAGS_READ_ONLY = 0x01,
    CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD = 0x02,
    CU_GRAPHICS_REGISTER_FLAGS_SURFACE_LDST = 0x04,
    CU_GRAPHICS_REGISTER_FLAGS_TEXTURE_GATHER = 0x08
};

enum CUmemorytype
{
    CU_MEMORYTYPE_HOST = 0x01,   /**< Host memory */
    CU_MEMORYTYPE_DEVICE = 0x02, /**< Device memory */
    CU_MEMORYTYPE_ARRAY = 0x03,  /**< Array memory */
    CU_MEMORYTYPE_UNIFIED = 0x04 /**< Unified device or host memory */
};

struct CUDA_MEMCPY2D_st
{
    size_t srcXInBytes; /**< Source X in bytes */
    size_t srcY;        /**< Source Y */

    CUmemorytype srcMemoryType; /**< Source memory type (host, device, array) */
    const void* srcHost;        /**< Source host pointer */
    CUdeviceptr srcDevice;      /**< Source device pointer */
    CUarray srcArray;           /**< Source array reference */
    size_t srcPitch;            /**< Source pitch (ignored when src is array) */

    size_t dstXInBytes; /**< Destination X in bytes */
    size_t dstY;        /**< Destination Y */

    CUmemorytype
        dstMemoryType;     /**< Destination memory type (host, device, array) */
    void* dstHost;         /**< Destination host pointer */
    CUdeviceptr dstDevice; /**< Destination device pointer */
    CUarray dstArray;      /**< Destination array reference */
    size_t dstPitch;       /**< Destination pitch (ignored when dst is array) */

    size_t WidthInBytes; /**< Width of 2D memory copy in bytes */
    size_t Height;       /**< Height of 2D memory copy */
};

/**
 * Array formats
 */
typedef enum CUarray_format_enum
{
    CU_AD_FORMAT_UNSIGNED_INT8 = 0x01,  /**< Unsigned 8-bit integers */
    CU_AD_FORMAT_UNSIGNED_INT16 = 0x02, /**< Unsigned 16-bit integers */
    CU_AD_FORMAT_UNSIGNED_INT32 = 0x03, /**< Unsigned 32-bit integers */
    CU_AD_FORMAT_SIGNED_INT8 = 0x08,    /**< Signed 8-bit integers */
    CU_AD_FORMAT_SIGNED_INT16 = 0x09,   /**< Signed 16-bit integers */
    CU_AD_FORMAT_SIGNED_INT32 = 0x0a,   /**< Signed 32-bit integers */
    CU_AD_FORMAT_HALF = 0x10,           /**< 16-bit floating point */
    CU_AD_FORMAT_FLOAT = 0x20           /**< 32-bit floating point */
} CUarray_format;

typedef struct CUDA_ARRAY_DESCRIPTOR_st
{
    size_t Width;  /**< Width of array */
    size_t Height; /**< Height of array */

    CUarray_format Format;    /**< Array format */
    unsigned int NumChannels; /**< Channels per array element */
} CUDA_ARRAY_DESCRIPTOR;

using CUDA_MEMCPY2D = CUDA_MEMCPY2D_st;
using CUgraphicsResource = struct CUgraphicsResource_st*;

namespace sc
{

struct NvCuda
{
    CUresult (*cuInit)(unsigned int Flags);
    CUresult (*cuDeviceGetCount)(int* count);
    CUresult (*cuDeviceGet)(CUdevice* device, int ordinal);
    CUresult (*cuCtxCreate_v2)(CUcontext* pctx,
                               unsigned int flags,
                               CUdevice dev);
    CUresult (*cuCtxDestroy_v2)(CUcontext ctx);
    CUresult (*cuCtxPushCurrent_v2)(CUcontext ctx);
    CUresult (*cuCtxPopCurrent_v2)(CUcontext* pctx);
    CUresult (*cuGetErrorString)(CUresult error, const char** pStr);
    CUresult (*cuMemsetD8_v2)(CUdeviceptr dstDevice,
                              unsigned char uc,
                              std::size_t N);
    CUresult (*cuMemcpy2D_v2)(const CUDA_MEMCPY2D* pCopy);
    CUresult (*cuGraphicsEGLRegisterImage)(CUgraphicsResource* pCudaResource,
                                           void* image,
                                           unsigned int flags);
    CUresult (*cuGraphicsResourceSetMapFlags)(CUgraphicsResource resource,
                                              unsigned int flags);
    CUresult (*cuGraphicsMapResources)(unsigned int count,
                                       CUgraphicsResource* resources,
                                       CUstream hStream);
    CUresult (*cuGraphicsUnmapResources)(unsigned int count,
                                         CUgraphicsResource* resources,
                                         CUstream hStream);
    CUresult (*cuGraphicsUnregisterResource)(CUgraphicsResource resource);
    CUresult (*cuGraphicsSubResourceGetMappedArray)(CUarray* pArray,
                                                    CUgraphicsResource resource,
                                                    unsigned int arrayIndex,
                                                    unsigned int mipLevel);
    CUresult (*cuArrayGetDescriptor_v2)(CUDA_ARRAY_DESCRIPTOR* pArrayDescriptor,
                                        CUarray hArray);
};

struct NvCudaError : std::runtime_error
{
    explicit NvCudaError(NvCuda, CUresult);
};

struct CUcontextDeleter
{
    CUcontextDeleter(NvCuda) noexcept;
    auto operator()(CUcontext) noexcept -> void;

private:
    NvCuda cuda_;
};

using CUcontextPtr =
    std::unique_ptr<std::remove_pointer_t<CUcontext>, CUcontextDeleter>;

[[nodiscard]] auto load_cuda() -> NvCuda;

[[nodiscard]] auto create_cuda_context(NvCuda) -> CUcontextPtr;

} // namespace sc

#endif // SHADOW_CAST_NVIDIA_CUDA_HPP_INCLUDED
