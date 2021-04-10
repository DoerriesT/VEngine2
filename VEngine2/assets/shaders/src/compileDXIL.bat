dxc.exe -T vs_6_0 -E main grid_vs.hlsl -Fo ./../grid_vs.dxil -Zi -Fd ./../grid_vs.pdb
dxc.exe -T ps_6_0 -E main grid_ps.hlsl -Fo ./../grid_ps.dxil -Zi -Fd ./../grid_ps.pdb
dxc.exe -T vs_6_0 -E main imgui_vs.hlsl -Fo ./../imgui_vs.dxil -Zi -Fd ./../imgui_vs.pdb
dxc.exe -T ps_6_0 -E main imgui_ps.hlsl -Fo ./../imgui_ps.dxil -Zi -Fd ./../imgui_ps.pdb
pause