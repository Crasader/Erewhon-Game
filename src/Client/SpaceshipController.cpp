// Copyright (C) 2017 Jérôme Leclercq
// This file is part of the "Erewhon Shared" project
// For conditions of distribution and use, see copyright notice in LICENSE

#include <Client/SpaceshipController.hpp>
#include <Nazara/Core/CallOnExit.hpp>
#include <NDK/Components/CameraComponent.hpp>
#include <NDK/Components/GraphicsComponent.hpp>
#include <NDK/Components/NodeComponent.hpp>
#include <NDK/LuaAPI.hpp>
#include <Client/ClientApplication.hpp>

namespace ewn
{
	SpaceshipController::SpaceshipController(ClientApplication* app, ServerConnection* server, Nz::RenderWindow& window, Ndk::World& world2D, const Ndk::EntityHandle& camera, const Ndk::EntityHandle& spaceship) :
	m_app(app),
	m_server(server),
	m_window(window),
	m_camera(camera),
	m_spaceship(spaceship),
	m_lastShootTime(0),
	m_executeScript(false),
	m_inputAccumulator(0.f),
	m_updateAccumulator(0.f)
	{
		m_shootSound.SetBuffer(Nz::SoundBufferLibrary::Get("ShootSound"));
		m_shootSound.EnableSpatialization(false);

		Nz::Vector2ui windowSize = m_window.GetSize();
		Nz::Mouse::SetPosition(windowSize.x / 2, windowSize.y / 2, m_window);

		Nz::EventHandler& eventHandler = m_window.GetEventHandler();

		// Connect every slot
		m_onKeyPressedSlot.Connect(eventHandler.OnKeyPressed, this, &SpaceshipController::OnKeyPressed);
		m_onKeyReleasedSlot.Connect(eventHandler.OnKeyReleased, this, &SpaceshipController::OnKeyReleased);
		m_onIntegrityUpdateSlot.Connect(m_server->OnIntegrityUpdate, this, &SpaceshipController::OnIntegrityUpdate);
		m_onLostFocusSlot.Connect(eventHandler.OnLostFocus, this, &SpaceshipController::OnLostFocus);
		m_onMouseButtonPressedSlot.Connect(eventHandler.OnMouseButtonPressed, this, &SpaceshipController::OnMouseButtonPressed);
		m_onMouseButtonReleasedSlot.Connect(eventHandler.OnMouseButtonReleased, this, &SpaceshipController::OnMouseButtonReleased);
		m_onMouseMovedSlot.Connect(eventHandler.OnMouseMoved, this, &SpaceshipController::OnMouseMoved);
		m_onTargetChangeSizeSlot.Connect(m_window.OnRenderTargetSizeChange, this, &SpaceshipController::OnRenderTargetSizeChange);

		LoadSprites(world2D);
		OnRenderTargetSizeChange(&m_window);

		// Load client script
		LoadScript();
	}

	SpaceshipController::~SpaceshipController()
	{
	}

	void SpaceshipController::Update(float elapsedTime)
	{
		// Update and send input
		m_inputAccumulator += elapsedTime;

		constexpr float inputSendInterval = 1.f / 60.f;
		if (m_inputAccumulator > inputSendInterval)
		{
			m_inputAccumulator -= inputSendInterval;
			UpdateInput(inputSendInterval);
		}

		m_updateAccumulator += elapsedTime;

		constexpr float updateScriptInterval = 1.f / 60.f;
		if (m_updateAccumulator > updateScriptInterval)
		{
			//m_updateAccumulator -= updateScriptInterval;
			if (m_executeScript)
			{
				if (m_controlScript.GetGlobal("OnUpdate") == Nz::LuaType_Function)
				{
					auto& spaceshipNode = m_spaceship->GetComponent<Ndk::NodeComponent>();
					Nz::Vector3f position = spaceshipNode.GetPosition();
					Nz::Quaternionf rotation = spaceshipNode.GetRotation();

					m_controlScript.PushTable(0, 3);
						m_controlScript.PushField("x", position.x);
						m_controlScript.PushField("y", position.y);
						m_controlScript.PushField("z", position.z);

					m_controlScript.PushTable(0, 4);
						m_controlScript.PushField("w", rotation.w);
						m_controlScript.PushField("x", rotation.x);
						m_controlScript.PushField("y", rotation.y);
						m_controlScript.PushField("z", rotation.z);

					if (!m_controlScript.Call(2))
						std::cerr << "OnUpdate failed: " << m_controlScript.GetLastError() << std::endl;
				}
			}
		}

		// Compute crosshair position (according to projectile path projection)
		auto& cameraComponent = m_camera->GetComponent<Ndk::CameraComponent>();
		auto& entityNode = m_spaceship->GetComponent<Ndk::NodeComponent>();

		Nz::Vector4f worldPosition(entityNode.GetPosition() + entityNode.GetForward() * 150.f, 1.f);
		worldPosition = cameraComponent.GetViewMatrix() * worldPosition;
		worldPosition = cameraComponent.GetProjectionMatrix() * worldPosition;
		worldPosition /= worldPosition.w;

		Nz::Vector3f screenPosition(worldPosition.x * 0.5f + 0.5f, -worldPosition.y * 0.5f + 0.5f, worldPosition.z * 0.5f + 0.5f);
		screenPosition.x *= m_window.GetSize().x;
		screenPosition.y *= m_window.GetSize().y;

		m_crosshairEntity->GetComponent<Ndk::NodeComponent>().SetPosition(screenPosition);
	}

	void SpaceshipController::OnKeyPressed(const Nz::EventHandler* /*eventHandler*/, const Nz::WindowEvent::KeyEvent& event)
	{
		if (event.code == Nz::Keyboard::F5)
			LoadScript();
		else
		{
			if (m_executeScript)
			{
				if (m_controlScript.GetGlobal("OnKeyPressed") == Nz::LuaType_Function)
				{
					PushToLua(event);

					if (!m_controlScript.Call(1))
						std::cerr << "OnKeyPressed failed: " << m_controlScript.GetLastError() << std::endl;
				}
			}
		}
	}

	void SpaceshipController::OnKeyReleased(const Nz::EventHandler* /*eventHandler*/, const Nz::WindowEvent::KeyEvent& event)
	{
		if (m_executeScript)
		{
			if (m_controlScript.GetGlobal("OnKeyReleased") == Nz::LuaType_Function)
			{
				PushToLua(event);

				if (!m_controlScript.Call(1))
					std::cerr << "OnKeyReleased failed: " << m_controlScript.GetLastError() << std::endl;
			}
		}
	}

	void SpaceshipController::OnLostFocus(const Nz::EventHandler* /*eventHandler*/)
	{
		if (m_executeScript)
		{
			if (m_controlScript.GetGlobal("OnLostFocus") == Nz::LuaType_Function)
			{
				if (!m_controlScript.Call(0))
					std::cerr << "OnLostFocus failed: " << m_controlScript.GetLastError() << std::endl;
			}
		}
	}

	void SpaceshipController::OnIntegrityUpdate(ServerConnection* /*server*/, const Packets::IntegrityUpdate& integrityUpdate)
	{
		float integrityPct = integrityUpdate.integrityValue / 255.f;

		m_healthBarSprite->SetSize({ integrityPct * 256.f, 32.f });

		if (m_executeScript)
		{
			if (m_controlScript.GetGlobal("OnIntegrityUpdate") == Nz::LuaType_Function)
			{
				m_controlScript.Push(integrityPct);

				if (!m_controlScript.Call(1))
					std::cerr << "OnIntegrityUpdate failed: " << m_controlScript.GetLastError() << std::endl;
			}
		}
	}

	void SpaceshipController::OnMouseButtonPressed(const Nz::EventHandler* /*eventHandler*/, const Nz::WindowEvent::MouseButtonEvent& event)
	{
		if (m_executeScript)
		{
			if (m_controlScript.GetGlobal("OnMouseButtonPressed") == Nz::LuaType_Function)
			{
				PushToLua(event);

				if (!m_controlScript.Call(1))
					std::cerr << "OnMouseButtonPressed failed: " << m_controlScript.GetLastError() << std::endl;
			}
		}
	}

	void SpaceshipController::OnMouseButtonReleased(const Nz::EventHandler* /*eventHandler*/, const Nz::WindowEvent::MouseButtonEvent& event)
	{
		if (m_executeScript)
		{
			if (m_controlScript.GetGlobal("OnMouseButtonReleased") == Nz::LuaType_Function)
			{
				PushToLua(event);

				if (!m_controlScript.Call(1))
					std::cerr << "OnMouseButtonReleased failed: " << m_controlScript.GetLastError() << std::endl;
			}
		}
	}

	void SpaceshipController::OnMouseMoved(const Nz::EventHandler* /*eventHandler*/, const Nz::WindowEvent::MouseMoveEvent& event)
	{
		if (m_executeScript)
		{
			if (m_controlScript.GetGlobal("OnMouseMoved") == Nz::LuaType_Function)
			{
				PushToLua(event);

				if (!m_controlScript.Call(1))
					std::cerr << "OnMouseMoved failed: " << m_controlScript.GetLastError() << std::endl;
			}
		}
	}

	void SpaceshipController::OnRenderTargetSizeChange(const Nz::RenderTarget* renderTarget)
	{
		m_healthBarEntity->GetComponent<Ndk::NodeComponent>().SetPosition({ renderTarget->GetSize().x - 300.f, renderTarget->GetSize().y - 70.f, 0.f });
	}

	void SpaceshipController::LoadScript()
	{
		m_controlScript = Nz::LuaInstance();
		m_executeScript = true;

		std::cout << "Loading spaceshipcontroller.lua" << std::endl;
		if (!m_controlScript.ExecuteFromFile("spaceshipcontroller.lua"))
		{
			std::cerr << "Failed to load spaceshipcontroller.lua: " << m_controlScript.GetLastError() << std::endl;
			m_executeScript = false;
			return;
		}

		// Check existence of some functions
		if (m_controlScript.GetGlobal("UpdateInput") != Nz::LuaType_Function)
		{
			std::cerr << "spaceshipcontroller.lua: UpdateInput is not a valid function!" << std::endl;
			m_executeScript = false;
		}
		m_controlScript.Pop();

		m_controlScript.PushFunction([this](Nz::LuaState& state) -> int
		{
			if (state.CheckBoolean(1))
			{
				m_rotationCursorOrigin = Nz::Mouse::GetPosition(m_window);

				m_window.SetCursor(Nz::SystemCursor_None);
				m_cursorEntity->Enable();
				m_cursorOrientationSprite->SetColor(Nz::Color(255, 255, 255, 0));
			}
			else
			{
				m_window.SetCursor(Nz::SystemCursor_Default);
				Nz::Mouse::SetPosition(m_rotationCursorOrigin.x, m_rotationCursorOrigin.y, m_window);

				m_cursorEntity->Disable();
			}
			return 0;
		});
		m_controlScript.SetGlobal("ShowRotationCursor");

		m_controlScript.PushFunction([this](Nz::LuaState& state) -> int
		{
			Shoot();
			return 0;
		});
		m_controlScript.SetGlobal("Shoot");

		m_controlScript.PushFunction([this](Nz::LuaState& state) -> int
		{
			Nz::Vector2ui windowCenter = m_window.GetSize() / 2;
			Nz::Mouse::SetPosition(windowCenter.x, windowCenter.y, m_window);

			return 0;
		});
		m_controlScript.SetGlobal("RecenterMouse");

		m_controlScript.PushFunction([this](Nz::LuaState& state) -> int
		{
			int argIndex = 1;
			Nz::Vector2f position = state.Check<Nz::Vector2f>(&argIndex);
			float angle = state.Check<float>(&argIndex);
			float alpha = state.Check<float>(&argIndex);

			Ndk::NodeComponent& cursorNode = m_cursorEntity->GetComponent<Ndk::NodeComponent>();
			cursorNode.SetPosition(position);
			cursorNode.SetRotation(Nz::EulerAnglesf(0.f, 0.f, angle));

			Nz::UInt8 alphaValue = Nz::UInt8(Nz::Clamp(alpha * 255.f, 0.f, 255.f));
			m_cursorOrientationSprite->SetColor(Nz::Color(255, 255, 255, alphaValue));

			return 0;
		});
		m_controlScript.SetGlobal("UpdateRotationCursor");

		m_controlScript.PushFunction([this](Nz::LuaState& state) -> int
		{
			int argIndex = 1;
			Nz::Vector3f position = state.Check<Nz::Vector3f>(&argIndex);
			Nz::Quaternionf rotation = state.Check<Nz::Quaternionf>(&argIndex);

			auto& cameraNode = m_camera->GetComponent<Ndk::NodeComponent>();
			cameraNode.SetPosition(position);
			cameraNode.SetRotation(rotation);

			return 0;
		});
		m_controlScript.SetGlobal("UpdateCamera");


		if (m_executeScript)
		{
			if (m_controlScript.GetGlobal("Init") == Nz::LuaType_Function)
			{
				if (!m_controlScript.Call(0))
					std::cerr << "Init failed: " << m_controlScript.GetLastError() << std::endl;
			}
		}
	}

	void SpaceshipController::LoadSprites(Ndk::World& world2D)
	{
		// Crosshair
		{
			Nz::MaterialRef cursorMat = Nz::Material::New("Translucent2D");
			cursorMat->SetDiffuseMap("Assets/weapons/crosshair.png");

			Nz::SpriteRef crosshairSprite = Nz::Sprite::New();
			crosshairSprite->SetMaterial(cursorMat);
			crosshairSprite->SetSize({ 32.f, 32.f });
			crosshairSprite->SetOrigin(crosshairSprite->GetSize() / 2.f);

			m_crosshairEntity = world2D.CreateEntity();
			m_crosshairEntity->AddComponent<Ndk::GraphicsComponent>().Attach(crosshairSprite);
			m_crosshairEntity->AddComponent<Ndk::NodeComponent>();
		}

		// Health bar
		{
			Nz::MaterialRef healthBarMat = Nz::Material::New();
			healthBarMat->EnableDepthBuffer(false);
			healthBarMat->EnableFaceCulling(false);

			Nz::SpriteRef healthBarBackground = Nz::Sprite::New();
			healthBarBackground->SetColor(Nz::Color::Black);
			healthBarBackground->SetOrigin({ 2.f, 2.f, 0.f });
			healthBarBackground->SetMaterial(healthBarMat);
			healthBarBackground->SetSize({ 256.f + 4.f, 32.f + 4.f });

			Nz::SpriteRef healthBarEmptySprite = Nz::Sprite::New();
			healthBarEmptySprite->SetSize({ 256.f, 32.f });
			healthBarEmptySprite->SetMaterial(healthBarMat);

			m_healthBarSprite = Nz::Sprite::New();
			m_healthBarSprite->SetCornerColor(Nz::RectCorner_LeftTop, Nz::Color::Orange);
			m_healthBarSprite->SetCornerColor(Nz::RectCorner_RightTop, Nz::Color::Orange);
			m_healthBarSprite->SetCornerColor(Nz::RectCorner_LeftBottom, Nz::Color::Yellow);
			m_healthBarSprite->SetCornerColor(Nz::RectCorner_RightBottom, Nz::Color::Yellow);
			m_healthBarSprite->SetMaterial(healthBarMat);
			m_healthBarSprite->SetSize({ 256.f, 32.f });

			m_healthBarEntity = world2D.CreateEntity();
			auto& crosshairGhx = m_healthBarEntity->AddComponent<Ndk::GraphicsComponent>();
			m_healthBarEntity->AddComponent<Ndk::NodeComponent>();

			crosshairGhx.Attach(healthBarBackground, 0);
			crosshairGhx.Attach(healthBarEmptySprite, 1);
			crosshairGhx.Attach(m_healthBarSprite, 2);
		}

		// Movement cursor
		{
			Nz::MaterialRef cursorMat = Nz::Material::New("Translucent2D");
			cursorMat->SetDiffuseMap("Assets/cursor/orientation.png");

			m_cursorOrientationSprite = Nz::Sprite::New();
			m_cursorOrientationSprite->SetMaterial(cursorMat);
			m_cursorOrientationSprite->SetSize({ 32.f, 32.f });
			m_cursorOrientationSprite->SetOrigin(m_cursorOrientationSprite->GetSize() / 2.f);

			m_cursorEntity = world2D.CreateEntity();
			m_cursorEntity->AddComponent<Ndk::GraphicsComponent>().Attach(m_cursorOrientationSprite);
			auto& cursorNode = m_cursorEntity->AddComponent<Ndk::NodeComponent>();
			cursorNode.SetParent(m_crosshairEntity);
			cursorNode.SetPosition({ 200.f, 200.f, 0.f });

			m_cursorEntity->Disable();
		}
	}

	void SpaceshipController::Shoot()
	{
		Nz::UInt64 currentTime = m_app->GetAppTime();
		if (currentTime - m_lastShootTime < 500)
			return;

		m_lastShootTime = currentTime;
		m_shootSound.Play();

		m_server->SendPacket(Packets::PlayerShoot());
	}

	void SpaceshipController::UpdateInput(float elapsedTime)
	{
		if (m_executeScript)
		{
			m_controlScript.GetGlobal("UpdateInput");
			m_controlScript.Push(elapsedTime);
			if (m_controlScript.Call(1))
			{
				// Use some RRID
				Nz::CallOnExit resetLuaStack([&]()
				{
					m_controlScript.Pop(m_controlScript.GetStackTop());
				});
				Nz::Vector3f movement;
				Nz::Vector3f rotation;
				try
				{
					int index = 1;
					movement = m_controlScript.Check<Nz::Vector3f>(&index);
					rotation = m_controlScript.Check<Nz::Vector3f>(&index);
				}
				catch (const std::exception&)
				{
					std::cerr << "UpdateInput failed: returned values are invalid" << std::endl;
					return;
				}

				// Send input to server
				Packets::PlayerMovement movementPacket;
				movementPacket.inputTime = m_server->EstimateServerTime();
				movementPacket.direction = movement;
				movementPacket.rotation = rotation;

				m_server->SendPacket(movementPacket);
			}
			else
				std::cerr << "UpdateInput failed: " << m_controlScript.GetLastError() << std::endl;
		}
	}

	void SpaceshipController::PushToLua(const Nz::WindowEvent::KeyEvent& event)
	{
		m_controlScript.PushTable(0, 6);
		{
			std::string keyName;
#define HandleKey(KeyName) case Nz::Keyboard::##KeyName : keyName = #KeyName ; break;
			switch (event.code)
			{
				HandleKey(Undefined)

				// Lettres
				HandleKey(A)
				HandleKey(B)
				HandleKey(C)
				HandleKey(D)
				HandleKey(E)
				HandleKey(F)
				HandleKey(G)
				HandleKey(H)
				HandleKey(I)
				HandleKey(J)
				HandleKey(K)
				HandleKey(L)
				HandleKey(M)
				HandleKey(N)
				HandleKey(O)
				HandleKey(P)
				HandleKey(Q)
				HandleKey(R)
				HandleKey(S)
				HandleKey(T)
				HandleKey(U)
				HandleKey(V)
				HandleKey(W)
				HandleKey(X)
				HandleKey(Y)
				HandleKey(Z)

				// Functional keys
				HandleKey(F1)
				HandleKey(F2)
				HandleKey(F3)
				HandleKey(F4)
				HandleKey(F5)
				HandleKey(F6)
				HandleKey(F7)
				HandleKey(F8)
				HandleKey(F9)
				HandleKey(F10)
				HandleKey(F11)
				HandleKey(F12)
				HandleKey(F13)
				HandleKey(F14)
				HandleKey(F15)

				// Directional keys
				HandleKey(Down)
				HandleKey(Left)
				HandleKey(Right)
				HandleKey(Up)

				// Numerical pad
				HandleKey(Add)
				HandleKey(Decimal)
				HandleKey(Divide)
				HandleKey(Multiply)
				HandleKey(Numpad0)
				HandleKey(Numpad1)
				HandleKey(Numpad2)
				HandleKey(Numpad3)
				HandleKey(Numpad4)
				HandleKey(Numpad5)
				HandleKey(Numpad6)
				HandleKey(Numpad7)
				HandleKey(Numpad8)
				HandleKey(Numpad9)
				HandleKey(Subtract)

				// Various
				HandleKey(Backslash)
				HandleKey(Backspace)
				HandleKey(Clear)
				HandleKey(Comma)
				HandleKey(Dash)
				HandleKey(Delete)
				HandleKey(End)
				HandleKey(Equal)
				HandleKey(Escape)
				HandleKey(Home)
				HandleKey(Insert)
				HandleKey(LAlt)
				HandleKey(LBracket)
				HandleKey(LControl)
				HandleKey(LShift)
				HandleKey(LSystem)
				HandleKey(Num0)
				HandleKey(Num1)
				HandleKey(Num2)
				HandleKey(Num3)
				HandleKey(Num4)
				HandleKey(Num5)
				HandleKey(Num6)
				HandleKey(Num7)
				HandleKey(Num8)
				HandleKey(Num9)
				HandleKey(PageDown)
				HandleKey(PageUp)
				HandleKey(Pause)
				HandleKey(Period)
				HandleKey(Print)
				HandleKey(PrintScreen)
				HandleKey(Quote)
				HandleKey(RAlt)
				HandleKey(RBracket)
				HandleKey(RControl)
				HandleKey(Return)
				HandleKey(RShift)
				HandleKey(RSystem)
				HandleKey(Semicolon)
				HandleKey(Slash)
				HandleKey(Space)
				HandleKey(Tab)
				HandleKey(Tilde)

				// Navigator keys
				HandleKey(Browser_Back)
				HandleKey(Browser_Favorites)
				HandleKey(Browser_Forward)
				HandleKey(Browser_Home)
				HandleKey(Browser_Refresh)
				HandleKey(Browser_Search)
				HandleKey(Browser_Stop)

				// Lecture control keys
				HandleKey(Media_Next)
				HandleKey(Media_Play)
				HandleKey(Media_Previous)
				HandleKey(Media_Stop)

				// Volume control keys
				HandleKey(Volume_Down)
				HandleKey(Volume_Mute)
				HandleKey(Volume_Up)

				// Locking keys
				HandleKey(CapsLock)
				HandleKey(NumLock)
				HandleKey(ScrollLock)

				default: keyName = "Unknown"; break;
			}
#undef HandleKey

			m_controlScript.PushField("key", keyName);
			m_controlScript.PushField("alt", event.alt);
			m_controlScript.PushField("control", event.control);
			m_controlScript.PushField("repeated", event.repeated);
			m_controlScript.PushField("shift", event.shift);
			m_controlScript.PushField("system", event.system);
		}
	}

	void SpaceshipController::PushToLua(const Nz::WindowEvent::MouseButtonEvent& event)
	{
		m_controlScript.PushTable(0, 3);
		{
			const char* buttonName;
			switch (event.button)
			{
				case Nz::Mouse::Left:
					buttonName = "Left";
					break;

				case Nz::Mouse::Middle:
					buttonName = "Middle";
					break;

				case Nz::Mouse::Right:
					buttonName = "Right";
					break;

				case Nz::Mouse::XButton1:
					buttonName = "XButton1";
					break;

				case Nz::Mouse::XButton2:
					buttonName = "XButton2";
					break;

				default:
					buttonName = "Unknown";
					break;
			}

			m_controlScript.PushField("button", std::string(buttonName));
			m_controlScript.PushField("x", event.x);
			m_controlScript.PushField("y", event.y);
		}
	}

	void SpaceshipController::PushToLua(const Nz::WindowEvent::MouseMoveEvent& event)
	{
		m_controlScript.PushTable(0, 4);
			m_controlScript.PushField("deltaX", event.deltaX);
			m_controlScript.PushField("deltaY", event.deltaY);
			m_controlScript.PushField("x", event.x);
			m_controlScript.PushField("y", event.y);
	}
}