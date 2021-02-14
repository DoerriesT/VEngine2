#include "QueryPoolDx12.h"
#include "UtilityDx12.h"
#include "assert.h"

gal::QueryPoolDx12::QueryPoolDx12(ID3D12Device *device, QueryType queryType, uint32_t queryCount, QueryPipelineStatisticFlags pipelineStatistics)
	:m_queryHeap(),
	m_heapType(UtilityDx12::translate(queryType)),
	m_queryType(D3D12_QUERY_TYPE_BINARY_OCCLUSION)
{
	D3D12_QUERY_HEAP_DESC queryHeapDesc{ m_heapType, queryCount, 0 };
	UtilityDx12::checkResult(device->CreateQueryHeap(&queryHeapDesc, __uuidof(ID3D12QueryHeap), (void **)&m_queryHeap), "Failed to create query heap!");

	switch (m_heapType)
	{
	case D3D12_QUERY_HEAP_TYPE_OCCLUSION:
		m_queryType = D3D12_QUERY_TYPE_BINARY_OCCLUSION;
		break;
	case D3D12_QUERY_HEAP_TYPE_TIMESTAMP:
		m_queryType = D3D12_QUERY_TYPE_TIMESTAMP;
		break;
	case D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS:
		m_queryType = D3D12_QUERY_TYPE_PIPELINE_STATISTICS;
		break;
	default:
		assert(false);
		break;
	}
}

gal::QueryPoolDx12::~QueryPoolDx12()
{
	m_queryHeap->Release();
}

void *gal::QueryPoolDx12::getNativeHandle() const
{
	return m_queryHeap;
}

D3D12_QUERY_HEAP_TYPE gal::QueryPoolDx12::getHeapType() const
{
	return m_heapType;
}

D3D12_QUERY_TYPE gal::QueryPoolDx12::getQueryType() const
{
	return m_queryType;
}
