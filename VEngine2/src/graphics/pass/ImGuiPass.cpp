#include "ImGuiPass.h"
#include "graphics/gal/Initializers.h"
#include "graphics/LinearGPUBufferAllocator.h"
#include "graphics/imgui/imgui.h"
#include <EASTL/iterator.h> // eastl::size()
#define PROFILING_GPU_ENABLE
#include "profiling/Profiling.h"

namespace
{
	struct ImGuiPushConsts
	{
		float scale[2];
		float translate[2];
		uint32_t textureIndex;
	};
}

ImGuiPass::ImGuiPass(gal::GraphicsDevice *device, gal::DescriptorSetLayout *bindlessSetLayout)
	:m_device(device)
{
	gal::PipelineColorBlendAttachmentState blendState{};
	blendState.m_blendEnable = true;
	blendState.m_srcColorBlendFactor = gal::BlendFactor::SRC_ALPHA;
	blendState.m_dstColorBlendFactor = gal::BlendFactor::ONE_MINUS_SRC_ALPHA;
	blendState.m_colorBlendOp = gal::BlendOp::ADD;
	blendState.m_srcAlphaBlendFactor = gal::BlendFactor::ONE_MINUS_SRC_ALPHA;
	blendState.m_dstAlphaBlendFactor = gal::BlendFactor::ZERO;
	blendState.m_alphaBlendOp = gal::BlendOp::ADD;
	blendState.m_colorWriteMask = gal::ColorComponentFlags::ALL_BITS;

	gal::VertexInputAttributeDescription attributeDescs[]
	{
		{ "POSITION", 0, 0, gal::Format::R32G32_SFLOAT, IM_OFFSETOF(ImDrawVert, pos) },
		{ "TEXCOORD", 1, 0, gal::Format::R32G32_SFLOAT, IM_OFFSETOF(ImDrawVert, uv) },
		{ "COLOR", 2, 0, gal::Format::R8G8B8A8_UNORM, IM_OFFSETOF(ImDrawVert, col) },
	};

	gal::GraphicsPipelineCreateInfo pipelineCreateInfo{};
	gal::GraphicsPipelineBuilder builder(pipelineCreateInfo);
	builder.setVertexShader("assets/shaders/imgui_vs");
	builder.setFragmentShader("assets/shaders/imgui_ps");
	builder.setVertexBindingDescription({ 0, sizeof(ImDrawVert), gal::VertexInputRate::VERTEX });
	builder.setVertexAttributeDescriptions((uint32_t)eastl::size(attributeDescs), attributeDescs);
	builder.setColorBlendAttachment(blendState);
	builder.setDynamicState(gal::DynamicStateFlags::VIEWPORT_BIT | gal::DynamicStateFlags::SCISSOR_BIT);
	//builder.setDepthStencilAttachmentFormat(gal::Format::D32_SFLOAT);
	builder.setColorAttachmentFormat(gal::Format::B8G8R8A8_UNORM);

	gal::DescriptorSetLayoutBinding usedBinding{ gal::DescriptorType::TEXTURE, 0, 0, 65536, gal::ShaderStageFlags::PIXEL_BIT, gal::DescriptorBindingFlags::UPDATE_AFTER_BIND_BIT | gal::DescriptorBindingFlags::PARTIALLY_BOUND_BIT };
	gal::DescriptorSetLayoutDeclaration layoutDecl{ bindlessSetLayout, 1, &usedBinding };
	gal::StaticSamplerDescription staticSamplerDesc = gal::Initializers::staticLinearRepeatSampler(0, 0, gal::ShaderStageFlags::PIXEL_BIT);

	builder.setPipelineLayoutDescription(1, &layoutDecl, sizeof(ImGuiPushConsts), gal::ShaderStageFlags::VERTEX_BIT, 1, &staticSamplerDesc, 1);

	device->createGraphicsPipelines(1, &pipelineCreateInfo, &m_pipeline);
}

ImGuiPass::~ImGuiPass()
{
	m_device->destroyGraphicsPipeline(m_pipeline);
}

void ImGuiPass::record(rg::RenderGraph *graph, const Data &data)
{
	if (!data.m_imGuiDrawData)
	{
		return;
	}

	rg::ResourceUsageDesc usageDescs[] =
	{
		{data.m_renderTargetHandle, {gal::ResourceState::WRITE_COLOR_ATTACHMENT}},
	};
	graph->addPass("ImGui", rg::QueueType::GRAPHICS, eastl::size(usageDescs), usageDescs, [=](gal::CommandList *cmdList, const rg::Registry &registry)
		{
			GAL_SCOPED_GPU_LABEL(cmdList, "ImGui");
			PROFILING_GPU_ZONE_SCOPED_N(data.m_profilingCtx, cmdList, "ImGuiPass");
			PROFILING_ZONE_SCOPED;

			// Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
			int fb_width = (int)(data.m_imGuiDrawData->DisplaySize.x * data.m_imGuiDrawData->FramebufferScale.x);
			int fb_height = (int)(data.m_imGuiDrawData->DisplaySize.y * data.m_imGuiDrawData->FramebufferScale.y);
			if (fb_width <= 0 || fb_height <= 0 || data.m_imGuiDrawData->TotalVtxCount == 0)
				return;

			// Upload vertex/index data into a single contiguous GPU buffer
			uint64_t vertexBufferByteOffset = 0;
			uint64_t indexBufferByteOffset = 0;
			{
				uint64_t vertexAllocSize = data.m_imGuiDrawData->TotalVtxCount * sizeof(ImDrawVert);
				uint64_t indexAllocSize = data.m_imGuiDrawData->TotalIdxCount * sizeof(ImDrawIdx);
				ImDrawVert *vtx_dst = (ImDrawVert *)data.m_vertexBufferAllocator->allocate(sizeof(ImDrawVert), &vertexAllocSize, &vertexBufferByteOffset);
				ImDrawIdx *idx_dst = (ImDrawIdx *)data.m_indexBufferAllocator->allocate(sizeof(ImDrawIdx), &indexAllocSize, &indexBufferByteOffset);

				for (int n = 0; n < data.m_imGuiDrawData->CmdListsCount; n++)
				{
					const ImDrawList *cmd_list = data.m_imGuiDrawData->CmdLists[n];
					memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
					memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
					vtx_dst += cmd_list->VtxBuffer.Size;
					idx_dst += cmd_list->IdxBuffer.Size;
				}
			}

			// begin renderpass
			gal::ColorAttachmentDescription colorAttachDesc{ registry.getImageView(data.m_renderTargetHandle), data.m_clear ? gal::AttachmentLoadOp::CLEAR : gal::AttachmentLoadOp::LOAD, gal::AttachmentStoreOp::STORE, {} };
			cmdList->beginRenderPass(1, &colorAttachDesc, nullptr, { {}, {(uint32_t)fb_width, (uint32_t)fb_height} }, false);
			{
				auto setupRenderState = [&]()
				{
					// Bind pipeline and descriptor sets:
					{
						cmdList->bindPipeline(m_pipeline);
						cmdList->bindDescriptorSets(m_pipeline, 0, 1, &data.m_bindlessSet, 0, nullptr);
					}

					// Bind Vertex And Index Buffer:
					{
						gal::Buffer *vertex_buffers[1] = { data.m_vertexBufferAllocator->getBuffer() };
						cmdList->bindVertexBuffers(0, 1, vertex_buffers, &vertexBufferByteOffset);
						cmdList->bindIndexBuffer(data.m_indexBufferAllocator->getBuffer(), 0, sizeof(ImDrawIdx) == 2 ? gal::IndexType::UINT16 : gal::IndexType::UINT32);
					}

					// Setup viewport:
					{
						gal::Viewport viewport{ 0.0f, 0.0f, static_cast<float>(fb_width), static_cast<float>(fb_height), 0.0f, 1.0f };

						cmdList->setViewport(0, 1, &viewport);
					}

					// Setup scale and translation:
					// Our visible imgui space lies from data.m_imGuiDrawData->DisplayPps (top left) to data.m_imGuiDrawData->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.
					{
#if 0
						// for vulkan ndc space (y down)
						float scale[2];
						scale[0] = 2.0f / data.m_imGuiDrawData->DisplaySize.x;
						scale[1] = 2.0f / data.m_imGuiDrawData->DisplaySize.y;
						float translate[2];
						translate[0] = -1.0f - data.m_imGuiDrawData->DisplayPos.x * scale[0];
						translate[1] = -1.0f - data.m_imGuiDrawData->DisplayPos.y * scale[1];
#else
						// for directx-style ndc space (y up)
						float scale[2];
						scale[0] = (1.0f / (data.m_imGuiDrawData->DisplaySize.x - data.m_imGuiDrawData->DisplayPos.x)) * 2.0f;
						scale[1] = (1.0f / (data.m_imGuiDrawData->DisplaySize.y - data.m_imGuiDrawData->DisplayPos.y)) * -2.0f;
						float translate[2];
						translate[0] = -1.0f;
						translate[1] = 1.0f;
#endif
						cmdList->pushConstants(m_pipeline, gal::ShaderStageFlags::VERTEX_BIT, sizeof(float) * 0, sizeof(float) * 2, scale);
						cmdList->pushConstants(m_pipeline, gal::ShaderStageFlags::VERTEX_BIT, sizeof(float) * 2, sizeof(float) * 2, translate);
					}
				};

				setupRenderState();


				// Will project scissor/clipping rectangles into framebuffer space
				ImVec2 clip_off = data.m_imGuiDrawData->DisplayPos;         // (0,0) unless using multi-viewports
				ImVec2 clip_scale = data.m_imGuiDrawData->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

				// Render command lists
				// (Because we merged all buffers into a single one, we maintain our own offset into them)
				int global_vtx_offset = 0;
				int global_idx_offset = 0;
				for (int n = 0; n < data.m_imGuiDrawData->CmdListsCount; n++)
				{
					const ImDrawList *cmd_list = data.m_imGuiDrawData->CmdLists[n];
					for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
					{
						const ImDrawCmd *pcmd = &cmd_list->CmdBuffer[cmd_i];
						if (pcmd->UserCallback != NULL)
						{
							// User callback, registered via ImDrawList::AddCallback()
							// (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
							if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
								setupRenderState();
							else
								pcmd->UserCallback(cmd_list, pcmd);
						}
						else
						{
							// Project scissor/clipping rectangles into framebuffer space
							ImVec4 clip_rect;
							clip_rect.x = (pcmd->ClipRect.x - clip_off.x) * clip_scale.x;
							clip_rect.y = (pcmd->ClipRect.y - clip_off.y) * clip_scale.y;
							clip_rect.z = (pcmd->ClipRect.z - clip_off.x) * clip_scale.x;
							clip_rect.w = (pcmd->ClipRect.w - clip_off.y) * clip_scale.y;

							if (clip_rect.x < fb_width && clip_rect.y < fb_height && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f)
							{
								// Negative offsets are illegal for vkCmdSetScissor
								if (clip_rect.x < 0.0f)
									clip_rect.x = 0.0f;
								if (clip_rect.y < 0.0f)
									clip_rect.y = 0.0f;

								// Apply scissor/clipping rectangle
								gal::Rect scissor;
								scissor.m_offset.m_x = (int32_t)(clip_rect.x);
								scissor.m_offset.m_y = (int32_t)(clip_rect.y);
								scissor.m_extent.m_width = (uint32_t)(clip_rect.z - clip_rect.x);
								scissor.m_extent.m_height = (uint32_t)(clip_rect.w - clip_rect.y);
								cmdList->setScissor(0, 1, &scissor);

								// Draw
								uint32_t textureIndex = (uint32_t)(size_t)pcmd->TextureId;
								assert(textureIndex != 0);

								cmdList->pushConstants(m_pipeline, gal::ShaderStageFlags::VERTEX_BIT, 4 * sizeof(float), sizeof(textureIndex), &textureIndex);
								cmdList->drawIndexed(pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
							}
						}
					}
					global_idx_offset += cmd_list->IdxBuffer.Size;
					global_vtx_offset += cmd_list->VtxBuffer.Size;
				}
			}
			cmdList->endRenderPass();
		});
}
