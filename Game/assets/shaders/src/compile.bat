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

pause