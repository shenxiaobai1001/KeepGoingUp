// Copyright Epic Games, Inc. All Rights Reserved.

#include "Prank/KeepGoingUpPrankBridge.h"
#include "Prank/KeepGoingUpPrankLibrary.h"
#include "KeepGoingUp.h"
#include "GameFramework/SaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

AKeepGoingUpPrankBridge::AKeepGoingUpPrankBridge()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	// Run before most actors so we still see Delay=0 entries that are removed next frame.
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	WebSocketServerClass = TSoftClassPtr<AActor>(
		FSoftObjectPath(TEXT("/Game/Game/Barrage/BP_WebSocketServer.BP_WebSocketServer_C")));
}

void AKeepGoingUpPrankBridge::BeginPlay()
{
	Super::BeginPlay();
	ResolveWebSocketServer();

	// Catch Delay=0 queue entries that live only between CallNormalQueue and the latent Delay resume.
	GetWorldTimerManager().SetTimer(
		FastPollTimer,
		this,
		&AKeepGoingUpPrankBridge::PollAllQueues,
		0.001f,
		true);
}

void AKeepGoingUpPrankBridge::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	PollAllQueues();
}

void AKeepGoingUpPrankBridge::PollAllQueues()
{
	if (!CachedWebSocketServer)
	{
		ResolveWebSocketServer();
		if (!CachedWebSocketServer)
		{
			return;
		}
	}

	PollQueueProperty(TEXT("NormalQueue"), SeenNormalKeys);
	PollQueueProperty(TEXT("BoxQueue"), SeenBoxKeys);
	PollQueueProperty(TEXT("SpecialQueue"), SeenSpecialKeys);
}

void AKeepGoingUpPrankBridge::ResolveWebSocketServer()
{
	UClass* Cls = WebSocketServerClass.LoadSynchronous();
	if (!Cls)
	{
		return;
	}

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(this, Cls, Found);
	CachedWebSocketServer = Found.Num() > 0 ? Found[0] : nullptr;
}

void AKeepGoingUpPrankBridge::NotifyPrankCallName(FName CallName, float DelaySeconds)
{
	const FString Name = CallName.ToString();
	int32 Count = 1;
	float Delay = DelaySeconds;
	int32 ConfiguredCount = 1;
	float ConfiguredDelay = 0.f;
	if (FindConfiguredPrank(Name, ConfiguredCount, ConfiguredDelay))
	{
		Count = ConfiguredCount;
		Delay = ConfiguredDelay;
	}
	SchedulePrankRepeats(Name, Count, Delay);
}

void AKeepGoingUpPrankBridge::PollQueueProperty(FName PropertyName, TSet<FString>& InOutSeenKeys)
{
	if (!CachedWebSocketServer)
	{
		return;
	}

	FProperty* Prop = CachedWebSocketServer->GetClass()->FindPropertyByName(PropertyName);
	FMapProperty* MapProp = CastField<FMapProperty>(Prop);
	if (!MapProp)
	{
		return;
	}

	void* MapPtr = MapProp->ContainerPtrToValuePtr<void>(CachedWebSocketServer);
	FScriptMapHelper Helper(MapProp, MapPtr);

	TSet<FString> NowKeys;
	TArray<int32> ConsumeIndices;

	for (int32 Index = 0; Index < Helper.GetMaxIndex(); ++Index)
	{
		if (!Helper.IsValidIndex(Index))
		{
			continue;
		}

		const uint8* KeyPtr = Helper.GetKeyPtr(Index);
		const uint8* ValPtr = Helper.GetValuePtr(Index);
		if (!KeyPtr)
		{
			continue;
		}

		FString CallNameStr;
		if (const FNameProperty* NameKey = CastField<FNameProperty>(MapProp->KeyProp))
		{
			CallNameStr = NameKey->GetPropertyValue(KeyPtr).ToString();
		}
		else if (const FStrProperty* StrKey = CastField<FStrProperty>(MapProp->KeyProp))
		{
			CallNameStr = StrKey->GetPropertyValue(KeyPtr);
		}
		else
		{
			continue;
		}

		if (!UKeepGoingUpPrankLibrary::IsGameplayPrankCallName(FName(*CallNameStr)))
		{
			continue;
		}

		if (InOutSeenKeys.Contains(CallNameStr))
		{
			NowKeys.Add(CallNameStr);
			continue;
		}

		float Delay = 0.f;
		int32 Count = 1;
		if (const FStructProperty* StructVal = CastField<FStructProperty>(MapProp->ValueProp))
		{
			ReadPrankNumbers(StructVal->Struct, ValPtr, Count, Delay);
		}

		// UI 倍率/间隔 live on the config row. The queued copy often has Count defaulted to 1 and Delay 0.
		int32 ConfiguredCount = 1;
		float ConfiguredDelay = 0.f;
		if (FindConfiguredPrank(CallNameStr, ConfiguredCount, ConfiguredDelay))
		{
			Count = ConfiguredCount;
			Delay = ConfiguredDelay;
		}

		SchedulePrankRepeats(CallNameStr, Count, Delay);
		ConsumeIndices.Add(Index);
	}

	for (int32 Offset = ConsumeIndices.Num() - 1; Offset >= 0; --Offset)
	{
		Helper.RemoveAt(ConsumeIndices[Offset]);
	}

	InOutSeenKeys = MoveTemp(NowKeys);
}

namespace
{
	bool NameIsCount(const FString& FieldName)
	{
		return FieldName.StartsWith(TEXT("Count")) && !FieldName.Contains(TEXT("Gift")) && !FieldName.Contains(TEXT("Like"));
	}

	bool NameIsDelay(const FString& FieldName)
	{
		return FieldName.StartsWith(TEXT("Delay"));
	}

	bool ParseNumberAfter(const FString& Text, int32 From, double& OutValue)
	{
		const int32 Eq = Text.Find(TEXT("="), ESearchCase::CaseSensitive, ESearchDir::FromStart, From);
		if (Eq == INDEX_NONE)
		{
			return false;
		}

		FString Num;
		for (int32 Index = Eq + 1; Index < Text.Len(); ++Index)
		{
			const TCHAR C = Text[Index];
			if ((C >= TEXT('0') && C <= TEXT('9')) || C == TEXT('.') || C == TEXT('-'))
			{
				Num.AppendChar(C);
			}
			else if (!Num.IsEmpty())
			{
				break;
			}
		}
		if (Num.IsEmpty())
		{
			return false;
		}
		OutValue = FCString::Atod(*Num);
		return true;
	}

	/** UserDefinedStruct export: Count_17_<guid>=10,Delay_20_<guid>=1.000000 */
	bool ParsePrankExport(const FString& Text, int32& OutCount, float& OutDelay)
	{
		bool bFound = false;
		int32 Search = 0;
		while (Search < Text.Len())
		{
			const int32 CountAt = Text.Find(TEXT("Count_"), ESearchCase::IgnoreCase, ESearchDir::FromStart, Search);
			const int32 DelayAt = Text.Find(TEXT("Delay_"), ESearchCase::IgnoreCase, ESearchDir::FromStart, Search);
			int32 At = INDEX_NONE;
			bool bDelay = false;
			if (CountAt != INDEX_NONE && (DelayAt == INDEX_NONE || CountAt < DelayAt))
			{
				At = CountAt;
			}
			else if (DelayAt != INDEX_NONE)
			{
				At = DelayAt;
				bDelay = true;
			}
			else
			{
				break;
			}

			const FString Slice = Text.Mid(At, 80);
			if (bDelay || (Slice.StartsWith(TEXT("Count_")) && !Slice.Contains(TEXT("Gift")) && !Slice.Contains(TEXT("Like"))))
			{
				double Value = 0.0;
				if (ParseNumberAfter(Text, At, Value))
				{
					if (bDelay)
					{
						OutDelay = static_cast<float>(Value);
					}
					else
					{
						OutCount = FMath::Max(1, FMath::RoundToInt(Value));
					}
					bFound = true;
				}
			}
			Search = At + 6;
		}
		return bFound;
	}
}

bool AKeepGoingUpPrankBridge::ReadPrankNumbers(const UScriptStruct* Struct, const void* StructPtr, int32& OutCount, float& OutDelay)
{
	if (!Struct || !StructPtr)
	{
		return false;
	}

	FString Exported;
	Struct->ExportText(Exported, StructPtr, nullptr, nullptr, PPF_None, nullptr);
	if (ParsePrankExport(Exported, OutCount, OutDelay))
	{
		return true;
	}

	bool bFound = false;
	for (TFieldIterator<FProperty> FieldIt(Struct); FieldIt; ++FieldIt)
	{
		const FString FieldName = FieldIt->GetName();
		if (!NameIsCount(FieldName) && !NameIsDelay(FieldName))
		{
			continue;
		}

		const void* ValuePtr = FieldIt->ContainerPtrToValuePtr<void>(StructPtr);
		double Value = 0.0;
		if (const FDoubleProperty* AsDouble = CastField<FDoubleProperty>(*FieldIt))
		{
			Value = AsDouble->GetPropertyValue(ValuePtr);
		}
		else if (const FFloatProperty* AsFloat = CastField<FFloatProperty>(*FieldIt))
		{
			Value = AsFloat->GetPropertyValue(ValuePtr);
		}
		else if (const FIntProperty* AsInt = CastField<FIntProperty>(*FieldIt))
		{
			Value = AsInt->GetPropertyValue(ValuePtr);
		}
		else if (const FInt64Property* AsInt64 = CastField<FInt64Property>(*FieldIt))
		{
			Value = static_cast<double>(AsInt64->GetPropertyValue(ValuePtr));
		}
		else
		{
			continue;
		}

		if (NameIsDelay(FieldName))
		{
			OutDelay = static_cast<float>(Value);
		}
		else
		{
			OutCount = FMath::Max(1, FMath::RoundToInt(Value));
		}
		bFound = true;
	}
	return bFound;
}

bool AKeepGoingUpPrankBridge::ReadCallNameFromStruct(const UScriptStruct* Struct, const void* StructPtr, FString& OutCallName)
{
	if (!Struct || !StructPtr)
	{
		return false;
	}

	for (TFieldIterator<FProperty> FieldIt(Struct); FieldIt; ++FieldIt)
	{
		const FString FieldName = FieldIt->GetName();
		if (!FieldName.StartsWith(TEXT("CallName")))
		{
			continue;
		}

		if (const FNameProperty* AsName = CastField<FNameProperty>(*FieldIt))
		{
			OutCallName = AsName->GetPropertyValue_InContainer(StructPtr).ToString();
			return !OutCallName.IsEmpty();
		}
		if (const FStrProperty* AsStr = CastField<FStrProperty>(*FieldIt))
		{
			OutCallName = AsStr->GetPropertyValue_InContainer(StructPtr);
			return !OutCallName.IsEmpty();
		}
		if (const FArrayProperty* AsArray = CastField<FArrayProperty>(*FieldIt))
		{
			FScriptArrayHelper Helper(AsArray, AsArray->ContainerPtrToValuePtr<void>(StructPtr));
			if (Helper.Num() <= 0)
			{
				return false;
			}
			if (const FStrProperty* InnerStr = CastField<FStrProperty>(AsArray->Inner))
			{
				OutCallName = InnerStr->GetPropertyValue(Helper.GetRawPtr(0));
				return !OutCallName.IsEmpty();
			}
			if (const FNameProperty* InnerName = CastField<FNameProperty>(AsArray->Inner))
			{
				OutCallName = InnerName->GetPropertyValue(Helper.GetRawPtr(0)).ToString();
				return !OutCallName.IsEmpty();
			}
		}
	}
	return false;
}

bool AKeepGoingUpPrankBridge::StructContainsCallName(const UScriptStruct* Struct, const void* StructPtr, const FString& CallName)
{
	if (!Struct || !StructPtr || CallName.IsEmpty())
	{
		return false;
	}

	for (TFieldIterator<FProperty> FieldIt(Struct); FieldIt; ++FieldIt)
	{
		const FString FieldName = FieldIt->GetName();
		if (!FieldName.StartsWith(TEXT("CallName")))
		{
			continue;
		}

		if (const FNameProperty* AsName = CastField<FNameProperty>(*FieldIt))
		{
			if (AsName->GetPropertyValue_InContainer(StructPtr).ToString().Equals(CallName))
			{
				return true;
			}
		}
		else if (const FStrProperty* AsStr = CastField<FStrProperty>(*FieldIt))
		{
			if (AsStr->GetPropertyValue_InContainer(StructPtr).Equals(CallName))
			{
				return true;
			}
		}
		else if (const FArrayProperty* AsArray = CastField<FArrayProperty>(*FieldIt))
		{
			FScriptArrayHelper Helper(AsArray, AsArray->ContainerPtrToValuePtr<void>(StructPtr));
			for (int32 Index = 0; Index < Helper.Num(); ++Index)
			{
				if (const FStrProperty* InnerStr = CastField<FStrProperty>(AsArray->Inner))
				{
					if (InnerStr->GetPropertyValue(Helper.GetRawPtr(Index)).Equals(CallName))
					{
						return true;
					}
				}
				else if (const FNameProperty* InnerName = CastField<FNameProperty>(AsArray->Inner))
				{
					if (InnerName->GetPropertyValue(Helper.GetRawPtr(Index)).ToString().Equals(CallName))
					{
						return true;
					}
				}
			}
		}
	}

	FString Exported;
	Struct->ExportText(Exported, StructPtr, nullptr, nullptr, PPF_None, nullptr);
	return Exported.Contains(CallName);
}

UObject* AKeepGoingUpPrankBridge::LoadPrankSaveGame() const
{
	static const TCHAR* SlotName = TEXT("Jv_PrankData");
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		return nullptr;
	}
	return UGameplayStatics::LoadGameFromSlot(SlotName, 0);
}

bool AKeepGoingUpPrankBridge::FindConfiguredPrankOnObject(const UObject* Object, const FString& CallName, int32& OutCount, float& OutDelay) const
{
	if (!Object)
	{
		return false;
	}

	const FName ArrayNames[] = {
		TEXT("NormalConfig"),
		TEXT("BoxConfig"),
		TEXT("SpecailConfig"),
		TEXT("SpecialConfig")
	};

	for (const FName ArrayName : ArrayNames)
	{
		FArrayProperty* ArrayProp = CastField<FArrayProperty>(Object->GetClass()->FindPropertyByName(ArrayName));
		const FStructProperty* InnerStruct = ArrayProp ? CastField<FStructProperty>(ArrayProp->Inner) : nullptr;
		if (!ArrayProp || !InnerStruct || !InnerStruct->Struct)
		{
			continue;
		}

		FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(const_cast<UObject*>(Object)));
		for (int32 Index = 0; Index < Helper.Num(); ++Index)
		{
			const void* Element = Helper.GetRawPtr(Index);
			if (!StructContainsCallName(InnerStruct->Struct, Element, CallName))
			{
				continue;
			}

			int32 Count = 1;
			float Delay = 0.f;
			FString Exported;
			InnerStruct->Struct->ExportText(Exported, Element, nullptr, nullptr, PPF_None, nullptr);
			if (!ParsePrankExport(Exported, Count, Delay))
			{
				ReadPrankNumbers(InnerStruct->Struct, Element, Count, Delay);
			}
			OutCount = FMath::Max(1, Count);
			OutDelay = FMath::Max(0.f, Delay);
			UE_LOG(LogKeepGoingUp, Log, TEXT("PrankBridge: config '%s' from %s.%s count=%d delay=%.2f"),
				*CallName, *GetNameSafe(Object), *ArrayName.ToString(), OutCount, OutDelay);
			return true;
		}
	}
	return false;
}

bool AKeepGoingUpPrankBridge::FindConfiguredPrank(const FString& CallName, int32& OutCount, float& OutDelay) const
{
	// PrankUI writes 倍率/间隔 into SaveGame slot Jv_PrankData. Prefer that over the live
	// WebSocketServer arrays, which often stay at Count=1 / Delay=0.
	if (UObject* SaveGame = LoadPrankSaveGame())
	{
		if (FindConfiguredPrankOnObject(SaveGame, CallName, OutCount, OutDelay))
		{
			return true;
		}
	}

	if (CachedWebSocketServer && FindConfiguredPrankOnObject(CachedWebSocketServer, CallName, OutCount, OutDelay))
	{
		return true;
	}

	if (CachedWebSocketServer)
	{
		for (TFieldIterator<FObjectProperty> FieldIt(CachedWebSocketServer->GetClass()); FieldIt; ++FieldIt)
		{
			if (UObject* Nested = FieldIt->GetObjectPropertyValue_InContainer(CachedWebSocketServer))
			{
				if (FindConfiguredPrankOnObject(Nested, CallName, OutCount, OutDelay))
				{
					return true;
				}
			}
		}
	}

	UE_LOG(LogKeepGoingUp, Warning, TEXT("PrankBridge: no config row for '%s' (using queue defaults)"), *CallName);
	return false;
}

void AKeepGoingUpPrankBridge::SchedulePrankRepeats(const FString& CallName, int32 Count, float IntervalSeconds)
{
	const int32 Repeats = FMath::Clamp(Count, 1, 30);
	const float Interval = FMath::Max(0.f, IntervalSeconds);
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	double& NextTime = NextPrankTime.FindOrAdd(CallName);
	if (NextTime < Now)
	{
		NextTime = Now;
	}

	UE_LOG(LogKeepGoingUp, Log, TEXT("PrankBridge: '%s' x%d interval=%.2f queued at %.2f"),
		*CallName, Repeats, Interval, NextTime - Now);

	for (int32 Index = 0; Index < Repeats; ++Index)
	{
		const float Wait = static_cast<float>(FMath::Max(0.0, NextTime - Now));
		NextTime += Interval;

		if (Wait <= KINDA_SMALL_NUMBER)
		{
			ExecutePrankNow(CallName);
			continue;
		}

		TWeakObjectPtr<AKeepGoingUpPrankBridge> WeakThis(this);
		const FString CapturedName = CallName;
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(
			Handle,
			FTimerDelegate::CreateLambda([WeakThis, CapturedName]()
			{
				if (AKeepGoingUpPrankBridge* Self = WeakThis.Get())
				{
					Self->ExecutePrankNow(CapturedName);
				}
			}),
			Wait,
			false);
	}
}

void AKeepGoingUpPrankBridge::ExecutePrankNow(const FString& CallName)
{
	const bool bOk = UKeepGoingUpPrankLibrary::ExecutePrankCallName(this, FName(*CallName));
	UE_LOG(LogKeepGoingUp, Log, TEXT("PrankBridge: execute '%s' → %s"), *CallName, bOk ? TEXT("ok") : TEXT("FAIL"));
}
