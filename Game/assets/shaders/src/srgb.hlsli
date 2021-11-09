#ifndef SRGB_H
#define SRGB_H

float3 approximateSRGBToLinear(in float3 sRGBCol)
{
	return pow(sRGBCol, 2.2);
}

float4 approximateSRGBToLinear(in float4 sRGBCol)
{
	return pow(sRGBCol, 2.2);
}

float3 approximateLinearToSRGB(in float3 linearCol)
{
	return pow(linearCol, 1.0 / 2.2);
}

float4 approximateLinearToSRGB(in float4 linearCol)
{
	return pow(linearCol, 1.0 / 2.2);
}

float3 accurateSRGBToLinear(in float3 sRGBCol)
{
	float3 linearRGBLo = sRGBCol * (1.0 / 12.92);
	float3 linearRGBHi = pow((sRGBCol + 0.055) * (1.0 / 1.055), 2.4);
	float3 linearRGB = (sRGBCol <= 0.04045) ? linearRGBLo : linearRGBHi;
	return linearRGB;
}

float4 accurateSRGBToLinear(in float4 sRGBCol)
{
	float4 linearRGBLo = sRGBCol * (1.0 / 12.92);
	float4 linearRGBHi = pow((sRGBCol + 0.055) * (1.0 / 1.055), 2.4);
	float4 linearRGB = (sRGBCol <= 0.04045) ? linearRGBLo : linearRGBHi;
	return linearRGB;
}

float3 accurateLinearToSRGB(in float3 linearCol)
{
	float3 sRGBLo = linearCol * 12.92;
	float3 sRGBHi = (pow(abs(linearCol), 1.0/2.4) * 1.055) - 0.055;
	float3 sRGB = (linearCol <= 0.0031308) ? sRGBLo : sRGBHi;
	return sRGB;
}

float4 accurateLinearToSRGB(in float4 linearCol)
{
	float4 sRGBLo = linearCol * 12.92;
	float4 sRGBHi = (pow(abs(linearCol), 1.0/2.4) * 1.055) - 0.055;
	float4 sRGB = (linearCol <= 0.0031308) ? sRGBLo : sRGBHi;
	return sRGB;
}

#endif // SRGB_H