dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_0 -E main grid_vs.hlsl -Fo ./../grid_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_0 -E main grid_ps.hlsl -Fo ./../grid_ps.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_0 -E main imgui_vs.hlsl -Fo ./../imgui_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_0 -E main imgui_ps.hlsl -Fo ./../imgui_ps.spv

pause