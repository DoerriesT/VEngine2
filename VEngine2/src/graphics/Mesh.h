#pragma once
#include "Handles.h"
#include "ViewHandles.h"
#include "gal/FwdDecl.h"

struct SubMeshCreateInfo
{
	float m_minCorner[3];
	float m_maxCorner[3];
	float m_minTexCoord[2];
	float m_maxTexCoord[2];
	uint32_t m_vertexCount;
	uint32_t m_indexCount;
	const uint16_t *m_indices;
	const float *m_positions;
	const float *m_normals;
	const float *m_tangents;
	const float *m_texCoords;
	const uint32_t *m_jointIndices;
	const uint32_t *m_jointWeights;
};

struct SubMeshDrawInfo
{
	uint32_t m_indexCount;
	uint32_t m_firstIndex;
	int32_t m_vertexOffset;
	uint32_t m_vertexCount;
	bool m_skinned;
};

struct SubMeshBufferHandles
{
	gal::Buffer *m_indexBuffer = nullptr;
	gal::Buffer *m_vertexBuffer = nullptr;
	StructuredBufferViewHandle m_positionsBufferViewHandle;
	StructuredBufferViewHandle m_normalsBufferViewHandle;
	StructuredBufferViewHandle m_tangentsBufferViewHandle;
	StructuredBufferViewHandle m_texCoordsBufferViewHandle;
	StructuredBufferViewHandle m_jointIndicesBufferViewHandle;
	StructuredBufferViewHandle m_jointWeightsBufferViewHandle;
};