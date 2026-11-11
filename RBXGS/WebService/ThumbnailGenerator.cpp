#include "ThumbnailGenerator.h"
#include "util/standardout.h"

// Registro de reflexao: nomes dos parametros ainda nao confirmados (TODO)
// static RBX::Reflection::BoundFuncDesc<ThumbnailGenerator, std::string(std::string, int, int, bool), 4> func_Click(&ThumbnailGenerator::click, "Click", ...);
// static RBX::Reflection::BoundFuncDesc<ThumbnailGenerator, std::string(std::string, std::string, int, int), 4> func_ClickTexture(&ThumbnailGenerator::clickTexture, "ClickTexture", ...);

const char* const sThumbnailGenerator = "ThumbnailGenerator";

ThumbnailGenerator::ThumbnailGenerator()
{
	// exe: 0x10006460 (so chama o construtor base e grava as vtables)
}

std::string ThumbnailGenerator::click(std::string fileType, int width, int height, bool hideSky)
{
	RBX::StandardOut::singleton()->print(RBX::MESSAGE_INFO, "ThumbnailGenerator::click(%s, %d, %d, %s)", fileType.c_str(), width, height, hideSky ? "true" : "false");
	// TODO: resto da funcao (chama doClick, devolve a imagem, loga "ThumbnailGenerator::click() success")
	return std::string();
}

std::string ThumbnailGenerator::clickTexture(std::string, std::string, int, int)
{
	// TODO: ainda nao desmontado alem do log inicial
	return std::string();
}

void ThumbnailGenerator::doClick(std::string fileType, int width, int height, bool hideSky, G3D::BinaryOutput* output)
{
	// TODO: ver notas do desmontado (OSContext, RenderDevice, View, Lighting::setClearColor, Workspace::setImageServerView)
}
