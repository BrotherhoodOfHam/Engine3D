/*
	Render Module header

	The rendering module is responsible for controlling the rendering pipeline and it's resources
*/

#pragma once

#include <tsgraphics/abi.h>

#include <tsconfig.h>
#include <tscore/system/memory.h>
#include <tscore/filesystem/path.h>
#include <tscore/maths.h>
#include <tscore/system/thread.h>

#include <tsgraphics/api/renderapi.h>
#include <tsgraphics/api/rendercommon.h>

#include <tsgraphics/shadermanager.h>
#include <tsgraphics/texturemanager.h>

#include "indexbuffer.h"
#include "vertexbuffer.h"
#include "uniformbuffer.h"

/////////////////////////////////////////////////////////////////////////////////////////////////

namespace ts
{
	enum ERenderApiID
	{
		eRenderApiNull	= 0,
		eRenderApiD3D11 = 1
	};

	enum EDisplayMode
	{
		eDisplayUnknown	   = 0,
		eDisplayWindowed   = 1,
		eDisplayBorderless = 2,
		eDisplayFullscreen = 3
	};

	struct SGraphicsSystemConfig
	{
		//Handle to display (application window)
		intptr windowHandle = 0;

		//Display dimensions
		uint32 width = 0;
		uint32 height = 0;
		SMultisampling multisampling;
		EDisplayMode displaymode = eDisplayWindowed;

		//Graphics API id
		ERenderApiID apiEnum = ERenderApiID::eRenderApiNull;

		//Root asset loading path for textures/shaders/models
		Path rootpath;
	};

	struct IRender;
	
	class GraphicsSystem
	{
	private:

		IRender* m_api = nullptr;
		IRenderContext* m_context = nullptr;

		CTextureManager m_textureManager;
		CShaderManager m_shaderManager;

		int TSGRAPHICS_API loadApi(ERenderApiID id);
		int TSGRAPHICS_API unloadApi();

		SGraphicsSystemConfig m_config;

	public:

		TSGRAPHICS_API GraphicsSystem(const SGraphicsSystemConfig&);
		TSGRAPHICS_API ~GraphicsSystem();

		GraphicsSystem(const GraphicsSystem&) = delete;
		GraphicsSystem& operator=(const GraphicsSystem&) = delete;
		
		CTextureManager& getTextureManager() { return m_textureManager; }
		CShaderManager& getShaderManager() { return m_shaderManager; }
		IRender* const getApi() const { return m_api; }
		IRenderContext* const getContext() const { return m_context; }

		TSGRAPHICS_API void setDisplayConfiguration(EDisplayMode displaymode, uint32 width = 0, uint32 height = 0, SMultisampling sampling = SMultisampling(0));
		TSGRAPHICS_API void getConfiguration(SGraphicsSystemConfig& cfg) { cfg = m_config; }

		TSGRAPHICS_API void drawBegin(const Vector& vec);
		TSGRAPHICS_API void drawEnd();
	};
}

/////////////////////////////////////////////////////////////////////////////////////////////////