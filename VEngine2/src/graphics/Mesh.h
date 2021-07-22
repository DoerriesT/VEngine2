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
	uint16_t *m_indices;
	float *m_positions;
	float *m_normals;
	float *m_tangents;
	float *m_texCoords;
};

struct SubMeshDrawInfo
{
	uint32_t m_indexCount;
	uint32_t m_firstIndex;
	int32_t m_vertexOffset;
	uint32_t m_vertexCount;
};

struct SubMeshBufferHandles
{
	gal::Buffer *m_indexBuffer = nullptr;
	gal::Buffer *m_vertexBuffer = nullptr;
	StructuredBufferViewHandle m_positionsBufferViewHandle;
	StructuredBufferViewHandle m_normalsBufferViewHandle;
	StructuredBufferViewHandle m_tangentsBufferViewHandle;
	StructuredBufferViewHandle m_texCoordsBufferViewHandle;
};