#ifndef DITHER_H
#define DITHER_H

// n must be normalized in [0..1] (e.g. texture coordinates)
float triangleNoise(float2 n)
{
    // triangle noise, in [-1.0..1.0[ range
    n  = frac(n * float2(5.3987, 5.4421));
    n += dot(n.yx, n.xy + float2(21.5351, 14.3137));

    float xy = n.x * n.y;
    // compute in [0..2[ and remap to [-1.0..1.0[
    return frac(xy * 95.4307) + frac(xy * 75.04961) - 1.0;
}

float3 ditherTriangleNoise(float3 rgb, float2 uv, float time)
{
    // Gjøl 2016, "Banding in Games: A Noisy Rant"
    uv += 0.07 * frac(time);

    float noise = triangleNoise(uv);

    // noise is in [-1..1[

    return rgb + (noise / 255.0);
}

#endif // DITHER_H