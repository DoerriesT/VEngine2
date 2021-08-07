#pragma once
#include "ImportedModel.h"

namespace GLTFLoader
{
	bool loadModel(const char *filepath, bool mergeByMaterial, bool invertTexcoordY, bool importMeshes, bool importSkeletons, bool importAnimations, ImportedModel &model);
};