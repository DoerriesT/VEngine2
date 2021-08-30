#pragma once
#include "LoadedModel.h"

namespace WavefrontOBJLoader
{
	bool loadModel(const char *filepath, bool mergeByMaterial, bool invertTexcoordY, bool importMeshes, bool importSkeletons, bool importAnimations, LoadedModel &model);
};