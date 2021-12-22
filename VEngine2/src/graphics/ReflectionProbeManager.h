#pragma once
#include "utility/DeletedCopyMove.h"
#include "RenderData.h"
#include "RenderGraph.h"

struct CommonViewData;
class ECS;

class ReflectionProbeManager
{
public:
	explicit ReflectionProbeManager(gal::GraphicsDevice *device, gal::DescriptorSetLayout *offsetBufferSetLayout, gal::DescriptorSetLayout *bindlessSetLayout) noexcept;
	DELETED_COPY_MOVE(ReflectionProbeManager);
	~ReflectionProbeManager() noexcept;
	
	void update(const CommonViewData &viewData, ECS *ecs, uint64_t cameraEntity, rg::RenderGraph *graph) noexcept;

private:
};