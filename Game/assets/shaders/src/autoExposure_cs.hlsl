#include "bindings.hlsli"

#define LOCAL_SIZE_X 64
#define LUMINANCE_HISTOGRAM_SIZE 256

struct PushConsts
{
	float precomputedTermUp;
	float precomputedTermDown;
	float invScale;
	float bias;
	uint lowerBound;
	uint upperBound;
	float exposureCompensation;
	float exposureMin;
	float exposureMax;
	uint fixExposureToMax;
	uint inputHistogramBufferIndex;
	uint resultExposureBufferIndex;
};

ByteAddressBuffer g_ByteAdressBuffers[65536] : REGISTER_SRV(4, 0, 0);
RWByteAddressBuffer g_RWByteAddressBuffers[65536] : REGISTER_UAV(5, 0, 0);

PUSH_CONSTS(PushConsts, g_PushConsts);

groupshared uint s_localHistogram[LUMINANCE_HISTOGRAM_SIZE];

float computeEV100(float aperture, float shutterTime, float ISO)
{
	// EV number is defined as:
	// 2^EV_s = N^2 / t and EV_s = EV_100 + log2(S/100)
	// This gives
	//   EV_s = log2(N^2 / t)
	//   EV_100 + log2(S/100) = log(N^2 / t)
	//   EV_100 = log2(N^2 / t) - log2(S/100)
	//   EV_100 = log2(N^2 / t * 100 / S)
	return log2((aperture * aperture) / shutterTime * 100 / ISO);
}

float computeEV100FromAvgLuminance(float avgLuminance)
{
	// We later use the middle gray at 12.7% in order to have
	// a middle gray at 12% with a sqrt(2) room for specular highlights
	// But here we deal with the spot meter measuring the middle gray
	// which is fixed at 12.5 for matching standard camera
	// constructor settings (i.e. calibration constant K = 12.5)
	// Reference: http://en.wikipedia.org/wiki/Film_speed
	return log2(max(avgLuminance, 1e-5) * 100.0 / 12.5);
}

float convertEV100ToExposure(float EV100)
{
	// Compute the maximum luminance possible with H_sbs sensitivity
	// maxLum = 78 / ( S * q ) * N^2 / t
	//        = 78 / ( S * q ) * 2^EV_100
	//        = 78 / ( 100 * 0.65) * 2^EV_100
	//        = 1.2 * 2^EV
	// Reference: http://en.wikipedia.org/wiki/Film_speed
	float maxLuminance = 1.2 * pow(2.0, EV100);
	return 1.0 / maxLuminance;
}

float computeExposureFromAvgLuminance(float avgLuminance)
{
	//float EV100 = computeEV100(16.0, 0.01, 100);
	float EV100 = computeEV100FromAvgLuminance(avgLuminance);
	EV100 -= g_PushConsts.exposureCompensation;
	float exposure = clamp(convertEV100ToExposure(EV100), g_PushConsts.exposureMin, g_PushConsts.exposureMax);
	return g_PushConsts.fixExposureToMax != 0 ? g_PushConsts.exposureMax : exposure;
}

[numthreads(LOCAL_SIZE_X, 1, 1)]
void main(uint3 groupThreadID : SV_GroupThreadID)
{
	// fill local histogram
	{
		ByteAddressBuffer inputBuffer = g_ByteAdressBuffers[g_PushConsts.inputHistogramBufferIndex];
		for (uint i = groupThreadID.x; i < LUMINANCE_HISTOGRAM_SIZE; i += LOCAL_SIZE_X)
		{
			s_localHistogram[i] = inputBuffer.Load(i << 2);
		}
	}
	
	GroupMemoryBarrierWithGroupSync();
	
	if (groupThreadID.x == 0)
	{
		float avgLuminance = 0.0;
		uint numProcessedPixels = 0;
		const uint lowerBound = g_PushConsts.lowerBound;
		const uint upperBound = g_PushConsts.upperBound;

		for (uint i = 0; i < LUMINANCE_HISTOGRAM_SIZE; ++i)
		{
			uint numPixels = s_localHistogram[i];
			
			uint tmpNumProcessedPixels = numProcessedPixels + numPixels;
			
			// subtract lower bound if it intersects this bucket
			numPixels -= min(lowerBound - min(numProcessedPixels, lowerBound), numPixels);
			// subtract upper bound if it intersects this bucket
			numPixels -= min(tmpNumProcessedPixels - min(upperBound, tmpNumProcessedPixels), numPixels);
			
			// get luma from bucket index
			//float luma = float(i) + 0.5;
			//luma *= (1.0 / 128.0);
			//luma = exp(luma);
			//luma -= 1.0;
			
			float luma = float(i) * (1.0 / (LUMINANCE_HISTOGRAM_SIZE - 1));
			luma = (luma - g_PushConsts.bias) * g_PushConsts.invScale;
			luma = exp2(luma);
			
			numProcessedPixels = tmpNumProcessedPixels;
			
			avgLuminance += numPixels * luma;
			
			if (numProcessedPixels >= upperBound)
			{
				break;
			}
		}
		
		avgLuminance /= max(upperBound - lowerBound, 1);
		
		RWByteAddressBuffer exposureBuffer = g_RWByteAddressBuffers[g_PushConsts.resultExposureBufferIndex];
		const float3 prevExposureBufferData = asfloat(exposureBuffer.Load3(0)); // x : cur exposure | y : previous to current exposure | z : luminance
		const float previousExposure = prevExposureBufferData[0];
		const float previousLum = prevExposureBufferData[2];
		
		const bool adaptUpwards = avgLuminance >= previousLum;
	
		// Adapt the luminance using Pattanaik's technique
		const float precomputedTerm = adaptUpwards ? g_PushConsts.precomputedTermUp : g_PushConsts.precomputedTermDown;
		const float curLum = previousLum + (avgLuminance - previousLum) * precomputedTerm;

		const float curExposure = computeExposureFromAvgLuminance(curLum);
		const float previousToCurrent = curExposure / previousExposure;
		
		exposureBuffer.Store3(0, asuint(float3(curExposure, previousToCurrent, curLum)));
	}
}