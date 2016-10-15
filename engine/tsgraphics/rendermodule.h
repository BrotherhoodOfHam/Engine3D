/*
	Render Module header

	The rendering module is responsible for controlling the rendering pipeline and it's resources
*/

#pragma once

#include <tsconfig.h>
#include <tscore/system/memory.h>
#include <tscore/filesystem/path.h>
#include <tscore/maths.h>
#include <tscore/system/thread.h>

#include "rendercommon.h"
#include "renderapi.h"

#include "shader/shadermanager.h"
#include "texturemanager.h"

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

	struct SRenderModuleConfiguration
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

	class IRenderApi;
	
	class CRenderModule
	{
	private:

		UniquePtr<IRenderApi> m_api;

		CTextureManager m_textureManager;
		CShaderManager m_shaderManager;

		bool loadApi(ERenderApiID id);

		SRenderModuleConfiguration m_config;

	public:

		CRenderModule(const SRenderModuleConfiguration&);
		~CRenderModule();

		CRenderModule(const CRenderModule&) = delete;
		CRenderModule& operator=(const CRenderModule&) = delete;
		
		CTextureManager& getTextureManager() { return m_textureManager; }
		CShaderManager& getShaderManager() { return m_shaderManager; }
		IRenderApi* const getApi() const { return m_api.get(); }

		void setDisplayConfiguration(EDisplayMode displaymode, uint32 width = 0, uint32 height = 0, SMultisampling sampling = SMultisampling(0));
		void getConfiguration(SRenderModuleConfiguration& cfg) { cfg = m_config; }

		void drawBegin(const Vector& vec);
		void drawEnd();
	};
}

/////////////////////////////////////////////////////////////////////////////////////////////////