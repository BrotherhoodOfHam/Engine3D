/*
	Sandbox application
*/

#pragma once

#include <tsengine.h>

#include "Camera.h"
#include "3D/SceneRender.h"

#include "Entity.h"
#include "TransformComponent.h"
#include "RenderComponent.h"

namespace ts
{
	class Sandbox : public Application, private InputSystem::IListener
	{
	private:

		Camera m_camera;
		SceneRender m_render;

		//Entities + Components
		EntityManager m_entityManager;
		ComponentMap<RenderComponent> m_renderables;
		ComponentMap<TransformComponent> m_transforms;

		Entity m_modelEntity, m_boxEntity;

		float m_scale;

		int onInit() override;
		void onExit() override;
		void onUpdate(double deltatime) override;

		void onKeyDown(EKeyCode code) override;

	public:

		Sandbox(int argc, char** argv);
		~Sandbox();

		EntityManager* getEntities() { return &m_entityManager; }

		bool addModelRenderComponent(Entity e, const String& modelFile);
	};
}
