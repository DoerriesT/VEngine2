#include "ResourceDx12.h"
#include "UtilityDx12.h"
#include "assert.h"
#include "utility/TLSFAllocator.h"
#include "utility/Utility.h"
#include "./../Initializers.h"

gal::SamplerDx12::SamplerDx12(ID3D12Device *device, const SamplerCreateInfo &createInfo, TLSFAllocator &cpuSamplerDescriptorAllocator, const D3D12_CPU_DESCRIPTOR_HANDLE &heapCpuBaseHandle, UINT descriptorIncSize)
	:m_sampler(),
	m_descriptorAllocationHandle(),
	m_cpuSamplerDescriptorAllocator(cpuSamplerDescriptorAllocator)
{
	// allocate m_sampler
	uint32_t offset = 0;
	bool allocSucceeded = m_cpuSamplerDescriptorAllocator.alloc(1, 1, offset, m_descriptorAllocationHandle);

	if (!allocSucceeded)
	{
		util::fatalExit("Failed to allocate CPU descriptor for sampler!", EXIT_FAILURE);
	}

	m_sampler.ptr = heapCpuBaseHandle.ptr + descriptorIncSize * offset;

	float borderColors[][4]
	{
		{0.0f, 0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f, 1.0f},
		{0.0f, 0.0f, 0.0f, 1.0f},
		{1.0f, 1.0f, 1.0f, 1.0f},
		{1.0f, 1.0f, 1.0f, 1.0f},
	};
	const size_t borderColorIdx = static_cast<size_t>(createInfo.m_borderColor);

	D3D12_SAMPLER_DESC samplerDesc{};
	samplerDesc.Filter = UtilityDx12::translate(createInfo.m_magFilter, createInfo.m_minFilter, createInfo.m_mipmapMode, createInfo.m_compareEnable, createInfo.m_anisotropyEnable);
	samplerDesc.AddressU = UtilityDx12::translate(createInfo.m_addressModeU);
	samplerDesc.AddressV = UtilityDx12::translate(createInfo.m_addressModeV);
	samplerDesc.AddressW = UtilityDx12::translate(createInfo.m_addressModeW);
	samplerDesc.MipLODBias = createInfo.m_mipLodBias;
	samplerDesc.MaxAnisotropy = createInfo.m_anisotropyEnable ? static_cast<UINT>(createInfo.m_maxAnisotropy) : 1;
	samplerDesc.ComparisonFunc = createInfo.m_compareEnable ? UtilityDx12::translate(createInfo.m_compareOp) : D3D12_COMPARISON_FUNC_ALWAYS;
	samplerDesc.BorderColor[0] = borderColors[borderColorIdx][0];
	samplerDesc.BorderColor[1] = borderColors[borderColorIdx][1];
	samplerDesc.BorderColor[2] = borderColors[borderColorIdx][2];
	samplerDesc.BorderColor[3] = borderColors[borderColorIdx][3];
	samplerDesc.MinLOD = createInfo.m_minLod;
	samplerDesc.MaxLOD = createInfo.m_maxLod;

	device->CreateSampler(&samplerDesc, m_sampler);
}

gal::SamplerDx12::~SamplerDx12()
{
	// free descriptor handle allocation
	m_cpuSamplerDescriptorAllocator.free(m_descriptorAllocationHandle);
}

void *gal::SamplerDx12::getNativeHandle() const
{
	return (void *)&m_sampler;
}

gal::ImageDx12::ImageDx12(ID3D12Resource *image, void *allocHandle, const ImageCreateInfo &createInfo, bool uploadHeap, bool readbackHeap, bool committedResource)
	:m_image(image),
	m_allocHandle(allocHandle),
	m_description(createInfo),
	m_uploadHeap(uploadHeap),
	m_readbackHeap(readbackHeap),
	m_committedResource(committedResource)
{
}

void *gal::ImageDx12::getNativeHandle() const
{
	return m_image;
}

const gal::ImageCreateInfo &gal::ImageDx12::getDescription() const
{
	return m_description;
}

void *gal::ImageDx12::getAllocationHandle()
{
	return m_allocHandle;
}

bool gal::ImageDx12::isUploadHeapResource() const
{
	return m_uploadHeap;
}

bool gal::ImageDx12::isReadbackHeapResource() const
{
	return m_readbackHeap;
}

bool gal::ImageDx12::isCommittedResource() const
{
	return m_committedResource;
}

gal::BufferDx12::BufferDx12(ID3D12Resource *buffer, void *allocHandle, const BufferCreateInfo &createInfo, bool uploadHeap, bool readbackHeap)
	:m_buffer(buffer),
	m_allocHandle(allocHandle),
	m_description(createInfo),
	m_uploadHeap(uploadHeap),
	m_readbackHeap(readbackHeap)
{
}

void *gal::BufferDx12::getNativeHandle() const
{
	return m_buffer;
}

const gal::BufferCreateInfo &gal::BufferDx12::getDescription() const
{
	return m_description;
}

void gal::BufferDx12::map(void **data)
{
	D3D12_RANGE range{ 0, m_description.m_size };
	UtilityDx12::checkResult(m_buffer->Map(0, &range, data), "Failed to map buffer!");
}

void gal::BufferDx12::unmap()
{
	m_buffer->Unmap(0, nullptr);
}

void gal::BufferDx12::invalidate(uint32_t count, const MemoryRange *ranges)
{
	// no implementation
	// TODO: ensure same behavior between vk and dx12
}

void gal::BufferDx12::flush(uint32_t count, const MemoryRange *ranges)
{
	// no implementation
	// TODO: ensure same behavior between vk and dx12
}

void *gal::BufferDx12::getAllocationHandle()
{
	return m_allocHandle;
}

bool gal::BufferDx12::isUploadHeapResource() const
{
	return m_uploadHeap;
}

bool gal::BufferDx12::isReadbackHeapResource() const
{
	return m_readbackHeap;
}

gal::ImageViewDx12::ImageViewDx12(ID3D12Device *device, const ImageViewCreateInfo &createInfo,
	TLSFAllocator &cpuDescriptorAllocator, const D3D12_CPU_DESCRIPTOR_HANDLE &descriptorHeapCpuBaseHandle, UINT descriptorIncSize,
	TLSFAllocator &cpuRTVDescriptorAllocator, const D3D12_CPU_DESCRIPTOR_HANDLE &descriptorRTVHeapCpuBaseHandle, UINT rtvDescriptorIncSize,
	TLSFAllocator &cpuDSVDescriptorAllocator, const D3D12_CPU_DESCRIPTOR_HANDLE &descriptorDSVHeapCpuBaseHandle, UINT dsvDescriptorIncSize)
	:m_srv(),
	m_uav(),
	m_rtv(),
	m_dsv(),
	m_dsvDepthReadOnly(),
	m_srvAllocationHandle(),
	m_uavAllocationHandle(),
	m_rtvAllocationHandle(),
	m_dsvAllocationHandle(),
	m_dsvDepthReadOnlyAllocationHandle(),
	m_cpuDescriptorAllocator(cpuDescriptorAllocator),
	m_cpuRTVDescriptorAllocator(cpuRTVDescriptorAllocator),
	m_cpuDSVDescriptorAllocator(cpuDSVDescriptorAllocator),
	m_description(createInfo)
{
	const auto &imageDesc = createInfo.m_image->getDescription();

	ID3D12Resource *resource = (ID3D12Resource *)createInfo.m_image->getNativeHandle();

	DXGI_FORMAT format = UtilityDx12::translate(createInfo.m_format);

	ImageViewType viewType = createInfo.m_viewType;

	// remap 1D/2D views of layered images to their array variants
	if (imageDesc.m_layers > 1 && (viewType == ImageViewType::_1D || viewType == ImageViewType::_2D))
	{
		switch (imageDesc.m_imageType)
		{
		case ImageType::_1D:
			viewType = ImageViewType::_1D_ARRAY;
			break;
		case ImageType::_2D:
			viewType = ImageViewType::_2D_ARRAY;
			break;
		default:
			assert(false);
			break;
		}
	}

	// SRV
	if ((imageDesc.m_usageFlags & ImageUsageFlags::TEXTURE_BIT) != 0)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Format = format;
		viewDesc.Shader4ComponentMapping = UtilityDx12::translate(createInfo.m_components);

		if ((imageDesc.m_usageFlags & ImageUsageFlags::DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
		{
			switch (imageDesc.m_format)
			{
			case Format::D16_UNORM:
				viewDesc.Format = DXGI_FORMAT_R16_UNORM;
				break;
			case Format::X8_D24_UNORM_PACK32:
				viewDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
				break;
			case Format::D32_SFLOAT:
				viewDesc.Format = DXGI_FORMAT_R32_FLOAT;
				break;
			case Format::S8_UINT:
				viewDesc.Format = DXGI_FORMAT_R8_UINT;
				break;
			case Format::D24_UNORM_S8_UINT:
				viewDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS; // TODO: is this correct?
				break;
			case Format::D32_SFLOAT_S8_UINT:
				viewDesc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
				break;
			default:
				assert(false);
				break;
			}
		}

		switch (viewType)
		{
		case ImageViewType::_1D:
			viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
			viewDesc.Texture1D.MostDetailedMip = createInfo.m_baseMipLevel;
			viewDesc.Texture1D.MipLevels = createInfo.m_levelCount;
			viewDesc.Texture1D.ResourceMinLODClamp = 0.0f;
			break;
		case ImageViewType::_2D:
			if (imageDesc.m_samples == SampleCount::_1)
			{
				viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				viewDesc.Texture2D.MostDetailedMip = createInfo.m_baseMipLevel;
				viewDesc.Texture2D.MipLevels = createInfo.m_levelCount;
				viewDesc.Texture2D.PlaneSlice = 0;
				viewDesc.Texture2D.ResourceMinLODClamp = 0.0f;
			}
			else
			{
				viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
			}
			break;
		case ImageViewType::_3D:
			viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
			viewDesc.Texture3D.MostDetailedMip = createInfo.m_baseMipLevel;
			viewDesc.Texture3D.MipLevels = createInfo.m_levelCount;
			viewDesc.Texture3D.ResourceMinLODClamp = 0.0f;
			break;
		case ImageViewType::CUBE:
			viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			viewDesc.TextureCube.MostDetailedMip = createInfo.m_baseMipLevel;
			viewDesc.TextureCube.MipLevels = createInfo.m_levelCount;
			viewDesc.TextureCube.ResourceMinLODClamp = 0.0f;
			break;
		case ImageViewType::_1D_ARRAY:
			viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
			viewDesc.Texture1DArray.MostDetailedMip = createInfo.m_baseMipLevel;
			viewDesc.Texture1DArray.MipLevels = createInfo.m_levelCount;
			viewDesc.Texture1DArray.FirstArraySlice = createInfo.m_baseArrayLayer;
			viewDesc.Texture1DArray.ArraySize = createInfo.m_layerCount;
			viewDesc.Texture1DArray.ResourceMinLODClamp = 0.0f;
			break;
		case ImageViewType::_2D_ARRAY:
			if (imageDesc.m_samples == SampleCount::_1)
			{
				viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
				viewDesc.Texture2DArray.MostDetailedMip = createInfo.m_baseMipLevel;
				viewDesc.Texture2DArray.MipLevels = createInfo.m_levelCount;
				viewDesc.Texture2DArray.FirstArraySlice = createInfo.m_baseArrayLayer;
				viewDesc.Texture2DArray.ArraySize = createInfo.m_layerCount;
				viewDesc.Texture2DArray.PlaneSlice = 0;
				viewDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
			}
			else
			{
				viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
				viewDesc.Texture2DMSArray.FirstArraySlice = createInfo.m_baseArrayLayer;
				viewDesc.Texture2DMSArray.ArraySize = createInfo.m_layerCount;
			}
			break;
		case ImageViewType::CUBE_ARRAY:
			assert(createInfo.m_layerCount % 6 == 0);
			viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
			viewDesc.TextureCubeArray.MostDetailedMip = createInfo.m_baseMipLevel;
			viewDesc.TextureCubeArray.MipLevels = createInfo.m_levelCount;
			viewDesc.TextureCubeArray.First2DArrayFace = createInfo.m_baseArrayLayer;
			viewDesc.TextureCubeArray.NumCubes = createInfo.m_layerCount / 6;
			viewDesc.TextureCubeArray.ResourceMinLODClamp = 0.0f;
			break;
		default:
			assert(false);
			break;
		}

		// allocate SRV descriptor
		{
			uint32_t offset = 0;
			bool allocSucceeded = m_cpuDescriptorAllocator.alloc(1, 1, offset, m_srvAllocationHandle);

			if (!allocSucceeded)
			{
				util::fatalExit("Failed to allocate CPU descriptor for SRV!", EXIT_FAILURE);
			}

			m_srv.ptr = descriptorHeapCpuBaseHandle.ptr + descriptorIncSize * offset;
		}

		device->CreateShaderResourceView(resource, &viewDesc, m_srv);
	}

	// UAV
	if ((imageDesc.m_usageFlags & ImageUsageFlags::RW_TEXTURE_BIT) != 0 && createInfo.m_viewType != ImageViewType::CUBE && createInfo.m_viewType != ImageViewType::CUBE_ARRAY && imageDesc.m_samples == SampleCount::_1)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc{};
		viewDesc.Format = format;

		switch (viewType)
		{
		case ImageViewType::_1D:
			viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
			viewDesc.Texture1D.MipSlice = createInfo.m_baseMipLevel;
			break;
		case ImageViewType::_2D:
			viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			viewDesc.Texture2D.MipSlice = createInfo.m_baseMipLevel;
			viewDesc.Texture2D.PlaneSlice = 0;
			break;
		case ImageViewType::_3D:
			viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
			viewDesc.Texture3D.MipSlice = createInfo.m_baseMipLevel;
			viewDesc.Texture3D.FirstWSlice = 0;
			viewDesc.Texture3D.WSize = imageDesc.m_depth;
			break;
		case ImageViewType::_1D_ARRAY:
			viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
			viewDesc.Texture1DArray.MipSlice = createInfo.m_baseMipLevel;
			viewDesc.Texture1DArray.FirstArraySlice = createInfo.m_baseArrayLayer;
			viewDesc.Texture1DArray.ArraySize = createInfo.m_layerCount;
			break;
		case ImageViewType::_2D_ARRAY:
			viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
			viewDesc.Texture2DArray.MipSlice = createInfo.m_baseMipLevel;
			viewDesc.Texture2DArray.FirstArraySlice = createInfo.m_baseArrayLayer;
			viewDesc.Texture2DArray.ArraySize = createInfo.m_layerCount;
			viewDesc.Texture2DArray.PlaneSlice = 0;
			break;
		default:
			assert(false);
			break;
		}

		// allocate UAV descriptor
		{
			uint32_t offset = 0;
			bool allocSucceeded = m_cpuDescriptorAllocator.alloc(1, 1, offset, m_uavAllocationHandle);

			if (!allocSucceeded)
			{
				util::fatalExit("Failed to allocate CPU descriptor for UAV!", EXIT_FAILURE);
			}

			m_uav.ptr = descriptorHeapCpuBaseHandle.ptr + descriptorIncSize * offset;
		}

		device->CreateUnorderedAccessView(resource, nullptr, &viewDesc, m_uav);
	}

	// RTV
	if ((imageDesc.m_usageFlags & ImageUsageFlags::COLOR_ATTACHMENT_BIT) != 0 && createInfo.m_viewType != ImageViewType::CUBE && createInfo.m_viewType != ImageViewType::CUBE_ARRAY)
	{
		D3D12_RENDER_TARGET_VIEW_DESC viewDesc{};
		viewDesc.Format = format;

		switch (viewType)
		{
		case ImageViewType::_1D:
			viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
			viewDesc.Texture1D.MipSlice = createInfo.m_baseMipLevel;
			break;
		case ImageViewType::_2D:
			if (imageDesc.m_samples == SampleCount::_1)
			{
				viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
				viewDesc.Texture2D.MipSlice = createInfo.m_baseMipLevel;
				viewDesc.Texture2D.PlaneSlice = 0;
			}
			else
			{
				viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
			}
			break;
		case ImageViewType::_3D:
			viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
			viewDesc.Texture3D.MipSlice = createInfo.m_baseMipLevel;
			viewDesc.Texture3D.FirstWSlice = 0;
			viewDesc.Texture3D.WSize = imageDesc.m_depth;
			break;
		case ImageViewType::_1D_ARRAY:
			viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
			viewDesc.Texture1DArray.MipSlice = createInfo.m_baseMipLevel;
			viewDesc.Texture1DArray.FirstArraySlice = createInfo.m_baseArrayLayer;
			viewDesc.Texture1DArray.ArraySize = createInfo.m_layerCount;
			break;
		case ImageViewType::_2D_ARRAY:
			if (imageDesc.m_samples == SampleCount::_1)
			{
				viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
				viewDesc.Texture2DArray.MipSlice = createInfo.m_baseMipLevel;
				viewDesc.Texture2DArray.FirstArraySlice = createInfo.m_baseArrayLayer;
				viewDesc.Texture2DArray.ArraySize = createInfo.m_layerCount;
				viewDesc.Texture2DArray.PlaneSlice = 0;
			}
			else
			{
				viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
				viewDesc.Texture2DMSArray.FirstArraySlice = createInfo.m_baseArrayLayer;
				viewDesc.Texture2DMSArray.ArraySize = createInfo.m_layerCount;
			}
			break;
		default:
			assert(false);
			break;
		}

		// allocate RTV descriptor
		{
			uint32_t offset = 0;
			bool allocSucceeded = m_cpuRTVDescriptorAllocator.alloc(1, 1, offset, m_rtvAllocationHandle);

			if (!allocSucceeded)
			{
				util::fatalExit("Failed to allocate CPU descriptor for RTV!", EXIT_FAILURE);
			}

			m_rtv.ptr = descriptorRTVHeapCpuBaseHandle.ptr + rtvDescriptorIncSize * offset;
		}

		device->CreateRenderTargetView(resource, &viewDesc, m_rtv);
	}

	// DSV
	if ((imageDesc.m_usageFlags & ImageUsageFlags::DEPTH_STENCIL_ATTACHMENT_BIT) != 0 && createInfo.m_viewType != ImageViewType::CUBE && createInfo.m_viewType != ImageViewType::CUBE_ARRAY && createInfo.m_viewType != ImageViewType::_3D)
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC viewDesc{};
		viewDesc.Format = format;
		viewDesc.Flags = D3D12_DSV_FLAG_NONE;

		switch (viewType)
		{
		case ImageViewType::_1D:
			viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
			viewDesc.Texture1D.MipSlice = createInfo.m_baseMipLevel;
			break;
		case ImageViewType::_2D:
			if (imageDesc.m_samples == SampleCount::_1)
			{
				viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
				viewDesc.Texture2D.MipSlice = createInfo.m_baseMipLevel;
			}
			else
			{
				viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
			}
			break;
		case ImageViewType::_1D_ARRAY:
			viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
			viewDesc.Texture1DArray.MipSlice = createInfo.m_baseMipLevel;
			viewDesc.Texture1DArray.FirstArraySlice = createInfo.m_baseArrayLayer;
			viewDesc.Texture1DArray.ArraySize = createInfo.m_layerCount;
			break;
		case ImageViewType::_2D_ARRAY:
			if (imageDesc.m_samples == SampleCount::_1)
			{
				viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
				viewDesc.Texture2DArray.MipSlice = createInfo.m_baseMipLevel;
				viewDesc.Texture2DArray.FirstArraySlice = createInfo.m_baseArrayLayer;
				viewDesc.Texture2DArray.ArraySize = createInfo.m_layerCount;
			}
			else
			{
				viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
				viewDesc.Texture2DMSArray.FirstArraySlice = createInfo.m_baseArrayLayer;
				viewDesc.Texture2DMSArray.ArraySize = createInfo.m_layerCount;
			}
			break;
		default:
			assert(false);
			break;
		}

		D3D12_DSV_FLAGS readOnlyFlags = D3D12_DSV_FLAG_NONE;
		if (Initializers::isDepthFormat(createInfo.m_format))
		{
			readOnlyFlags |= D3D12_DSV_FLAG_READ_ONLY_DEPTH;
		}
		if (Initializers::isStencilFormat(createInfo.m_format))
		{
			readOnlyFlags |= D3D12_DSV_FLAG_READ_ONLY_STENCIL;
		}

		// create two descriptors, a normal DSV and a depth read only DSV
		for (int i = 0; i < 2; ++i)
		{
			auto &descriptorHandle = (i == 0) ? m_dsv : m_dsvDepthReadOnly;
			viewDesc.Flags = (i == 0) ? D3D12_DSV_FLAG_NONE : readOnlyFlags;

			// allocate DSV descriptor
			{
				uint32_t offset = 0;
				bool allocSucceeded = m_cpuDSVDescriptorAllocator.alloc(1, 1, offset, (i == 0) ? m_dsvAllocationHandle : m_dsvDepthReadOnlyAllocationHandle);

				if (!allocSucceeded)
				{
					util::fatalExit("Failed to allocate CPU descriptor for DSV!", EXIT_FAILURE);
				}

				descriptorHandle.ptr = descriptorDSVHeapCpuBaseHandle.ptr + dsvDescriptorIncSize * offset;
			}

			device->CreateDepthStencilView(resource, &viewDesc, descriptorHandle);
		}
	}
}

gal::ImageViewDx12::~ImageViewDx12()
{
	// free descriptor handle(s)
	if (m_srvAllocationHandle)
	{
		m_cpuDescriptorAllocator.free(m_srvAllocationHandle);
	}
	if (m_uavAllocationHandle)
	{
		m_cpuDescriptorAllocator.free(m_uavAllocationHandle);
	}
	if (m_rtvAllocationHandle)
	{
		m_cpuRTVDescriptorAllocator.free(m_rtvAllocationHandle);
	}
	if (m_dsvAllocationHandle)
	{
		m_cpuDSVDescriptorAllocator.free(m_dsvAllocationHandle);
	}
	if (m_dsvDepthReadOnlyAllocationHandle)
	{
		m_cpuDSVDescriptorAllocator.free(m_dsvDepthReadOnlyAllocationHandle);
	}
}

void *gal::ImageViewDx12::getNativeHandle() const
{
	return nullptr;
}

const gal::Image *gal::ImageViewDx12::getImage() const
{
	return m_description.m_image;
}

const gal::ImageViewCreateInfo &gal::ImageViewDx12::getDescription() const
{
	return m_description;
}

D3D12_CPU_DESCRIPTOR_HANDLE gal::ImageViewDx12::getSRV() const
{
	return m_srv;
}

D3D12_CPU_DESCRIPTOR_HANDLE gal::ImageViewDx12::getUAV() const
{
	return m_uav;
}

D3D12_CPU_DESCRIPTOR_HANDLE gal::ImageViewDx12::getRTV() const
{
	return m_rtv;
}

D3D12_CPU_DESCRIPTOR_HANDLE gal::ImageViewDx12::getDSV() const
{
	return m_dsv;
}

D3D12_CPU_DESCRIPTOR_HANDLE gal::ImageViewDx12::getDSVDepthReadOnly() const
{
	return m_dsvDepthReadOnly;
}

gal::BufferViewDx12::BufferViewDx12(ID3D12Device *device, const BufferViewCreateInfo &createInfo, TLSFAllocator &cpuDescriptorAllocator, const D3D12_CPU_DESCRIPTOR_HANDLE &descriptorHeapCpuBaseHandle, UINT descriptorIncSize)
	:m_srv(),
	m_uav(),
	m_srvAllocationHandle(),
	m_uavAllocationHandle(),
	m_cpuDescriptorAllocator(cpuDescriptorAllocator),
	m_description(createInfo)
{
	const auto &bufferDesc = createInfo.m_buffer->getDescription();

	ID3D12Resource *resource = (ID3D12Resource *)createInfo.m_buffer->getNativeHandle();

	DXGI_FORMAT format = UtilityDx12::translate(createInfo.m_format);
	UINT formatSize = UtilityDx12::formatByteSize(createInfo.m_format);

	assert((createInfo.m_offset % formatSize) == 0);
	assert((createInfo.m_range % formatSize) == 0);

	// SRV
	if ((bufferDesc.m_usageFlags & BufferUsageFlags::TYPED_BUFFER_BIT) != 0)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Format = format;
		viewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		viewDesc.Buffer.FirstElement = createInfo.m_offset / formatSize;
		viewDesc.Buffer.NumElements = static_cast<UINT>(createInfo.m_range / formatSize);
		viewDesc.Buffer.StructureByteStride = 0;
		viewDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		// allocate SRV descriptor
		{
			uint32_t offset = 0;
			bool allocSucceeded = m_cpuDescriptorAllocator.alloc(1, 1, offset, m_srvAllocationHandle);

			if (!allocSucceeded)
			{
				util::fatalExit("Failed to allocate CPU descriptor for SRV!", EXIT_FAILURE);
			}

			m_srv.ptr = descriptorHeapCpuBaseHandle.ptr + descriptorIncSize * offset;
		}

		device->CreateShaderResourceView(resource, &viewDesc, m_srv);
	}

	// UAV
	if ((bufferDesc.m_usageFlags & BufferUsageFlags::RW_TYPED_BUFFER_BIT) != 0)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc{};
		viewDesc.Format = format;
		viewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		viewDesc.Buffer.FirstElement = createInfo.m_offset / formatSize;
		viewDesc.Buffer.NumElements = static_cast<UINT>(createInfo.m_range / formatSize);
		viewDesc.Buffer.StructureByteStride = 0;
		viewDesc.Buffer.CounterOffsetInBytes = 0;
		viewDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		// allocate UAV descriptor
		{
			uint32_t offset = 0;
			bool allocSucceeded = m_cpuDescriptorAllocator.alloc(1, 1, offset, m_uavAllocationHandle);

			if (!allocSucceeded)
			{
				util::fatalExit("Failed to allocate CPU descriptor for UAV!", EXIT_FAILURE);
			}

			m_uav.ptr = descriptorHeapCpuBaseHandle.ptr + descriptorIncSize * offset;
		}

		device->CreateUnorderedAccessView(resource, nullptr, &viewDesc, m_uav);
	}
}

gal::BufferViewDx12::~BufferViewDx12()
{
	// free descriptor handle(s)
	if (m_uavAllocationHandle)
	{
		m_cpuDescriptorAllocator.free(m_uavAllocationHandle);
	}
	if (m_srvAllocationHandle)
	{
		m_cpuDescriptorAllocator.free(m_srvAllocationHandle);
	}
	if (m_uavAllocationHandle)
	{
		m_cpuDescriptorAllocator.free(m_uavAllocationHandle);
	}
}

void *gal::BufferViewDx12::getNativeHandle() const
{
	return nullptr;
}

const gal::Buffer *gal::BufferViewDx12::getBuffer() const
{
	return m_description.m_buffer;
}

const gal::BufferViewCreateInfo &gal::BufferViewDx12::getDescription() const
{
	return m_description;
}

D3D12_CPU_DESCRIPTOR_HANDLE gal::BufferViewDx12::getSRV() const
{
	return m_srv;
}

D3D12_CPU_DESCRIPTOR_HANDLE gal::BufferViewDx12::getUAV() const
{
	return m_uav;
}
