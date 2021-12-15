#pragma once
#include <glm/mat4x4.hpp>
#include "gal/FwdDecl.h"

class ResourceViewRegistry;
class RendererResources;
namespace gal
{
	class GraphicsDevice;
}

struct CommonViewData
{
	glm::mat4 m_viewMatrix;
	glm::mat4 m_invViewMatrix;
	glm::mat4 m_projectionMatrix;
	glm::mat4 m_invProjectionMatrix;
	glm::mat4 m_jitteredProjectionMatrix;
	glm::mat4 m_invJitteredProjectionMatrix;
	glm::mat4 m_viewProjectionMatrix;
	glm::mat4 m_invViewProjectionMatrix;
	glm::mat4 m_jitteredViewProjectionMatrix;
	glm::mat4 m_invJitteredViewProjectionMatrix;
	glm::mat4 m_prevViewMatrix;
	glm::mat4 m_prevInvViewMatrix;
	glm::mat4 m_prevProjectionMatrix;
	glm::mat4 m_prevInvProjectionMatrix;
	glm::mat4 m_prevJitteredProjectionMatrix;
	glm::mat4 m_prevInvJitteredProjectionMatrix;
	glm::mat4 m_prevViewProjectionMatrix;
	glm::mat4 m_prevInvViewProjectionMatrix;
	glm::mat4 m_prevJitteredViewProjectionMatrix;
	glm::mat4 m_prevInvJitteredViewProjectionMatrix;
	glm::vec4 m_viewMatrixDepthRow;
	glm::vec3 m_cameraPosition;
	float m_near;
	float m_far;
	float m_fovy;
	uint32_t m_width;
	uint32_t m_height;
	float m_aspectRatio;
	uint32_t m_frame;
	gal::GraphicsDevice *m_device;
	RendererResources *m_rendererResources;
	ResourceViewRegistry *m_viewRegistry;
	uint32_t m_resIdx;
	uint32_t m_prevResIdx;
};