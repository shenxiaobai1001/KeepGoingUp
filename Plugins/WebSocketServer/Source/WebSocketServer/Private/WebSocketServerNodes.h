// Copyright Pandores Marketplace 2025. All Righst Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "WebSocketServer.h"
#include "WebSocketServerModule.h"
#include "WebSocketServerNodes.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDynMultListen);

UCLASS()
class UWebSocketListenProxy : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:

	/**
	 * Called when the server successfully started listening
	 * for clients.
	*/
	UPROPERTY(BlueprintAssignable)
	FDynMultListen Listening;

	/**
	 * Called when the server failed to start listening
	 * for clients.
	*/
	UPROPERTY(BlueprintAssignable)
	FDynMultListen OnError;

public:

	/**
	 * Starts listening for WebSocket clients' connections.
	 * @param WebSocketServer The WebSocket Server.
	 * @param URI The URI where the WebSocket Server should listen.
	 * @param Port The port where the WebSocket Server should listen.
	*/
	UFUNCTION(BlueprintCallable, Category = "WebSocket|Server", meta = (BlueprintInternalUseOnly = "true"))
	static UWebSocketListenProxy* Listen(UWebSocketServer* WebSocketServer, 
		FString Host = TEXT("0.0.0.0"), /* NOTE: UHT crashes without newlines. */
		FString URI = TEXT("/*"), 
		const int32 Port = 8080);

	virtual void Activate();

private:
	UFUNCTION()
	void OnTaskOver(const bool bSuccess);

private:
	UPROPERTY()
	UWebSocketServer* Server;
	int32 Port;
	FString Host;
	FString URI;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDynMultPublish);

UCLASS()
class UwebSocketPublishProxy : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	/**
	 * The message has successfully been published to the topic.
	*/
	UPROPERTY(BlueprintAssignable)
	FDynMultPublish Published;

	/**
	 * Failed to publish, probably because the server is too busy
	 * and can't handle the publication.
	*/
	UPROPERTY(BlueprintAssignable)
	FDynMultPublish Error;

public:
	virtual void Activate();

	/**
	 * Publishes a message to all WebSocket subscribed to a topic.
	 * @param Topic    The topic to publish to.
	 * @param Message  The message to publish.
	*/
	UFUNCTION(BlueprintCallable, Category = "WebSocket|Server", meta = (BlueprintInternalUseOnly = "true"))
	static UwebSocketPublishProxy* Publish(UWebSocket* WebSocket, const FString& Topic, const FString& Message);

private:
	void OnTaskOver(bool bSuccess);

private:
	FString Topic;
	FString Message;

	UPROPERTY()
	UWebSocket* WebSocket;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FDynMultSubscribe,
		int32, SubscriberCount
);

UCLASS()
class UwebSocketSubscribeProxy : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	/**
	 * The socket has successfully been subscribed to the topic.
	*/
	UPROPERTY(BlueprintAssignable)
	FDynMultSubscribe Subscribed;

	/**
	 * Failed to subscribe.
	*/
	UPROPERTY(BlueprintAssignable)
	FDynMultSubscribe Error;

public:
	virtual void Activate();

	/**
	 * Subscribes this WebSocket to a Topic.
	 * @param Topic The topic to subscribe to.
	*/
	UFUNCTION(BlueprintCallable, Category = "WebSocket|Server", meta = (BlueprintInternalUseOnly = "true"))
	static UwebSocketSubscribeProxy* Subscribe(UWebSocket* WebSocket, const FString& Topic);

private:
	void OnTaskOver(bool, int32);

private:
	FString Topic;

	UPROPERTY()
	UWebSocket* WebSocket;
};

UCLASS()
class UwebSocketUnsubscribeProxy : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	/**
	 * The socket has successfully been unsubscribed to the topic.
	*/
	UPROPERTY(BlueprintAssignable)
	FDynMultSubscribe Unsubscribed;

	/**
	 * Failed to unsubscribe.
	*/
	UPROPERTY(BlueprintAssignable)
	FDynMultSubscribe Error;

public:
	virtual void Activate();

	/**
	 * Unsubscribes this WebSocket to a Topic.
	 * @param Topic The topic to Unsubscribe to.
	*/
	UFUNCTION(BlueprintCallable, Category = "WebSocket|Server", meta = (BlueprintInternalUseOnly = "true"))
	static UwebSocketUnsubscribeProxy* Unsubscribe(UWebSocket* WebSocket, const FString& Topic);

private:
	void OnTaskOver(bool, int32);

private:
	FString Topic;

	UPROPERTY()
	UWebSocket* WebSocket;
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FWSServerPin,
		UWebSocketServer*, WebSocketServer,
		class UWebSocket*, ClientWebSocket,
		const FString&, Message,
		EWebSocketOpCode, OpCode,
		int32, CloseCode
);


UCLASS()
class UWebSocketListenEventsProxy : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	/**
	 * Called when the server is ready to accept clients' connection
	 * and respond to their requests.
	*/
	UPROPERTY(BlueprintAssignable, Category = "WebSocket|Server")
	FWSServerPin OnListening;

	/**
	 * Called when the server failed to listen for clients.
	*/
	UPROPERTY(BlueprintAssignable, Category = "WebSocket|Server")
	FWSServerPin OnError;

	/**
	 * Called when a new client established a WebSocket connection.
	*/
	UPROPERTY(BlueprintAssignable, Category = "WebSocket|Server")
	FWSServerPin OnNewClient;

	/**
	 * Called when a WebSocket client sent us a message.
	*/
	UPROPERTY(BlueprintAssignable, Category = "WebSocket|Server")
	FWSServerPin OnMessage;

	/**
	 * Called when a WebSocket client disconnected.
	*/
	UPROPERTY(BlueprintAssignable, Category = "WebSocket|Server")
	FWSServerPin OnClientLeft;

	/**
	 * Called when the WebSocket server was closed and no longer
	 * listen for clients.
	*/
	UPROPERTY(BlueprintAssignable, Category = "WebSocket|Server")
	FWSServerPin OnServerClosed;

public:
	virtual void Activate();
	
	/**
	 * Starts to listen and binds events to output pins.
	 * @param WebSocketServer The server that will start listening.
	 * @param URI Where the WebSocket server is reachable.
	 * @param Port The port where to reach the WebSocket server.
	*/
	UFUNCTION(BlueprintCallable, Category = "WebSocket|Server", meta = (BlueprintInternalUseOnly = "true"))
	static UWebSocketListenEventsProxy* ListenAndBindEvents(
		UWebSocketServer* WebSocketServer,
		const FString& URI = TEXT("/*"), 
		const int32 Port = 8080);

private:
	UFUNCTION()
	void InternalOnListening (const bool bSuccess);
	UFUNCTION()
	void InternalOnMessage   (class UWebSocket* Socket, const FString& Message, EWebSocketOpCode Code);
	UFUNCTION()
	void InternalOnNewClient (class UWebSocket* Socket);
	UFUNCTION()
	void InternalOnClientLeft(class UWebSocket* Socket, int32 Code, const FString& Message);
	UFUNCTION()
	void InternalOnClosed();

private:
	UPROPERTY()
	UWebSocketServer* Server;

	FString URI;
	int32 Port;
};

UCLASS()
class UWebSocketCreateProxy : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	/**
	 * Called when the server is ready to accept clients' connection
	 * and respond to their requests.
	*/
	UPROPERTY(BlueprintAssignable)
	FWSServerPin OnListening;

	/**
	 * Called when the server failed to listen for clients.
	*/
	UPROPERTY(BlueprintAssignable)
	FWSServerPin OnError;

	/**
	 * Called when a new client established a WebSocket connection.
	*/
	UPROPERTY(BlueprintAssignable)
	FWSServerPin OnNewClient;

	/**
	 * Called when a WebSocket client sent us a message.
	*/
	UPROPERTY(BlueprintAssignable)
	FWSServerPin OnMessage;

	/**
	 * Called when a WebSocket client disconnected.
	*/
	UPROPERTY(BlueprintAssignable)
	FWSServerPin OnClientLeft;

	/**
	 * Called when the WebSocket server was closed and no longer
	 * listen for clients.
	*/
	UPROPERTY(BlueprintAssignable)
	FWSServerPin OnServerClosed;

public:
	virtual void Activate();
	
	/**
	 * Creates and starts a new WebSocket server with the specified options.
	 * @param URI Where the WebSocket server is reachable.
	 * @param Port The port where to reach the WebSocket server.
	 * @param Compression The compression method used. Disabled recommended.
	 * @param bInSendPingsAutomatically If we should send ping automatically internally.
	 *                                  Setting it to false requires to send ping on regular 
	 *                                  intervals to prevent the clients from getting disconnected.
	 * @param InIdleTimeout The maximum idle timeout before disconnecting a client.
	*/
	UFUNCTION(BlueprintCallable, Category = "WebSocket|Server", meta = (BlueprintInternalUseOnly = "true"))
	static UWebSocketCreateProxy* StartWebSocketServer(
		const FString& URI = TEXT("/*"), 
		const int32 Port = 8080, 
		EWebSocketCompressOptions Compression = EWebSocketCompressOptions::DISABLED,
		bool bSendPingsAutomatically = true,
		const int64 IdleTimeout = 120);

private:
	UFUNCTION()
	void InternalOnListening (const bool bSuccess);
	UFUNCTION()
	void InternalOnMessage   (class UWebSocket* Socket, const FString& Message, EWebSocketOpCode Code);
	UFUNCTION()
	void InternalOnNewClient (class UWebSocket* Socket);
	UFUNCTION()
	void InternalOnClientLeft(class UWebSocket* Socket, int32 Code, const FString& Message);
	UFUNCTION()
	void InternalOnClosed();

private:
	UPROPERTY()
	UWebSocketServer* Server;

	FString URI;
	int32 Port;
	EWebSocketCompressOptions Compression;
	bool bSendPingsAutomatically;
	int64 IdleTimeout;
};



