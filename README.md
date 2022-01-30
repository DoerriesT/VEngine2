# VEngine2
![VEngine2 GI](https://doerriest.github.io/img/vengine2_editor_screen0.png?raw=true "VEngine2 Global Illumination")
This is my current (work in progress) hobby render engine that I like to work on in my free time. It is written in C++ and uses a custom abstraction layer to access both Vulkan and D3D12 as rendering API. See https://doerriest.github.io/project/vengine2/ for more details.

Among other things, it has these features:
- Static Global Illumination based on ["Dynamic Diffuse Global Illumination (DDGI)" by Majercik et al.](https://jcgt.org/published/0008/02/01/)
- Relightable Reflection Probes
- Volumetric Fog
- TAA, SSAO/GTAO, bloom and automatic exposure.
- Skinned animation with blend trees authored as node graphs
- Lua scripting
- Archetype Entity Component System (ECS)
- Fiber based job system, inspired by ["Parallelizing the Naughty Dog Engine Using Fibers" as presented by Christian Gyrling](https://www.gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine).
- Physics (thanks to PhysX)

# Controls
- Right click + mouse rotates the camera.
- WASD moves the camera
- Global illumination can be baked by clicking "Scene->Bake Irradiance Volumes" in the menu bar at the top left.

# How to build
The project comes as a Visual Studio 2019 solution and already includes all code dependencies. The Application should be build as x64.
Since project/level saving and loading is not yet properly implemented, the scene and its asset dependencies are currently hardcoded in main.cpp.
The assets are not part of this repository, so they must be manually copied into the Game/assets folder from one of the releases. 
