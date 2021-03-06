/*
	Application definition
*/

#include <iostream>
#include "application.h"

#include <tscore/strings.h>
#include <tscore/delegate.h>
#include <tscore/debug/assert.h>
#include <tscore/debug/log.h>
#include <tscore/filesystem/pathhelpers.h>

#include "helpers/geometry.h"
#include "helpers/appinfo.h"

#include "scene/camera.h"
#include "scene/modelimporter.h"

#include "ui/ui.h"
#include "ui/commandconsole.h"
#include "ui/debugmenu.h"

#include <tscore/system/info.h>

using namespace ts;
using namespace std;

#define VECTOR_OFFSET(idx) (uint32)(idx * sizeof(Vector))

/////////////////////////////////////////////////////////////////////////////////////////////////
//Ctor/dtor
/////////////////////////////////////////////////////////////////////////////////////////////////

Application::Application(CEngineEnv& env) :
	m_env(env)
{

}

Application::~Application() {}

/////////////////////////////////////////////////////////////////////////////////////////////////
//Window/Input event handlers
/////////////////////////////////////////////////////////////////////////////////////////////////

int Application::onWindowEvent(const SWindowEventArgs& args)
{
	if (args.eventcode == EWindowEvent::eEventResize)
	{
		//Recreate the depth target
		m_rebuildDepthTarget = true;
	}
	else if (args.eventcode == EWindowEvent::eEventKillfocus)
	{
		m_mouseHeld = false;
	}

	return 0;
}

int Application::onKeyDown(EKeyCode code)
{
	switch (code)
	{
	case EKeyCode::eKeyEsc:
		m_env.shutdown();
		break;
	case EKeyCode::eKeyY:
		m_simulation = !m_simulation;
		break;
	case EKeyCode::eKeyTab:
		m_showUI = !m_showUI;
		break;
	case EKeyCode::eKeyGrave:
		m_showConsole = !m_showConsole;
		break;
	}

	return 0;
}

inline string getMouseCodeName(EMouseButtons button)
{
	switch (button)
	{
	case(EMouseButtons::eMouseButtonLeft): { return "Mouse Left"; }
	case(EMouseButtons::eMouseButtonRight): { return "Mouse Right"; }
	case(EMouseButtons::eMouseButtonMiddle): { return "Mouse Middle"; }
	case(EMouseButtons::eMouseXbutton1): { return "Mouse 1"; }
	case(EMouseButtons::eMouseXbutton2): { return "Mouse 2"; }
	}

	return "";
}

int Application::onMouseDown(const SInputMouseEvent& args)
{
	if (args.buttons == EMouseButtons::eMouseButtonLeft)
		m_mouseHeld = true;

	return 0;
}

int Application::onMouseUp(const SInputMouseEvent& args)
{
	if (args.buttons == EMouseButtons::eMouseButtonLeft)
		m_mouseHeld = false;

	return 0;
}

int Application::onMouse(int16 dx, int16 dy)
{
	return 0;
}

int Application::onMouseScroll(const SInputMouseEvent& args)
{
	const float scrollMax = 20.0f;
	const float scrollMin = 2.0f;
	const float scrollInterval = 0.8f;

	float depth = m_scrollDepth;
	depth += (args.deltaScroll * scrollInterval);
	depth = min(depth, scrollMax);
	depth = max(depth, scrollMin);
	m_scrollDepth = depth;

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

struct SUniforms
{
	Matrix world;
	Matrix invWorld;
	Matrix view;
	Matrix invView;
	Matrix projection;
	Matrix invProjection;

	Vector lightPos;
	Vector lightColour;
	Vector globalAmbientColour;
	Vector eyePos;

	float nearplane;
	float farplane;

	float lightConstantAttenuation;
	float lightLinearAttenuation;
	float lightQuadraticAttenuation;

	void init()
	{
		invWorld = world.inverse();
		invView = view.inverse();
		invProjection = projection.inverse();

		Matrix::transpose(invWorld);
		Matrix::transpose(invView);
		Matrix::transpose(invProjection);
		Matrix::transpose(world);
		Matrix::transpose(view);
		Matrix::transpose(projection);
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////////
//Application event handlers
/////////////////////////////////////////////////////////////////////////////////////////////////
int Application::onInit()
{
	//Register event listeners
	m_env.getInputModule()->addEventListener(this);
	m_env.getWindow()->addEventListener(this);

	//Initialize UI
	m_ui.reset(new UISystem(
		m_env.getInputModule(),
		m_env.getRenderModule()
	));
	
	m_consoleMenu.reset(new UICommandConsole(this));
	m_debugMenu.reset(new UIDebugMenu(this));
	
	/////////////////////////////////////////////////////////////////////////////////////////////////
	//Console commands
	m_consoleMenu->setCommand("exit", [&](const SCommandConsoleCallbackArgs& args) { m_env.shutdown(); });
	m_consoleMenu->setCommand("listvars", [&](const SCommandConsoleCallbackArgs& args)
	{
		CVarTable::CVarArray array;
		m_env.getCVarTable()->getVarArray(array);
		for (auto& s : array)
		{
			tsinfo("% = %", s.name, s.value);
		}
	});

	m_consoleMenu->setCommand("setvar", [&](const SCommandConsoleCallbackArgs& args)
	{
		string commandargs = trim(args.params);
		size_t pos = commandargs.find_first_of(' ');

		if (pos == string::npos)
		{
			tswarn("a value must be specified");
		}
		else
		{
			string cvar(commandargs.substr(0, pos));
			string cval(commandargs.substr(pos));
			m_env.getCVarTable()->setVar(cvar.c_str(), cval.c_str());
		}
	});

	m_consoleMenu->setCommand("getvar", [&](const SCommandConsoleCallbackArgs& args)
	{
		string commandargs = trim(args.params);
		string val;
		if (m_env.getCVarTable()->isVar(commandargs))
		{
			m_env.getCVarTable()->getVarString(commandargs.c_str(), val);
			tsinfo(val);
		}
		else
		{
			tswarn("cvar '%' does not exist", commandargs);
		}
	});

	/////////////////////////////////////////////////////////////////////////////////////////////////

	//Set up scene camera
	m_camera.reset(new CCamera(m_env.getInputModule()));
	m_camera->setPosition(Vector(0, 1.0f, 0));

	//Print basic information
	printRepositoryInfo();
	printSystemInfo();

	/////////////////////////////////////////////////////////////////////////////////////////////////

	ERenderStatus status = eOk;

	auto api = m_env.getRenderModule()->getApi();

	api->createContext(&m_context);

	SRenderModuleConfiguration rendercfg;
	m_env.getRenderModule()->getConfiguration(rendercfg);

	/////////////////////////////////////////////////////////////////////////////////////////////////
	//Shaders
	/////////////////////////////////////////////////////////////////////////////////////////////////


	bool shader_debug = true;
	
	CShaderManager& shaderMng = m_env.getRenderModule()->getShaderManager();

	//Standard shader
	{
		tsassert((shaderMng.createShaderFromFile(m_standardVertexshader, "standard", "VS", EShaderStage::eShaderStageVertex)));
		tsassert((shaderMng.createShaderFromFile(m_standardPixelshader, "standard", "PS", EShaderStage::eShaderStagePixel)));

		//Shader input desc
		vector<SShaderInputDescriptor> inputdescriptor;
		buildVertexInputDescriptor(inputdescriptor, 0xffffffff);
		tsassert(!api->createShaderInputDescriptor(m_vertexInput, shaderMng.getShaderProxy(m_standardVertexshader), &inputdescriptor[0], (uint32)inputdescriptor.size()));
	}
	
	//Light source shader
	{
		tsassert((shaderMng.createShaderFromFile(m_lightVertexShader, "lightsource", "VS", EShaderStage::eShaderStageVertex)));
		tsassert((shaderMng.createShaderFromFile(m_lightPixelShader, "lightsource", "PS", EShaderStage::eShaderStagePixel)));
		
		//Shader input desc
		vector<SShaderInputDescriptor> inputdescriptor;
		buildVertexInputDescriptor(inputdescriptor, eModelVertexAttributePosition | eModelVertexAttributeColour);
		tsassert(!api->createShaderInputDescriptor(m_vertexInputLight, shaderMng.getShaderProxy(m_lightVertexShader), &inputdescriptor[0], (uint32)inputdescriptor.size()));
	}

	//Shadow mapping shader
	{
		tsassert((shaderMng.createShaderFromFile(m_shadowVertexShader, "shadowmap", "VS", EShaderStage::eShaderStageVertex)));
		tsassert((shaderMng.createShaderFromFile(m_shadowPixelShader, "shadowmap", "PS", EShaderStage::eShaderStagePixel)));

		//Shader input desc
		vector<SShaderInputDescriptor> inputdescriptor;
		buildVertexInputDescriptor(inputdescriptor, eModelVertexAttributePosition);
		tsassert(!api->createShaderInputDescriptor(m_vertexInputShadow, shaderMng.getShaderProxy(m_shadowVertexShader), &inputdescriptor[0], (uint32)inputdescriptor.size()));
	}
	
	//Skybox shader
	{
		tsassert((shaderMng.createShaderFromFile(m_skyboxVertexShader, "skybox", "VS", EShaderStage::eShaderStageVertex)));
		tsassert((shaderMng.createShaderFromFile(m_skyboxPixelShader, "skybox", "PS", EShaderStage::eShaderStagePixel)));

		//No shader input descriptor required for the skybox
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////
	//Assets
	/////////////////////////////////////////////////////////////////////////////////////////////////
	
	Path modelfile(rendercfg.rootpath);
	
	string mdlstr;
	m_env.getCVarTable()->getVarString("model", mdlstr);
	modelfile.addDirectories(mdlstr.c_str());

	if (!isFile(modelfile))
	{
		tserror("Model path \"%\" is not a file");
		return 1;
	}
	
	Path spherefile(rendercfg.rootpath);
	spherefile.addDirectories("sphere.tsm");
	
	if (!isFile(spherefile))
	{
		tserror("Model path \"%\" is not a file");
		return 1;
	}

	//Skybox texture
	m_env.getRenderModule()->getTextureManager().loadTextureCube("skybox.png", m_skybox);

	m_sphere.reset(new CModel(m_env.getRenderModule(), spherefile));
	m_model.reset(new CModel(m_env.getRenderModule(), modelfile));

	m_materialBuffer = CUniformBuffer(m_env.getRenderModule(), SMaterial::SParams());
	m_sceneBuffer = CUniformBuffer(m_env.getRenderModule(), SUniforms());
	
	/////////////////////////////////////////////////////////////////////////////////////////////////

	//Texture sampler
	STextureSamplerDescriptor sampledesc;
	sampledesc.addressU = ETextureAddressMode::eTextureAddressWrap;
	sampledesc.addressV = ETextureAddressMode::eTextureAddressWrap;
	sampledesc.addressW = ETextureAddressMode::eTextureAddressWrap;
	sampledesc.filtering = eTextureFilterAnisotropic16x;

	status = api->createTextureSampler(m_texSampler, sampledesc);
	tsassert(!status);
	
	//Depth target view
	buildDepthTarget();

	/////////////////////////////////////////////////////////////////////////////////////////////////

	{
		//Shadow render target	
		STextureResourceDescriptor shadowdesc;
		shadowdesc.arraySize = 6;
		shadowdesc.depth = 0;
		shadowdesc.width = 1024;
		shadowdesc.height = 1024;
		shadowdesc.multisampling.count = 1;
		shadowdesc.texformat = eTextureFormatFloat2;
		shadowdesc.texmask = eTextureMaskShaderResource | eTextureMaskRenderTarget;
		shadowdesc.textype = eTypeTextureCube;
		shadowdesc.useMips = false;
		status = api->createResourceTexture(m_shadowCubeRsc, nullptr, shadowdesc);
		tsassert(!status);

		STextureViewDescriptor cubeviewdesc;
		cubeviewdesc.arrayCount = 1;
		cubeviewdesc.arrayIndex = 0;
		status = api->createViewTextureCube(m_shadowCube, m_shadowCubeRsc, cubeviewdesc);
		tsassert(!status);

		//Shadow depth target view
		ResourceProxy depthtargetrsc;
		STextureResourceDescriptor depthdesc;
		depthdesc.height = 1024;
		depthdesc.width = 1024;
		depthdesc.texformat = ETextureFormat::eTextureFormatDepth32;
		depthdesc.texmask = eTextureMaskDepthTarget;
		depthdesc.textype = eTypeTexture2D;
		depthdesc.useMips = false;
		depthdesc.multisampling.count = 1;
		status = api->createResourceTexture(depthtargetrsc, nullptr, depthdesc);
		tsassert(!status);

		STextureViewDescriptor depthviewdesc;
		depthviewdesc.arrayCount = 1;
		depthviewdesc.arrayIndex = 0;
		status = api->createViewDepthTarget(m_shadowDepthTarget, depthtargetrsc, depthviewdesc);
		tsassert(!status);
	}

	//Reset mouse held variable
	m_mouseHeld = false;
	m_scrollDepth = 5.0f;

	/////////////////////////////////////////////////////////////////////////////////////////////////

	//return success
	return 0;
}

void Application::onUpdate(double dt)
{
	SRenderModuleConfiguration rendercfg;
	m_env.getRenderModule()->getConfiguration(rendercfg);
	
	//CVar table
	auto table = m_env.getCVarTable();

	if (m_rebuildDepthTarget)
	{
		m_rebuildDepthTarget = !m_rebuildDepthTarget;
		buildDepthTarget();
	}

	//Update camera
	if (!ImGui::GetIO().WantTextInput)
	{
		m_camera->setAspectRatio((float)rendercfg.width / rendercfg.height);
		m_camera->update(dt);
	}

	float scale = 1.0f;
	table->getVarFloat("scale", scale);

	//Update displacement of light source
	if (m_simulation)
		m_pulsatance += (float)(((2.0 * Pi) / 5.0) * dt);
	Vector lightpos;
	lightpos.x() = 12.0f * sin(m_pulsatance = fmod(m_pulsatance, 2 * Pi));
	//lightpos.y() = 4.0f + 2.0f * sin(m_pulsatance * 2.0f);
	lightpos.y() = 2.0f;
	lightpos.z() = 0.0f;
	lightpos = lightpos / scale;

	Matrix projection = m_camera->getProjectionMatrix();
	Matrix view = m_camera->getViewMatrix();
	Vector position = m_camera->getPosition();

	//Check if mouse held and ui isn't capturing input
	if (m_mouseHeld && !ImGui::GetIO().WantCaptureMouse)
	{
		int16 mousePosX = 0;
		int16 mousePosY = 0;
		m_env.getInputModule()->getCursorPos(mousePosX, mousePosY);
		mousePosX = max(min(mousePosX, (uint16)rendercfg.width), 0);
		mousePosY = max(min(mousePosY, (uint16)rendercfg.height), 0);

		float depth = m_scrollDepth;

		//Convert screen coordinates into clip space coordinates
		float x = (((2.0f * mousePosX) / (float)rendercfg.width) - 1);
		float y = -(((2.0f * mousePosY) / (float)rendercfg.height) - 1);

		Vector ray(x, y, 0.0f, 0.0f);
		//Transform ray from clip space to view space
		ray = Matrix::transform3D(ray, projection.inverse());
		
		Vector vscale;
		Vector vpos;
		Quaternion vrot;
		view.inverse().decompose(vscale, vrot, vpos);

		//Transform ray from view space to world space
		ray = Quaternion::transform(ray, vrot);
		ray.normalize();

		lightpos = position + (ray * depth);
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////

	SUniforms sceneuniforms;

	//Set uniforms
	sceneuniforms.world = Matrix::scale(scale, scale, scale);
	sceneuniforms.view = view;
	sceneuniforms.projection = projection;

	sceneuniforms.lightPos = lightpos;
	sceneuniforms.lightColour = Vector(1.0f, 1.0f, 1.0f);
	sceneuniforms.globalAmbientColour = Vector(0.21f, 0.2f, 0.21f);
	sceneuniforms.eyePos = position;

	sceneuniforms.lightConstantAttenuation = 1.0f;
	sceneuniforms.lightLinearAttenuation = 0.04f;
	sceneuniforms.lightQuadraticAttenuation = 0.0f;

	//Load values from cvars
	table->getVarVector3D("lightColour", sceneuniforms.lightColour);
	table->getVarVector3D("ambientColour", sceneuniforms.globalAmbientColour);
	table->getVarFloat("attenConst", sceneuniforms.lightConstantAttenuation);
	table->getVarFloat("attenLinear", sceneuniforms.lightLinearAttenuation);
	table->getVarFloat("attenQuad", sceneuniforms.lightQuadraticAttenuation);

	sceneuniforms.init();

	/////////////////////////////////////////////////////////////////////////////////////////////////

	auto api = m_env.getRenderModule()->getApi();
	auto& shaderMng = m_env.getRenderModule()->getShaderManager();

	ResourceProxy defaultrendertarget;
	api->getDisplayRenderTarget(defaultrendertarget);

	Viewport viewport;
	viewport.x = 0;
	viewport.y = 0;
	viewport.w = rendercfg.width;
	viewport.h = rendercfg.height;

	/////////////////////////////////////////////////////////////////////////////////////////////////
	//Shadow pass
	/////////////////////////////////////////////////////////////////////////////////////////////////

	{
		SRenderCommand command;
		SUniforms shadowuniforms(sceneuniforms);

		const float lightFarPlane = 100.0f / scale;
		const float lightNearPlane = 0.1f;

		const Matrix lightProjection(Matrix::perspectiveFieldOfView(Pi / 2, 1.0f, lightNearPlane, lightFarPlane));

		//View vectors for point light shadow cube
		Matrix lightViews[6];
		lightViews[0] = Matrix::lookTo(sceneuniforms.lightPos, Vector(+1, 0, 0), Vector(0, +1, 0));
		lightViews[1] = Matrix::lookTo(sceneuniforms.lightPos, Vector(-1, 0, 0), Vector(0, +1, 0));
		lightViews[2] = Matrix::lookTo(sceneuniforms.lightPos, Vector(0, +1, 0), Vector(0, 0, -1));
		lightViews[3] = Matrix::lookTo(sceneuniforms.lightPos, Vector(0, -1, 0), Vector(0, 0, +1));
		lightViews[4] = Matrix::lookTo(sceneuniforms.lightPos, Vector(0, 0, +1), Vector(0, +1, 0));
		lightViews[5] = Matrix::lookTo(sceneuniforms.lightPos, Vector(0, 0, -1), Vector(0, +1, 0));

		command.depthTarget = m_shadowDepthTarget;
		command.viewport.w = 1024;
		command.viewport.h = 1024;
		command.viewport.x = 0;
		command.viewport.y = 0;

		command.uniformBuffers[0] = m_sceneBuffer.getBuffer();
		command.shaders.stageVertex = shaderMng.getShaderProxy(m_shadowVertexShader);
		command.shaders.stagePixel = shaderMng.getShaderProxy(m_shadowPixelShader);
		command.vertexInputDescriptor = m_vertexInputShadow;
		command.vertexTopology = EVertexTopology::eTopologyTriangleList;

		CVertexBuffer vbuf;
		CIndexBuffer ibuf;
		m_model->getVertexBuffer(vbuf);
		m_model->getIndexBuffer(ibuf);

		command.vertexBuffer = vbuf.getBuffer();
		command.indexBuffer = ibuf.getBuffer();
		command.vertexStride = vbuf.getVertexStride();

		for (int i = 0; i < 6; i++)
		{
			shadowuniforms.eyePos = sceneuniforms.lightPos;
			shadowuniforms.nearplane = lightNearPlane;
			shadowuniforms.farplane = lightFarPlane;
			shadowuniforms.world = Matrix::scale(scale, scale, scale);
			shadowuniforms.projection = lightProjection;
			shadowuniforms.view = lightViews[i];

			shadowuniforms.init();

			ResourceProxy shadowRenderTarget;
			
			STextureViewDescriptor shadowtargetdesc;
			shadowtargetdesc.arrayCount = 1;
			shadowtargetdesc.arrayIndex = i;
			tsassert(!api->createViewRenderTarget(shadowRenderTarget, m_shadowCubeRsc, shadowtargetdesc));
			
			m_context->clearRenderTarget(shadowRenderTarget, Vector());
			m_context->clearDepthTarget(m_shadowDepthTarget, 1.0f);
			m_context->resourceBufferUpdate(m_sceneBuffer.getBuffer(), &shadowuniforms);

			command.renderTarget[0] = shadowRenderTarget;
			command.depthTarget = m_shadowDepthTarget;

			for (uint i = 0; i < m_model->getMeshCount(); i++)
			{
				const SMesh& mesh = m_model->getMesh(i);
				command.indexStart = mesh.indexOffset;
				command.indexCount = mesh.indexCount;
				command.vertexBase = mesh.vertexBase;

				//Execute draw call
				m_context->execute(command);
			}
		}
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////
	//Colour pass
	/////////////////////////////////////////////////////////////////////////////////////////////////
	
	m_context->resourceBufferUpdate(m_sceneBuffer.getBuffer(), &sceneuniforms);

	//Draw skybox
	{
		SRenderCommand command;

		//Clear the depth buffer
		m_context->clearDepthTarget(m_depthTarget, 1.0f);

		command.renderTarget[0] = defaultrendertarget;
		command.depthTarget = m_depthTarget;
		command.viewport = viewport;

		command.uniformBuffers[0] = m_sceneBuffer.getBuffer();
		command.shaders.stageVertex = shaderMng.getShaderProxy(m_skyboxVertexShader);
		command.shaders.stagePixel = shaderMng.getShaderProxy(m_skyboxPixelShader);
		command.vertexTopology = EVertexTopology::eTopologyTriangleList;

		command.vertexCount = 6;
		command.vertexStart = 0;

		command.textures[0] = m_skybox.getView();
		command.textureSamplers[0] = m_texSampler;

		m_context->execute(command);
	}


	//Draw model
	{
		SRenderCommand command;

		//Clear the depth buffer
		m_context->clearDepthTarget(m_depthTarget, 1.0f);

		//Fill out render command
		command.renderTarget[0] = defaultrendertarget;
		command.depthTarget = m_depthTarget;
		command.viewport = viewport;

		command.uniformBuffers[0] = m_sceneBuffer.getBuffer();
		command.uniformBuffers[1] = m_materialBuffer.getBuffer();
		command.shaders.stageVertex = shaderMng.getShaderProxy(m_standardVertexshader);
		command.shaders.stagePixel = shaderMng.getShaderProxy(m_standardPixelshader);

		command.textureSamplers[0] = m_texSampler;

		CVertexBuffer vbuf;
		CIndexBuffer ibuf;
		m_model->getVertexBuffer(vbuf);
		m_model->getIndexBuffer(ibuf);

		command.vertexBuffer = vbuf.getBuffer();
		command.indexBuffer = ibuf.getBuffer();
		command.vertexStride = vbuf.getVertexStride();

		//command.vertexTopology = EVertexTopology::eTopologyTriangleList;
		command.vertexTopology = EVertexTopology::eTopologyTriangleList;
		command.vertexInputDescriptor = m_vertexInput;

		command.indexStart = 0;
		command.indexCount = ibuf.getIndexCount();
		command.vertexBase = 0;

		for (uint i = 0; i < m_model->getMeshCount(); i++)
		{
			const SMesh& mesh = m_model->getMesh(i);

			command.textures[0] = mesh.material.diffuseMap.getView();
			command.textures[1] = mesh.material.normalMap.getView();
			command.textures[2] = mesh.material.specularMap.getView();
			command.textures[3] = mesh.material.displacementMap.getView();

			command.textures[8] = m_shadowCube;

			command.indexStart = mesh.indexOffset;
			command.indexCount = mesh.indexCount;
			command.vertexBase = mesh.vertexBase;
			
			//Update material buffer	
			SMaterial::SParams params = mesh.material.params;

			//Get global cvars for enabling different maps
			bool use = 1;
			table->getVarBool("useDiffMap", use);
			params.useDiffuseMap *= use;
			
			use = 1;
			table->getVarBool("useNormMap", use);
			params.useNormalMap *= use;
			
			use = 1;
			table->getVarBool("useDispMap", use);
			params.useDisplacementMap *= use;
			
			use = 1;
			table->getVarBool("useSpecMap", use);
			params.useSpecularMap *= use;
			
			m_context->resourceBufferUpdate(m_materialBuffer.getBuffer(), (const void*)&params);

			//Execute draw call
			m_context->execute(command);
		}
	}

	//Draw light source
	{
		SRenderCommand command;

		command.renderTarget[0] = defaultrendertarget;
		command.depthTarget = m_depthTarget;
		command.viewport = viewport;

		//Sphere
		const SMesh& mesh = m_sphere->getMesh(0);

		command.shaders.stageVertex = shaderMng.getShaderProxy(m_lightVertexShader);
		command.shaders.stagePixel = shaderMng.getShaderProxy(m_lightPixelShader);
		command.vertexInputDescriptor = m_vertexInputLight;
		command.vertexTopology = EVertexTopology::eTopologyTriangleList;

		CVertexBuffer vbuf;
		CIndexBuffer ibuf;
		m_sphere->getVertexBuffer(vbuf);
		m_sphere->getIndexBuffer(ibuf);

		command.vertexBuffer = vbuf.getBuffer();
		command.indexBuffer = ibuf.getBuffer();
		command.vertexStride = vbuf.getVertexStride();

		command.indexStart = mesh.indexOffset;
		command.indexCount = mesh.indexCount;
		command.vertexBase = mesh.vertexBase;
		sceneuniforms.world = Matrix::translation(sceneuniforms.lightPos);
		sceneuniforms.view = m_camera->getViewMatrix();
		sceneuniforms.projection = m_camera->getProjectionMatrix();

		sceneuniforms.init();

		command.uniformBuffers[0] = m_sceneBuffer.getBuffer();
		m_context->resourceBufferUpdate(m_sceneBuffer.getBuffer(), (const void*)&sceneuniforms);

		m_context->execute(command);
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////
	//Draw user interface
	/////////////////////////////////////////////////////////////////////////////////////////////////

	if (m_showUI && m_ui.get())
	{
		m_ui->setDisplaySize(rendercfg.width, rendercfg.height);

		m_ui->begin(m_context, dt);

		if (m_showConsole)
		{
			m_consoleMenu->show();
		}

		m_debugMenu->show(dt);

		m_ui->end(defaultrendertarget);
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////

	//Mark context as finished
	m_context->finish();
	//Execute context
	m_env.getRenderModule()->getApi()->executeContext(m_context);
	
}

void Application::onExit()
{
	m_env.getRenderModule()->getApi()->destroyContext(m_context);
	m_env.getWindow()->removeEventListener(this);
	m_env.getInputModule()->removeEventListener(this);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void Application::buildDepthTarget()
{
	m_depthTarget.reset(nullptr);

	SRenderModuleConfiguration rendercfg;
	m_env.getRenderModule()->getConfiguration(rendercfg);
	ERenderStatus status = eOk;
	auto api = m_env.getRenderModule()->getApi();

	//Depth target view
	ResourceProxy depthtargetrsc;
	STextureResourceDescriptor depthdesc;
	depthdesc.height = rendercfg.height;
	depthdesc.width = rendercfg.width;
	depthdesc.texformat = ETextureFormat::eTextureFormatDepth32;
	depthdesc.texmask = eTextureMaskDepthTarget;
	depthdesc.textype = eTypeTexture2D;
	depthdesc.useMips = false;
	depthdesc.multisampling = rendercfg.multisampling;
	status = api->createResourceTexture(depthtargetrsc, nullptr, depthdesc);
	tsassert(!status);

	STextureViewDescriptor depthviewdesc;
	depthviewdesc.arrayCount = 1;
	depthviewdesc.arrayIndex = 0;
	status = api->createViewDepthTarget(m_depthTarget, depthtargetrsc, depthviewdesc);
	tsassert(!status);
}

void Application::buildVertexInputDescriptor(vector<SShaderInputDescriptor>& inputdescriptor, uint32 vertexFlags)
{
	inputdescriptor.clear();

	//Position attribute
	if (vertexFlags & eModelVertexAttributePosition)
	{
		SShaderInputDescriptor sid;
		sid.bufferSlot = 0;
		sid.byteOffset = VECTOR_OFFSET(0);
		sid.channel = EShaderInputChannel::eInputPerVertex;
		sid.semanticName = "POSITION";
		sid.type = eShaderInputFloat4;
		inputdescriptor.push_back(sid);
	}

	//Texcoord attribute
	if (vertexFlags & eModelVertexAttributeTexcoord)
	{
		SShaderInputDescriptor sid;
		sid.bufferSlot = 0;
		sid.byteOffset = VECTOR_OFFSET(1);
		sid.channel = EShaderInputChannel::eInputPerVertex;
		sid.semanticName = "TEXCOORD";
		sid.type = eShaderInputFloat2;
		inputdescriptor.push_back(sid);
	}

	//Colour attribute
	if (vertexFlags & eModelVertexAttributeColour)
	{
		SShaderInputDescriptor sid;
		sid.bufferSlot = 0;
		sid.byteOffset = VECTOR_OFFSET(2);
		sid.channel = EShaderInputChannel::eInputPerVertex;
		sid.semanticName = "COLOUR";
		sid.type = eShaderInputFloat4;
		inputdescriptor.push_back(sid);
	}

	//Normal attribute
	if (vertexFlags & eModelVertexAttributeNormal)
	{
		SShaderInputDescriptor sid;
		sid.bufferSlot = 0;
		sid.byteOffset = VECTOR_OFFSET(3);
		sid.channel = EShaderInputChannel::eInputPerVertex;
		sid.semanticName = "NORMAL";
		sid.type = eShaderInputFloat3;
		inputdescriptor.push_back(sid);
	}

	//Tangent attribute
	if (vertexFlags & eModelVertexAttributeTangent)
	{
		SShaderInputDescriptor sid;
		sid.bufferSlot = 0;
		sid.byteOffset = VECTOR_OFFSET(4);
		sid.channel = EShaderInputChannel::eInputPerVertex;
		sid.semanticName = "TANGENT";
		sid.type = eShaderInputFloat3;
		inputdescriptor.push_back(sid);
	}
	
	//Bitangent attribute
	if (vertexFlags & eModelVertexAttributeBitangent)
	{
		SShaderInputDescriptor sid;
		sid.bufferSlot = 0;
		sid.byteOffset = VECTOR_OFFSET(5);
		sid.channel = EShaderInputChannel::eInputPerVertex;
		sid.semanticName = "BITANGENT";
		sid.type = eShaderInputFloat3;
		inputdescriptor.push_back(sid);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////