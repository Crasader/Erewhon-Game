// Copyright (C) 2017 J�r�me Leclercq
// This file is part of the "Erewhon Shared" project
// For conditions of distribution and use, see copyright notice in LICENSE

#pragma once

#ifndef EREWHON_CLIENT_COMMANDSTORE_HPP
#define EREWHON_CLIENT_COMMANDSTORE_HPP

#include <Shared/CommandStore.hpp>

namespace ewn
{
	class ClientApplication;

	class ClientCommandStore final : public CommandStore
	{
		public:
			ClientCommandStore(ClientApplication* app);
			~ClientCommandStore() = default;
	};
}

#include <Client/ClientCommandStore.inl>

#endif // EREWHON_CLIENT_COMMANDSTORE_HPP