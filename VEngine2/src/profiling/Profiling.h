#pragma once
#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#include <tracy/Tracy.hpp>

#define PROFILING_FRAME_MARK FrameMark
#define PROFILING_ZONE_SCOPED ZoneScoped
#define PROFILING_ZONE_SCOPED_N ZoneScopedN

#ifdef PROFILING_GPU_ENABLE

// need to set define PROFILING_VULKAN or PROFILING_D3D12 when PROFILING_GPU_ENABLE is defined
#define PROFILING_VULKAN

// VULKAN

#ifdef PROFILING_VULKAN

#include "graphics/gal/vulkan/volk.h"
#include <tracy/TracyVulkan.hpp>

#define PROFILING_GPU_ZONE_SCOPED_N(ctx, cmdList, name) TracyVkZone(((TracyVkCtx)ctx), (VkCommandBuffer)cmdList->getNativeHandle(), name )
#define PROFILING_GPU_COLLECT(ctx, cmdList) TracyVkCollect(((TracyVkCtx)ctx), (VkCommandBuffer)cmdList->getNativeHandle())
#define PROFILING_GPU_NEW_FRAME

#endif // PROFILING_VULKAN


// D3D12

#ifdef PROFILING_D3D12

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <tracy/TracyD3D12.hpp>

#define PROFILING_GPU_ZONE_SCOPED_N(ctx, cmdList, name) TracyD3D12Zone(((TracyD3D12Ctx)ctx), (ID3D12GraphicsCommandList *)cmdList->getNativeHandle(), name )
#define PROFILING_GPU_COLLECT(ctx, cmdList) TracyD3D12Collect(((TracyD3D12Ctx)ctx))
#define PROFILING_GPU_NEW_FRAME(ctx) TracyD3D12NewFrame(((TracyD3D12Ctx)ctx))

#endif // PROFILING_D3D12

#else  // PROFILING_GPU_ENABLE

#define PROFILING_GPU_ZONE_SCOPED_N
#define PROFILING_GPU_COLLECT
#define PROFILING_GPU_NEW_FRAME

#endif // PROFILING_GPU_ENABLE