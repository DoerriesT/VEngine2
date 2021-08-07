#pragma once
#include <stdint.h>

enum TextureHandle : uint32_t { NULL_TEXTURE_HANDLE = 0 };
enum SubMeshHandle : uint32_t { NULL_SUB_MESH_HANDLE = 0 };
enum MaterialHandle : uint32_t { NULL_MATERIAL_HANDLE = 0 };
enum PhysicsMaterialHandle : uint32_t { NULL_PHYSICS_MATERIAL_HANDLE = 0 };
enum PhysicsConvexMeshHandle : uint32_t { NULL_PHYSICS_CONVEX_MESH_HANDLE = 0 };
enum PhysicsTriangleMeshHandle : uint32_t { NULL_PHYSICS_TRIANGLE_MESH_HANDLE = 0 };
enum AnimationSkeletonHandle : uint32_t { NULL_ANIMATION_SKELETON_HANDLE = 0 };
enum AnimationClipHandle : uint32_t { NULL_ANIMATION_CLIP_HANDLE = 0 };