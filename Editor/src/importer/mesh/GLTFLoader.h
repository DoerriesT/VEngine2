#pragma once
#include "ImportedModel.h"

namespace GLTFLoader
{
	bool loadModel(const char *filepath, bool mergeByMaterial, bool invertTexcoordY, ImportedModel &model);
};