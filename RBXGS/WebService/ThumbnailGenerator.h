#pragma once
#include "v8tree/Instance.h"
#include <string>

namespace G3D
{
	class BinaryOutput;
}

// Original: c:\Roblox\Releases\2007.11.14\RBXGS\WebService\ThumbnailGenerator.h
// Classe no namespace GLOBAL (simbolo ??0ThumbnailGenerator@@QAE@XZ)
extern const char* const sThumbnailGenerator;

class ThumbnailGenerator : public RBX::DescribedCreatable<ThumbnailGenerator, RBX::Instance, &sThumbnailGenerator>
{
public:
	ThumbnailGenerator();

	// exe: 0x10007150
	std::string click(std::string fileType, int width, int height, bool hideSky);
	// exe: 0x10006AD0
	std::string clickTexture(std::string textureId, std::string fileType, int width, int height);

private:
	// exe: 0x10007450 (o exe passa "this" na pilha; o pdb diz thiscall - conferir no objdiff)
	void doClick(std::string fileType, int width, int height, bool hideSky, G3D::BinaryOutput* output);
};
