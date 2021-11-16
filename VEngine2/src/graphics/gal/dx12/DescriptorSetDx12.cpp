#include "DescriptorSetDx12.h"
#include "ResourceDx12.h"
#include "utility/TLSFAllocator.h"
#include "utility/Utility.h"

gal::DescriptorSetDx12::DescriptorSetDx12(
	ID3D12Device *device,
	const D3D12_CPU_DESCRIPTOR_HANDLE &cpuBaseHandle,
	const D3D12_GPU_DESCRIPTOR_HANDLE &gpuBaseHandle,
	const DescriptorSetLayoutDx12 *layout,
	UINT incSize,
	bool samplerHeap)
	:m_rootDescriptorAddresses(),
	m_device(device),
	m_cpuBaseHandle(cpuBaseHandle),
	m_gpuBaseHandle(gpuBaseHandle),
	m_layout(layout),
	m_incSize(incSize),
	m_rootDescriptorMask(layout->getRootDescriptorMask()),
	m_samplerHeap(samplerHeap)
{
}

void *gal::DescriptorSetDx12::getNativeHandle() const
{
	return (void *)&m_gpuBaseHandle;
}

void gal::DescriptorSetDx12::update(uint32_t count, const DescriptorSetUpdate *updates)
{
	for (size_t i = 0; i < count; ++i)
	{
		const auto &update = updates[i];

		// handle offset buffers seperately
		if (update.m_descriptorType == DescriptorType::OFFSET_CONSTANT_BUFFER)
		{
			assert(update.m_dstBinding < 32);
			assert(update.m_descriptorCount == 1);
			const auto &bufferInfo = update.m_bufferInfo ? update.m_bufferInfo[0] : update.m_bufferInfo1;
			m_rootDescriptorAddresses[update.m_dstBinding] = ((ID3D12Resource *)bufferInfo.m_buffer->getNativeHandle())->GetGPUVirtualAddress() + bufferInfo.m_offset;
			continue;
		}

		const auto *descriptorSetLayoutBindings = m_layout->getBindings();
		const auto *descriptorSetLayoutBindingOffsets = m_layout->getBindingTableStartOffsets();
		const uint32_t descriptorSetLayoutBindingCount = m_layout->getBindingCount();
		size_t bindingIdxInLayout = -1;
		for (size_t layoutBindingIdx = 0; layoutBindingIdx < descriptorSetLayoutBindingCount; ++layoutBindingIdx)
		{
			if (descriptorSetLayoutBindings[layoutBindingIdx].m_binding == update.m_dstBinding)
			{
				bindingIdxInLayout = layoutBindingIdx;
				break;
			}
		}

		if (bindingIdxInLayout == -1)
		{
			util::fatalExit("DescriptorSet dstBinding declared in DescriptorSetUpdate does not match any binding in the DescriptorSetLayout!", EXIT_FAILURE);
		}

		const uint32_t baseOffset = descriptorSetLayoutBindingOffsets[bindingIdxInLayout];//update.m_dstBinding;

		assert((update.m_descriptorType == DescriptorType::SAMPLER) == m_samplerHeap);

		for (size_t j = 0; j < update.m_descriptorCount; ++j)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE dstRangeStart = m_cpuBaseHandle;
			dstRangeStart.ptr += m_incSize * (baseOffset + update.m_dstArrayElement + j);

			D3D12_CPU_DESCRIPTOR_HANDLE srcRangeStart{};

			bool copy = false;

			switch (update.m_descriptorType)
			{
			case DescriptorType::SAMPLER:
			{
				copy = true;
				const Sampler *sampler = update.m_samplers ? update.m_samplers[j] : update.m_sampler;
				srcRangeStart = *(D3D12_CPU_DESCRIPTOR_HANDLE *)sampler->getNativeHandle();
				break;
			}
			case DescriptorType::TEXTURE:
			{
				copy = true;
				const ImageViewDx12 *imageViewDx = dynamic_cast<const ImageViewDx12 *>(update.m_imageViews ? update.m_imageViews[j] : update.m_imageView);
				assert(imageViewDx);
				srcRangeStart = imageViewDx->getSRV();
				break;
			}
			case DescriptorType::RW_TEXTURE:
			{
				const auto *view = update.m_imageViews ? update.m_imageViews[j] : update.m_imageView;
				const auto &viewDesc = view->getDescription();

				// UAVs for cubemaps or multisampled textures are invalid, so skip this descriptor update
				if (viewDesc.m_viewType != ImageViewType::CUBE && viewDesc.m_viewType != ImageViewType::CUBE_ARRAY && viewDesc.m_image->getDescription().m_samples == SampleCount::_1)
				{
					copy = true;
					const ImageViewDx12 *imageViewDx = dynamic_cast<const ImageViewDx12 *>(view);
					assert(imageViewDx);
					srcRangeStart = imageViewDx->getUAV();
				}
				break;
			}
			case DescriptorType::TYPED_BUFFER:
			{
				copy = true;
				const BufferViewDx12 *bufferViewDx = dynamic_cast<const BufferViewDx12 *>(update.m_bufferViews ? update.m_bufferViews[j] : update.m_bufferView);
				assert(bufferViewDx);
				srcRangeStart = bufferViewDx->getSRV();
				break;
			}
			case DescriptorType::RW_TYPED_BUFFER:
			{
				copy = true;
				const BufferViewDx12 *bufferViewDx = dynamic_cast<const BufferViewDx12 *>(update.m_bufferViews ? update.m_bufferViews[j] : update.m_bufferView);
				assert(bufferViewDx);
				srcRangeStart = bufferViewDx->getUAV();
				break;
			}
			case DescriptorType::CONSTANT_BUFFER:
			{
				const auto &info = update.m_bufferInfo ? update.m_bufferInfo[j] : update.m_bufferInfo1;

				D3D12_CONSTANT_BUFFER_VIEW_DESC viewDesc{};
				viewDesc.BufferLocation = ((ID3D12Resource *)info.m_buffer->getNativeHandle())->GetGPUVirtualAddress() + info.m_offset;
				viewDesc.SizeInBytes = util::alignUp(static_cast<UINT>(info.m_range), (UINT)D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
				m_device->CreateConstantBufferView(&viewDesc, dstRangeStart);
				break;
			}
			case DescriptorType::BYTE_BUFFER:
			{
				const auto &info = update.m_bufferInfo ? update.m_bufferInfo[j] : update.m_bufferInfo1;

				D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc{};
				viewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
				viewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				viewDesc.Buffer.FirstElement = info.m_offset / 4;
				viewDesc.Buffer.NumElements = static_cast<UINT>(info.m_range / 4);
				viewDesc.Buffer.StructureByteStride = 0;
				viewDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

				m_device->CreateShaderResourceView((ID3D12Resource *)info.m_buffer->getNativeHandle(), &viewDesc, dstRangeStart);
				break;
			}
			case DescriptorType::RW_BYTE_BUFFER:
			{
				const auto &info = update.m_bufferInfo ? update.m_bufferInfo[j] : update.m_bufferInfo1;

				D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc{};
				viewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
				viewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
				viewDesc.Buffer.FirstElement = info.m_offset / 4;
				viewDesc.Buffer.NumElements = static_cast<UINT>(info.m_range / 4);
				viewDesc.Buffer.StructureByteStride = 0;
				viewDesc.Buffer.CounterOffsetInBytes = 0;
				viewDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

				m_device->CreateUnorderedAccessView((ID3D12Resource *)info.m_buffer->getNativeHandle(), nullptr, &viewDesc, dstRangeStart);
				break;
			}
			case DescriptorType::STRUCTURED_BUFFER:
			{
				const auto &info = update.m_bufferInfo ? update.m_bufferInfo[j] : update.m_bufferInfo1;

				assert(info.m_offset % info.m_structureByteStride == 0);
				assert(info.m_range % info.m_structureByteStride == 0);

				D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc{};
				viewDesc.Format = DXGI_FORMAT_UNKNOWN;
				viewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				viewDesc.Buffer.FirstElement = info.m_offset / info.m_structureByteStride;
				viewDesc.Buffer.NumElements = static_cast<UINT>(info.m_range / info.m_structureByteStride);
				viewDesc.Buffer.StructureByteStride = info.m_structureByteStride;
				viewDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

				m_device->CreateShaderResourceView((ID3D12Resource *)info.m_buffer->getNativeHandle(), &viewDesc, dstRangeStart);
				break;
			}
			case DescriptorType::RW_STRUCTURED_BUFFER:
			{
				const auto &info = update.m_bufferInfo ? update.m_bufferInfo[j] : update.m_bufferInfo1;

				assert(info.m_offset % info.m_structureByteStride == 0);
				assert(info.m_range % info.m_structureByteStride == 0);

				D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc{};
				viewDesc.Format = DXGI_FORMAT_UNKNOWN;
				viewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
				viewDesc.Buffer.FirstElement = info.m_offset / info.m_structureByteStride;
				viewDesc.Buffer.NumElements = static_cast<UINT>(info.m_range / info.m_structureByteStride);
				viewDesc.Buffer.StructureByteStride = info.m_structureByteStride;
				viewDesc.Buffer.CounterOffsetInBytes = 0;
				viewDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

				m_device->CreateUnorderedAccessView((ID3D12Resource *)info.m_buffer->getNativeHandle(), nullptr, &viewDesc, dstRangeStart);
				break;
			}
			default:
				assert(false);
				break;
			}

			// TODO: batch these calls
			if (copy)
			{
				assert(srcRangeStart.ptr);
				m_device->CopyDescriptorsSimple(1, dstRangeStart, srcRangeStart, m_samplerHeap ? D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER : D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			}
		}
	}
}

uint32_t gal::DescriptorSetDx12::getRootDescriptorMask() const
{
	return m_rootDescriptorMask;
}

const D3D12_GPU_VIRTUAL_ADDRESS *gal::DescriptorSetDx12::getRootDescriptorAddresses() const
{
	return m_rootDescriptorAddresses;
}

gal::DescriptorSetLayoutDx12::DescriptorSetLayoutDx12(uint32_t bindingCount, const DescriptorSetLayoutBinding *bindings)
	:m_bindings(),
	m_bindingCount(bindingCount),
	m_descriptorCount(),
	m_rootDescriptorMask(),
	m_samplerHeap()
{
	if (bindingCount > 32)
	{
		util::fatalExit("Exceeded maximum DescriptorSetLayout binding count (32)!", EXIT_FAILURE);
	}
	bool hasSamplers = false;
	bool hasNonSamplers = false;
	bool hasRootDescriptors = false;
	bool hasNonRootDescriptors = false;

	uint32_t currentTableStartOffset = 0;

	for (size_t i = 0; i < bindingCount; ++i)
	{
		auto type = bindings[i].m_descriptorType;
		m_rootDescriptorMask |= ((type == DescriptorType::OFFSET_CONSTANT_BUFFER) ? 1u : 0u) << i;
		hasSamplers = hasSamplers || type == DescriptorType::SAMPLER;
		hasNonSamplers = hasNonSamplers || type != DescriptorType::SAMPLER;
		hasRootDescriptors = hasRootDescriptors || type == DescriptorType::OFFSET_CONSTANT_BUFFER;
		hasNonRootDescriptors = hasNonRootDescriptors || type != DescriptorType::OFFSET_CONSTANT_BUFFER;

		m_bindingTableStartOffsets[i] = currentTableStartOffset;
		currentTableStartOffset += bindings[i].m_descriptorCount;
		m_descriptorCount = currentTableStartOffset;
	}

	if (hasSamplers && hasNonSamplers)
	{
		util::fatalExit("Tried to create descriptor set layout with both sampler and non-sampler descriptors!", EXIT_FAILURE);
	}
	if (hasRootDescriptors && hasNonRootDescriptors)
	{
		util::fatalExit("Tried to create descriptor set layout with both root descriptors and non-root descriptors!", EXIT_FAILURE);
	}

	// copy bindings to member variable
	memcpy(m_bindings, bindings, bindingCount * sizeof(DescriptorSetLayoutBinding));

	m_samplerHeap = hasSamplers;
}

gal::DescriptorSetLayoutDx12::~DescriptorSetLayoutDx12()
{
	// no implementation
}

void *gal::DescriptorSetLayoutDx12::getNativeHandle() const
{
	// DescriptorSetLayout does not map to any native D3D12 concept
	return nullptr;
}

uint32_t gal::DescriptorSetLayoutDx12::getDescriptorCount() const
{
	return m_descriptorCount;
}

bool gal::DescriptorSetLayoutDx12::needsSamplerHeap() const
{
	return m_samplerHeap;
}

const gal::DescriptorSetLayoutBinding *gal::DescriptorSetLayoutDx12::getBindings() const
{
	return m_bindings;
}

const uint32_t *gal::DescriptorSetLayoutDx12::getBindingTableStartOffsets() const
{
	return m_bindingTableStartOffsets;
}

uint32_t gal::DescriptorSetLayoutDx12::getBindingCount() const
{
	return m_bindingCount;
}

uint32_t gal::DescriptorSetLayoutDx12::getRootDescriptorMask() const
{
	return m_rootDescriptorMask;
}

gal::DescriptorSetPoolDx12::DescriptorSetPoolDx12(
	ID3D12Device *device,
	TLSFAllocator &gpuHeapAllocator,
	const D3D12_CPU_DESCRIPTOR_HANDLE &heapCpuBaseHandle,
	const D3D12_GPU_DESCRIPTOR_HANDLE &heapGpuBaseHandle,
	UINT incrementSize,
	const DescriptorSetLayoutDx12 *layout,
	uint32_t poolSize)
	:m_device(device),
	m_gpuHeapAllocator(gpuHeapAllocator),
	m_cpuBaseHandle(heapCpuBaseHandle),
	m_gpuBaseHandle(heapGpuBaseHandle),
	m_layout(layout),
	m_incSize(incrementSize),
	m_poolSize(poolSize),
	m_currentOffset()
{
	// allocate space in global gpu descriptor heap
	uint32_t spanOffset = 0;
	bool allocSucceeded = m_gpuHeapAllocator.alloc(m_layout->getDescriptorCount() * m_poolSize, 1, spanOffset, m_allocationHandle);

	if (!allocSucceeded)
	{
		util::fatalExit("Failed to allocate descriptors for descriptor set pool!", EXIT_FAILURE);
	}

	// adjust descriptor base handles to start of allocated range
	m_cpuBaseHandle.ptr += m_incSize * spanOffset;
	m_gpuBaseHandle.ptr += m_incSize * spanOffset;

	// allocate memory to placement new DescriptorSetDx12
	m_descriptorSetMemory = new char[sizeof(DescriptorSetDx12) * m_poolSize];
}

gal::DescriptorSetPoolDx12::~DescriptorSetPoolDx12()
{
	// free allocated range in descriptor heap
	m_gpuHeapAllocator.free(m_allocationHandle);
	delete[] m_descriptorSetMemory;
	m_descriptorSetMemory = nullptr;
}

void *gal::DescriptorSetPoolDx12::getNativeHandle() const
{
	// this class doesnt map to any native D3D12 concept
	return nullptr;
}

void gal::DescriptorSetPoolDx12::allocateDescriptorSets(uint32_t count, DescriptorSet **sets)
{
	if (m_currentOffset + count > m_poolSize)
	{
		util::fatalExit("Tried to allocate more descriptor sets from descriptor set pool than available!", EXIT_FAILURE);
	}

	// placement new the sets and increment the offset
	for (size_t i = 0; i < count; ++i)
	{
		uint32_t setOffset = m_currentOffset * m_layout->getDescriptorCount() * m_incSize;

		D3D12_CPU_DESCRIPTOR_HANDLE tableCpuBaseHandle{ m_cpuBaseHandle.ptr + setOffset };
		D3D12_GPU_DESCRIPTOR_HANDLE tableGpuBaseHandle{ m_gpuBaseHandle.ptr + setOffset };


		sets[i] = new (m_descriptorSetMemory + sizeof(DescriptorSetDx12) * m_currentOffset) DescriptorSetDx12(m_device, tableCpuBaseHandle, tableGpuBaseHandle, m_layout, m_incSize, m_layout->needsSamplerHeap());
		++m_currentOffset;
	}
}

void gal::DescriptorSetPoolDx12::reset()
{
	// DescriptorSetDx12 is a POD class, so we can simply reset the allocator offset of the backing memory
	// and dont need to call the destructors
	m_currentOffset = 0;
}
