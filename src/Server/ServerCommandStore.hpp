// Copyright (C) 2017 J�r�me Leclercq
// This file is part of the "Erewhon Shared" project
// For conditions of distribution and use, see copyright notice in LICENSE

#pragma once

#ifndef EREWHON_SERVER_COMMANDSTORE_HPP
#define EREWHON_SERVER_COMMANDSTORE_HPP

#include <Shared/CommandStore.hpp>

namespace ewn
{
	class ServerApplication;

	class ServerCommandStore final : public CommandStore
	{
		public:
			ServerCommandStore(ServerApplication* app);
			~ServerCommandStore() = default;
	};
}

#include <Server/ServerCommandStore.inl>

#endif // EREWHON_SERVER_COMMANDSTORE_HPP