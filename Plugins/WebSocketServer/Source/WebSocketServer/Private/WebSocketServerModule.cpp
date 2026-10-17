// Copyright Pandores Marketplace 2025. All Righst Reserved.

#include "WebSocketServerModule.h"

DEFINE_LOG_CATEGORY(LogWebSocketServer);

#define LOCTEXT_NAMESPACE "FWebSocketServerModule"

#if WITH_EDITOR
void FWebSocketServerModule::AddWebSocketClientButton()
{
	static const FName WindowMenuName("MainFrame.MainMenu.Window");

	UToolMenu* const WindowMenu = UToolMenus::Get()->RegisterMenu(WindowMenuName, NAME_None, EMultiBoxType::Menu, false);

	check(WindowMenu);

	FToolUIAction Action;
	
	Action.ExecuteAction.BindRaw(this, &FWebSocketServerModule::OpenWebSocketClient);

	WindowMenu->AddMenuEntry
	(
		TEXT("WebSocket"),
		FToolMenuEntry::InitMenuEntry
		(
			TEXT("WebSocket"),
			LOCTEXT("MenuWebSocket", "WebSocket Client"),
			LOCTEXT("WebSocket_Tooltip", "Open the WebSocket Client."),
			FSlateIcon("EditorStyle", "GraphEditor.Comment_16x"),
			MoveTemp(Action)
		)
	);
}
#endif // WITH_EDITOR

#if WITH_EDITOR
void FWebSocketServerModule::OpenWebSocketClient(const FToolMenuContext& Context)
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("WebSocketServer"));

	if (Plugin)
	{
		const FString ClientLocation = FPaths::ConvertRelativePathToFull(
			Plugin->GetBaseDir() / TEXT("Source/ThirdParty/uWebSockets/example/WebSocketClient.html"));

		FString Error;
		FPlatformProcess::LaunchURL(*ClientLocation, TEXT(""), &Error);
	
		if (!Error.IsEmpty())
		{
			UE_LOG(LogWebSocketServer, Error, TEXT("Failed to launch WebSocket Client. Error: %s"), *Error);
		}
	}
}
#endif // WITH_EDITOR

void FWebSocketServerModule::StartupModule()
{
#if WITH_EDITOR
	AddWebSocketClientButton();
#endif
}

void FWebSocketServerModule::ShutdownModule()
{

}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FWebSocketServerModule, WebSocketServer)