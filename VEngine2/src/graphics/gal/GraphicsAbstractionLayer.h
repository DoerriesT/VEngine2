#pragma once
#include <cstdint>
#include "FwdDecl.h"
#include "utility/Enum.h"

namespace gal
{
	// ENUMS

	enum class GraphicsBackendType
	{
		VULKAN,
		D3D12
	};

	enum class PresentMode
	{
		IMMEDIATE,
		V_SYNC,
	};

	enum class QueueType
	{
		GRAPHICS,
		COMPUTE,
		TRANSFER
	};

	enum class ObjectType
	{
		QUEUE,
		SEMAPHORE,
		COMMAND_LIST,
		BUFFER,
		IMAGE,
		QUERY_POOL,
		BUFFER_VIEW,
		IMAGE_VIEW,
		GRAPHICS_PIPELINE,
		COMPUTE_PIPELINE,
		DESCRIPTOR_SET_LAYOUT,
		SAMPLER,
		DESCRIPTOR_SET_POOL,
		DESCRIPTOR_SET,
		COMMAND_LIST_POOL,
		SWAPCHAIN,
	};

	enum class ComponentSwizzle
	{
		IDENTITY = 0,
		ZERO = 1,
		ONE = 2,
		R = 3,
		G = 4,
		B = 5,
		A = 6,
	};

	enum class Filter
	{
		NEAREST = 0,
		LINEAR = 1,
	};

	enum class SamplerMipmapMode
	{
		NEAREST = 0,
		LINEAR = 1,
	};

	enum class SamplerAddressMode
	{
		REPEAT = 0,
		MIRRORED_REPEAT = 1,
		CLAMP_TO_EDGE = 2,
		CLAMP_TO_BORDER = 3,
		MIRROR_CLAMP_TO_EDGE = 4,
	};

	enum class BorderColor
	{
		FLOAT_TRANSPARENT_BLACK = 0,
		INT_TRANSPARENT_BLACK = 1,
		FLOAT_OPAQUE_BLACK = 2,
		INT_OPAQUE_BLACK = 3,
		FLOAT_OPAQUE_WHITE = 4,
		INT_OPAQUE_WHITE = 5,
	};

	enum class DescriptorType
	{
		SAMPLER = 0,
		TEXTURE = 1,
		DEPTH_STENCIL_TEXTURE = 2,
		RW_TEXTURE = 3,
		TYPED_BUFFER = 4,
		RW_TYPED_BUFFER = 5,
		CONSTANT_BUFFER = 6,
		BYTE_BUFFER = 7,
		RW_BYTE_BUFFER = 8,
		STRUCTURED_BUFFER = 9,
		RW_STRUCTURED_BUFFER = 10,
		OFFSET_CONSTANT_BUFFER = 11,
		RANGE_SIZE = OFFSET_CONSTANT_BUFFER + 1
	};

	enum class IndexType
	{
		UINT16 = 0,
		UINT32 = 1,
	};

	enum class QueryType
	{
		OCCLUSION = 0,
		PIPELINE_STATISTICS = 1,
		TIMESTAMP = 2,
	};

	enum class ResourceState
	{
		UNDEFINED,
		READ_HOST,
		READ_DEPTH_STENCIL,
		READ_DEPTH_STENCIL_SHADER,
		READ_RESOURCE,
		READ_RW_RESOURCE,
		READ_CONSTANT_BUFFER,
		READ_VERTEX_BUFFER,
		READ_INDEX_BUFFER,
		READ_INDIRECT_BUFFER,
		READ_TRANSFER,
		READ_WRITE_HOST,
		READ_WRITE_RW_RESOURCE,
		READ_WRITE_DEPTH_STENCIL,
		WRITE_HOST,
		WRITE_COLOR_ATTACHMENT,
		WRITE_RW_RESOURCE,
		WRITE_TRANSFER,
		CLEAR_RESOURCE,
		PRESENT,
	};

	enum class AttachmentLoadOp
	{
		LOAD,
		CLEAR,
		DONT_CARE
	};

	enum class AttachmentStoreOp
	{
		STORE,
		DONT_CARE
	};

	enum class VertexInputRate
	{
		VERTEX,
		INSTANCE
	};

	enum class Format
	{
		UNDEFINED = 0,
		R4G4_UNORM_PACK8 = 1,
		R4G4B4A4_UNORM_PACK16 = 2,
		B4G4R4A4_UNORM_PACK16 = 3,
		R5G6B5_UNORM_PACK16 = 4,
		B5G6R5_UNORM_PACK16 = 5,
		R5G5B5A1_UNORM_PACK16 = 6,
		B5G5R5A1_UNORM_PACK16 = 7,
		A1R5G5B5_UNORM_PACK16 = 8,
		R8_UNORM = 9,
		R8_SNORM = 10,
		R8_USCALED = 11,
		R8_SSCALED = 12,
		R8_UINT = 13,
		R8_SINT = 14,
		R8_SRGB = 15,
		R8G8_UNORM = 16,
		R8G8_SNORM = 17,
		R8G8_USCALED = 18,
		R8G8_SSCALED = 19,
		R8G8_UINT = 20,
		R8G8_SINT = 21,
		R8G8_SRGB = 22,
		R8G8B8_UNORM = 23,
		R8G8B8_SNORM = 24,
		R8G8B8_USCALED = 25,
		R8G8B8_SSCALED = 26,
		R8G8B8_UINT = 27,
		R8G8B8_SINT = 28,
		R8G8B8_SRGB = 29,
		B8G8R8_UNORM = 30,
		B8G8R8_SNORM = 31,
		B8G8R8_USCALED = 32,
		B8G8R8_SSCALED = 33,
		B8G8R8_UINT = 34,
		B8G8R8_SINT = 35,
		B8G8R8_SRGB = 36,
		R8G8B8A8_UNORM = 37,
		R8G8B8A8_SNORM = 38,
		R8G8B8A8_USCALED = 39,
		R8G8B8A8_SSCALED = 40,
		R8G8B8A8_UINT = 41,
		R8G8B8A8_SINT = 42,
		R8G8B8A8_SRGB = 43,
		B8G8R8A8_UNORM = 44,
		B8G8R8A8_SNORM = 45,
		B8G8R8A8_USCALED = 46,
		B8G8R8A8_SSCALED = 47,
		B8G8R8A8_UINT = 48,
		B8G8R8A8_SINT = 49,
		B8G8R8A8_SRGB = 50,
		A8B8G8R8_UNORM_PACK32 = 51,
		A8B8G8R8_SNORM_PACK32 = 52,
		A8B8G8R8_USCALED_PACK32 = 53,
		A8B8G8R8_SSCALED_PACK32 = 54,
		A8B8G8R8_UINT_PACK32 = 55,
		A8B8G8R8_SINT_PACK32 = 56,
		A8B8G8R8_SRGB_PACK32 = 57,
		A2R10G10B10_UNORM_PACK32 = 58,
		A2R10G10B10_SNORM_PACK32 = 59,
		A2R10G10B10_USCALED_PACK32 = 60,
		A2R10G10B10_SSCALED_PACK32 = 61,
		A2R10G10B10_UINT_PACK32 = 62,
		A2R10G10B10_SINT_PACK32 = 63,
		A2B10G10R10_UNORM_PACK32 = 64,
		A2B10G10R10_SNORM_PACK32 = 65,
		A2B10G10R10_USCALED_PACK32 = 66,
		A2B10G10R10_SSCALED_PACK32 = 67,
		A2B10G10R10_UINT_PACK32 = 68,
		A2B10G10R10_SINT_PACK32 = 69,
		R16_UNORM = 70,
		R16_SNORM = 71,
		R16_USCALED = 72,
		R16_SSCALED = 73,
		R16_UINT = 74,
		R16_SINT = 75,
		R16_SFLOAT = 76,
		R16G16_UNORM = 77,
		R16G16_SNORM = 78,
		R16G16_USCALED = 79,
		R16G16_SSCALED = 80,
		R16G16_UINT = 81,
		R16G16_SINT = 82,
		R16G16_SFLOAT = 83,
		R16G16B16_UNORM = 84,
		R16G16B16_SNORM = 85,
		R16G16B16_USCALED = 86,
		R16G16B16_SSCALED = 87,
		R16G16B16_UINT = 88,
		R16G16B16_SINT = 89,
		R16G16B16_SFLOAT = 90,
		R16G16B16A16_UNORM = 91,
		R16G16B16A16_SNORM = 92,
		R16G16B16A16_USCALED = 93,
		R16G16B16A16_SSCALED = 94,
		R16G16B16A16_UINT = 95,
		R16G16B16A16_SINT = 96,
		R16G16B16A16_SFLOAT = 97,
		R32_UINT = 98,
		R32_SINT = 99,
		R32_SFLOAT = 100,
		R32G32_UINT = 101,
		R32G32_SINT = 102,
		R32G32_SFLOAT = 103,
		R32G32B32_UINT = 104,
		R32G32B32_SINT = 105,
		R32G32B32_SFLOAT = 106,
		R32G32B32A32_UINT = 107,
		R32G32B32A32_SINT = 108,
		R32G32B32A32_SFLOAT = 109,
		R64_UINT = 110,
		R64_SINT = 111,
		R64_SFLOAT = 112,
		R64G64_UINT = 113,
		R64G64_SINT = 114,
		R64G64_SFLOAT = 115,
		R64G64B64_UINT = 116,
		R64G64B64_SINT = 117,
		R64G64B64_SFLOAT = 118,
		R64G64B64A64_UINT = 119,
		R64G64B64A64_SINT = 120,
		R64G64B64A64_SFLOAT = 121,
		B10G11R11_UFLOAT_PACK32 = 122,
		E5B9G9R9_UFLOAT_PACK32 = 123,
		D16_UNORM = 124,
		X8_D24_UNORM_PACK32 = 125,
		D32_SFLOAT = 126,
		S8_UINT = 127,
		D16_UNORM_S8_UINT = 128,
		D24_UNORM_S8_UINT = 129,
		D32_SFLOAT_S8_UINT = 130,
		BC1_RGB_UNORM_BLOCK = 131,
		BC1_RGB_SRGB_BLOCK = 132,
		BC1_RGBA_UNORM_BLOCK = 133,
		BC1_RGBA_SRGB_BLOCK = 134,
		BC2_UNORM_BLOCK = 135,
		BC2_SRGB_BLOCK = 136,
		BC3_UNORM_BLOCK = 137,
		BC3_SRGB_BLOCK = 138,
		BC4_UNORM_BLOCK = 139,
		BC4_SNORM_BLOCK = 140,
		BC5_UNORM_BLOCK = 141,
		BC5_SNORM_BLOCK = 142,
		BC6H_UFLOAT_BLOCK = 143,
		BC6H_SFLOAT_BLOCK = 144,
		BC7_UNORM_BLOCK = 145,
		BC7_SRGB_BLOCK = 146,
	};

	enum class ImageViewType
	{
		_1D = 0,
		_2D = 1,
		_3D = 2,
		CUBE = 3,
		_1D_ARRAY = 4,
		_2D_ARRAY = 5,
		CUBE_ARRAY = 6,
	};

	enum class ImageType
	{
		_1D = 0,
		_2D = 1,
		_3D = 2,
	};

	enum class PrimitiveTopology
	{
		POINT_LIST = 0,
		LINE_LIST = 1,
		LINE_STRIP = 2,
		TRIANGLE_LIST = 3,
		TRIANGLE_STRIP = 4,
		TRIANGLE_FAN = 5,
		LINE_LIST_WITH_ADJACENCY = 6,
		LINE_STRIP_WITH_ADJACENCY = 7,
		TRIANGLE_LIST_WITH_ADJACENCY = 8,
		TRIANGLE_STRIP_WITH_ADJACENCY = 9,
		PATCH_LIST = 10,
	};

	enum class PolygonMode
	{
		FILL = 0,
		LINE = 1,
		POINT = 2,
	};

	enum class FrontFace
	{
		COUNTER_CLOCKWISE = 0,
		CLOCKWISE = 1,
	};

	enum class CompareOp
	{
		NEVER = 0,
		LESS = 1,
		EQUAL = 2,
		LESS_OR_EQUAL = 3,
		GREATER = 4,
		NOT_EQUAL = 5,
		GREATER_OR_EQUAL = 6,
		ALWAYS = 7,
	};

	enum class StencilOp
	{
		KEEP = 0,
		ZERO = 1,
		REPLACE = 2,
		INCREMENT_AND_CLAMP = 3,
		DECREMENT_AND_CLAMP = 4,
		INVERT = 5,
		INCREMENT_AND_WRAP = 6,
		DECREMENT_AND_WRAP = 7,
	};

	enum class SampleCount
	{
		_1 = 0x00000001,
		_2 = 0x00000002,
		_4 = 0x00000004,
		_8 = 0x00000008,
		_16 = 0x00000010,
	};

	enum class LogicOp
	{
		CLEAR = 0,
		AND = 1,
		AND_REVERSE = 2,
		COPY = 3,
		AND_INVERTED = 4,
		NO_OP = 5,
		XOR = 6,
		OR = 7,
		NOR = 8,
		EQUIVALENT = 9,
		INVERT = 10,
		OR_REVERSE = 11,
		COPY_INVERTED = 12,
		OR_INVERTED = 13,
		NAND = 14,
		SET = 15,
	};

	enum class BlendFactor
	{
		ZERO = 0,
		ONE = 1,
		SRC_COLOR = 2,
		ONE_MINUS_SRC_COLOR = 3,
		DST_COLOR = 4,
		ONE_MINUS_DST_COLOR = 5,
		SRC_ALPHA = 6,
		ONE_MINUS_SRC_ALPHA = 7,
		DST_ALPHA = 8,
		ONE_MINUS_DST_ALPHA = 9,
		CONSTANT_COLOR = 10,
		ONE_MINUS_CONSTANT_COLOR = 11,
		CONSTANT_ALPHA = 12,
		ONE_MINUS_CONSTANT_ALPHA = 13,
		SRC_ALPHA_SATURATE = 14,
		SRC1_COLOR = 15,
		ONE_MINUS_SRC1_COLOR = 16,
		SRC1_ALPHA = 17,
		ONE_MINUS_SRC1_ALPHA = 18,
	};

	enum class BlendOp
	{
		ADD = 0,
		SUBTRACT = 1,
		REVERSE_SUBTRACT = 2,
		MIN = 3,
		MAX = 4,
	};



	// FLAGS

	enum class ShaderStageFlags
	{
		VERTEX_BIT = 0x00000001,
		HULL_BIT = 0x00000002,
		DOMAIN_BIT = 0x00000004,
		GEOMETRY_BIT = 0x00000008,
		PIXEL_BIT = 0x00000010,
		COMPUTE_BIT = 0x00000020,
		ALL_STAGES = VERTEX_BIT | HULL_BIT | DOMAIN_BIT | GEOMETRY_BIT | PIXEL_BIT | COMPUTE_BIT,
	};
	DEF_ENUM_FLAG_OPERATORS(ShaderStageFlags);

	enum class MemoryPropertyFlags
	{
		DEVICE_LOCAL_BIT = 0x00000001,
		HOST_VISIBLE_BIT = 0x00000002,
		HOST_COHERENT_BIT = 0x00000004,
		HOST_CACHED_BIT = 0x00000008,
	};
	DEF_ENUM_FLAG_OPERATORS(MemoryPropertyFlags);

	enum class QueryPipelineStatisticFlags
	{
		INPUT_ASSEMBLY_VERTICES_BIT = 0x00000001,
		INPUT_ASSEMBLY_PRIMITIVES_BIT = 0x00000002,
		VERTEX_SHADER_INVOCATIONS_BIT = 0x00000004,
		GEOMETRY_SHADER_INVOCATIONS_BIT = 0x00000008,
		GEOMETRY_SHADER_PRIMITIVES_BIT = 0x00000010,
		CLIPPING_INVOCATIONS_BIT = 0x00000020,
		CLIPPING_PRIMITIVES_BIT = 0x00000040,
		PIXEL_SHADER_INVOCATIONS_BIT = 0x00000080,
		HULL_SHADER_PATCHES_BIT = 0x00000100,
		DOMAIN_SHADER_INVOCATIONS_BIT = 0x00000200,
		COMPUTE_SHADER_INVOCATIONS_BIT = 0x00000400,
	};
	DEF_ENUM_FLAG_OPERATORS(QueryPipelineStatisticFlags);

	enum class StencilFaceFlags
	{
		FRONT_BIT = 1,
		BACK_BIT = 2,
		FRONT_AND_BACK = FRONT_BIT | BACK_BIT,
	};
	DEF_ENUM_FLAG_OPERATORS(StencilFaceFlags);

	enum class PipelineStageFlags
	{
		TOP_OF_PIPE_BIT = 0x00000001,
		DRAW_INDIRECT_BIT = 0x00000002,
		VERTEX_INPUT_BIT = 0x00000004,
		VERTEX_SHADER_BIT = 0x00000008,
		HULL_SHADER_BIT = 0x00000010,
		DOMAIN_SHADER_BIT = 0x00000020,
		GEOMETRY_SHADER_BIT = 0x00000040,
		PIXEL_SHADER_BIT = 0x00000080,
		EARLY_FRAGMENT_TESTS_BIT = 0x00000100,
		LATE_FRAGMENT_TESTS_BIT = 0x00000200,
		COLOR_ATTACHMENT_OUTPUT_BIT = 0x00000400,
		COMPUTE_SHADER_BIT = 0x00000800,
		TRANSFER_BIT = 0x00001000,
		BOTTOM_OF_PIPE_BIT = 0x00002000,
		HOST_BIT = 0x00004000,
		CLEAR_BIT = 0x00050000,
	};
	DEF_ENUM_FLAG_OPERATORS(PipelineStageFlags);


	enum class ImageUsageFlags
	{
		TRANSFER_SRC_BIT = 1u << 0u,
		TRANSFER_DST_BIT = 1u << 1u,
		TEXTURE_BIT = 1u << 2u,
		RW_TEXTURE_BIT = 1u << 3u,
		COLOR_ATTACHMENT_BIT = 1u << 4u,
		DEPTH_STENCIL_ATTACHMENT_BIT = 1u << 5u,
		CLEAR_BIT = 1u << 6u,
	};
	DEF_ENUM_FLAG_OPERATORS(ImageUsageFlags);

	enum class ImageCreateFlags
	{
		MUTABLE_FORMAT_BIT = 0x00000008,
		CUBE_COMPATIBLE_BIT = 0x00000010,
		_2D_ARRAY_COMPATIBLE_BIT = 0x00000020,
	};
	DEF_ENUM_FLAG_OPERATORS(ImageCreateFlags);

	enum class BufferUsageFlags
	{
		TRANSFER_SRC_BIT = 1u << 0u,
		TRANSFER_DST_BIT = 1u << 1u,
		TYPED_BUFFER_BIT = 1u << 2u,
		RW_TYPED_BUFFER_BIT = 1u << 3u,
		CONSTANT_BUFFER_BIT = 1u << 4u,
		BYTE_BUFFER_BIT = 1u << 5u,
		RW_BYTE_BUFFER_BIT = 1u << 6u,
		STRUCTURED_BUFFER_BIT = 1u << 7u,
		RW_STRUCTURED_BUFFER_BIT = 1u << 8u,
		INDEX_BUFFER_BIT = 1u << 9u,
		VERTEX_BUFFER_BIT = 1u << 10u,
		INDIRECT_BUFFER_BIT = 1u << 11u,
		CLEAR_BIT = 1u << 12u,
		SHADER_DEVICE_ADDRESS_BIT = 1u << 13u
	};
	DEF_ENUM_FLAG_OPERATORS(BufferUsageFlags);

	enum class BufferCreateFlags
	{

	};
	DEF_ENUM_FLAG_OPERATORS(BufferCreateFlags);

	enum class CullModeFlags
	{
		NONE = 0,
		FRONT_BIT = 1,
		BACK_BIT = 2,
		FRONT_AND_BACK = FRONT_BIT | BACK_BIT,
	};
	DEF_ENUM_FLAG_OPERATORS(CullModeFlags);

	enum class ColorComponentFlags
	{
		R_BIT = 0x00000001,
		G_BIT = 0x00000002,
		B_BIT = 0x00000004,
		A_BIT = 0x00000008,
		ALL_BITS = R_BIT | G_BIT | B_BIT | A_BIT,
	};
	DEF_ENUM_FLAG_OPERATORS(ColorComponentFlags);


	enum class QueryResultFlags
	{
		_64_BIT = 0x00000001,
		WAIT_BIT = 0x00000002,
		WITH_AVAILABILITY_BIT = 0x00000004,
		PARTIAL_BIT = 0x00000008,
	};
	DEF_ENUM_FLAG_OPERATORS(QueryResultFlags);


	enum class DynamicStateFlags
	{
		VIEWPORT_BIT = 1u << 0u,
		SCISSOR_BIT = 1u << 1u,
		LINE_WIDTH_BIT = 1u << 2u,
		DEPTH_BIAS_BIT = 1u << 3u,
		BLEND_CONSTANTS_BIT = 1u << 4u,
		DEPTH_BOUNDS_BIT = 1u << 5u,
		STENCIL_COMPARE_MASK_BIT = 1u << 6u,
		STENCIL_WRITE_MASK_BIT = 1u << 7u,
		STENCIL_REFERENCE_BIT = 1u << 8u,
	};
	DEF_ENUM_FLAG_OPERATORS(DynamicStateFlags);


	enum class DescriptorBindingFlags
	{
		UPDATE_AFTER_BIND_BIT = 1u << 0u,
		UPDATE_UNUSED_WHILE_PENDING_BIT = 1u << 1u,
		PARTIALLY_BOUND_BIT = 1u << 2u,
	};
	DEF_ENUM_FLAG_OPERATORS(DescriptorBindingFlags);


	enum class BarrierFlags
	{
		QUEUE_OWNERSHIP_RELEASE = 1u << 0u,
		QUEUE_OWNERSHIP_AQUIRE = 1u << 1u,
		FIRST_ACCESS_IN_SUBMISSION = 1u << 2u,
		BARRIER_BEGIN = 1u << 3u,
		BARRIER_END = 1u << 4u
	};
	DEF_ENUM_FLAG_OPERATORS(BarrierFlags);



	// STRUCTS

	struct DispatchIndirectCommand
	{
		uint32_t m_groupCountX;
		uint32_t m_groupCountY;
		uint32_t m_groupCountZ;
	};

	struct DrawIndirectCommand
	{
		uint32_t m_vertexCount;
		uint32_t m_instanceCount;
		uint32_t m_firstVertex;
		uint32_t m_firstInstance;
	};

	struct DrawIndexedIndirectCommand
	{
		uint32_t m_indexCount;
		uint32_t m_instanceCount;
		uint32_t m_firstIndex;
		int32_t m_vertexOffset;
		uint32_t m_firstInstance;
	};

	struct DescriptorSetLayoutBinding
	{
		DescriptorType m_descriptorType;
		uint32_t m_binding;
		uint32_t m_space;
		uint32_t m_descriptorCount;
		ShaderStageFlags m_stageFlags;
		DescriptorBindingFlags m_bindingFlags;
	};

	struct VertexInputBindingDescription
	{
		uint32_t m_binding;
		uint32_t m_stride;
		VertexInputRate m_inputRate;
	};

	struct VertexInputAttributeDescription
	{
		enum { MAX_SEMANTIC_NAME_LENGTH = 63 };
		char m_semanticName[MAX_SEMANTIC_NAME_LENGTH + 1] = {};
		uint32_t m_location;
		uint32_t m_binding;
		Format m_format;
		uint32_t m_offset;
	};

	struct StencilOpState
	{
		StencilOp m_failOp;
		StencilOp m_passOp;
		StencilOp m_depthFailOp;
		CompareOp m_compareOp;
		uint32_t m_compareMask;
		uint32_t m_writeMask;
		uint32_t m_reference;
	};

	struct PipelineColorBlendAttachmentState
	{
		bool m_blendEnable;
		BlendFactor m_srcColorBlendFactor;
		BlendFactor m_dstColorBlendFactor;
		BlendOp m_colorBlendOp;
		BlendFactor m_srcAlphaBlendFactor;
		BlendFactor m_dstAlphaBlendFactor;
		BlendOp m_alphaBlendOp;
		ColorComponentFlags m_colorWriteMask;
	};

	struct Viewport
	{
		float m_x;
		float m_y;
		float m_width;
		float m_height;
		float m_minDepth;
		float m_maxDepth;
	};

	struct Offset2D
	{
		int32_t m_x;
		int32_t m_y;
	};

	struct Offset3D
	{
		int32_t m_x;
		int32_t m_y;
		int32_t m_z;
	};

	struct Extent2D
	{
		uint32_t m_width;
		uint32_t m_height;
	};

	struct Extent3D
	{
		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_depth;
	};

	struct Rect
	{
		Offset2D m_offset;
		Extent2D m_extent;
	};

	struct BufferCopy
	{
		uint64_t m_srcOffset;
		uint64_t m_dstOffset;
		uint64_t m_size;
	};

	struct ImageCopy
	{
		uint32_t m_srcMipLevel;
		uint32_t m_srcBaseLayer;
		uint32_t m_srcLayerCount;
		Offset3D m_srcOffset;
		uint32_t m_dstMipLevel;
		uint32_t m_dstBaseLayer;
		uint32_t m_dstLayerCount;
		Offset3D m_dstOffset;
		Extent3D m_extent;
	};

	struct BufferImageCopy
	{
		uint64_t m_bufferOffset;
		uint32_t m_bufferRowLength;
		uint32_t m_bufferImageHeight;
		uint32_t m_imageMipLevel;
		uint32_t m_imageBaseLayer;
		uint32_t m_imageLayerCount;
		Offset3D m_offset;
		Extent3D m_extent;
	};

	union ClearColorValue
	{
		float m_float32[4];
		int32_t m_int32[4];
		uint32_t m_uint32[4];
	};

	struct ClearDepthStencilValue
	{
		float m_depth;
		uint32_t m_stencil;
	};

	union ClearValue
	{
		ClearColorValue m_color;
		ClearDepthStencilValue m_depthStencil;
	};

	struct ClearAttachment
	{
		uint32_t m_colorAttachment; // set to 0xFFFFFFFF to clear depth/stencil attachment
		ClearValue m_clearValue;
	};

	struct ClearRect
	{
		Rect m_rect;
		uint32_t m_baseArrayLayer;
		uint32_t m_layerCount;
	};

	struct ImageSubresourceRange
	{
		uint32_t m_baseMipLevel = 0;
		uint32_t m_levelCount = 1;
		uint32_t m_baseArrayLayer = 0;
		uint32_t m_layerCount = 1;
	};

	struct Barrier
	{
		const Image *m_image;
		const Buffer *m_buffer;
		PipelineStageFlags m_stagesBefore;
		PipelineStageFlags m_stagesAfter;
		ResourceState m_stateBefore;
		ResourceState m_stateAfter;
		Queue *m_srcQueue;
		Queue *m_dstQueue;
		ImageSubresourceRange m_imageSubresourceRange;
		BarrierFlags m_flags;
	};

	struct ColorAttachmentDescription
	{
		const ImageView *m_imageView;
		AttachmentLoadOp m_loadOp;
		AttachmentStoreOp m_storeOp;
		ClearColorValue m_clearValue;
	};

	struct DepthStencilAttachmentDescription
	{
		const ImageView *m_imageView;
		AttachmentLoadOp m_loadOp;
		AttachmentStoreOp m_storeOp;
		AttachmentLoadOp m_stencilLoadOp;
		AttachmentStoreOp m_stencilStoreOp;
		ClearDepthStencilValue m_clearValue;
		bool m_readOnly;
	};

	struct ShaderStageCreateInfo
	{
		enum { MAX_PATH_LENGTH = 255 };
		char m_path[MAX_PATH_LENGTH + 1] = {};
	};

	struct VertexInputState
	{
		enum
		{
			MAX_VERTEX_BINDING_DESCRIPTIONS = 8,
			MAX_VERTEX_ATTRIBUTE_DESCRIPTIONS = 8,
		};
		uint32_t m_vertexBindingDescriptionCount = 0;
		VertexInputBindingDescription m_vertexBindingDescriptions[MAX_VERTEX_BINDING_DESCRIPTIONS] = {};
		uint32_t m_vertexAttributeDescriptionCount = 0;
		VertexInputAttributeDescription m_vertexAttributeDescriptions[MAX_VERTEX_ATTRIBUTE_DESCRIPTIONS] = {};
	};

	struct InputAssemblyState
	{
		PrimitiveTopology m_primitiveTopology = PrimitiveTopology::TRIANGLE_LIST;
		uint32_t m_primitiveRestartEnable = false;
	};

	struct TesselationState
	{
		uint32_t m_patchControlPoints = 0;
	};

	struct ViewportState
	{
		enum { MAX_VIEWPORTS = 1 };
		uint32_t m_viewportCount = 1;
		Viewport m_viewports[MAX_VIEWPORTS] = { { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f } };
		Rect m_scissors[MAX_VIEWPORTS] = { { {0, 0}, {1, 1} } };
	};

	struct RasterizationState
	{
		uint32_t m_depthClampEnable = false;
		uint32_t m_rasterizerDiscardEnable = false;
		PolygonMode m_polygonMode = PolygonMode::FILL;
		CullModeFlags m_cullMode = CullModeFlags::NONE;
		FrontFace m_frontFace = FrontFace::COUNTER_CLOCKWISE;
		uint32_t m_depthBiasEnable = false;
		float m_depthBiasConstantFactor = 1.0f;
		float m_depthBiasClamp = 0.0f;
		float m_depthBiasSlopeFactor = 1.0f;
		float m_lineWidth = 1.0f;
	};

	struct MultisampleState
	{
		SampleCount m_rasterizationSamples = SampleCount::_1;
		uint32_t m_sampleShadingEnable = false;
		float m_minSampleShading = 0.0f;
		uint32_t m_sampleMask = 0xFFFFFFFF;
		uint32_t m_alphaToCoverageEnable = false;
		uint32_t m_alphaToOneEnable = false;
	};

	struct DepthStencilState
	{
		uint32_t m_depthTestEnable = false;
		uint32_t m_depthWriteEnable = false;
		CompareOp m_depthCompareOp = CompareOp::ALWAYS;
		uint32_t m_depthBoundsTestEnable = false;
		uint32_t m_stencilTestEnable = false;
		StencilOpState m_front = {};
		StencilOpState m_back = {};
		float m_minDepthBounds = 0.0f;
		float m_maxDepthBounds = 1.0f;
	};

	struct BlendState
	{
		uint32_t m_logicOpEnable = false;
		LogicOp m_logicOp = LogicOp::COPY;
		uint32_t m_attachmentCount = 0;
		PipelineColorBlendAttachmentState m_attachments[8] = {};
		float m_blendConstants[4] = {};
	};

	struct AttachmentFormats
	{
		uint32_t m_colorAttachmentCount = 0;
		Format m_colorAttachmentFormats[8] = {};
		Format m_depthStencilFormat = Format::UNDEFINED;
	};

	struct StaticSamplerDescription
	{
		uint32_t m_binding;
		uint32_t m_space;
		ShaderStageFlags m_stageFlags;
		Filter m_magFilter;
		Filter m_minFilter;
		SamplerMipmapMode m_mipmapMode;
		SamplerAddressMode m_addressModeU;
		SamplerAddressMode m_addressModeV;
		SamplerAddressMode m_addressModeW;
		float m_mipLodBias;
		bool m_anisotropyEnable;
		float m_maxAnisotropy;
		bool m_compareEnable;
		CompareOp m_compareOp;
		float m_minLod;
		float m_maxLod;
		BorderColor m_borderColor;
		bool m_unnormalizedCoordinates;
	};

	struct DescriptorSetLayoutDeclaration
	{
		DescriptorSetLayout *m_layout;
		uint32_t m_usedBindingCount;
		DescriptorSetLayoutBinding *m_usedBindings;
	};

	struct PipelineLayoutCreateInfo
	{
		uint32_t m_descriptorSetLayoutCount;
		DescriptorSetLayoutDeclaration m_descriptorSetLayoutDeclarations[4];
		uint32_t m_pushConstRange;
		ShaderStageFlags m_pushConstStageFlags;
		uint32_t m_staticSamplerSet;
		uint32_t m_staticSamplerCount;
		const StaticSamplerDescription *m_staticSamplerDescriptions;
	};

	struct GraphicsPipelineCreateInfo
	{
		ShaderStageCreateInfo m_vertexShader;
		ShaderStageCreateInfo m_hullShader;
		ShaderStageCreateInfo m_domainShader;
		ShaderStageCreateInfo m_geometryShader;
		ShaderStageCreateInfo m_pixelShader;

		VertexInputState m_vertexInputState;
		InputAssemblyState m_inputAssemblyState;
		TesselationState m_tesselationState;
		ViewportState m_viewportState;
		RasterizationState m_rasterizationState;
		MultisampleState m_multiSampleState;
		DepthStencilState m_depthStencilState;
		BlendState m_blendState;
		DynamicStateFlags m_dynamicStateFlags = (DynamicStateFlags)0;
		AttachmentFormats m_attachmentFormats;
		PipelineLayoutCreateInfo m_layoutCreateInfo;
	};

	struct ComputePipelineCreateInfo
	{
		ShaderStageCreateInfo m_computeShader;
		PipelineLayoutCreateInfo m_layoutCreateInfo;
	};

	struct ImageCreateInfo
	{
		uint32_t m_width = 1;
		uint32_t m_height = 1;
		uint32_t m_depth = 1;
		uint32_t m_levels = 1;
		uint32_t m_layers = 1;
		SampleCount m_samples = SampleCount::_1;
		ImageType m_imageType = ImageType::_2D;
		Format m_format = Format::UNDEFINED;
		ImageCreateFlags m_createFlags = (ImageCreateFlags)0;
		ImageUsageFlags m_usageFlags = (ImageUsageFlags)0;
		ClearValue m_optimizedClearValue;
	};

	struct BufferCreateInfo
	{
		uint64_t m_size = 1;
		BufferCreateFlags m_createFlags = (BufferCreateFlags)0;
		BufferUsageFlags m_usageFlags = (BufferUsageFlags)0;
	};

	struct MemoryRange
	{
		uint64_t m_offset;
		uint64_t m_size;
	};

	struct SubmitInfo
	{
		uint32_t m_waitSemaphoreCount;
		const Semaphore *const *m_waitSemaphores;
		const uint64_t *m_waitValues;
		const PipelineStageFlags *m_waitDstStageMask;
		uint32_t m_commandListCount;
		const CommandList *const *m_commandLists;
		uint32_t m_signalSemaphoreCount;
		const Semaphore *const *m_signalSemaphores;
		const uint64_t *m_signalValues;
	};

	struct DescriptorBufferInfo
	{
		Buffer *m_buffer;
		uint64_t m_offset;
		uint64_t m_range;
		uint32_t m_structureByteStride;
	};

	struct DescriptorSetUpdate
	{
		uint32_t m_dstBinding;
		uint32_t m_dstArrayElement;
		uint32_t m_descriptorCount;
		DescriptorType m_descriptorType;
		const Sampler *const *m_samplers;
		const ImageView *const *m_imageViews;
		const BufferView *const *m_bufferViews;
		const DescriptorBufferInfo *m_bufferInfo;
		const Sampler *m_sampler;
		const ImageView *m_imageView;
		const BufferView *m_bufferView;
		DescriptorBufferInfo m_bufferInfo1;
	};

	struct SamplerCreateInfo
	{
		Filter m_magFilter;
		Filter m_minFilter;
		SamplerMipmapMode m_mipmapMode;
		SamplerAddressMode m_addressModeU;
		SamplerAddressMode m_addressModeV;
		SamplerAddressMode m_addressModeW;
		float m_mipLodBias;
		bool m_anisotropyEnable;
		float m_maxAnisotropy;
		bool m_compareEnable;
		CompareOp m_compareOp;
		float m_minLod;
		float m_maxLod;
		BorderColor m_borderColor;
		bool m_unnormalizedCoordinates;
	};

	struct ComponentMapping
	{
		ComponentSwizzle m_r;
		ComponentSwizzle m_g;
		ComponentSwizzle m_b;
		ComponentSwizzle m_a;
	};

	struct ImageViewCreateInfo
	{
		Image *m_image;
		ImageViewType m_viewType = gal::ImageViewType::_2D;
		Format m_format = gal::Format::UNDEFINED;
		ComponentMapping m_components = {};
		uint32_t m_baseMipLevel = 0;
		uint32_t m_levelCount = 1;
		uint32_t m_baseArrayLayer = 0;
		uint32_t m_layerCount = 1;
	};

	struct BufferViewCreateInfo
	{
		Buffer *m_buffer;
		Format m_format;
		uint64_t m_offset;
		uint64_t m_range;
	};



	// INTERFACES

	class DescriptorSetLayout
	{
	public:
		virtual ~DescriptorSetLayout() = default;
		virtual void *getNativeHandle() const = 0;
	};

	class DescriptorSet
	{
	public:
		virtual ~DescriptorSet() = default;
		virtual void *getNativeHandle() const = 0;
		virtual void update(uint32_t count, const DescriptorSetUpdate *updates) = 0;
	};

	class DescriptorSetPool
	{
	public:
		virtual ~DescriptorSetPool() = default;
		virtual void *getNativeHandle() const = 0;
		virtual void allocateDescriptorSets(uint32_t count, DescriptorSet **sets) = 0;
		virtual void reset() = 0;
	};

	class GraphicsPipeline
	{
	public:
		virtual ~GraphicsPipeline() = default;
		virtual void *getNativeHandle() const = 0;
		virtual uint32_t getDescriptorSetLayoutCount() const = 0;
		virtual const DescriptorSetLayout *getDescriptorSetLayout(uint32_t index) const = 0;
	};

	class ComputePipeline
	{
	public:
		virtual ~ComputePipeline() = default;
		virtual void *getNativeHandle() const = 0;
		virtual uint32_t getDescriptorSetLayoutCount() const = 0;
		virtual const DescriptorSetLayout *getDescriptorSetLayout(uint32_t index) const = 0;
	};

	class QueryPool
	{
	public:
		virtual ~QueryPool() = default;
		virtual void *getNativeHandle() const = 0;
	};

	class CommandList
	{
	public:
		virtual ~CommandList() = default;
		virtual void *getNativeHandle() const = 0;
		virtual void begin() = 0;
		virtual void end() = 0;
		virtual void bindPipeline(const GraphicsPipeline *pipeline) = 0;
		virtual void bindPipeline(const ComputePipeline *pipeline) = 0;
		virtual void setViewport(uint32_t firstViewport, uint32_t viewportCount, const Viewport *viewports) = 0;
		virtual void setScissor(uint32_t firstScissor, uint32_t scissorCount, const Rect *scissors) = 0;
		virtual void setLineWidth(float lineWidth) = 0;
		virtual void setDepthBias(float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor) = 0;
		virtual void setBlendConstants(const float blendConstants[4]) = 0;
		virtual void setDepthBounds(float minDepthBounds, float maxDepthBounds) = 0;
		virtual void setStencilCompareMask(StencilFaceFlags faceMask, uint32_t compareMask) = 0;
		virtual void setStencilWriteMask(StencilFaceFlags faceMask, uint32_t writeMask) = 0;
		virtual void setStencilReference(StencilFaceFlags faceMask, uint32_t reference) = 0;
		virtual void bindDescriptorSets(const GraphicsPipeline *pipeline, uint32_t firstSet, uint32_t count, const DescriptorSet *const *sets, uint32_t offsetCount, uint32_t *offsets) = 0;
		virtual void bindDescriptorSets(const ComputePipeline *pipeline, uint32_t firstSet, uint32_t count, const DescriptorSet *const *sets, uint32_t offsetCount, uint32_t *offsets) = 0;
		virtual void bindIndexBuffer(const Buffer *buffer, uint64_t offset, IndexType indexType) = 0;
		virtual void bindVertexBuffers(uint32_t firstBinding, uint32_t count, const Buffer *const *buffers, uint64_t *offsets) = 0;
		virtual void draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) = 0;
		virtual void drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) = 0;
		virtual void drawIndirect(const Buffer *buffer, uint64_t offset, uint32_t drawCount, uint32_t stride) = 0;
		virtual void drawIndexedIndirect(const Buffer *buffer, uint64_t offset, uint32_t drawCount, uint32_t stride) = 0;
		virtual void dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) = 0;
		virtual void dispatchIndirect(const Buffer *buffer, uint64_t offset) = 0;
		virtual void copyBuffer(const Buffer *srcBuffer, const Buffer *dstBuffer, uint32_t regionCount, const BufferCopy *regions) = 0;
		virtual void copyImage(const Image *srcImage, const Image *dstImage, uint32_t regionCount, const ImageCopy *regions) = 0;
		virtual void copyBufferToImage(const Buffer *srcBuffer, const Image *dstImage, uint32_t regionCount, const BufferImageCopy *regions) = 0;
		virtual void copyImageToBuffer(const Image *srcImage, const Buffer *dstBuffer, uint32_t regionCount, const BufferImageCopy *regions) = 0;
		virtual void updateBuffer(const Buffer *dstBuffer, uint64_t dstOffset, uint64_t dataSize, const void *data) = 0;
		virtual void fillBuffer(const Buffer *dstBuffer, uint64_t dstOffset, uint64_t size, uint32_t data) = 0;
		virtual void clearColorImage(const Image *image, const ClearColorValue *color, uint32_t rangeCount, const ImageSubresourceRange *ranges) = 0;
		virtual void clearDepthStencilImage(const Image *image, const ClearDepthStencilValue *depthStencil, uint32_t rangeCount, const ImageSubresourceRange *ranges) = 0;
		virtual void barrier(uint32_t count, const Barrier *barriers) = 0;
		virtual void beginQuery(const QueryPool *queryPool, uint32_t query) = 0;
		virtual void endQuery(const QueryPool *queryPool, uint32_t query) = 0;
		virtual void resetQueryPool(const QueryPool *queryPool, uint32_t firstQuery, uint32_t queryCount) = 0;
		virtual void writeTimestamp(PipelineStageFlags pipelineStage, const QueryPool *queryPool, uint32_t query) = 0;
		virtual void copyQueryPoolResults(const QueryPool *queryPool, uint32_t firstQuery, uint32_t queryCount, const Buffer *dstBuffer, uint64_t dstOffset) = 0;
		virtual void pushConstants(const GraphicsPipeline *pipeline, ShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void *values) = 0;
		virtual void pushConstants(const ComputePipeline *pipeline, ShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void *values) = 0;
		virtual void beginRenderPass(uint32_t colorAttachmentCount, ColorAttachmentDescription *colorAttachments, DepthStencilAttachmentDescription *depthStencilAttachment, const Rect &renderArea, bool rwTextureBufferAccess) = 0;
		virtual void endRenderPass() = 0;
		virtual void insertDebugLabel(const char *label) = 0;
		virtual void beginDebugLabel(const char *label) = 0;
		virtual void endDebugLabel() = 0;
	};

	class CommandListPool
	{
	public:
		virtual ~CommandListPool() = default;
		virtual void *getNativeHandle() const = 0;
		virtual void allocate(uint32_t count, CommandList **commandLists) = 0;
		virtual void free(uint32_t count, CommandList **commandLists) = 0;
		virtual void reset() = 0;
	};

	class Queue
	{
	public:
		virtual ~Queue() = default;
		virtual void *getNativeHandle() const = 0;
		virtual QueueType getQueueType() const = 0;
		virtual uint32_t getTimestampValidBits() const = 0;
		virtual float getTimestampPeriod() const = 0;
		virtual bool canPresent() const = 0;
		virtual void submit(uint32_t count, const SubmitInfo *submitInfo) = 0;
		virtual void waitIdle() const = 0;
	};

	class Sampler
	{
	public:
		virtual ~Sampler() = default;
		virtual void *getNativeHandle() const = 0;
	};

	class Image
	{
	public:
		virtual ~Image() = default;
		virtual void *getNativeHandle() const = 0;
		virtual const ImageCreateInfo &getDescription() const = 0;
	};

	class Buffer
	{
	public:
		virtual ~Buffer() = default;
		virtual void *getNativeHandle() const = 0;
		virtual const BufferCreateInfo &getDescription() const = 0;
		virtual void map(void **data) = 0;
		virtual void unmap() = 0;
		virtual void invalidate(uint32_t count, const MemoryRange *ranges) = 0;
		virtual void flush(uint32_t count, const MemoryRange *ranges) = 0;
	};

	class ImageView
	{
	public:
		virtual ~ImageView() = default;
		virtual void *getNativeHandle() const = 0;
		virtual const Image *getImage() const = 0;
		virtual const ImageViewCreateInfo &getDescription() const = 0;
	};

	class BufferView
	{
	public:
		virtual ~BufferView() = default;
		virtual void *getNativeHandle() const = 0;
		virtual const Buffer *getBuffer() const = 0;
		virtual const BufferViewCreateInfo &getDescription() const = 0;
	};

	class Semaphore
	{
	public:
		virtual ~Semaphore() = default;
		virtual void *getNativeHandle() const = 0;
		virtual uint64_t getCompletedValue() const = 0;
		virtual void wait(uint64_t waitValue) const = 0;
		virtual void signal(uint64_t signalValue) const = 0;
	};

	class SwapChain
	{
	public:
		virtual ~SwapChain() = default;
		virtual void *getNativeHandle() const = 0;
		virtual void resize(uint32_t width, uint32_t height, bool fullscreen, PresentMode presentMode) = 0;
		virtual uint32_t getCurrentImageIndex() = 0;
		virtual void present(Semaphore *waitSemaphore, uint64_t semaphoreWaitValue, Semaphore *signalSemaphore, uint64_t semaphoreSignalValue) = 0;
		virtual Extent2D getExtent() const = 0;
		virtual Extent2D getRecreationExtent() const = 0;
		virtual Format getImageFormat() const = 0;
		virtual Image *getImage(size_t index) const = 0;
		virtual Queue *getPresentQueue() const = 0;
	};

	class GraphicsDevice
	{
	public:
		static GraphicsDevice *create(void *windowHandle, bool debugLayer, GraphicsBackendType backend);
		static void destroy(const GraphicsDevice *device);
		virtual ~GraphicsDevice() = default;
		virtual void createGraphicsPipelines(uint32_t count, const GraphicsPipelineCreateInfo *createInfo, GraphicsPipeline **pipelines) = 0;
		virtual void createComputePipelines(uint32_t count, const ComputePipelineCreateInfo *createInfo, ComputePipeline **pipelines) = 0;
		virtual void destroyGraphicsPipeline(GraphicsPipeline *pipeline) = 0;
		virtual void destroyComputePipeline(ComputePipeline *pipeline) = 0;
		virtual void createCommandListPool(const Queue *queue, CommandListPool **commandListPool) = 0;
		virtual void destroyCommandListPool(CommandListPool *commandListPool) = 0;
		virtual void createQueryPool(QueryType queryType, uint32_t queryCount, QueryPool **queryPool) = 0;
		virtual void destroyQueryPool(QueryPool *queryPool) = 0;
		virtual void createImage(const ImageCreateInfo &imageCreateInfo, MemoryPropertyFlags requiredMemoryPropertyFlags, MemoryPropertyFlags preferredMemoryPropertyFlags, bool dedicated, Image **image) = 0;
		virtual void createBuffer(const BufferCreateInfo &bufferCreateInfo, MemoryPropertyFlags requiredMemoryPropertyFlags, MemoryPropertyFlags preferredMemoryPropertyFlags, bool dedicated, Buffer **buffer) = 0;
		virtual void destroyImage(Image *image) = 0;
		virtual void destroyBuffer(Buffer *buffer) = 0;
		virtual void createImageView(const ImageViewCreateInfo &imageViewCreateInfo, ImageView **imageView) = 0;
		virtual void createImageView(Image *image, ImageView **imageView) = 0;
		virtual void createBufferView(const BufferViewCreateInfo &bufferViewCreateInfo, BufferView **bufferView) = 0;
		virtual void destroyImageView(ImageView *imageView) = 0;
		virtual void destroyBufferView(BufferView *bufferView) = 0;
		virtual void createSampler(const SamplerCreateInfo &samplerCreateInfo, Sampler **sampler) = 0;
		virtual void destroySampler(Sampler *sampler) = 0;
		virtual void createSemaphore(uint64_t initialValue, Semaphore **semaphore) = 0;
		virtual void destroySemaphore(Semaphore *semaphore) = 0;
		virtual void createDescriptorSetPool(uint32_t maxSets, const DescriptorSetLayout *descriptorSetLayout, DescriptorSetPool **descriptorSetPool) = 0;
		virtual void destroyDescriptorSetPool(DescriptorSetPool *descriptorSetPool) = 0;
		virtual void createDescriptorSetLayout(uint32_t bindingCount, const DescriptorSetLayoutBinding *bindings, DescriptorSetLayout **descriptorSetLayout) = 0;
		virtual void destroyDescriptorSetLayout(DescriptorSetLayout *descriptorSetLayout) = 0;
		virtual void createSwapChain(const Queue *presentQueue, uint32_t width, uint32_t height, bool fullscreen, PresentMode presentMode, SwapChain **swapChain) = 0;
		virtual void destroySwapChain() = 0;
		virtual void waitIdle() = 0;
		virtual void setDebugObjectName(ObjectType objectType, void *object, const char *name) = 0;
		virtual Queue *getGraphicsQueue() = 0;
		virtual Queue *getComputeQueue() = 0;
		virtual Queue *getTransferQueue() = 0;
		virtual uint64_t getBufferAlignment(DescriptorType bufferType, uint64_t elementSize) const = 0;
		virtual uint64_t getMinUniformBufferOffsetAlignment() const = 0;
		virtual uint64_t getMinStorageBufferOffsetAlignment() const = 0;
		virtual uint64_t getBufferCopyOffsetAlignment() const = 0;
		virtual uint64_t getBufferCopyRowPitchAlignment() const = 0;
		virtual float getMaxSamplerAnisotropy() const = 0;
	};
}