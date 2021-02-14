#include "QueryPoolVk.h"
#include "UtilityVk.h"

gal::QueryPoolVk::QueryPoolVk(VkDevice device, QueryType queryType, uint32_t queryCount, QueryPipelineStatisticFlags pipelineStatistics)
	:m_device(device),
	m_queryPool(VK_NULL_HANDLE)
{
	VkQueryPoolCreateInfo createInfo{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
	createInfo.queryType = static_cast<VkQueryType>(queryType);
	createInfo.queryCount = queryCount;
	createInfo.pipelineStatistics = UtilityVk::translateQueryPipelineStatisticFlags(pipelineStatistics);

	UtilityVk::checkResult(vkCreateQueryPool(device, &createInfo, nullptr, &m_queryPool), "Failed to create Query Pool!");
}

gal::QueryPoolVk::~QueryPoolVk()
{
	vkDestroyQueryPool(m_device, m_queryPool, nullptr);
}

void *gal::QueryPoolVk::getNativeHandle() const
{
	return (void*)m_queryPool;
}
