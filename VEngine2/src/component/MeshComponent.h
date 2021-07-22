#pragma once
#include "Handles.h"
#include "asset/MeshAsset.h"
#include "reflection/TypeInfo.h"

class Reflection;

struct MeshComponent
{
	Asset<MeshAssetData> m_mesh;

	static void reflect(Reflection &refl) noexcept;
};
DEFINE_TYPE_INFO(MeshComponent, "B934A3FB-00D8-4767-9988-40996DF21099")