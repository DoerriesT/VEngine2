#pragma once
#include <stdint.h>
#include <EASTL/functional.h>
#include "gal/GraphicsAbstractionLayer.h"
#include "ViewHandles.h"
#include <EASTL/vector.h>
#include <EASTL/bitvector.h>
#include "CommandListFramePool.h"
#include "utility/DeletedCopyMove.h"

class ResourceViewRegistry;

namespace rg
{
	class RenderGraph;
	class Registry;
	enum ResourceHandle : uint32_t;
	enum ResourceViewHandle : uint32_t;
	using RecordFunc = eastl::function<void(gal::CommandList *cmdList, const Registry &registry)>;

	enum class QueueType
	{
		GRAPHICS, COMPUTE, TRANSFER,
	};

	struct ResourceStateAndStage
	{
		gal::ResourceState m_resourceState = gal::ResourceState::UNDEFINED;
		gal::PipelineStageFlags m_stageMask = gal::PipelineStageFlags::TOP_OF_PIPE_BIT;
	};

	struct ResourceStateData
	{
		ResourceStateAndStage m_stateAndStage;
		gal::Queue *m_queue;
	};

	struct ResourceUsageDesc
	{
		ResourceViewHandle m_viewHandle;
		ResourceStateAndStage m_stateAndStage;
		ResourceStateAndStage m_finalStateAndStage; // leave default initialized if the pass does no internal state transitions
	};

	struct ImageDesc
	{
		const char *m_name;
		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_depth;
		uint32_t m_layers;
		uint32_t m_levels;
		gal::SampleCount m_samples;
		gal::ImageType m_imageType;
		gal::Format m_format;
		gal::ImageUsageFlags m_usageFlags;
		gal::ClearValue m_optimizedClearValue;

		static ImageDesc create(
			const char *name,
			gal::Format format,
			gal::ImageUsageFlags usageFlags,
			uint32_t width,
			uint32_t height,
			uint32_t depth = 1,
			uint32_t layers = 1,
			uint32_t levels = 1,
			gal::ImageType imageType = gal::ImageType::_2D,
			gal::ClearValue optimizedClearValue = {},
			gal::SampleCount samples = gal::SampleCount::_1
		) noexcept;
	};

	struct BufferDesc
	{
		const char *m_name;
		uint64_t m_size;
		gal::BufferUsageFlags m_usageFlags;
		bool m_hostVisible;

		static BufferDesc create(
			const char *name,
			uint64_t size,
			gal::BufferUsageFlags usageFlags,
			bool hostVisible = false
		) noexcept;
	};

	struct ImageViewDesc
	{
		const char *m_name;
		ResourceHandle m_imageHandle;
		gal::ImageSubresourceRange m_subresourceRange;
		gal::ImageViewType m_viewType;
		gal::Format m_format;
		gal::ComponentMapping m_components;

		static ImageViewDesc create(
			const char *name,
			ResourceHandle imageHandle,
			const gal::ImageSubresourceRange &subresourceRange = { 0, 1, 0, 1 },
			gal::ImageViewType viewType = gal::ImageViewType::_2D,
			gal::Format format = gal::Format::UNDEFINED,
			const gal::ComponentMapping &components = {}
		) noexcept;

		static ImageViewDesc createDefault(const char *name, ResourceHandle imageHandle, RenderGraph *graph) noexcept;
	};

	struct BufferViewDesc
	{
		const char *m_name;
		ResourceHandle m_bufferHandle;
		uint64_t m_offset;
		uint64_t m_range;
		uint32_t m_structureByteStride;
		gal::Format m_format;

		static BufferViewDesc create(
			const char *name,
			ResourceHandle bufferHandle,
			uint64_t range,
			uint64_t offset = 0,
			uint32_t structureByteStride = 0,
			gal::Format format = gal::Format::UNDEFINED
		) noexcept;

		static BufferViewDesc createDefault(const char *name, ResourceHandle bufferHandle, RenderGraph *graph) noexcept;
	};

	class Registry
	{
	public:
		explicit Registry(RenderGraph *renderGraph) noexcept;
		uint32_t getBindlessHandle(ResourceViewHandle handle, gal::DescriptorType type) const noexcept;
		gal::Image *getImage(ResourceHandle handle) const noexcept;
		gal::Image *getImage(ResourceViewHandle handle) const noexcept;
		gal::ImageView *getImageView(ResourceViewHandle handle) const noexcept;
		void map(ResourceViewHandle handle, void **data) const noexcept;
		void unmap(ResourceViewHandle handle) const noexcept;

	private:
		RenderGraph *m_graph;
	};

	class RenderGraph
	{
		friend class Registry;
		friend struct ImageViewDesc;
		friend struct BufferViewDesc;
	public:
		explicit RenderGraph(gal::GraphicsDevice *device, gal::Semaphore **semaphores, uint64_t *semaphoreValues, ResourceViewRegistry *resourceViewRegistry) noexcept;
		DELETED_COPY_MOVE(RenderGraph);
		~RenderGraph() noexcept;
		void nextFrame() noexcept;
		ResourceViewHandle createImageView(const ImageViewDesc &viewDesc) noexcept;
		ResourceViewHandle createBufferView(const BufferViewDesc &viewDesc) noexcept;
		ResourceHandle createImage(const ImageDesc &imageDesc) noexcept;
		ResourceHandle createBuffer(const BufferDesc &bufferDesc) noexcept;
		ResourceHandle importImage(gal::Image *image, const char *name, ResourceStateData *resourceStateData = nullptr) noexcept;
		ResourceHandle importBuffer(gal::Buffer *buffer, const char *name, ResourceStateData *resourceStateData = nullptr) noexcept;
		void addPass(const char *name, QueueType queueType, size_t usageCount, const ResourceUsageDesc *usageDescs, const RecordFunc &recordFunc) noexcept;
		void execute() noexcept;

	private:
		struct ResourceDescription
		{
			const char *m_name = "";
			uint32_t m_usageFlags = 0;
			uint32_t m_width = 1;
			uint32_t m_height = 1;
			uint32_t m_depth = 1;
			uint32_t m_layers = 1;
			uint32_t m_levels = 1;
			gal::SampleCount m_samples = gal::SampleCount::_1;
			gal::ImageType m_imageType = gal::ImageType::_2D;
			gal::Format m_format = gal::Format::UNDEFINED;
			uint64_t m_offset = 0;
			uint64_t m_size = 0;
			gal::ImageCreateFlags m_imageFlags = {};
			gal::ClearValue m_optimizedClearValue = {};
			uint32_t m_subresourceCount = 1;
			uint32_t m_subresourceUsageInfoOffset = 0;
			ResourceStateData *m_externalStateData = nullptr;
			bool m_concurrent = false;
			bool m_external = false;
			bool m_image = false;
			bool m_hostVisible;
		};

		struct ResourceViewDescription
		{
			const char *m_name = "";
			ResourceHandle m_resourceHandle;
			gal::ImageViewType m_viewType;
			gal::Format m_format;
			gal::ComponentMapping m_components;
			gal::ImageSubresourceRange m_subresourceRange;
			uint64_t m_offset; // for buffers
			uint64_t m_range; // for buffers
			uint32_t m_structureByteStride; // for structured buffers
			bool m_image;
		};

		struct Resource
		{
			gal::Image *m_image;
			gal::Buffer *m_buffer;
			bool m_external;
		};

		struct ResourceView
		{
			gal::ImageView *m_imageView;
			gal::BufferView *m_bufferView;
			TextureViewHandle m_textureHandle;
			RWTextureViewHandle m_rwTextureHandle;
			TypedBufferViewHandle m_typedBufferHandle;
			RWTypedBufferViewHandle m_rwTypedBufferHandle;
			ByteBufferViewHandle m_byteBufferHandle;
			RWByteBufferViewHandle m_rwByteBufferHandle;
			StructuredBufferViewHandle m_structuredBufferHandle;
			RWStructuredBufferViewHandle m_rwStructuredBufferHandle;
		};

		struct SubResourceUsage
		{
			uint16_t m_passHandle;
			ResourceStateAndStage m_initialResourceState;
			ResourceStateAndStage m_finalResourceState;
		};

		struct PassData
		{
			RecordFunc m_recordFunc;
			const char *m_name;
			gal::Queue *m_queue;
			uint32_t m_signalValue;
			eastl::vector<gal::Barrier> m_beforeBarriers;
			eastl::vector<gal::Barrier> m_afterBarriers;
		};

		struct Batch
		{
			gal::Queue *m_queue;
			uint16_t m_passIndexOffset;
			uint16_t m_passIndexCount;
			gal::PipelineStageFlags m_waitDstStageMasks[3];
			uint64_t m_waitValues[3];
			uint64_t m_signalValue;
			bool m_lastBatchOnQueue;
		};

		struct FrameGPUResources
		{
			eastl::vector<Resource> m_resources;
			eastl::vector<ResourceView> m_resourceViews;
			CommandListFramePool m_commandListFramePool;
			uint64_t m_finalWaitValues[3] = {};
		};

		static constexpr size_t k_numFrames = 2;
		uint64_t m_frame = 0;
		gal::GraphicsDevice *m_device;
		gal::Queue *m_queues[3];
		gal::Semaphore *m_semaphores[3];
		uint64_t *m_semaphoreValues[3];
		ResourceViewRegistry *m_resourceViewRegistry;

		eastl::vector<ResourceDescription> m_resourceDescriptions;
		eastl::vector<ResourceViewDescription> m_viewDescriptions;
		eastl::bitvector<> m_culledResources;
		eastl::vector<eastl::vector<SubResourceUsage>> m_subresourceUsages; // m_subresourceUsages[subresourceIndex][usageIndex]
		eastl::vector<PassData> m_passData;
		eastl::vector<Batch> m_recordBatches;
		eastl::vector<gal::Barrier> m_externalReleaseBarriers[3];

		FrameGPUResources m_frameResources[k_numFrames];

		void createResources() noexcept;
		void createSynchronization() noexcept;
		void recordAndSubmit() noexcept;
	};
}

