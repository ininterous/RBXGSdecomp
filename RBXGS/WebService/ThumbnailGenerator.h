#pragma once
#include "v8tree/Instance.h"
#include <string>

namespace G3D
{
	class BinaryOutput;
}

// Classe no namespace GLOBAL (simbolo ??0ThumbnailGenerator@@QAE@XZ), nao em RBX::
extern const char* const sThumbnailGenerator;

class ThumbnailGenerator : public RBX::DescribedCreatable<ThumbnailGenerator, RBX::Instance, &sThumbnailGenerator>
{
public:
	ThumbnailGenerator();

	// exe: 0x10007150
	std::string click(std::string fileType, int width, int height, bool hideSky);
	// exe: 0x10006AD0
	std::string clickTexture(std::string, std::string, int, int);

private:
	// exe: 0x10007450
	void doClick(std::string fileType, int width, int height, bool hideSky, G3D::BinaryOutput* output);
};
