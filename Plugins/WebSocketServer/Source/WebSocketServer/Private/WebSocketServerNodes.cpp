// Copyright Pandores Marketplace 2025. All Righst Reserved.

#include "WebSocketServerNodes.h"

UWebSocketListenProxy* UWebSocketListenProxy::Listen(UWebSocketServer* WebSocketServer, FString Host, FString URI, const int32 Port)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->Server	= WebSocketServer;
	Proxy->Host     = MoveTemp(Host);
	Proxy->URI		= MoveTemp(URI);
	Proxy->Port		= Port;

	return Proxy;
}

void UWebSocketListenProxy::Activate()
{
	if (!Server)
	{
		FFrame::KismetExecutionMessage(TEXT("Passed an invalid WebSocket Server to Listen()."), ELogVerbosity::Error);
		OnTaskOver(false);
		return;
	}

	Server->Listen(MoveTemp(Host), MoveTemp(URI), Port, FOnWebSocketServerListening::CreateUObject(this, &ThisClass::OnTaskOver));
}

void UWebSocketListenProxy::OnTaskOver(const bool bSuccess)
{
	(bSuccess ? Listening : OnError).Broadcast();
	SetReadyToDestroy();
}

UwebSocketPublishProxy* UwebSocketPublishProxy::Publish(UWebSocket* WebSocket, const FString& Topic, const FString& Message)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->WebSocket = WebSocket;
	Proxy->Topic     = Topic;
	Proxy->Message   = Message;

	return Proxy;
}

void UwebSocketPublishProxy::Activate()
{
	if (!WebSocket)
	{
		FFrame::KismetExecutionMessage(TEXT("Passed an invalid WebSocket to Publish()."), ELogVerbosity::Error);
		OnTaskOver(false);
		return;
	}

	WebSocket->Publish(MoveTemp(Topic), MoveTemp(Message), 
		FOnWebSocketPublished::CreateUObject(this, &UwebSocketPublishProxy::OnTaskOver));
}

void UwebSocketPublishProxy::OnTaskOver(bool bSuccess)
{
	(bSuccess ? Published : Error).Broadcast();
	SetReadyToDestroy();
}

UwebSocketSubscribeProxy* UwebSocketSubscribeProxy::Subscribe(UWebSocket* WebSocket, const FString& Topic)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->WebSocket = WebSocket;
	Proxy->Topic     = Topic;

	return Proxy;
}

void UwebSocketSubscribeProxy::Activate()
{
	if (!WebSocket)
	{
		FFrame::KismetExecutionMessage(TEXT("Passed an invalid WebSocket to Subscribe()."), ELogVerbosity::Error);
		OnTaskOver(false, 0);
		return;
	}

	WebSocket->Subscribe(MoveTemp(Topic), FOnWebSocketSubscribed::CreateUObject(this, &ThisClass::OnTaskOver));
}

void UwebSocketSubscribeProxy::OnTaskOver(bool bSuccess, int32 Count)
{
	(bSuccess ? Subscribed : Error).Broadcast(Count);
	SetReadyToDestroy();
}

UwebSocketUnsubscribeProxy* UwebSocketUnsubscribeProxy::Unsubscribe(UWebSocket* WebSocket, const FString& Topic)
{
	ThisClass* const Proxy = NewObject<ThisClass>();

	Proxy->WebSocket = WebSocket;
	Proxy->Topic = Topic;

	return Proxy;
}

void UwebSocketUnsubscribeProxy::Activate()
{
	if (!WebSocket)
	{
		FFrame::KismetExecutionMessage(TEXT("Passed an invalid WebSocket to unsubscribe()."), ELogVerbosity::Error);
		OnTaskOver(false, 0);
		return;
	}

	WebSocket->Unsubscribe(MoveTemp(Topic), FOnWebSocketSubscribed::CreateUObject(this, &ThisClass::OnTaskOver));
}

void UwebSocketUnsubscribeProxy::OnTaskOver(bool bSuccess, int32 Count)
{
	(bSuccess ? Unsubscribed : Error).Broadcast(Count);
	SetReadyToDestroy();
}


UWebSocketListenEventsProxy* UWebSocketListenEventsProxy::ListenAndBindEvents(
	UWebSocketServer* WebSocketServer,
	const FString& URI,
	const int32 Port)
{
	UWebSocketListenEventsProxy* const Proxy = NewObject<UWebSocketListenEventsProxy>();

	Proxy->Server = WebSocketServer;
	Proxy->URI = URI;
	Proxy->Port = Port;

	return Proxy;
}

void UWebSocketListenEventsProxy::Activate()
{
	if (!Server)
	{
		FFrame::KismetExecutionMessage(TEXT("Passed an invalid WebSocketServer to ListenAndBindEvents()."), ELogVerbosity::Error);
		SetReadyToDestroy();
		return;
	}

	Server->OnWebSocketMessage.AddDynamic(this, &UWebSocketListenEventsProxy::InternalOnMessage);
	Server->OnWebSocketPing   .AddDynamic(this, &UWebSocketListenEventsProxy::InternalOnMessage);
	Server->OnWebSocketPong   .AddDynamic(this, &UWebSocketListenEventsProxy::InternalOnMessage);
	Server->OnWebSocketClosed .AddDynamic(this, &UWebSocketListenEventsProxy::InternalOnClientLeft);//L1
	Server->OnWebSocketOpened .AddDynamic(this, &UWebSocketListenEventsProxy::InternalOnNewClient);

	Server->OnWebSocketServerClosed.AddDynamic(this, &UWebSocketListenEventsProxy::InternalOnClosed);

	Server->Listen(MoveTemp(URI), Port, FOnWebSocketServerListening::CreateUObject(this, &ThisClass::InternalOnListening));
}

void UWebSocketListenEventsProxy::InternalOnListening(const bool bSuccess)
{
	if (!bSuccess)
	{
		OnError.Broadcast(nullptr, nullptr, TEXT("Failed to listen"), EWebSocketOpCode::None, 0);
		SetReadyToDestroy();
		return;
	}

	OnListening.Broadcast(Server, nullptr, TEXT("Listening"), EWebSocketOpCode::None, 0);
}

void UWebSocketListenEventsProxy::InternalOnMessage(class UWebSocket* Socket, const FString& Message, EWebSocketOpCode Code)
{
	OnMessage.Broadcast(Server, Socket, Message, Code, 0);
}

void UWebSocketListenEventsProxy::InternalOnNewClient(class UWebSocket* Socket)
{
	OnNewClient.Broadcast(Server, Socket, TEXT(""), EWebSocketOpCode::None, 0);
}

void UWebSocketListenEventsProxy::InternalOnClientLeft(class UWebSocket* Socket, int32 Code, const FString& Message)
{
	OnClientLeft.Broadcast(Server, Socket, Message, EWebSocketOpCode::CLOSE, Code);
}

void UWebSocketListenEventsProxy::InternalOnClosed()
{
	OnServerClosed.Broadcast(Server, nullptr, TEXT(""), EWebSocketOpCode::CLOSE, 0);
	SetReadyToDestroy();
}

UWebSocketCreateProxy* UWebSocketCreateProxy::StartWebSocketServer(
	const FString& URI,
	const int32 Port,
	EWebSocketCompressOptions Compression,
	bool bInSendPingsAutomatically,
	const int64 InIdleTimeout)
{
	UWebSocketCreateProxy* const Proxy = NewObject<UWebSocketCreateProxy>();

	Proxy->URI						= URI;
	Proxy->Port						= Port;
	Proxy->Compression				= Compression;
	Proxy->bSendPingsAutomatically	= bInSendPingsAutomatically;
	Proxy->IdleTimeout				= InIdleTimeout;

	return Proxy;
}

void UWebSocketCreateProxy::Activate()
{
	Server = UWebSocketServer::CreateWebSocketServer();

	Server->SetCompression				(Compression);
	Server->SetSendPingsAutomatically	(bSendPingsAutomatically);
	Server->SetIdleTimeout				(IdleTimeout);

	Server->OnWebSocketMessage.AddDynamic(this, &UWebSocketCreateProxy::InternalOnMessage);
	Server->OnWebSocketPing	  .AddDynamic(this, &UWebSocketCreateProxy::InternalOnMessage);
	Server->OnWebSocketPong	  .AddDynamic(this, &UWebSocketCreateProxy::InternalOnMessage);
	Server->OnWebSocketClosed .AddDynamic(this, &UWebSocketCreateProxy::InternalOnClientLeft);
	Server->OnWebSocketOpened .AddDynamic(this, &UWebSocketCreateProxy::InternalOnNewClient);
	
	Server->OnWebSocketServerClosed.AddDynamic(this, &UWebSocketCreateProxy::InternalOnClosed);

	Server->Listen(MoveTemp(URI), Port, FOnWebSocketServerListening::CreateUObject(this, &UWebSocketCreateProxy::InternalOnListening));
}

void UWebSocketCreateProxy::InternalOnListening(const bool bSuccess)
{
	if (!bSuccess)
	{
		OnError.Broadcast(nullptr, nullptr, TEXT("Failed to listen"), EWebSocketOpCode::None, 0);
		SetReadyToDestroy();
		return;
	}

	OnListening.Broadcast(Server, nullptr, TEXT("Listening"), EWebSocketOpCode::None, 0);
}

void UWebSocketCreateProxy::InternalOnMessage(class UWebSocket* Socket, const FString& Message, EWebSocketOpCode Code)
{
	OnMessage.Broadcast(Server, Socket, Message, Code, 0);
}

void UWebSocketCreateProxy::InternalOnNewClient(class UWebSocket* Socket)
{
	OnNewClient.Broadcast(Server, Socket, TEXT(""), EWebSocketOpCode::None, 0);
}

void UWebSocketCreateProxy::InternalOnClientLeft(class UWebSocket* Socket, int32 Code, const FString& Message)
{
	OnClientLeft.Broadcast(Server, Socket, Message, EWebSocketOpCode::CLOSE, Code);
}

void UWebSocketCreateProxy::InternalOnClosed()
{
	OnServerClosed.Broadcast(Server, nullptr, TEXT(""), EWebSocketOpCode::CLOSE, 0);
	SetReadyToDestroy();
}


