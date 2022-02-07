#ifndef IRRADIANCE_VOLUME_H
#define IRRADIANCE_VOLUME_H

#include "packing.hlsli"

struct IrradianceVolume
{
	float4 worldToLocal0;
	float4 worldToLocal1;
	float4 worldToLocal2;
	float4 localToWorld0;
	float4 localToWorld1;
	float4 localToWorld2;
	float3 volumeSize;
	float normalBias;
	float2 volumeTexelSize;
	uint diffuseTextureIndex;
	uint visibilityTextureIndex;
	uint averageDiffuseTextureIndex;
	float fadeoutStart;
	float fadeoutEnd;
	float pad2;
};

float3 sampleIrradianceProbe(Texture2D<float4> tex, SamplerState linearSampler, IrradianceVolume volume, uint3 probeIdx, float3 dir, float probeSideLength)
{
	const float probeSideLengthWithPadding = probeSideLength + 2.0f;
	
	float2 texelCoord = 1.0f;
	texelCoord.x += (volume.volumeSize.x * probeIdx.y + probeIdx.x) * probeSideLengthWithPadding;
	texelCoord.y += probeIdx.z * probeSideLengthWithPadding;
	texelCoord += (encodeOctahedron(dir) * 0.5f + 0.5f) * probeSideLength;
	
	float2 uv = texelCoord * (volume.volumeTexelSize / probeSideLengthWithPadding);
	
	return tex.SampleLevel(linearSampler, uv, 0.0f).rgb;
}

float4 sampleIrradianceVolumeInternal(
	Texture2D<float4> diffuseTex, 
	Texture2D<float4> visibilityTex, 
	SamplerState linearSampler, 
	IrradianceVolume volume,
	float3 worldSpacePos,
	float3 N,
	float3 V,
	bool directionless
)
{
	// transform world space position into irradiance volume space
	float3 pointGridCoord;
	pointGridCoord.x = dot(volume.worldToLocal0, float4(worldSpacePos, 1.0f));
	pointGridCoord.y = dot(volume.worldToLocal1, float4(worldSpacePos, 1.0f));
	pointGridCoord.z = dot(volume.worldToLocal2, float4(worldSpacePos, 1.0f));
	
	if (any(pointGridCoord < (0.5f - volume.fadeoutEnd)) || any(pointGridCoord > (volume.volumeSize - 0.5f + volume.fadeoutEnd)))
	{
		return 0.0f;
	}
	
	float alpha = 1.0f;
	if (volume.fadeoutStart < volume.fadeoutEnd)
	{
		alpha *= smoothstep(0.5f - volume.fadeoutEnd, 0.5f - volume.fadeoutStart, min(pointGridCoord.x, min(pointGridCoord.y, pointGridCoord.z)));
		const float maxCoordComp = max(pointGridCoord.x - (volume.volumeSize.x - 0.5f), max(pointGridCoord.y - (volume.volumeSize.y - 0.5f), pointGridCoord.z - (volume.volumeSize.z - 0.5f)));
		alpha *= 1.0f - smoothstep(volume.fadeoutStart, volume.fadeoutEnd, maxCoordComp);
	}
	
	// clamp to the actual minimum and maximum positions of probes in the volume
	pointGridCoord = clamp(pointGridCoord, 0.5f, volume.volumeSize - 0.5f);
	
	float4 sum = 0.0f;
	
	for (uint i = 0; i < 8; ++i)
	{
		// probes are located at .5f values in volume space, not .0f values
		const float3 probeGridCoord = floor(pointGridCoord - 0.5f) + (uint3(i, i >> 1, i >> 2) & 1) + 0.5f;
	
		// transform probe position into world space
		float3 probeWorldSpacePos;
		probeWorldSpacePos.x = dot(volume.localToWorld0, float4(probeGridCoord, 1.0f));
		probeWorldSpacePos.y = dot(volume.localToWorld1, float4(probeGridCoord, 1.0f));
		probeWorldSpacePos.z = dot(volume.localToWorld2, float4(probeGridCoord, 1.0f));
		
		float weight = 1.0f;
		
		const float3 trueDirToProbe = probeWorldSpacePos - worldSpacePos;
		const bool probeInPoint = dot(trueDirToProbe, trueDirToProbe) < 1e-6f;
		
		// backface test
		if (!probeInPoint && !directionless)
		{
			float backfaceWeight = max(0.0001f, (dot(normalize(trueDirToProbe), N) + 1.0f) * 0.5f);
			backfaceWeight *= backfaceWeight;
			backfaceWeight += 0.2f;
			weight *= backfaceWeight;
		}
		
		// moment visibility test
		if (!probeInPoint)
		{
			const float3 bias = directionless ? 0.0f : (N + 3.0f * V) * volume.normalBias;
			const float3 biasedProbeToPoint = worldSpacePos - probeWorldSpacePos + bias;
			const float distToProbe = length(biasedProbeToPoint);
			
			float2 moments = sampleIrradianceProbe(visibilityTex, linearSampler, volume, (uint3)(probeGridCoord - 0.5f), normalize(biasedProbeToPoint), 16.0f).xy;
				
			float mean = moments.x;
			float variance = abs(moments.x * moments.x - moments.y);
			
			float squareTerm = max(distToProbe - mean, 0.0f);
			squareTerm *= squareTerm;
			float chebyshevWeight = variance / (variance + squareTerm);
			chebyshevWeight = max(chebyshevWeight * chebyshevWeight * chebyshevWeight, 0.0f);
			
			if (distToProbe > mean)
			{
				weight *= chebyshevWeight;
			}
		}
		
		// avoid zero weight
		weight = max(0.000001f, weight);
		
		const float crushThreshold = 0.2f;
		if (weight < crushThreshold)
		{
			weight *= weight * weight / (crushThreshold * crushThreshold);
		}
		
		// trilinear
		float3 trilinear = 1.0f - abs(probeGridCoord - pointGridCoord);
		weight *= trilinear.x * trilinear.y * trilinear.z;
		
		float3 tap = 0.0f;
		if (weight > 0.0f)
		{
			if (directionless)
			{
				uint3 probeIdx = (uint3)(probeGridCoord - 0.5f);
				uint2 texelCoord = uint2(volume.volumeSize.x * probeIdx.y + probeIdx.x, probeIdx.z);
				tap = diffuseTex.Load(int3(texelCoord, 0)).rgb;
			}
			else
			{
				tap = sampleIrradianceProbe(diffuseTex, linearSampler, volume, (uint3)(probeGridCoord - 0.5f), N, 8.0f);
			}
			
			tap = sqrt(tap);
		}
		
		sum += float4(tap * weight, weight);
	}
	
	float3 result = sum.xyz / sum.w;
	
	result *= result;
	
	return float4(result, alpha);
}

float4 sampleIrradianceVolume(
	Texture2D<float4> diffuseTex, 
	Texture2D<float4> visibilityTex, 
	SamplerState linearSampler, 
	IrradianceVolume volume,
	float3 worldSpacePos,
	float3 N,
	float3 V
)
{
	return sampleIrradianceVolumeInternal(diffuseTex, visibilityTex, linearSampler, volume, worldSpacePos, N, V, false);
}

float4 sampleIrradianceVolumeDirectionless(
	Texture2D<float4> averageDiffuseTex, 
	Texture2D<float4> visibilityTex, 
	SamplerState linearSampler, 
	IrradianceVolume volume,
	float3 worldSpacePos
)
{
	return sampleIrradianceVolumeInternal(averageDiffuseTex, visibilityTex, linearSampler, volume, worldSpacePos, 0.0f, 0.0f, true);
}

#endif // IRRADIANCE_VOLUME_H