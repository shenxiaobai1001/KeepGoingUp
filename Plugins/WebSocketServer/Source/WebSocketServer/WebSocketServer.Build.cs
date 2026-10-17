// Copyright Pandores Marketplace 2025. All Righst Reserved.

using UnrealBuildTool;
using System.IO;

public class WebSocketServer : ModuleRules
{
	public WebSocketServer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Root of the uWebSockets folder.
		string uWebSocketsRootDir = Path.Combine(PluginDirectory, "Source/ThirdParty/uWebSockets/");

		// uWebSockets requires C++17+.
		CppStandard = CppStandardVersion.Cpp20;

		// Third-party libuv / uWebSockets: keep MSVC noise from failing the build on UE 5.8.
		bEnableExceptions = true;
		CppCompileWarningSettings.ShadowVariableWarningLevel = WarningLevel.Off;
		CppCompileWarningSettings.UnsafeTypeCastWarningLevel = WarningLevel.Off;

		// Engine dependencies.
		PublicDependencyModuleNames .AddRange(new string[] { "Core", });
		PrivateDependencyModuleNames.AddRange(new string[] { "CoreUObject", "Engine", "zlib", "OpenSSL" });

		// With Editor Engine dependencies.
		if (Target.Type == TargetRules.TargetType.Editor)
        {
			PrivateDependencyModuleNames.AddRange(new string[] { "ToolMenus", "Projects", "SlateCore" });
		}

		// MacOS system frameworks required.
		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicFrameworks.AddRange(new string[]
			{
				"CoreFoundation",
				"CoreServices"
			});
		}

		// Module includes.
		PublicIncludePaths .Add(Path.Combine(ModuleDirectory, "Public"));
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

		// uWebSockets includes.
		PrivateIncludePaths.Add(Path.Combine(uWebSocketsRootDir, "includes"));

		// Global definitions.
		PublicDefinitions.Add("WITH_WEBSOCKET_SERVER=1");
	}
}
