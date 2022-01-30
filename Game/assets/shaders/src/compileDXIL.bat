dxc.exe -T vs_6_0 -E main grid_vs.hlsl -Fo ./../grid_vs.cso -Zi -Fd ./../grid_vs.pdb
dxc.exe -T ps_6_0 -E main grid_ps.hlsl -Fo ./../grid_ps.cso -Zi -Fd ./../grid_ps.pdb
dxc.exe -T vs_6_0 -E main imgui_vs.hlsl -Fo ./../imgui_vs.cso -Zi -Fd ./../imgui_vs.pdb
dxc.exe -T ps_6_0 -E main imgui_ps.hlsl -Fo ./../imgui_ps.cso -Zi -Fd ./../imgui_ps.pdb
dxc.exe -T vs_6_0 -E main forward_vs.hlsl -Fo ./../forward_vs.cso -Zi -Fd ./../forward_vs.pdb
dxc.exe -T vs_6_0 -E main -D SKINNED=1 forward_vs.hlsl -Fo ./../forward_skinned_vs.cso -Zi -Fd ./../forward_skinned_vs.pdb
dxc.exe -T ps_6_0 -E main forward_ps.hlsl -Fo ./../forward_ps.cso -Zi -Fd ./../forward_ps.pdb
dxc.exe -T cs_6_0 -E main tonemap_cs.hlsl -Fo ./../tonemap_cs.cso -Zi -Fd ./../tonemap_cs.pdb
dxc.exe -T vs_6_0 -E main debugNormals_vs.hlsl -Fo ./../debugNormals_vs.cso -Zi -Fd ./../debugNormals_vs.pdb
dxc.exe -T vs_6_0 -E main -D SKINNED=1 debugNormals_vs.hlsl -Fo ./../debugNormals_skinned_vs.cso -Zi -Fd ./../debugNormals_skinned_vs.pdb
dxc.exe -T gs_6_0 -E main debugNormals_gs.hlsl -Fo ./../debugNormals_gs.cso -Zi -Fd ./../debugNormals_gs.pdb
dxc.exe -T ps_6_0 -E main debugNormals_ps.hlsl -Fo ./../debugNormals_ps.cso -Zi -Fd ./../debugNormals_ps.pdb

dxc.exe -T vs_6_0 -E main shadow_vs.hlsl -Fo ./../shadow_vs.cso -Zi -Fd ./../shadow_vs.pdb
dxc.exe -T vs_6_0 -E main -D SKINNED=1 shadow_vs.hlsl -Fo ./../shadow_skinned_vs.cso -Zi -Fd ./../shadow_skinned_vs.pdb
dxc.exe -T vs_6_0 -E main -D ALPHA_TESTED=1 shadow_vs.hlsl -Fo ./../shadow_alpha_tested_vs.cso -Zi -Fd ./../shadow_alpha_tested_vs.pdb
dxc.exe -T vs_6_0 -E main -D SKINNED=1 -D ALPHA_TESTED=1 shadow_vs.hlsl -Fo ./../shadow_skinned_alpha_tested_vs.cso -Zi -Fd ./../shadow_skinned_alpha_tested_vs.pdb
dxc.exe -T ps_6_0 -E main shadow_ps.hlsl -Fo ./../shadow_ps.cso -Zi -Fd ./../shadow_ps.pdb

dxc.exe -T cs_6_0 -E main luminanceHistogram_cs.hlsl -Fo ./../luminanceHistogram_cs.cso -Zi -Fd ./../luminanceHistogram_cs.pdb
dxc.exe -T cs_6_0 -E main autoExposure_cs.hlsl -Fo ./../autoExposure_cs.cso -Zi -Fd ./../autoExposure_cs.pdb

dxc.exe -T vs_6_0 -E main outlineID_vs.hlsl -Fo ./../outlineID_vs.cso -Zi -Fd ./../outlineID_vs.pdb
dxc.exe -T vs_6_0 -E main -D SKINNED=1 outlineID_vs.hlsl -Fo ./../outlineID_skinned_vs.cso -Zi -Fd ./../outlineID_skinned_vs.pdb
dxc.exe -T vs_6_0 -E main -D ALPHA_TESTED=1 outlineID_vs.hlsl -Fo ./../outlineID_alpha_tested_vs.cso -Zi -Fd ./../outlineID_alpha_tested_vs.pdb
dxc.exe -T vs_6_0 -E main -D SKINNED=1 -D ALPHA_TESTED=1 outlineID_vs.hlsl -Fo ./../outlineID_skinned_alpha_tested_vs.cso -Zi -Fd ./../outlineID_skinned_alpha_tested_vs.pdb
dxc.exe -T ps_6_0 -E main outlineID_ps.hlsl -Fo ./../outlineID_ps.cso -Zi -Fd ./../outlineID_ps.pdb
dxc.exe -T ps_6_0 -E main -D ALPHA_TESTED=1 outlineID_ps.hlsl -Fo ./../outlineID_alpha_tested_ps.cso -Zi -Fd ./../outlineID_alpha_tested_ps.pdb

dxc.exe -T ps_6_0 -E main outline_ps.hlsl -Fo ./../outline_ps.cso -Zi -Fd ./../outline_ps.pdb
dxc.exe -T vs_6_0 -E main fullscreenTriangle_vs.hlsl -Fo ./../fullscreenTriangle_vs.cso -Zi -Fd ./../fullscreenTriangle_vs.pdb

dxc.exe -T vs_6_0 -E main sky_vs.hlsl -Fo ./../sky_vs.cso -Zi -Fd ./../sky_vs.pdb
dxc.exe -T ps_6_0 -E main sky_ps.hlsl -Fo ./../sky_ps.cso -Zi -Fd ./../sky_ps.pdb

dxc.exe -T cs_6_0 -E main gtao_cs.hlsl -Fo ./../gtao_cs.cso -Zi -Fd ./../gtao_cs.pdb
dxc.exe -T cs_6_0 -E main gtaoBlur_cs.hlsl -Fo ./../gtaoBlur_cs.cso -Zi -Fd ./../gtaoBlur_cs.pdb
dxc.exe -T ps_6_0 -E main indirectLighting_ps.hlsl -Fo ./../indirectLighting_ps.cso -Zi -Fd ./../indirectLighting_ps.pdb

dxc.exe -T vs_6_0 -E main lightTileAssignment_vs.hlsl -Fo ./../lightTileAssignment_vs.cso -Zi -Fd ./../lightTileAssignment_vs.pdb
dxc.exe -T ps_6_0 -E main lightTileAssignment_ps.hlsl -Fo ./../lightTileAssignment_ps.cso -Zi -Fd ./../lightTileAssignment_ps.pdb

dxc.exe -T vs_6_0 -E main debugDraw_vs.hlsl -Fo ./../debugDraw_vs.cso -Zi -Fd ./../debugDraw_vs.pdb
dxc.exe -T ps_6_0 -E main debugDraw_ps.hlsl -Fo ./../debugDraw_ps.cso -Zi -Fd ./../debugDraw_ps.pdb

dxc.exe -T cs_6_0 -E main volumetricFogScatter_cs.hlsl -Fo ./../volumetricFogScatter_cs.cso -Zi -Fd ./../volumetricFogScatter_cs.pdb
dxc.exe -T cs_6_0 -E main volumetricFogFilter_cs.hlsl -Fo ./../volumetricFogFilter_cs.cso -Zi -Fd ./../volumetricFogFilter_cs.pdb
dxc.exe -T cs_6_0 -E main volumetricFogIntegrate_cs.hlsl -Fo ./../volumetricFogIntegrate_cs.cso -Zi -Fd ./../volumetricFogIntegrate_cs.pdb

dxc.exe -T cs_6_0 -E main temporalAA_cs.hlsl -Fo ./../temporalAA_cs.cso -Zi -Fd ./../temporalAA_cs.pdb

dxc.exe -T cs_6_0 -E main bloomDownsample_cs.hlsl -Fo ./../bloomDownsample_cs.cso -Zi -Fd ./../bloomDownsample_cs.pdb
dxc.exe -T cs_6_0 -E main bloomUpsample_cs.hlsl -Fo ./../bloomUpsample_cs.cso -Zi -Fd ./../bloomUpsample_cs.pdb

dxc.exe -T cs_6_0 -E main fidelityFxSharpen_cs.hlsl -Fo ./../fidelityFxSharpen_cs.cso -Zi -Fd ./../fidelityFxSharpen_cs.pdb

dxc.exe -T vs_6_0 -E main reflectionProbeGBuffer_vs.hlsl -Fo ./../reflectionProbeGBuffer_vs.cso -Zi -Fd ./../reflectionProbeGBuffer_vs.pdb
dxc.exe -T ps_6_0 -E main reflectionProbeGBuffer_ps.hlsl -Fo ./../reflectionProbeGBuffer_ps.cso -Zi -Fd ./../reflectionProbeGBuffer_ps.pdb
dxc.exe -T ps_6_0 -E main -D ALPHA_TESTED=1 reflectionProbeGBuffer_ps.hlsl -Fo ./../reflectionProbeGBuffer_alpha_tested_ps.cso -Zi -Fd ./../reflectionProbeGBuffer_alpha_tested_ps.pdb

dxc.exe -T cs_6_0 -E main reflectionProbeRelight_cs.hlsl -Fo ./../reflectionProbeRelight_cs.cso -Zi -Fd ./../reflectionProbeRelight_cs.pdb

dxc.exe -T cs_6_0 -E main fidelityFxDownsample_cs.hlsl -Fo ./../fidelityFxDownsample_cs.cso -Zi -Fd ./../fidelityFxDownsample_cs.pdb
dxc.exe -T cs_6_0 -E main reflectionProbeFilter_cs.hlsl -Fo ./../reflectionProbeFilter_cs.cso -Zi -Fd ./../reflectionProbeFilter_cs.pdb
dxc.exe -T cs_6_0 -E main brdfLUT_cs.hlsl -Fo ./../brdfLUT_cs.cso -Zi -Fd ./../brdfLUT_cs.pdb

dxc.exe -T cs_6_0 -E main irradianceProbeFilter_cs.hlsl -Fo ./../irradianceProbeFilter_diffuse_cs.cso -Zi -Fd ./../irradianceProbeFilter_diffuse_cs.pdb
dxc.exe -T cs_6_0 -E main -D OUTPUT_VISIBILITY=1 irradianceProbeFilter_cs.hlsl -Fo ./../irradianceProbeFilter_visibility_cs.cso -Zi -Fd ./../irradianceProbeFilter_visibility_cs.pdb
dxc.exe -T cs_6_0 -E main irradianceProbeAverageFilter_cs.hlsl -Fo ./../irradianceProbeAverageFilter_cs.cso -Zi -Fd ./../irradianceProbeAverageFilter_cs.pdb

dxc.exe -T vs_6_0 -E main irradianceProbeForward_vs.hlsl -Fo ./../irradianceProbeForward_vs.cso -Zi -Fd ./../irradianceProbeForward_vs.pdb
dxc.exe -T ps_6_0 -E main irradianceProbeForward_ps.hlsl -Fo ./../irradianceProbeForward_ps.cso -Zi -Fd ./../irradianceProbeForward_ps.pdb
dxc.exe -T ps_6_0 -E main -D ALPHA_TESTED=1 irradianceProbeForward_ps.hlsl -Fo ./../irradianceProbeForward_alpha_tested_ps.cso -Zi -Fd ./../irradianceProbeForward_alpha_tested_ps.pdb
dxc.exe -T ps_6_0 -E main irradianceProbeSky_ps.hlsl -Fo ./../irradianceProbeSky_ps.cso -Zi -Fd ./../irradianceProbeSky_ps.pdb

dxc.exe -T vs_6_0 -E main irradianceVolumeDebug_vs.hlsl -Fo ./../irradianceVolumeDebug_vs.cso -Zi -Fd ./../irradianceVolumeDebug_vs.pdb
dxc.exe -T ps_6_0 -E main irradianceVolumeDebug_ps.hlsl -Fo ./../irradianceVolumeDebug_ps.cso -Zi -Fd ./../irradianceVolumeDebug_ps.pdb

pause