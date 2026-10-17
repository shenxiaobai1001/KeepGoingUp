// Copyright Pandores Marketplace 2025. All Righst Reserved.

#include "WebSocketServer.h"
#include "WebSocketServerInternal.h"

UWebSocketServer* UWebSocketServer::CreateWebSocketServer()
{
	return NewObject<UWebSocketServer>();
}

UWebSocketServer* UWebSocketServer::CreateSecureWebSocketServer(
	FString KeyFile,
	FString CertFile,
	FString PassPhrase,
	FString DhParamsFile,
	FString CaFileName)
{
	UWebSocketServer* const Server = NewObject<UWebSocketServer>();

	Server->Internal = MakeShared<TWebSocketServerInternal<true>, ESPMode::ThreadSafe>();

	Server->Internal->SetSSLOptions(
		MoveTemp(KeyFile),
		MoveTemp(CertFile),
		MoveTemp(PassPhrase),
		MoveTemp(DhParamsFile),
		MoveTemp(CaFileName));

	return Server;
}

UWebSocketServer::UWebSocketServer()
	: Internal(MakeShared<TWebSocketServerInternal<false>, ESPMode::ThreadSafe>())
{
}

UWebSocketServer::~UWebSocketServer()
{
	if (GetServerState() != EWebSocketServerState::Closed)
	{
		Internal->Close();
	}
}

void UWebSocketServer::Listen(FString Host, FString URI, const int32 Port, FOnWebSocketServerListening Callback)
{
	Internal->OnOpened().BindUObject(this, &UWebSocketServer::InternalOnWebSocketOpened);
	Internal->OnMessage().BindUObject(this, &UWebSocketServer::InternalOnWebSocketMessage);
	Internal->OnPing().BindUObject(this, &UWebSocketServer::InternalOnWebSocketMessage);
	Internal->OnPong().BindUObject(this, &UWebSocketServer::InternalOnWebSocketMessage);
	Internal->OnClosed().BindUObject(this, &UWebSocketServer::InternalOnWebSocketClosed);

	Internal->Listen(MoveTemp(Host), MoveTemp(URI), Port, MoveTemp(Callback),
		FOnServerClosed::CreateUObject(this, &UWebSocketServer::InternalOnServerClosed));
}

void UWebSocketServer::Listen(FString URI, const int32 Port, FOnWebSocketServerListening Callback)
{
	Listen(TEXT("0.0.0.0"), MoveTemp(URI), Port, MoveTemp(Callback));
}

void UWebSocketServer::StopListening()
{
	if (Internal->GetServerState() == EWebSocketServerState::Running)
	{
		Internal->Close();
	}
}

void UWebSocketServer::SetIdleTimeout(const int64 InIdleTimeout)
{
	ensureMsgf(InIdleTimeout == 0 || InIdleTimeout >= 8, TEXT("Idle timeout must be 0 or in [8;inf[. Provided: %d."), InIdleTimeout);

	Internal->SetIdleTimeout(InIdleTimeout);
}

void UWebSocketServer::SetMaxLifetime(const int64 InMaxLifetime)
{
	Internal->SetMaxLifetime(InMaxLifetime);
}

void UWebSocketServer::SetCompression(const EWebSocketCompressOptions InCompression)
{
	Internal->SetCompression(InCompression);
}

void UWebSocketServer::SetSendPingsAutomatically(const bool bInSendPingsAutomatically)
{
	Internal->SetSendPingsAutomatically(bInSendPingsAutomatically);
}

void UWebSocketServer::SetResetIdleTimeoutOnSend(const bool bInResetIdleTimeoutOnSend)
{
	Internal->SetResetIdleTimeoutOnSend(bInResetIdleTimeoutOnSend);
}

void UWebSocketServer::SetCloseOnBackpressureLimit(const bool bInCloseOnBackpressureLimit)
{
	Internal->SetCloseOnBackpressureLimit(bInCloseOnBackpressureLimit);
}

void UWebSocketServer::SetMaxPayloadLength(const int64 InMaxPayloadLength)
{
	ensure(InMaxPayloadLength >= 0);

	Internal->SetMaxPayloadLength(InMaxPayloadLength);
}

void UWebSocketServer::SetMaxBackPressure(const int64 InMaxBackPressure)
{
	Internal->SetMaxBackPressure(InMaxBackPressure);
}

EWebSocketServerState UWebSocketServer::GetServerState() const
{
	return Internal->GetServerState();
}

void UWebSocketServer::InternalOnServerClosed()
{
	OnWebSocketServerClosed.Broadcast();
}

void UWebSocketServer::InternalOnWebSocketOpened(UWebSocket* Socket)
{
	OnWebSocketOpened.Broadcast(Socket);
}

void UWebSocketServer::InternalOnWebSocketMessage(UWebSocket* Socket, TArray<uint8> Message, EWebSocketOpCode Code)
{
	auto ConvertMessage = [&]() -> FString
	{
		const FUTF8ToTCHAR Converter((const char*)Message.GetData(), Message.Num());
		return FString(Converter.Length(), Converter.Get());
	};

	switch (Code)
	{
	case EWebSocketOpCode::PING:
		OnWebSocketPing.Broadcast(Socket, ConvertMessage(), Code);
		break;

	case EWebSocketOpCode::PONG:
		OnWebSocketPong.Broadcast(Socket, ConvertMessage(), Code);
		break;

	case EWebSocketOpCode::BINARY:
		if (OnWebSocketRawMessage.IsBound())
		{
			OnWebSocketRawMessage.Broadcast(Socket, Message, Code);
			break;
		}

	case EWebSocketOpCode::TEXT:
	default: 
		
		OnWebSocketMessage.Broadcast(Socket, ConvertMessage(), Code);
		break;
	}	
}

void UWebSocketServer::InternalOnWebSocketClosed(UWebSocket* Socket, const int32 Code, const FString& Message)
{
	OnWebSocketClosed.Broadcast(Socket, Code, Message);
}

void UWebSocketServer::Publish(const FString& Topic, const FString& Message, EWebSocketOpCode OpCode)
{
	Publish(FString(Topic), FString(Message), OpCode);
}

void UWebSocketServer::Publish(const FString& Topic, FString&& Message, EWebSocketOpCode OpCode)
{
	Publish(FString(Topic), MoveTemp(Message), OpCode);
}

void UWebSocketServer::Publish(FString&& Topic, const FString& Message, EWebSocketOpCode OpCode)
{
	Publish(MoveTemp(Topic), FString(Message), OpCode);
}

void UWebSocketServer::Publish(FString&& Topic, FString&& Message, EWebSocketOpCode OpCode)
{
	Internal->Publish(MoveTemp(Topic), MoveTemp(Message), OpCode);
}

/* static */ void UWebSocketServer::StringToBinary(const FString& InData, TArray<uint8>& OutData)
{
	OutData = TArray<uint8>((uint8*)InData.GetCharArray().GetData(), InData.GetAllocatedSize());
}

