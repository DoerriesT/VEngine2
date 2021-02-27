dxc.exe -T vs_6_0 -E main grid_vs.hlsl -Fo ./../grid_vs.dxil -Zi -Fd ./../grid_vs.pdb
dxc.exe -T ps_6_0 -E main grid_ps.hlsl -Fo ./../grid_ps.dxil -Zi -Fd ./../grid_ps.pdb
pause