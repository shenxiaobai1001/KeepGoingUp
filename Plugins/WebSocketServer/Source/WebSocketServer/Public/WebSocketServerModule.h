// Copyright Pandores Marketplace 2025. All Righst Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#	include "ToolMenus.h"
#	include "Interfaces/IPluginManager.h"
#endif // WITH_EDITOR

DECLARE_LOG_CATEGORY_EXTERN(LogWebSocketServer, Log, All);

#if PLATFORM_WINDOWS
#	pragma comment(lib, "Userenv.lib")
#endif

class FWebSocketServerModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
#if WITH_EDITOR
	void AddWebSocketClientButton();
	void OpenWebSocketClient(const FToolMenuContext& Context);
#endif // WITH_EDITOR
};
