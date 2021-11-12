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
pause