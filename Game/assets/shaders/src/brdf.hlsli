#ifndef BRDF_H
#define BRDF_H

#ifndef PI
#define PI (3.14159265359)
#endif // PI

float pow5(float v)
{
	float v2 = v * v;
	return v2 * v2 * v;
}

// "Moving Frostbite to Physically Based Rendering"
float D_GGX(float NdotH, float a2)
{
	float d = (NdotH * a2 - NdotH) * NdotH + 1.0;
	return a2 / (PI * d * d);
}

// "Moving Frostbite to Physically Based Rendering"
float V_SmithGGXCorrelated(float NdotV, float NdotL, float a2)
{
	float lambdaGGXV = NdotL * sqrt((-NdotV * a2 + NdotV) * NdotV + a2);
	float lambdaGGXL = NdotV * sqrt((-NdotL * a2 + NdotL) * NdotL + a2);
	
	return 0.5 / (lambdaGGXV + lambdaGGXL + 1e-5); // avoids artifacs on some normal mapped surfaces
}

// https://google.github.io/filament/Filament.html#materialsystem/specularbrdf/fresnel(specularf)
float3 F_Schlick(float3 F0, float VdotH)
{
	float pow5Term = pow5(1.0 - VdotH);
	//return F0 + (1.0 - F0) * pow5Term;
	return pow5Term + F0 * (1.0 - pow5Term);
}

// https://google.github.io/filament/Filament.html#materialsystem/specularbrdf/fresnel(specularf)
float3 F_Schlick(float3 F0, float F90, float VdotH)
{
	return F0 + (F90 - F0) * pow5(1.0 - VdotH);
}

// renormalized according to "Moving Frostbite to Physically Based Rendering"
float3 Diffuse_Disney(float NdotV, float NdotL, float VdotH, float roughness, float3 baseColor)
{
	float energyBias = lerp(0.0, 0.5, roughness);
	float energyFactor = lerp(1.0, 1.0 / 1.51, roughness);
	float fd90 = energyBias + 2.0 * VdotH * VdotH * roughness;
	float lightScatter = (1.0 + (fd90 - 1.0) * pow5(1.0 - NdotL));
	float viewScatter = (1.0 + (fd90 - 1.0) * pow5(1.0 - NdotV));
	return lightScatter * viewScatter * energyFactor * baseColor * (1.0 / PI);
}

float3 Diffuse_Lambert(float3 baseColor)
{
	return baseColor * (1.0 / PI);
}

float3 Specular_GGX(float3 F0, float NdotV, float NdotL, float NdotH, float VdotH, float a2)
{
	float D = D_GGX(NdotH, a2);
	float V = V_SmithGGXCorrelated(NdotV, NdotL, a2);
	float3 F = F_Schlick(F0, VdotH);
	return D * V * F;
}

#endif // BRDF_H