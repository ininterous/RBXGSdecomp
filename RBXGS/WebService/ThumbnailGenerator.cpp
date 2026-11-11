#include "ThumbnailGenerator.h"
#include "util/standardout.h"
// FIXME: ainda nao existem no repo: OSContext (global), GuiDrawImage, ContentProvider, View,
// Workspace::setImageServerView, Lighting::setClearColor. Decompilar depois.

// FIXME: nomes dos parametros do registro de reflexao ainda nao confirmados
static RBX::Reflection::BoundFuncDesc<ThumbnailGenerator, std::string(std::string, int, int, bool), 4> func_Click(&ThumbnailGenerator::click, "Click", "fileType", "width", "height", "hideSky", RBX::Reflection::FunctionDescriptor::NeedTrustedCaller);
static RBX::Reflection::BoundFuncDesc<ThumbnailGenerator, std::string(std::string, std::string, int, int), 4> func_ClickTexture(&ThumbnailGenerator::clickTexture, "ClickTexture", "textureId", "fileType", "width", "height", RBX::Reflection::FunctionDescriptor::NeedTrustedCaller);

const char* const sThumbnailGenerator = "ThumbnailGenerator";

// exe: 0x10006460 (so chama o construtor base e grava as vtables)
ThumbnailGenerator::ThumbnailGenerator()
{
}

// exe: 0x10007150
std::string ThumbnailGenerator::click(std::string fileType, int width, int height, bool hideSky)
{
	RBX::StandardOut::singleton()->print(RBX::MESSAGE_INFO, "ThumbnailGenerator::click(%s, %d, %d, %s)", fileType.c_str(), width, height, hideSky ? "true" : "false");

	G3D::BinaryOutput output;
	output.setEndian(G3D::G3D_LITTLE_ENDIAN);

	// A primeira chamada nao grava nada (output = NULL), a segunda grava na BinaryOutput.
	// Entre as duas o exe chama uma funcao sem argumentos em 0x10006A30 (FIXME: nao identificada)
	doClick(fileType, width, height, hideSky, NULL);
	// FIXME: sub_10006A30();
	doClick(fileType, width, height, hideSky, &output);

	std::string data;
	data.resize(output.getLength());
	memcpy(&data[0], output.getCArray(), output.getLength());

	std::stringstream ss(std::ios::in | std::ios::out);
	ss << data; // FIXME: o exe usa um put<> com iteradores; conferir se bate
	RBX::StandardOut::singleton()->print(RBX::MESSAGE_INFO, "ThumbnailGenerator::click() success");
	return ss.str();
}

// exe: 0x10006AD0
std::string ThumbnailGenerator::clickTexture(std::string textureId, std::string fileType, int width, int height)
{
	RBX::StandardOut::singleton()->print(RBX::MESSAGE_INFO, "ThumbnailGenerator::clickTexture(%s, %s, %d, %d)", textureId.c_str(), fileType.c_str(), width, height);

	// Pre-carrega a textura (o resultado do getContent e descartado)
	RBX::ContentId contentId(textureId);
	RBX::ContentProvider::singleton().getContent(contentId);

	G3D::BinaryOutput output;
	output.setEndian(G3D::G3D_LITTLE_ENDIAN);
	G3D::GImage::Format format = G3D::GImage::stringToFormat(fileType);

	OSContext context(width, height, true); // FIXME: ordem dos argumentos
	// ... lock do mutex estatico do OSContext, Mesh::makeVAR ...
	G3D::RenderDevice renderDevice;
	renderDevice.init(&context, NULL);
	renderDevice.pushState();
	renderDevice.push2D();
	renderDevice.setProjectionMatrix(G3D::Matrix4::orthogonalProjection(0, (float)width, (float)height, 0, 0 /*FIXME*/, 1));

	G3D::TextureManager textureManager(0xa00000);
	RBX::GuiDrawImage image;
	// FIXME: setImage(adorn, TextureId(textureId)) e, se ok, render2d(Rect(0,0,width,height))

	// FIXME: GImage a partir do framebuffer, depois:
	// image.encode(format, output);
	renderDevice.pop2D();
	renderDevice.popState();
	renderDevice.cleanup();

	std::string data;
	data.resize(output.getLength());
	memcpy(&data[0], output.getCArray(), output.getLength());
	std::stringstream ss(std::ios::in | std::ios::out);
	ss << data;
	RBX::StandardOut::singleton()->print(RBX::MESSAGE_INFO, "ThumbnailGenerator::click() success");
	return ss.str();
}

// exe: 0x10007450
void ThumbnailGenerator::doClick(std::string fileType, int width, int height, bool hideSky, G3D::BinaryOutput* output)
{
	G3D::GImage::Format format = G3D::GImage::stringToFormat(fileType);
	bool hasAlpha = false;

	if (hideSky)
	{
		RBX::Lighting* lighting = RBX::ServiceProvider::create<RBX::Lighting>(this);
		// FIXME: lighting + 0x200 = false (provavelmente "sky ligado")
		if (format == G3D::GImage::PNG) // formato 5: fundo transparente
		{
			lighting->setClearColor(G3D::Color4(0, 0, 0, 0));
			hasAlpha = true;
		}
		else
			lighting->setClearColor(G3D::Color4(1, 1, 1, 1));

		RBX::ServiceProvider::find<RBX::Workspace>(this)->setImageServerView(G3D::Rect2D::xywh(0, 0, (float)width, (float)height));
	}

	OSContext context(width, height, hasAlpha);
	// ... lock do mutex estatico do OSContext, Mesh::makeVAR ...
	G3D::RenderDevice renderDevice;
	renderDevice.init(&context, NULL);

	// View (0x58 bytes) criada a partir do DataModel pai desta instancia
	// FIXME: RBX::View* view = new RBX::View(shared_from(dataModel)); view->(virtual +0x10)(...); view->(virtual +4)(&renderDevice);
	// glMatrixMode(GL_MODELVIEW) chamado antes e depois do render

	// FIXME: inverte a imagem na vertical (flipRGBAVertical se hasAlpha, senao flipRGBVertical)
	// FIXME: se brilho/gamma != 1.0, aplica tabela de 256 entradas nos pixels

	if (output)
		; // image.encode(format, *output);
	else if (hideSky)
		RBX::ServiceProvider::find<RBX::Workspace>(this)->setImageServerView(G3D::Rect2D::xywh(0, 0, (float)width, (float)height));

	renderDevice.cleanup();
}
