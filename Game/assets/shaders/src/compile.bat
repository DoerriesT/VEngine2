dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_0 -E main grid_vs.hlsl -Fo ./../grid_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_0 -E main grid_ps.hlsl -Fo ./../grid_ps.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_0 -E main imgui_vs.hlsl -Fo ./../imgui_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_0 -E main imgui_ps.hlsl -Fo ./../imgui_ps.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_0 -E main forward_vs.hlsl -Fo ./../forward_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_0 -E main -D SKINNED=1 forward_vs.hlsl -Fo ./../forward_skinned_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_0 -E main forward_ps.hlsl -Fo ./../forward_ps.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_0 -E main tonemap_cs.hlsl -Fo ./../tonemap_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_0 -E main debugNormals_vs.hlsl -Fo ./../debugNormals_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_0 -E main -D SKINNED=1 debugNormals_vs.hlsl -Fo ./../debugNormals_skinned_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T gs_6_0 -E main debugNormals_gs.hlsl -Fo ./../debugNormals_gs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_0 -E main debugNormals_ps.hlsl -Fo ./../debugNormals_ps.spv

dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_0 -E main shadow_vs.hlsl -Fo ./../shadow_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_0 -E main -D SKINNED=1 shadow_vs.hlsl -Fo ./../shadow_skinned_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_0 -E main -D ALPHA_TESTED=1 shadow_vs.hlsl -Fo ./../shadow_alpha_tested_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_0 -E main -D SKINNED=1 -D ALPHA_TESTED=1 shadow_vs.hlsl -Fo ./../shadow_skinned_alpha_tested_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_0 -E main shadow_ps.hlsl -Fo ./../shadow_ps.spv

dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_0 -E main luminanceHistogram_cs.hlsl -Fo ./../luminanceHistogram_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_0 -E main autoExposure_cs.hlsl -Fo ./../autoExposure_cs.spv

dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_0 -E main outlineID_vs.hlsl -Fo ./../outlineID_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_0 -E main -D SKINNED=1 outlineID_vs.hlsl -Fo ./../outlineID_skinned_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_0 -E main -D ALPHA_TESTED=1 outlineID_vs.hlsl -Fo ./../outlineID_alpha_tested_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_0 -E main -D SKINNED=1 -D ALPHA_TESTED=1 outlineID_vs.hlsl -Fo ./../outlineID_skinned_alpha_tested_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_0 -E main outlineID_ps.hlsl -Fo ./../outlineID_ps.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_0 -E main -D ALPHA_TESTED=1 outlineID_ps.hlsl -Fo ./../outlineID_alpha_tested_ps.spv

dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_0 -E main outline_ps.hlsl -Fo ./../outline_ps.spv

dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_0 -E main fullscreenTriangle_vs.hlsl -Fo ./../fullscreenTriangle_vs.spv

dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_0 -E main sky_vs.hlsl -Fo ./../sky_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_0 -E main sky_ps.hlsl -Fo ./../sky_ps.spv

dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_0 -E main gtao_cs.hlsl -Fo ./../gtao_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_0 -E main gtaoBlur_cs.hlsl -Fo ./../gtaoBlur_cs.spv

dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_0 -E main indirectLighting_ps.hlsl -Fo ./../indirectLighting_ps.spv

dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_0 -E main lightTileAssignment_vs.hlsl -Fo ./../lightTileAssignment_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_0 -E main lightTileAssignment_ps.hlsl -Fo ./../lightTileAssignment_ps.spv

dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_0 -E main debugDraw_vs.hlsl -Fo ./../debugDraw_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_0 -E main debugDraw_ps.hlsl -Fo ./../debugDraw_ps.spv

dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_0 -E main volumetricFogScatter_cs.hlsl -Fo ./../volumetricFogScatter_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_0 -E main volumetricFogFilter_cs.hlsl -Fo ./../volumetricFogFilter_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_0 -E main volumetricFogIntegrate_cs.hlsl -Fo ./../volumetricFogIntegrate_cs.spv

dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_0 -E main temporalAA_cs.hlsl -Fo ./../temporalAA_cs.spv

dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_0 -E main bloomDownsample_cs.hlsl -Fo ./../bloomDownsample_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_0 -E main bloomUpsample_cs.hlsl -Fo ./../bloomUpsample_cs.spv

dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_0 -E main fidelityFxSharpen_cs.hlsl -Fo ./../fidelityFxSharpen_cs.spv

dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_0 -E main reflectionProbeGBuffer_vs.hlsl -Fo ./../reflectionProbeGBuffer_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_0 -E main reflectionProbeGBuffer_ps.hlsl -Fo ./../reflectionProbeGBuffer_ps.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_0 -E main -D ALPHA_TESTED=1 reflectionProbeGBuffer_ps.hlsl -Fo ./../reflectionProbeGBuffer_alpha_tested_ps.spv

dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_0 -E main reflectionProbeRelight_cs.hlsl -Fo ./../reflectionProbeRelight_cs.spv

dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_0 -E main fidelityFxDownsample_cs.hlsl -Fo ./../fidelityFxDownsample_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_0 -E main reflectionProbeFilter_cs.hlsl -Fo ./../reflectionProbeFilter_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_0 -E main brdfLUT_cs.hlsl -Fo ./../brdfLUT_cs.spv

dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_0 -E main irradianceProbeFilter_cs.hlsl -Fo ./../irradianceProbeFilter_diffuse_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_0 -E main -D OUTPUT_VISIBILITY=1 irradianceProbeFilter_cs.hlsl -Fo ./../irradianceProbeFilter_visibility_cs.spv

pause