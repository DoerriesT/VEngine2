#ifndef SRGB_H
#define SRGB_H

float approximateSRGBToLinear(in float sRGBCol)
{
	return pow(sRGBCol, 2.2f);
}

float3 approximateSRGBToLinear(in float3 sRGBCol)
{
	return pow(sRGBCol, 2.2f);
}

float4 approximateSRGBToLinear(in float4 sRGBCol)
{
	return pow(sRGBCol, 2.2f);
}

float approximateLinearToSRGB(in float linearCol)
{
	return pow(linearCol, 1.0f / 2.2f);
}

float3 approximateLinearToSRGB(in float3 linearCol)
{
	return pow(linearCol, 1.0f / 2.2f);
}

float4 approximateLinearToSRGB(in float4 linearCol)
{
	return pow(linearCol, 1.0f / 2.2f);
}

float accurateSRGBToLinear(in float sRGBCol)
{
	float linearRGBLo = sRGBCol * (1.0f / 12.92f);
	float linearRGBHi = pow((sRGBCol + 0.055f) * (1.0f / 1.055f), 2.4f);
	float linearRGB = (sRGBCol <= 0.04045f) ? linearRGBLo : linearRGBHi;
	return linearRGB;
}

float3 accurateSRGBToLinear(in float3 sRGBCol)
{
	float3 linearRGBLo = sRGBCol * (1.0f / 12.92f);
	float3 linearRGBHi = pow((sRGBCol + 0.055f) * (1.0f / 1.055f), 2.4f);
	float3 linearRGB = (sRGBCol <= 0.04045f) ? linearRGBLo : linearRGBHi;
	return linearRGB;
}

float4 accurateSRGBToLinear(in float4 sRGBCol)
{
	float4 linearRGBLo = sRGBCol * (1.0f / 12.92f);
	float4 linearRGBHi = pow((sRGBCol + 0.055f) * (1.0f / 1.055f), 2.4f);
	float4 linearRGB = (sRGBCol <= 0.04045f) ? linearRGBLo : linearRGBHi;
	return linearRGB;
}

float accurateLinearToSRGB(in float linearCol)
{
	float sRGBLo = linearCol * 12.92f;
	float sRGBHi = (pow(abs(linearCol), 1.0f / 2.4f) * 1.055f) - 0.055f;
	float sRGB = (linearCol <= 0.0031308f) ? sRGBLo : sRGBHi;
	return sRGB;
}

float3 accurateLinearToSRGB(in float3 linearCol)
{
	float3 sRGBLo = linearCol * 12.92f;
	float3 sRGBHi = (pow(abs(linearCol), 1.0f / 2.4f) * 1.055f) - 0.055f;
	float3 sRGB = (linearCol <= 0.0031308f) ? sRGBLo : sRGBHi;
	return sRGB;
}

float4 accurateLinearToSRGB(in float4 linearCol)
{
	float4 sRGBLo = linearCol * 12.92f;
	float4 sRGBHi = (pow(abs(linearCol), 1.0f / 2.4f) * 1.055f) - 0.055f;
	float4 sRGB = (linearCol <= 0.0031308f) ? sRGBLo : sRGBHi;
	return sRGB;
}

#endif // SRGB_H