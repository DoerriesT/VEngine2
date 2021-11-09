#ifndef SHADING_MODEL_H
#define SHADING_MODEL_H

#include "brdf.hlsli"

#ifndef SPECULAR_AA
#define SPECULAR_AA 0
#endif // SPECULAR_AA

#if SPECULAR_AA
// "Improved Geometric Specular AA" (2019)
float specularAA(float3 N, float a2)
{
	const float SIGMA2 = 0.25;
	const float KAPPA = 0.18;
	float3 dndu = ddx(N);
	float3 dndv = ddy(N);
	float variance = SIGMA2 * (dot(dndu, dndu) + dot(dndv, dndv));
	float kernelRoughness2 = min(variance, KAPPA);
	float filteredRoughness2 = saturate(a2 + kernelRoughness2);
	return filteredRoughness2;
}
#endif // SPECULAR_AA

float3 Default_Lit(float3 baseColor, float3 F0, float3 radiance, float3 N, float3 V, float3 L, float roughness, float metalness)
{
	// avoid precision problems
	roughness = max(roughness, 0.04);
	float NdotV = abs(dot(N, V)) + 1e-5;
	float3 H = normalize(V + L);
	float VdotH = saturate(dot(V, H));
	float NdotH = saturate(dot(N, H));
	float NdotL = saturate(dot(N, L));
	
	float a = roughness * roughness;
	float a2 = a * a;
	
#if SPECULAR_AA
	a2 = specularAA(N, a2);
#endif // SPECULAR_AA
	
	float3 specular = Specular_GGX(F0, NdotV, NdotL, NdotH, VdotH, a2);
	
	// TODO: how can the diffuse term be made energy conserving with respect to the specular term?
	//float3 diffuse = Diffuse_Lambert(baseColor);// * (1.0 - F_Schlick(F0, NdotL)) * (1.0 - F_Schlick(F0, NdotV));
	//float3 diffuse = (21.0 / (20.0 * PI)) * (1.0 - F0) * baseColor * (1.0 - pow5(1.0 - NdotL)) * (1.0 - pow5(1.0 - NdotV));
	float3 diffuse = Diffuse_Disney(NdotV, NdotL, VdotH, roughness, baseColor);
	
	diffuse *= 1.0 - metalness;
	
	return (specular + diffuse) * radiance * NdotL;
}

float3 Diffuse_Lit(float3 baseColor, float3 radiance, float3 N, float3 V, float3 L, float roughness)
{
	// avoid precision problems
	roughness = max(roughness, 0.04);
	float NdotV = abs(dot(N, V)) + 1e-5;
	float3 H = normalize(V + L);
	float VdotH = saturate(dot(V, H));
	float NdotL = saturate(dot(N, L));
	
	float3 diffuse = Diffuse_Disney(NdotV, NdotL, VdotH, roughness, baseColor);
	return diffuse * radiance * NdotL;
}

#endif // SHADING_MODEL_H