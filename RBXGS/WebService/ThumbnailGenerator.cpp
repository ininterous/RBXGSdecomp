#include "ThumbnailGenerator.h"
#include "util/standardout.h"
// FIXME: ainda nao existem no repo: GuiDrawImage, ContentProvider, View,
// Workspace::setImageServerView, Lighting::setClearColor.

// OSContext NAO e uma classe separada: o pdb mostra que ela e definida dentro do
// ThumbnailGenerator.obj (junto com StaticMutex<OSContext>). E uma janela G3D "falsa"
// que usa o OSMesa (OpenGL por software, sem janela) pra desenhar fora da tela.
class OSContext : public G3D::GWindow
{
	static OSContext* current;   // exe: 0x10302EB0
	OSMesaContext context;       // +0x1c
	G3D::GImage image;           // +0x20 (buffer onde o OSMesa desenha)

	OSContext(bool alpha);                    // exe: 0x100064B0 (so pro contexto compartilhado)
	static OSMesaContext shared(bool alpha);  // exe: 0x10006580

public:
	OSContext(int width, int height, bool alpha); // exe: 0x10006640
	virtual ~OSContext();                         // exe: 0x10006720

	virtual void getSettings(G3D::GWindow::Settings& settings) const; // exe: 0x10006840
	virtual int width() const;                                        // exe: 0x100068B0
	virtual int height() const;                                       // exe: 0x100068C0
	virtual bool inputCapture() const;                                // exe: 0x10006960
	virtual void reallyMakeCurrent() const;                           // exe: 0x100067B0
	// FIXME: os outros virtuais (setInputCapture, mouseVisible, setMouseVisible, joystickName,
	// numJoysticks, setCaption, setGammaRamp...) sao stubs vazios que o compilador juntou num
	// so (0x10006900). Conferir o retorno de cada um.
};

OSContext* OSContext::current = NULL;

// exe: 0x100064B0 (cria o contexto "pai" de que os outros dividem texturas/listas)
OSContext::OSContext(bool alpha)
{
	context = OSMesaCreateContext(alpha ? GL_RGBA : GL_RGB, NULL);
	if (context == NULL)
		throw std::runtime_error("shared OSMesaCreateContext failed");
}

// exe: 0x10006580 (duas variaveis estaticas locais, uma com alpha e outra sem)
OSMesaContext OSContext::shared(bool alpha)
{
	if (alpha)
	{
		static OSContext sharedAlpha(true);
		return sharedAlpha.context;
	}
	else
	{
		static OSContext sharedNoAlpha(false);
		return sharedNoAlpha.context;
	}
}

// exe: 0x10006640
OSContext::OSContext(int width, int height, bool alpha)
	: image(width, height, alpha ? 4 : 3)
{
	// OSMesa: GL_RGBA (0x1908) com alpha, GL_RGB (0x1907) sem
	context = OSMesaCreateContext(alpha ? GL_RGBA : GL_RGB, shared(alpha));
	if (context == NULL)
		throw std::runtime_error("OSMesaCreateContext failed");
}

// exe: 0x10006720
OSContext::~OSContext()
{
	OSMesaDestroyContext(context);
	if (current == this)
		current = NULL;
}

// exe: 0x10006840
void OSContext::getSettings(G3D::GWindow::Settings& settings) const
{
	settings = G3D::GWindow::Settings();
}

// exe: 0x100068B0 / 0x100068C0 (le direto do GImage)
int OSContext::width() const { return image.width; }
int OSContext::height() const { return image.height; }

// exe: 0x10006960
bool OSContext::inputCapture() const { return false; }

// exe: 0x100067B0 - liga o buffer da GImage ao OSMesa
void OSContext::reallyMakeCurrent() const
{
	if (!OSMesaMakeCurrent(context, image.byte(), GL_UNSIGNED_BYTE, image.width, image.height))
		throw std::runtime_error("OSMesaMakeCurrent failed");
}

// exe: 0x10006A30 (funcao estatica local do arquivo, sem argumentos)
// Fica esperando o ContentProvider terminar de baixar tudo antes de tirar a foto.
static void waitForContent()
{
	while (!RBX::ContentProvider::singleton().isRequestQueueEmpty())
	{
		boost::xtime xt;
		boost::xtime_get(&xt, boost::TIME_UTC);
		boost::thread::sleep(xt);
	}
}

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
	// Entre as duas, espera o conteudo (texturas, meshes) terminar de carregar.
	doClick(fileType, width, height, hideSky, NULL);
	waitForContent();
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
	boost::mutex::scoped_lock lock(StaticMutex<OSContext>::get()); // FIXME: nome exato do mutex estatico
	if (OSContext::current != &context) { /* makeCurrent virtual (vtable +0x98) */ OSContext::current = &context; }
	RBX::Render::Mesh::makeVAR();
	G3D::RenderDevice renderDevice;
	renderDevice.init(&context, NULL);

	// View (0x58 bytes) criada a partir do DataModel pai desta instancia; guardada num scoped_ptr
	// FIXME: boost::scoped_ptr<RBX::View::View> view(new RBX::View::View(shared_from(dataModel)));
	//        view->(virtual +0x10)(...); view->(virtual +4)(&renderDevice);
	// glMatrixMode(GL_MODELVIEW) chamado antes e depois do render

	// FIXME: inverte a imagem na vertical (flipRGBAVertical se hasAlpha, senao flipRGBVertical)
	// FIXME: se brilho/gamma != 1.0, aplica tabela de 256 entradas nos pixels

	if (output)
		; // image.encode(format, *output);
	else if (hideSky)
		RBX::ServiceProvider::find<RBX::Workspace>(this)->setImageServerView(G3D::Rect2D::xywh(0, 0, (float)width, (float)height));

	renderDevice.cleanup();
}
