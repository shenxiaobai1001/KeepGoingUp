// Copyright Pandores Marketplace 2025. All Righst Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"
#include "WebSocketServer.h"

#if PLATFORM_WINDOWS
#	include "Windows/AllowWindowsPlatformTypes.h"
#	pragma warning(push)
#	pragma warning(disable : 4138)
#	pragma warning(disable : 4701)
#	pragma warning(disable : 4706)
#	pragma warning(disable : 4582)
#endif // PLATFORM_WINDOWS

THIRD_PARTY_INCLUDES_START
#	include "App.h"
THIRD_PARTY_INCLUDES_END

#if PLATFORM_WINDOWS
#	pragma warning(pop)
#	include "Windows/HideWindowsPlatformTypes.h"
#endif // PLATFORM_WINDOWS



