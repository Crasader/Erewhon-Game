// Copyright (C) 2018 Jérôme Leclercq
// This file is part of the "Erewhon Server" project
// For conditions of distribution and use, see copyright notice in LICENSE

#include <Client/ClientChatCommandStore.hpp>
#include <Nazara/Core/File.hpp>
#include <Shared/Protocol/Packets.hpp>
#include <Client/ClientApplication.hpp>
#include <iostream>

namespace ewn
{
	void ClientChatCommandStore::BuildStore(ClientApplication* app)
	{
		RegisterCommand("deletebot", &ClientChatCommandStore::HandleDeleteBot);
	}

	bool ClientChatCommandStore::HandleDeleteBot(ClientApplication* app, ServerConnection* server, std::string botName)
	{
		Packets::DeleteSpaceship packet;
		packet.spaceshipName = std::move(botName);

		server->SendPacket(packet);

		return true;
	}
}
