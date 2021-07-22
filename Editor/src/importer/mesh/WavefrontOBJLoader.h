#pragma once
#include "ImportedModel.h"

	namespace WavefrontOBJLoader
	{
		bool loadModel(const char *filepath, bool mergeByMaterial, bool invertTexcoordY, ImportedModel &model);
	};