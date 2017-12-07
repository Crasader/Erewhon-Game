// Copyright (C) 2017 Jérôme Leclercq
// This file is part of the "Erewhon Shared" project
// For conditions of distribution and use, see copyright notice in LICENSE

// THIS IS ONLY A TEST FOR SPACESHIP CONTROL, IT'S UGLY

#include <Client/States/GameState.hpp>
#include <Nazara/Core/Primitive.hpp>
#include <Nazara/Graphics/ColorBackground.hpp>
#include <Nazara/Graphics/Model.hpp>
#include <Nazara/Graphics/ParticleDeclaration.hpp>
#include <Nazara/Graphics/ParticleFunctionController.hpp>
#include <Nazara/Graphics/ParticleFunctionRenderer.hpp>
#include <Nazara/Graphics/ParticleMapper.hpp>
#include <Nazara/Graphics/ParticleStruct.hpp>
#include <Nazara/Graphics/SkyboxBackground.hpp>
#include <Nazara/Utility/Mesh.hpp>
#include <NDK/Components/CollisionComponent3D.hpp>
#include <NDK/Components/GraphicsComponent.hpp>
#include <NDK/Components/ParticleEmitterComponent.hpp>
#include <NDK/Components/ParticleGroupComponent.hpp>
#include <NDK/Components/NodeComponent.hpp>
#include <NDK/Components/PhysicsComponent3D.hpp>
#include <NDK/Systems/RenderSystem.hpp>
#include <cassert>
#include <cmath>
#include <iostream>
#include <random>

namespace ewn
{
	static constexpr bool showServerGhosts = false;
	static constexpr std::size_t maxChatLines = 15;

	void GameState::Enter(Ndk::StateMachine& /*fsm*/)
	{
		if (Nz::Texture* background = Nz::TextureLibrary::Get("Background"); background && background->IsValid())
			m_stateData.world3D->GetSystem<Ndk::RenderSystem>().SetDefaultBackground(Nz::SkyboxBackground::New(background));
		else
			m_stateData.world3D->GetSystem<Ndk::RenderSystem>().SetDefaultBackground(Nz::ColorBackground::New(Nz::Color::Black));

		Nz::ModelParameters params;
		params.mesh.center = true;
		params.material.shaderName = "Basic";

		// Ball
		Nz::ModelRef ballModel = Nz::Model::New();
		ballModel->LoadFromFile("Assets/ball/ball.obj", params);

		{
			m_ballTemplateEntity = m_stateData.world3D->CreateEntity();

			constexpr float radius = 18.251904f / 2.f;

			m_ballTemplateEntity->AddComponent<Ndk::CollisionComponent3D>(Nz::SphereCollider3D::New(radius));
			m_ballTemplateEntity->AddComponent<Ndk::GraphicsComponent>().Attach(ballModel);
			m_ballTemplateEntity->AddComponent<Ndk::NodeComponent>();

			auto& physComponent = m_ballTemplateEntity->AddComponent<Ndk::PhysicsComponent3D>();
			physComponent.SetMass(10.f);

			m_ballTemplateEntity->Disable();
		}

		// Earth
		Nz::MeshRef earthMesh = Nz::Mesh::New();
		earthMesh->CreateStatic();
		earthMesh->BuildSubMesh(Nz::Primitive::UVSphere(1.f, 40, 40));

		Nz::MaterialRef earthMaterial = Nz::Material::New();
		earthMaterial->SetDiffuseMap("Assets/earth/earthmap1k.jpg");
		earthMaterial->SetShader("Basic");

		Nz::ModelRef earthModel = Nz::Model::New();
		earthModel->SetMesh(earthMesh);
		earthModel->SetMaterial(0, earthMaterial);

		m_earthTemplateEntity = m_stateData.world3D->CreateEntity();
		m_earthTemplateEntity->AddComponent<Ndk::CollisionComponent3D>(Nz::SphereCollider3D::New(20.f));
		m_earthTemplateEntity->AddComponent<Ndk::GraphicsComponent>().Attach(earthModel);
		m_earthTemplateEntity->AddComponent<Ndk::PhysicsComponent3D>();

		auto& earthNode = m_earthTemplateEntity->AddComponent<Ndk::NodeComponent>();
		earthNode.SetPosition(Nz::Vector3f::Forward() * 50.f);
		earthNode.SetRotation(Nz::EulerAnglesf(0.f, 180.f, 0.f));
		earthNode.SetScale(20.f);

		m_earthTemplateEntity->Disable();

		// Spaceship
		params.mesh.matrix.MakeTransform(Nz::Vector3f::Zero(), Nz::EulerAnglesf(0.f, 90.f, 0.f), Nz::Vector3f(0.01f));
		params.mesh.texCoordScale.Set(1.f, -1.f);

		Nz::ModelRef spaceshipModel = Nz::Model::New();
		spaceshipModel->LoadFromFile("Assets/spaceship/spaceship.obj", params);

		{
			m_spaceshipTemplateEntity = m_stateData.world3D->CreateEntity();

			Nz::SphereCollider3DRef collider = Nz::SphereCollider3D::New(5.f);
			auto& collisionComponent = m_spaceshipTemplateEntity->AddComponent<Ndk::CollisionComponent3D>(collider);

			m_spaceshipTemplateEntity->AddComponent<Ndk::GraphicsComponent>().Attach(spaceshipModel);
			m_spaceshipTemplateEntity->AddComponent<Ndk::NodeComponent>();
			auto& spaceshipPhys = m_spaceshipTemplateEntity->AddComponent<Ndk::PhysicsComponent3D>();
			spaceshipPhys.SetMass(42.f);
			spaceshipPhys.SetAngularDamping(Nz::Vector3f(0.3f));
			spaceshipPhys.SetLinearDamping(0.25f);

			m_spaceshipTemplateEntity->Disable();
		}

		Nz::MaterialRef debugMaterial = Nz::Material::New("Translucent3D");
		debugMaterial->SetDiffuseColor(Nz::Color(255, 255, 255, 50));

		Nz::ModelRef ghostSpaceship = Nz::Model::New(*spaceshipModel);
		for (std::size_t i = 0; i < ghostSpaceship->GetMaterialCount(); ++i)
			ghostSpaceship->SetMaterial(i, debugMaterial);

		m_debugTemplateEntity = m_stateData.world3D->CreateEntity();
		m_debugTemplateEntity->AddComponent<Ndk::GraphicsComponent>().Attach(ghostSpaceship);
		m_debugTemplateEntity->AddComponent<Ndk::NodeComponent>();
		m_debugTemplateEntity->Disable();

		// Particle effect
#if 0
		Ndk::ParticleEmitterComponent& particleEmitter = m_spaceshipEntity->AddComponent<Ndk::ParticleEmitterComponent>();
		particleEmitter.SetEmissionRate(30.f);

		particleEmitter.SetSetupFunc([this](const Ndk::EntityHandle& emitterEntity, Nz::ParticleMapper& particleMapper, unsigned int count)
		{
			Nz::Vector3f pos = emitterEntity->GetComponent<Ndk::NodeComponent>().GetPosition();

			Nz::ParticleStruct_Billboard* billboards = static_cast<Nz::ParticleStruct_Billboard*>(particleMapper.GetPointer());
			for (unsigned int j = 0; j < count; ++j)
			{
				billboards[j].color = Nz::Color::White;
				billboards[j].life = 10.f;
				billboards[j].position = pos;
				billboards[j].rotation = 0.f;
				billboards[j].size = { 1.28f, 1.28f };
			}
		});

		Nz::MaterialRef fireMat = Nz::Material::New("Translucent3D");
		fireMat->EnableFaceCulling(true);
		fireMat->SetDiffuseMap("Assets/particles/fire_particle.png");
		// Additive blending for fire
		fireMat->SetDstBlend(Nz::BlendFunc_One);
		fireMat->SetSrcBlend(Nz::BlendFunc_SrcAlpha);

		Ndk::ParticleGroupComponent& particleGroup = m_spaceshipEntity->AddComponent<Ndk::ParticleGroupComponent>(5000, Nz::ParticleDeclaration::Get(Nz::ParticleLayout_Billboard));
		particleGroup.AddEmitter(m_spaceshipEntity);
		particleGroup.SetRenderer(Nz::ParticleFunctionRenderer::New([fireMat](const Nz::ParticleGroup& /*group*/, const Nz::ParticleMapper& mapper, unsigned int startId, unsigned int endId, Nz::AbstractRenderQueue* renderQueue)
		{
			auto colorPtr = mapper.GetComponentPtr<const Nz::Color>(Nz::ParticleComponent_Color);
			auto posPtr = mapper.GetComponentPtr<const Nz::Vector3f>(Nz::ParticleComponent_Position);
			auto rotPtr = mapper.GetComponentPtr<const float>(Nz::ParticleComponent_Rotation);
			auto sizePtr = mapper.GetComponentPtr<const Nz::Vector2f>(Nz::ParticleComponent_Size);

			renderQueue->AddBillboards(0, fireMat, endId - startId + 1, posPtr, sizePtr, rotPtr, colorPtr);
		}));
		particleGroup.AddController(Nz::ParticleFunctionController::New([](Nz::ParticleGroup& group, Nz::ParticleMapper& mapper, unsigned int startId, unsigned int endId, float elapsedTime)
		{
			auto colorPtr = mapper.GetComponentPtr<Nz::Color>(Nz::ParticleComponent_Color);
			auto lifePtr = mapper.GetComponentPtr<float>(Nz::ParticleComponent_Life);

			for (unsigned int i = startId; i <= endId; ++i)
			{
				if ((lifePtr[i] -= elapsedTime) < 0.f)
					group.KillParticle(i);

				colorPtr[i].a = static_cast<Nz::UInt8>(Nz::Clamp(lifePtr[i] * 255.f, 0.f, 255.f));
			}
		}));
#endif

		//m_spaceshipEntity->GetComponent<Ndk::NodeComponent>().SetParent(m_spaceshipMovementNode);

		m_controlledEntity = std::numeric_limits<std::size_t>::max();
		m_inputAccumulator = 0.f;
		m_isCurrentlyRotating = false;
		m_lastInputTime = 0;
		m_lastStateId = 0;
		m_resetSnapshots = true;
		m_rotationDirection.MakeZero();
		m_spaceshipRotation.MakeZero();
		m_spaceshipSpeed.MakeZero();
		m_snapshotUpdateAccumulator = 0.f;
		m_syncEnabled = true;

		Nz::Vector2ui windowSize = m_stateData.window->GetSize();
		Nz::Mouse::SetPosition(windowSize.x / 2, windowSize.y / 2, *m_stateData.window);
		Nz::EventHandler& eventHandler = m_stateData.window->GetEventHandler();
		eventHandler.OnMouseButtonPressed.Connect([&](const Nz::EventHandler*, const Nz::WindowEvent::MouseButtonEvent& event)
		{
			if (event.button == Nz::Mouse::Right)
			{
				m_isCurrentlyRotating = true;
				m_rotationCursorOrigin = Nz::Mouse::GetPosition(*m_stateData.window);
				m_rotationCursorPosition.MakeZero();

				m_stateData.window->SetCursor(Nz::SystemCursor_None);
				m_cursorEntity->Enable();
				m_cursorOrientationSprite->SetColor(Nz::Color(255, 255, 255, 0));
			}
		});

		eventHandler.OnMouseButtonReleased.Connect([&](const Nz::EventHandler*, const Nz::WindowEvent::MouseButtonEvent& event)
		{
			if (event.button == Nz::Mouse::Right)
			{
				m_isCurrentlyRotating = false;
				m_stateData.window->SetCursor(Nz::SystemCursor_Default);
				Nz::Mouse::SetPosition(m_rotationCursorOrigin.x, m_rotationCursorOrigin.y, *m_stateData.window);

				m_cursorEntity->Disable();
			}
		});

		eventHandler.OnMouseMoved.Connect([&](const Nz::EventHandler*, const Nz::WindowEvent::MouseMoveEvent& event)
		{
			if (m_isCurrentlyRotating)
			{
				constexpr int distMax = 200;

				m_rotationCursorPosition.x += event.deltaX;
				m_rotationCursorPosition.y += event.deltaY;
				if (m_rotationCursorPosition.GetSquaredLength() > Nz::IntegralPow(distMax, 2))
				{
					Nz::Vector2f tempCursor(m_rotationCursorPosition);
					tempCursor.Normalize();
					tempCursor *= float(distMax);
					m_rotationCursorPosition = Nz::Vector2i(tempCursor);
				}

				Nz::Vector2ui windowCenter = m_stateData.window->GetSize() / 2;

				// Position
				Ndk::NodeComponent& cursorNode = m_cursorEntity->GetComponent<Ndk::NodeComponent>();
				cursorNode.SetPosition(float(windowCenter.x + m_rotationCursorPosition.x), float(windowCenter.y + m_rotationCursorPosition.y));

				// Angle
				float cursorAngle = std::atan2(float(m_rotationCursorPosition.y), float(m_rotationCursorPosition.x));
				cursorNode.SetRotation(Nz::EulerAnglesf(0.f, 0.f, Nz::RadianToDegree(cursorAngle)));

				// Alpha
				float cursorAlpha = float(m_rotationCursorPosition.GetSquaredLength()) / Nz::IntegralPow(distMax, 2);
				m_cursorOrientationSprite->SetColor(Nz::Color(255, 255, 255, static_cast<Nz::UInt8>(std::min(cursorAlpha * 255.f, 255.f))));

				Nz::Mouse::SetPosition(windowCenter.x, windowCenter.y, *m_stateData.window);
			}
		});




		// Movement cursor
		Nz::MaterialRef cursorMat = Nz::Material::New("Translucent2D");
		cursorMat->SetDiffuseMap("Assets/cursor/orientation.png");

		m_cursorOrientationSprite = Nz::Sprite::New();
		m_cursorOrientationSprite->SetMaterial(cursorMat);
		m_cursorOrientationSprite->SetSize({ 32.f, 32.f });
		m_cursorOrientationSprite->SetOrigin(m_cursorOrientationSprite->GetSize() / 2.f);

		m_cursorEntity = m_stateData.world2D->CreateEntity();
		m_cursorEntity->AddComponent<Ndk::GraphicsComponent>().Attach(m_cursorOrientationSprite);
		m_cursorEntity->AddComponent<Ndk::NodeComponent>().SetPosition({ 200.f, 200.f, 0.f });

		m_cursorEntity->Disable();

		/*m_stateData.window->SetCursor(Nz::SystemCursor_None);
		Nz::Vector2ui windowSize = m_stateData.window->GetSize();
		Nz::Mouse::SetPosition(windowSize.x / 2, windowSize.y / 2, *m_stateData.window);*/

		/*eventHandler.OnMouseMoved.Connect([&](const Nz::EventHandler*, const Nz::WindowEvent::MouseMoveEvent& mouse)
		{
			m_spaceshipRotation.x -= mouse.deltaX * 0.1f;
			m_spaceshipRotation.y += mouse.deltaY * 0.1f;
		});
		*/

		m_cameraRotation = Nz::Vector3f::Zero();
		m_chatEnteringBox = nullptr;
		m_chatLines.resize(maxChatLines);
		m_interpolationFactor = 0;

		m_chatBox = m_stateData.canvas->Add<Ndk::TextAreaWidget>();
		m_chatBox->EnableBackground(false);
		//m_chatBox->SetBackgroundColor(Nz::Color(70, 8, 15, 20));
		m_chatBox->SetSize({ 320.f, maxChatLines * 30.f });
		m_chatBox->SetPosition({ 5.f, m_stateData.window->GetSize().y - 40.f - m_chatBox->GetSize().y - 5.f, 0.f });
		m_chatBox->SetTextColor(Nz::Color::White);
		m_chatBox->SetReadOnly(true);

		m_onArenaStateSlot.Connect(m_stateData.server->OnArenaState, this, &GameState::OnArenaState);
		m_onChatMessageSlot.Connect(m_stateData.server->OnChatMessage, this, &GameState::OnChatMessage);
		m_onControlEntitySlot.Connect(m_stateData.server->OnControlEntity, this, &GameState::OnControlEntity);
		m_onCreateEntitySlot.Connect(m_stateData.server->OnCreateEntity, this, &GameState::OnCreateEntity);
		m_onDeleteEntitySlot.Connect(m_stateData.server->OnDeleteEntity, this, &GameState::OnDeleteEntity);
		m_onKeyPressedSlot.Connect(m_stateData.window->GetEventHandler().OnKeyPressed, this, &GameState::OnKeyPressed);
		m_onTargetChangeSizeSlot.Connect(m_stateData.window->OnRenderTargetSizeChange, [this](const Nz::RenderTarget*) { m_chatBox->SetPosition({ 5.f, m_stateData.window->GetSize().y - 30 - m_chatBox->GetSize().y, 0.f }); });

		m_stateData.server->SendPacket(Packets::JoinArena());

		// Listen to debug state
		if constexpr (showServerGhosts)
		{
			m_debugStateSocket.Create(Nz::NetProtocol_IPv4);
			m_debugStateSocket.Bind(2050);

			m_debugStateSocket.EnableBlocking(false);
		}
	}

	void GameState::Leave(Ndk::StateMachine& /*fsm*/)
	{
		m_onArenaStateSlot.Disconnect();
		m_onChatMessageSlot.Disconnect();
		m_onControlEntitySlot.Disconnect();
		m_onCreateEntitySlot.Disconnect();
		m_onDeleteEntitySlot.Disconnect();
		m_onKeyPressedSlot.Disconnect();
		m_onTargetChangeSizeSlot.Disconnect();

		m_chatBox->Destroy();
		if (m_chatEnteringBox)
			m_chatEnteringBox->Destroy();

		for (const auto& spaceshipData : m_serverEntities)
		{
			if (spaceshipData.debugGhostEntity)
				spaceshipData.debugGhostEntity->Kill();

			if (spaceshipData.entity)
				spaceshipData.entity->Kill();

			if (spaceshipData.textEntity)
				spaceshipData.textEntity->Kill();
		}

		m_cursorEntity->Kill();
		m_earthTemplateEntity->Kill();
		m_spaceshipTemplateEntity->Kill();
	}

	bool GameState::Update(Ndk::StateMachine& /*fsm*/, float elapsedTime)
	{
		//auto& earthNode = m_earthEntity->GetComponent<Ndk::NodeComponent>();
		//earthNode.Rotate(Nz::EulerAnglesf(0.f, 2.f * elapsedTime, 0.f));

		// Update and send input
		m_inputAccumulator += elapsedTime;
		m_snapshotUpdateAccumulator += elapsedTime;

		constexpr float inputSendInterval = 1.f / 60.f;
		if (m_inputAccumulator > inputSendInterval)
		{
			m_inputAccumulator -= inputSendInterval;
			UpdateInput(inputSendInterval);
		}

		constexpr float snapshotUpdateInterval = 1.f / 10.f;
		if (m_snapshotUpdateAccumulator > snapshotUpdateInterval)
		{
			m_snapshotUpdateAccumulator -= snapshotUpdateInterval;

			std::cout << m_snapshots.front().snapshotId << std::endl;
		}



		/*m_interpolationFactor = std::min(m_interpolationFactor + elapsedTime * 10.f, 3.0f);

		auto& cameraNode = m_stateData.camera3D->GetComponent<Ndk::NodeComponent>();
		Nz::Quaternionf camRot = cameraNode.GetRotation();

		for (std::size_t i = 0; i < m_serverEntities.size(); ++i)
		{
			const ServerEntity& entityData = m_serverEntities[i];
			if (!entityData.isValid)
				continue;

			if (i == m_controlledEntity && false)
				continue;

			Nz::Vector3f currentPosition = Nz::Lerp(entityData.oldPosition, entityData.newPosition, m_interpolationFactor);
			Nz::Quaternionf currentRotation = Nz::Quaternionf::Slerp(entityData.oldRotation, entityData.newRotation, m_interpolationFactor);

			auto& spaceshipNode = entityData.entity->GetComponent<Ndk::NodeComponent>();
			/*spaceshipNode.SetPosition(currentPosition);
			spaceshipNode.SetRotation(currentRotation);*/

			// Update text position
			/*auto& textGfx = entityData.textEntity->GetComponent<Ndk::GraphicsComponent>();
			auto& textNode = entityData.textEntity->GetComponent<Ndk::NodeComponent>();
			textNode.SetPosition(spaceshipNode.GetPosition() + camRot * Nz::Vector3f::Up() * 6.f + Nz::Vector3f::Right() * textGfx.GetBoundingVolume().obb.localBox.width / 2.f);
			textNode.SetRotation(cameraNode.GetRotation());
		}*/

		m_cameraRotation.x = Nz::Approach(m_cameraRotation.x, m_spaceshipRotation.x / 10.f, 10.f * elapsedTime);
		m_cameraRotation.y = Nz::Approach(m_cameraRotation.y, m_spaceshipRotation.y / 10.f, 10.f * elapsedTime);
		m_cameraRotation.z = Nz::Approach(m_cameraRotation.z, m_spaceshipRotation.z / 10.f, 10.f * elapsedTime);
		m_cameraNode.SetRotation(Nz::EulerAnglesf(m_cameraRotation.x, m_cameraRotation.y, m_cameraRotation.z));

		// Debug state socket
		if constexpr (showServerGhosts)
		{
			Nz::NetPacket packet;
			if (m_debugStateSocket.ReceivePacket(&packet, nullptr))
			{
				Packets::ArenaState arenaState;
				Packets::Unserialize(packet, arenaState);

				for (auto& serverData : arenaState.entities)
				{
					// Since we're using a different channel for debug purpose, we may receive information about a spaceship we're not aware yet
					if (!IsServerEntityValid(serverData.id))
						continue;

					ServerEntity& entityData = GetServerEntity(serverData.id);

					// Ensure ghost entity existence
					if (!entityData.debugGhostEntity)
						entityData.debugGhostEntity = m_debugTemplateEntity->Clone();

					auto& ghostNode = entityData.debugGhostEntity->GetComponent<Ndk::NodeComponent>();
					ghostNode.SetPosition(serverData.position);
					ghostNode.SetRotation(serverData.rotation);
				}
			}
		}

		return true;
	}

	void GameState::ControlEntity(std::size_t entityId)
	{
		if (m_controlledEntity != std::numeric_limits<std::size_t>::max())
		{
			ServerEntity& oldData = GetServerEntity(m_controlledEntity);
			oldData.textEntity->Enable();
		}

		if (IsServerEntityValid(entityId))
		{
			ServerEntity& data = GetServerEntity(entityId);

			// Don't show our own name
			data.textEntity->Disable();

			m_cameraNode.SetParent(data.entity->GetComponent<Ndk::NodeComponent>());

			Ndk::NodeComponent& cameraNode = m_stateData.camera3D->GetComponent<Ndk::NodeComponent>();
			cameraNode.SetParent(m_cameraNode);
			cameraNode.SetPosition(Nz::Vector3f::Backward() * 12.f + Nz::Vector3f::Up() * 5.f);
			cameraNode.SetRotation(Nz::EulerAnglesf(-10.f, 0.f, 0.f));
		}

		m_controlledEntity = entityId;
	}

	void GameState::OnArenaState(ServerConnection*, const Packets::ArenaState& arenaState)
	{
		if (!m_syncEnabled)
			return;

		std::cout << "Received snapshot #" << arenaState.stateId << std::endl;

		std::size_t firstValidState;
		std::size_t lastValidState;
		Nz::UInt16 missedStates = arenaState.stateId - m_lastStateId;
		if (missedStates >= m_snapshots.size() || m_resetSnapshots)
		{
			// Build first snapshot according to current data
			ServerSnapshot& firstSnapshot = m_snapshots.front();
			firstSnapshot.snapshotId = arenaState.stateId - m_snapshots.size();
			firstSnapshot.states.resize(m_serverEntities.size());
			for (std::size_t i = 0; i < m_serverEntities.size(); ++i)
			{
				ServerEntity& serverEntity = GetServerEntity(i);
				auto& entityPhys = serverEntity.entity->GetComponent<Ndk::PhysicsComponent3D>();

				auto& entityData = firstSnapshot.states[i];
				entityData.angularVelocity = entityPhys.GetAngularVelocity();
				entityData.linearVelocity = entityPhys.GetVelocity();
				entityData.position = entityPhys.GetPosition();
				entityData.rotation = entityPhys.GetRotation();
			}

			m_resetSnapshots = false;

			firstValidState = 0;
			lastValidState = m_snapshots.size() - 1;
		}
		else
		{
			firstValidState = m_snapshots.size() - missedStates - 1;
			lastValidState = m_snapshots.size() - 1;

			std::rotate(m_snapshots.begin(), m_snapshots.begin() + missedStates, m_snapshots.end());
		}

		ServerSnapshot& firstSnapshot = m_snapshots[firstValidState];
		ServerSnapshot& lastSnapshot = m_snapshots[lastValidState];
		lastSnapshot.snapshotId = arenaState.stateId;

		if (!arenaState.entities.empty())
			lastSnapshot.states.resize(arenaState.entities.back().id + 1);

		for (const auto& entityData : arenaState.entities)
		{
			auto& state = lastSnapshot.states[entityData.id];
			state.angularVelocity = entityData.angularVelocity;
			state.linearVelocity = entityData.linearVelocity;
			state.position = entityData.position;
			state.rotation = entityData.rotation;
		}

		// Recreate lost states
		std::size_t lostStateCount = lastValidState - firstValidState - 1;
		if (lostStateCount > 0)
			std::cerr << "Lost " << lostStateCount << " states!" << std::endl;

		assert(lostStateCount < m_snapshots.size());
		for (std::size_t i = 0; i < lostStateCount; ++i)
		{
			// Interpolate data to recreate approximately lost state
			ServerSnapshot& missedSnapshot = m_snapshots[firstValidState + i];

			float interpolationFactor = float(i + 1) / float(lostStateCount + 1);

			assert(firstSnapshot.states.size() == lastSnapshot.states.size());
			missedSnapshot.snapshotId = firstSnapshot.snapshotId + i;
			missedSnapshot.states.resize(lastSnapshot.states.size());
			for (std::size_t j = 0; j < missedSnapshot.states.size(); ++j)
			{
				const auto& firstData = firstSnapshot.states[j];
				const auto& lastData = lastSnapshot.states[j];

				auto& interpolatedData = missedSnapshot.states[j];
				interpolatedData.angularVelocity = Nz::Vector3f::Lerp(firstData.angularVelocity, lastData.angularVelocity, interpolationFactor);
				interpolatedData.linearVelocity = Nz::Vector3f::Lerp(firstData.linearVelocity, lastData.linearVelocity, interpolationFactor);
				interpolatedData.position = Nz::Vector3f::Lerp(firstData.position, lastData.position, interpolationFactor);
				interpolatedData.rotation = Nz::Quaternionf::Slerp(firstData.rotation, lastData.rotation, interpolationFactor);
			}
		}

		m_lastStateId = arenaState.stateId;

		// Compute new interpolation factor from estimated server time/packet server time 
		//Nz::UInt64 estimatedServerTime = m_stateData.app->GetServerTimeCetteMethodeEstAussiDegueu();
		/*std::cout << "Estimated server time:" << estimatedServerTime << std::endl;
		std::cout << "Received server time:" << arenaState.serverTime << std::endl;
		std::cout << "Diff server time:" << (estimatedServerTime - std::min(estimatedServerTime, arenaState.serverTime)) << std::endl;*/

		//float lateBy = (estimatedServerTime - std::min(estimatedServerTime, arenaState.serverTime)) / 100.f;

		//m_interpolationFactor = lateBy;
		/*m_interpolationFactor = 0.f;

		for (const auto& serverData : arenaState.entities)
		{
			ServerEntity& entityData = GetServerEntity(serverData.id);

			auto& spaceshipNode = entityData.entity->GetComponent<Ndk::NodeComponent>();
			auto& spaceshipPhys = entityData.entity->GetComponent<Ndk::PhysicsComponent3D>();

			if (serverData.id == m_controlledEntity && false)
			{
				// Reconciliation

				// First, remove every treated input from the server
				auto firstClientInput = std::find_if(m_predictedInputs.begin(), m_predictedInputs.end(), [processedTime = arenaState.lastProcessedInputTime](const ClientInput& input)
				{
					return input.inputTime > processedTime;
				});
				m_predictedInputs.erase(m_predictedInputs.begin(), firstClientInput);

				Nz::Vector3f clientPosition = spaceshipNode.GetPosition();
				Nz::Quaternionf clientRotation = spaceshipNode.GetRotation();

				// Restore server position
				spaceshipNode.SetPosition(serverData.position);
				spaceshipNode.SetRotation(serverData.rotation);

				// Apply unacknowledged inputs
				Nz::UInt64 lastInputTime = arenaState.lastProcessedInputTime;
				for (const ClientInput& input : m_predictedInputs)
				{
					ApplyInput(spaceshipNode, lastInputTime, input);
					lastInputTime = input.inputTime;
				}

				// Smooth position over time (or teleport if reconciliated position is too far away from current position)
				Nz::Vector3f reconciliatedPosition = spaceshipNode.GetPosition();
				if (reconciliatedPosition.SquaredDistance(clientPosition) < Nz::IntegralPow(2.f, 2))
					spaceshipNode.SetPosition(Nz::Vector3f::Lerp(clientPosition, reconciliatedPosition, 0.1f));
				else
					std::cout << "Teleport!" << std::endl;

				// Smooth rotation over time
				Nz::Quaternionf reconciliatedRotation = spaceshipNode.GetRotation();

				spaceshipNode.SetRotation(Nz::Quaternionf::Slerp(clientRotation, reconciliatedRotation, 0.1f));
			}
			else
			{
				spaceshipPhys.SetAngularVelocity(serverData.angularVelocity);
				spaceshipPhys.SetPosition(serverData.position);
				spaceshipPhys.SetRotation(serverData.rotation);
				spaceshipPhys.SetVelocity(serverData.linearVelocity);
			}
		}*/
	}

	void GameState::OnChatMessage(ServerConnection*, const Packets::ChatMessage & chatMessage)
	{
		PrintMessage(chatMessage.message);
	}

	void GameState::OnControlEntity(ServerConnection*, const Packets::ControlEntity& controlPacket)
	{
		ControlEntity(controlPacket.id);
	}

	void GameState::OnCreateEntity(ServerConnection*, const Packets::CreateEntity& createPacket)
	{
		ServerEntity& data = CreateServerEntity(createPacket.id);

		for (auto& snapShot : m_snapshots)
		{
			if (createPacket.id >= snapShot.states.size())
				snapShot.states.resize(createPacket.id + 1);

			auto& entityData = snapShot.states[createPacket.id];
			entityData.position = createPacket.position;
			entityData.rotation = createPacket.rotation;
		}

		if (createPacket.entityType == "spaceship")
			data.entity = m_spaceshipTemplateEntity->Clone();
		else if (createPacket.entityType == "earth")
			data.entity = m_earthTemplateEntity->Clone();
		else if (createPacket.entityType == "ball")
			data.entity = m_ballTemplateEntity->Clone();
		else if (createPacket.entityType == "projectile")
		{
			data.entity = m_ballTemplateEntity->Clone();
			data.entity->GetComponent<Ndk::NodeComponent>().SetScale(1.f / 5.f);
		}
		else
			return; //< TODO: Fallback

		auto& entityNode = data.entity->GetComponent<Ndk::NodeComponent>();
		entityNode.SetPosition(createPacket.position);
		entityNode.SetRotation(createPacket.rotation);

		Nz::Color textColor = (createPacket.name == "Lynix") ? Nz::Color::Cyan : Nz::Color::White;

		// Create entity name entity
		Nz::TextSpriteRef textSprite = Nz::TextSprite::New();
		textSprite->SetMaterial(Nz::MaterialLibrary::Get("SpaceshipText"));
		textSprite->Update(Nz::SimpleTextDrawer::Draw(createPacket.name, 96, 0U, textColor));
		textSprite->SetScale(0.01f);

		data.textEntity = m_stateData.world3D->CreateEntity();
		data.textEntity->AddComponent<Ndk::GraphicsComponent>().Attach(textSprite);
		data.textEntity->AddComponent<Ndk::NodeComponent>();

		if (createPacket.id == m_controlledEntity)
			ControlEntity(createPacket.id);
	}

	void GameState::OnDeleteEntity(ServerConnection*, const Packets::DeleteEntity& deletePacket)
	{
		ServerEntity& data = GetServerEntity(deletePacket.id);

		if (data.debugGhostEntity)
			data.debugGhostEntity->Kill();

		data.entity->Kill();
		data.textEntity->Kill();
		data.isValid = false;

		if (m_controlledEntity == deletePacket.id)
			m_controlledEntity = std::numeric_limits<std::size_t>::max();
	}

	void GameState::OnKeyPressed(const Nz::EventHandler* /*eventHandler*/, const Nz::WindowEvent::KeyEvent& event)
	{
		if (event.code == Nz::Keyboard::Return)
		{
			if (m_chatEnteringBox)
			{
				Nz::String text = m_chatEnteringBox->GetText();

				if (!text.IsEmpty())
				{
					Packets::PlayerChat chat;
					chat.text = text;
					m_stateData.server->SendPacket(chat);
				}

				m_chatEnteringBox->Destroy();
				m_chatEnteringBox = nullptr;
				return;
			}

			m_chatEnteringBox = m_stateData.canvas->Add<Ndk::TextAreaWidget>();
			m_chatEnteringBox->EnableBackground(true);
			m_chatEnteringBox->SetBackgroundColor(Nz::Color(0, 0, 0, 150));
			m_chatEnteringBox->SetSize({ float(m_stateData.window->GetSize().x), 40.f });
			m_chatEnteringBox->SetPosition({ 0.f, m_stateData.window->GetSize().y - m_chatEnteringBox->GetSize().y - 5.f, 0.f });
			m_chatEnteringBox->SetTextColor(Nz::Color::White);
			m_chatEnteringBox->SetFocus();
		}
		else if (event.code == Nz::Keyboard::Space)
		{
			m_stateData.server->SendPacket(Packets::PlayerShoot());
		}
		else if (event.code == Nz::Keyboard::F1)
		{
			m_syncEnabled = !m_syncEnabled;
			if (m_syncEnabled)
				PrintMessage("INFO: Sync enabled");
			else
				PrintMessage("INFO: Sync disabled");
		}
	}

	void GameState::UpdateInput(float elapsedTime)
	{
		if (m_controlledEntity == std::numeric_limits<std::size_t>::max())
			return;

		if (m_stateData.window->HasFocus() && !m_chatEnteringBox)
		{
			constexpr float acceleration = 30.f;
			constexpr float strafeSpeed = 20.f;
			constexpr float jumpSpeed = 20.f;
			constexpr float rollSpeed = 150.f;

			float forwardSpeedModifier = 0.f;
			if (Nz::Keyboard::IsKeyPressed(Nz::Keyboard::Z))
				forwardSpeedModifier += acceleration * elapsedTime;

			if (Nz::Keyboard::IsKeyPressed(Nz::Keyboard::S))
				forwardSpeedModifier -= acceleration * elapsedTime;

			float leftSpeedModifier = 0.f;
			if (Nz::Keyboard::IsKeyPressed(Nz::Keyboard::Q))
				leftSpeedModifier += strafeSpeed * elapsedTime;

			if (Nz::Keyboard::IsKeyPressed(Nz::Keyboard::D))
				leftSpeedModifier -= strafeSpeed * elapsedTime;

			float jumpSpeedModifier = 0.f;
			if (Nz::Keyboard::IsKeyPressed(Nz::Keyboard::LShift))
				jumpSpeedModifier += jumpSpeed * elapsedTime;

			if (Nz::Keyboard::IsKeyPressed(Nz::Keyboard::LControl))
				jumpSpeedModifier -= jumpSpeed * elapsedTime;

			float rollSpeedModifier = 0.f;
			if (Nz::Keyboard::IsKeyPressed(Nz::Keyboard::A))
				rollSpeedModifier += rollSpeed * elapsedTime;

			if (Nz::Keyboard::IsKeyPressed(Nz::Keyboard::E))
				rollSpeedModifier -= rollSpeed * elapsedTime;

			// Rotation
			if (Nz::Mouse::IsButtonPressed(Nz::Mouse::Right))
			{
				m_rotationDirection = Nz::Vector2f(m_rotationCursorPosition) / 2.f;
				if (m_rotationDirection.GetSquaredLength() < Nz::IntegralPow(20.f, 2))
					m_rotationDirection.MakeZero();
			}

			m_spaceshipSpeed.x = Nz::Clamp(m_spaceshipSpeed.x + forwardSpeedModifier, -20.f, 20.f);
			m_spaceshipSpeed.y = Nz::Clamp(m_spaceshipSpeed.y + leftSpeedModifier, -15.f, 15.f);
			m_spaceshipSpeed.z = Nz::Clamp(m_spaceshipSpeed.z + jumpSpeedModifier, -15.f, 15.f);
			m_spaceshipRotation.z = Nz::Clamp(m_spaceshipRotation.z + rollSpeedModifier, -100.f, 100.f);
		}

		m_spaceshipSpeed.x = Nz::Approach(m_spaceshipSpeed.x, 0.f, 10.f * elapsedTime);
		m_spaceshipSpeed.y = Nz::Approach(m_spaceshipSpeed.y, 0.f, 10.f * elapsedTime);
		m_spaceshipSpeed.z = Nz::Approach(m_spaceshipSpeed.z, 0.f, 10.f * elapsedTime);
		m_spaceshipRotation.z = Nz::Approach(m_spaceshipRotation.z, 0.f, 50.f * elapsedTime);

		//m_controlledSpaceship->GetComponent<Ndk::NodeComponent>().SetRotation(Nz::EulerAnglesf(-m_spaceshipSpeed.x / 5.f, 0.f, m_spaceshipSpeed.y));

		m_spaceshipRotation.x = Nz::Clamp(-m_rotationDirection.y, -200.f, 200.f);
		m_spaceshipRotation.y = Nz::Clamp(-m_rotationDirection.x, -200.f, 200.f);

		m_rotationDirection.x = Nz::Approach(m_rotationDirection.x, 0.f, 200.f * elapsedTime);
		m_rotationDirection.y = Nz::Approach(m_rotationDirection.y, 0.f, 200.f * elapsedTime);

		//m_spaceshipMovementNode.Move(elapsedTime * (m_spaceshipSpeed.x * Nz::Vector3f::Forward() + m_spaceshipSpeed.y * Nz::Vector3f::Left() + m_spaceshipSpeed.z * Nz::Vector3f::Up()));
		//m_spaceshipMovementNode.Rotate(Nz::EulerAnglesf(m_spaceshipRotation.x * elapsedTime, m_spaceshipRotation.y * elapsedTime, m_spaceshipRotation.z * elapsedTime));

		Nz::UInt64 serverTime = m_stateData.server->EstimateServerTime();

		ClientInput input;
		input.inputTime = serverTime;
		input.movement = m_spaceshipSpeed;
		input.rotation = m_spaceshipRotation;

		m_predictedInputs.push_back(input);

		// Client-side prediction
		//ApplyInput(clientNode, m_lastInputTime, m_predictedInputs.back());

		m_lastInputTime = serverTime;

		// Send input to server
		Packets::PlayerMovement movementPacket;
		movementPacket.inputTime = serverTime;
		movementPacket.direction = m_spaceshipSpeed;
		movementPacket.rotation = m_spaceshipRotation;

		m_stateData.server->SendPacket(movementPacket);
	}

	void GameState::ApplyInput(Nz::Node& node, Nz::UInt64 lastInputTime, const ClientInput& input)
	{
		float inputElapsedTime = (lastInputTime != 0) ? (input.inputTime - lastInputTime) / 1000.f : 0.f;

		Nz::Vector3f totalMovement = inputElapsedTime * (input.movement.x * Nz::Vector3f::Forward() + input.movement.y * Nz::Vector3f::Left() + input.movement.z * Nz::Vector3f::Up());
		Nz::EulerAnglesf totalRotation = Nz::EulerAnglesf(input.rotation.x * inputElapsedTime, input.rotation.y * inputElapsedTime, input.rotation.z * inputElapsedTime);

		node.Move(totalMovement);
		node.Rotate(totalRotation);

		/*
		std::cout << "Prediction:" << std::endl;
		std::cout << "At " << serverTime << ": Move by " << totalMovement << " (final pos: " << clientNode.GetPosition() << ")\n";
		std::cout << "   " << serverTime << ": Rotate by " << totalRotation << " (final rot: " << clientNode.GetRotation().ToEulerAngles() << ')' << std::endl;
		*/
	}

	void GameState::PrintMessage(const std::string& message)
	{
		std::cout << message << std::endl;

		m_chatLines.emplace_back(message);
		if (m_chatLines.size() > maxChatLines)
			m_chatLines.erase(m_chatLines.begin());

		m_chatBox->Clear();
		for (const Nz::String& message : m_chatLines)
			m_chatBox->AppendText(message + "\n");
	}
}
