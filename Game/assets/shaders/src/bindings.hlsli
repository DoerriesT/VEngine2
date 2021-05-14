#ifndef BINDINGS_H
#define BINDINGS_H

#ifndef VULKAN
#define VULKAN 0
#endif // VULKAN

// these do the actual register declaration
#define REGISTER_SRV_ACTUAL(reg, registerSpace) register(t##reg, space##registerSpace)
#define REGISTER_SAMPLER_ACTUAL(reg, registerSpace) register(s##reg, space##registerSpace)
#define REGISTER_UAV_ACTUAL(reg, registerSpace) register(u##reg, space##registerSpace)
#define REGISTER_CBV_ACTUAL(reg, registerSpace) register(b##reg, space##registerSpace)

#if VULKAN

// these are used to expand the macro used as argument; use these in shader code
#define REGISTER_SRV(binding, registerSpace, set) REGISTER_SRV_ACTUAL(binding, set)
#define REGISTER_SAMPLER(binding, registerSpace, set) REGISTER_SAMPLER_ACTUAL(binding, set)
#define REGISTER_UAV(binding, registerSpace, set) REGISTER_UAV_ACTUAL(binding, set)
#define REGISTER_CBV(binding, registerSpace, set) REGISTER_CBV_ACTUAL(binding, set)
#define PUSH_CONSTS(push_const_type, name) [[vk::push_constant]] push_const_type name

#else

// these are used to expand the macro used as argument; use these in shader code
#define REGISTER_SRV(binding, registerSpace, set) REGISTER_SRV_ACTUAL(binding, registerSpace)
#define REGISTER_SAMPLER(binding, registerSpace, set) REGISTER_SAMPLER_ACTUAL(binding, registerSpace)
#define REGISTER_UAV(binding, registerSpace, set) REGISTER_UAV_ACTUAL(binding, registerSpace)
#define REGISTER_CBV(binding, registerSpace, set) REGISTER_CBV_ACTUAL(binding, registerSpace)	
#define PUSH_CONSTS(push_const_type, name) ConstantBuffer<push_const_type> name : register(b0, space5000);
	
#endif // VULKAN

#endif //BINDINGS_H