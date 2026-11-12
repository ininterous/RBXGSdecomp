#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include "ThumbnailGenerator.h"
#include <sstream>
#include <stdexcept>
#include <string.h>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/xtime.hpp>
#include <G3D/GImage.h>
#include <G3D/BinaryOutput.h>
#include <G3D/Matrix4.h>
#include <G3D/Color4.h>
#include <G3D/Rect2D.h>
#include <GLG3D/GWindow.h>
#include <GLG3D/RenderDevice.h>
#include <GLG3D/TextureManager.h>
#include <GL/osmesa.h>
#include <iterator>
#include "util/base64.hpp"
#include "util/standardout.h"
#include "util/ContentProvider.h"
#include "gui/GuiDraw.h"
#include "util/TextureId.h"
#include "AppDraw/AdornG3D.h"
#include "v8datamodel/Lighting.h"
#include "v8datamodel/Workspace.h"

class OSContext : public G3D::GWindow
{
public:
	static OSContext* current;
	OSMesaContext context;
	G3D::GImage image;

	OSContext(bool alpha);
	static OSMesaContext shared(bool alpha);

public:
	OSContext(int width, int height, bool alpha);
	virtual ~OSContext();

	virtual void getSettings(G3D::GWindow::Settings& settings) const;
	virtual int width() const;
	virtual int height() const;
	virtual bool inputCapture() const;
	virtual void reallyMakeCurrent() const;

	virtual G3D::Rect2D dimensions() const { return G3D::Rect2D::xywh(0, 0, (float)image.width, (float)image.height); }
	virtual void setDimensions(const G3D::Rect2D&) {}
	virtual void setPosition(int, int) {}
	virtual bool hasFocus() const { return false; }
	virtual std::string getAPIVersion() const { return ""; }
	virtual std::string getAPIName() const { return ""; }
	virtual void setGammaRamp(const G3D::Array<G3D::uint16>&) {}
	virtual void setCaption(const std::string&) {}
	virtual int numJoysticks() const { return 0; }
	virtual std::string joystickName(unsigned int) { return ""; }
	virtual std::string caption() { return ""; }
	virtual void swapGLBuffers() {}
	virtual void notifyResize(int, int) {}
	virtual void setRelativeMousePosition(double, double) {}
	virtual void setRelativeMousePosition(const G3D::Vector2&) {}
	virtual void getRelativeMouseState(G3D::Vector2&, G3D::uint8&) const {}
	virtual void getRelativeMouseState(int&, int&, G3D::uint8&) const {}
	virtual void getRelativeMouseState(double&, double&, G3D::uint8&) const {}
	virtual void getJoystickState(unsigned int, G3D::Array<float>&, G3D::Array<bool>&) {}
	virtual void setInputCapture(bool) {}
	virtual void setMouseVisible(bool) {}
	virtual bool mouseVisible() const { return false; }

};

template<class T> struct StaticMutex
{
	static boost::mutex& get() { static boost::mutex m; return m; }
};

OSContext* OSContext::current = NULL;

OSContext::OSContext(bool alpha)
{
	context = OSMesaCreateContext(alpha ? GL_RGBA : GL_RGB, NULL);
	if (context == NULL)
		throw std::string("shared OSMesaCreateContext failed");
}

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

OSContext::OSContext(int width, int height, bool alpha)
	: image(width, height, alpha ? 4 : 3)
{

	context = OSMesaCreateContext(alpha ? GL_RGBA : GL_RGB, shared(alpha));
	if (context == NULL)
		throw std::string("OSMesaCreateContext failed");
}

OSContext::~OSContext()
{
	OSMesaDestroyContext(context);
	if (current == this)
		current = NULL;
}

void OSContext::getSettings(G3D::GWindow::Settings& settings) const
{
	settings = G3D::GWindow::Settings();
}

int OSContext::width() const { return image.width; }
int OSContext::height() const { return image.height; }

bool OSContext::inputCapture() const { return false; }

void OSContext::reallyMakeCurrent() const
{
	if (!OSMesaMakeCurrent(context, const_cast<void*>(static_cast<const void*>(image.byte())), GL_UNSIGNED_BYTE, image.width, image.height))
		throw std::string("OSMesaMakeCurrent failed");
}

static std::string toBase64(const std::string& raw)
{
	std::stringstream in(raw, std::ios::in | std::ios::out | std::ios::binary);
	std::stringstream out;
	std::istreambuf_iterator<char> from(in.rdbuf());
	std::istreambuf_iterator<char> to;
	std::ostreambuf_iterator<char> o(out.rdbuf());
	int state = 0;
	base64<char> encoder;
	encoder.put(from, to, o, state, base64<>::crlf());
	return out.str();
}

static void waitForContent()
{
	while (!RBX::ContentProvider::singleton().isRequestQueueEmpty())
	{
		boost::xtime xt;
		boost::xtime_get(&xt, boost::TIME_UTC);
		boost::thread::sleep(xt);
	}
}

static RBX::Reflection::BoundFuncDesc<ThumbnailGenerator, std::string(std::string, int, int, bool), 4> func_Click(&ThumbnailGenerator::click, "Click", "fileType", "width", "height", "hideSky", RBX::Reflection::FunctionDescriptor::NeedTrustedCaller);
static RBX::Reflection::BoundFuncDesc<ThumbnailGenerator, std::string(std::string, std::string, int, int), 4> func_ClickTexture(&ThumbnailGenerator::clickTexture, "ClickTexture", "textureId", "fileType", "width", "height", RBX::Reflection::FunctionDescriptor::NeedTrustedCaller);

const char* const sThumbnailGenerator = "ThumbnailGenerator";

ThumbnailGenerator::ThumbnailGenerator()
{
}

std::string ThumbnailGenerator::click(std::string fileType, int width, int height, bool hideSky)
{
	RBX::StandardOut::singleton()->print(RBX::MESSAGE_INFO, "ThumbnailGenerator::click(%s, %d, %d, %s)", fileType.c_str(), width, height, hideSky ? "true" : "false");

	G3D::BinaryOutput output;
	output.setEndian(G3D::G3D_LITTLE_ENDIAN);

	doClick(fileType, width, height, hideSky, NULL);
	waitForContent();
	doClick(fileType, width, height, hideSky, &output);

	std::string data;
	data.resize(output.getLength());
	memcpy(&data[0], output.getCArray(), output.getLength());

	std::string result = toBase64(data);
	RBX::StandardOut::singleton()->print(RBX::MESSAGE_INFO, "ThumbnailGenerator::click() success");
	return result;
}

class ThumbnailAdorn : public RBX::AdornG3D
{
public:
	ThumbnailAdorn(G3D::RenderDevice* rd) : RBX::AdornG3D(rd) {}
	virtual void prepareRenderPass() {}
	virtual void finishRenderPass() {}
};

std::string ThumbnailGenerator::clickTexture(std::string textureId, std::string fileType, int width, int height)
{
	RBX::StandardOut::singleton()->print(RBX::MESSAGE_INFO, "ThumbnailGenerator::clickTexture(%s, %s, %d, %d)", textureId.c_str(), fileType.c_str(), width, height);

	RBX::ContentId contentId(textureId);
	RBX::ContentProvider::singleton().getContent(contentId);

	G3D::BinaryOutput output;
	output.setEndian(G3D::G3D_LITTLE_ENDIAN);
	G3D::GImage::Format format = G3D::GImage::stringToFormat(fileType);

	OSContext context(width, height, true);
	boost::mutex::scoped_lock lock(StaticMutex<OSContext>::get());
	if (OSContext::current != &context)
	{
		context.reallyMakeCurrent();
		OSContext::current = &context;
	}

	G3D::RenderDevice renderDevice;
	renderDevice.init(&context, NULL);
	renderDevice.pushState();
	renderDevice.push2D();
	renderDevice.setProjectionMatrix(G3D::Matrix4::orthogonalProjection(0, (float)width, 0, (float)height, -1, 1));

	{
		ThumbnailAdorn adorn(&renderDevice);
		RBX::GuiDrawImage image;
		if (image.setImage(&adorn, RBX::TextureId(textureId)))
		{
			RBX::Rect rect(0, 0, (float)width, (float)height);
			image.render2d(&adorn, true, rect, RBX::Widget::NOTHING);
		}
	}

	context.image.encode(format, output);

	renderDevice.pop2D();
	renderDevice.popState();
	renderDevice.cleanup();

	std::string data;
	data.resize(output.getLength());
	memcpy(&data[0], output.getCArray(), output.getLength());
	std::string result = toBase64(data);
	RBX::StandardOut::singleton()->print(RBX::MESSAGE_INFO, "ThumbnailGenerator::click() success");
	return result;
}

void ThumbnailGenerator::doClick(std::string fileType, int width, int height, bool hideSky, G3D::BinaryOutput* output)
{
	G3D::GImage::Format format = G3D::GImage::stringToFormat(fileType);
	bool hasAlpha = false;

	if (hideSky)
	{
		RBX::Lighting* lighting = RBX::ServiceProvider::create<RBX::Lighting>(this);

		if (format == G3D::GImage::PNG)
		{
			lighting->setClearColor(G3D::Color4(0, 0, 0, 0));
			hasAlpha = true;
		}
		else
			lighting->setClearColor(G3D::Color4(1, 1, 1, 1));

		RBX::ServiceProvider::find<RBX::Workspace>(this)->setImageServerView(G3D::Rect2D::xywh(0, 0, (float)width, (float)height));
	}

	OSContext context(width, height, hasAlpha);
	boost::mutex::scoped_lock lock(StaticMutex<OSContext>::get());
	if (OSContext::current != &context)
	{
		context.reallyMakeCurrent();
		OSContext::current = &context;
	}

	G3D::RenderDevice renderDevice;
	renderDevice.init(&context, NULL);

	glMatrixMode(GL_MODELVIEW);

	glMatrixMode(GL_MODELVIEW);

	if (hasAlpha)
		G3D::GImage::flipRGBAVertical(context.image.byte(), (G3D::uint8*)context.image.byte(), context.image.width, context.image.height);
	else
		G3D::GImage::flipRGBVertical(context.image.byte(), (G3D::uint8*)context.image.byte(), context.image.width, context.image.height);

	double brightness = renderDevice.getLightSaturation();
	if (brightness != 1.0)
	{
		G3D::uint8 table[256];
		for (int i = 255; i >= 0; --i)
		{
			int v = (int)(i * brightness + 0.5);
			table[i] = (G3D::uint8)(v > 255 ? 255 : v);
		}
		int total = context.image.width * context.image.height * context.image.channels;
		int stride = hasAlpha ? 4 : 3;
		G3D::uint8* px = (G3D::uint8*)context.image.byte();
		for (int i = 0; i < total; i += stride)
			for (int c = 0; c < 3; ++c)
				px[i + c] = table[px[i + c]];
	}

	if (output)
		context.image.encode(format, *output);
	else if (hideSky)
		RBX::ServiceProvider::find<RBX::Workspace>(this)->setImageServerView(G3D::Rect2D::xywh(0, 0, (float)width, (float)height));

	renderDevice.cleanup();
}