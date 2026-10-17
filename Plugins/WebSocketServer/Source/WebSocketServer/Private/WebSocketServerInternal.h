// Copyright Pandores Marketplace 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS
#	pragma warning(push)
#	pragma warning(disable: 4668)
#	pragma warning(disable: 4582)
#endif

THIRD_PARTY_INCLUDES_START
#include <thread>
#include <string_view>
THIRD_PARTY_INCLUDES_END

#if PLATFORM_WINDOWS
#	pragma warning(pop)
#endif

#include "Async/Async.h"

#include "WebSocketServer.h"
#include "WebSocketInternal.h"
#include "WebSocketServerModule.h"


DECLARE_DELEGATE_OneParam(
	FOnOpened,
		class UWebSocket*
);

DECLARE_DELEGATE_ThreeParams(
	FOnMessage,
		class UWebSocket*,
		TArray<uint8>, 
		EWebSocketOpCode
);

DECLARE_DELEGATE_ThreeParams(
	FOnClosed,
		class UWebSocket*, 
		const int32, 
		const FString&
);

DECLARE_DELEGATE(
	FOnServerClosed
);

#define DEFINE_TYPES()															\
	using FWebSocket                    = TWebSocket<bSSL>;						\
	using FWebSocketProxy               = TWebSocketProxy<bSSL>;				\
	using FWebSocketProxyPtr            = TWebSocketProxyPtr<bSSL>;				\
	using FWebSocketServerInternal      = TWebSocketServerInternal<bSSL>;		\
	using FWebSocketData                = TWebSocketData<bSSL>;					\
	using FWebSocketServerInternalPtr	= TWebSocketServerInternalPtr <bSSL>;	\
	using uWSApp						= uWS::TemplatedApp<bSSL>;

#define DEFINE_FRIEND(Class)	\
	template<bool b_SSL>		\
	friend class Class;

template<bool bSSL>
class TWebSocketServerInternal;

template<bool bSSL>
class TWebSocketData;

template<bool bSSL>
using TWebSocket = uWS::WebSocket<bSSL, true, TWebSocketData<bSSL>>;

template<bool bSSL>
class TWebSocketProxy;

template<bool bSSL>
using TWebSocketProxyPtr = TSharedPtr<TWebSocketProxy<bSSL>, ESPMode::ThreadSafe>;

template<bool bSSL>
using TWebSocketServerInternalPtr = TSharedPtr<TWebSocketServerInternal<bSSL>, ESPMode::ThreadSafe>;

class FWebSocketServerSharedRessourcesManager
{
private:
	using FListenSocketPtr = struct us_listen_socket_t*;
	using FAtomicStatus = TAtomic<EWebSocketServerState>;

public:
	FWebSocketServerSharedRessourcesManager();

	/**
	 * The underlying listen socket.
	 * Must be accessed only on Server Thread.
	*/
	FListenSocketPtr ListenSocket;

	/**
	 * If the WebSocket Server is running or not.
	*/
	FAtomicStatus ServerStatus;

	/**
	 * Mutex to synchronize access to the loop and prevent the loop
	 * from dying when we are pushing work on it.
	*/
	FCriticalSection LoopAccess;

	/**
	 * The loop.
	*/
	uWS::Loop* Loop;
};

using FSharedRessourcesPtr = TSharedPtr<class FWebSocketServerSharedRessourcesManager, ESPMode::ThreadSafe>;

class IWebSocketProxy
{
public:
	virtual void SendMessage(FString&& Message) = 0;
	virtual void SendData(TArray<uint8>&& Data) = 0;
	virtual void Close() = 0;
	virtual void End(const int32 OpCode, FString&& Message) = 0;
	virtual void Ping(FString&& Message) = 0;
	virtual void Pong(FString&& Message) = 0;
	virtual bool IsSocketValid() const   = 0;
	virtual void Subscribe(FString&& Topic, FOnWebSocketSubscribed&& Callback, bool bNonStrict)   = 0;
	virtual void Unsubscribe(FString&& Topic, FOnWebSocketSubscribed&& Callback, bool bNonStrict) = 0;
	virtual void Publish(FString&& Topic, FString&& Message, FOnWebSocketPublished&& Callback)    = 0;
};

/**
 * A proxy to safely access the internal socket.
*/
template<bool bSSL>
class TWebSocketProxy final : 
	public IWebSocketProxy, 
	public TSharedFromThis<TWebSocketProxy<bSSL>, ESPMode::ThreadSafe>
{
private:
	DEFINE_TYPES();
	DEFINE_FRIEND(TWebSocketData);;
	DEFINE_FRIEND(TWebSocketServerInternal);

public:
	TWebSocketProxy();
	TWebSocketProxy(const TWebSocketProxy&) = delete;

	void OnOpen(FWebSocketServerInternalPtr InServer, FWebSocket* RawWebSocket, FOnOpened UserCallback);

	void OnMessage(std::string_view Message, uWS::OpCode Code, FOnMessage UserCallback);
	void OnClosed(const int Code, const std::string_view Message, FOnClosed UserCallback);
	void OnPing(const std::string_view Message, FOnMessage UserCallback);
	void OnPong(const std::string_view Message, FOnMessage UserCallback);

	~TWebSocketProxy();

	virtual void SendMessage(FString&& Message) override;
	virtual void SendData(TArray<uint8>&& Data) override;
	virtual void Close() override;
	virtual void End(const int32 OpCode, FString&& Message) override;
	virtual void Ping(FString&& Message) override;
	virtual void Pong(FString&& Message) override;
	virtual void Subscribe  (FString&& Topic, FOnWebSocketSubscribed&& Callback, bool bNonStrict) override;
	virtual void Unsubscribe(FString&& Topic, FOnWebSocketSubscribed&& Callback, bool bNonStrict) override;
	virtual void Publish    (FString&& Topic, FString&& Message, FOnWebSocketPublished&& Callback) override;

	virtual bool IsSocketValid() const;

private:
	// Executes a function on Server thread iff the WebSocket is
	// still valid.
	void ExecuteOnServerThread(TUniqueFunction<void(FWebSocket*)> Function);

	void SendInternal(FString&& Message, const uWS::OpCode Code);

private:
	FWebSocket* RawWebSocket;

	TAtomic<bool> bIsSocketValid;

	// The associated UObject.
	TStrongObjectPtr<class UWebSocket> WebSocket;

	TWeakPtr<FWebSocketServerInternal, ESPMode::ThreadSafe> WebSocketServer;
};

/**
 * The WebSocket data passed along to uWebSockets.
*/
template<bool bSSL>
class TWebSocketData
{
private:
	DEFINE_TYPES();

public:
	TWebSocketData();
	TWebSocketData(TWebSocketData&& Other);
	TWebSocketData(const TWebSocketData&) = delete;

	~TWebSocketData();

	FWebSocketProxyPtr GetProxy();

	void SetSocket(FWebSocket* const InWebSocket);

	void OnClosed();

private:
	FWebSocketProxyPtr Proxy;

	FWebSocket* WebSocket;
};

class IWebSocketServerInternal
{
private:
	using FThreadPtr = TUniquePtr<std::thread>;

public:
	IWebSocketServerInternal();

	virtual void Listen(FString&& Host, FString&& URI, const uint16 Port, FOnWebSocketServerListening&& Callback,
		FOnServerClosed&& OnClosed) = 0;
	virtual void Close() = 0;
	virtual void Publish(FString&& Topic, FString&& Message, EWebSocketOpCode OpCode) = 0;

	virtual ~IWebSocketServerInternal() = default;
	
public:
	/* Events */
	FORCEINLINE FOnOpened & OnOpened()  { return OnOpenedEvent;  }
	FORCEINLINE FOnMessage& OnMessage() { return OnMessageEvent; }
	FORCEINLINE FOnMessage& OnPong()    { return OnPongEvent;    }
	FORCEINLINE FOnMessage& OnPing()    { return OnPingEvent;    }
	FORCEINLINE FOnClosed & OnClosed()  { return OnClosedEvent;	 }

protected:
	FOnOpened  OnOpenedEvent;
	FOnMessage OnMessageEvent;
	FOnMessage OnPongEvent;
	FOnMessage OnPingEvent;
	FOnClosed  OnClosedEvent;

public:
	void SetMaxLifetime(const int64 InMaxLifetime);
	void SetMaxPayloadLength(const int64 InMaxLifetime);
	void SetIdleTimeout(const int64 InIdleTimeout);
	void SetMaxBackPressure(const int64 InMaxBackPressure);
	void SetCloseOnBackpressureLimit(const bool bInCloseOnBackpressureLimit);
	void SetResetIdleTimeoutOnSend(const bool bInResetIdleTimeoutOnSend);
	void SetSendPingsAutomatically(const bool bInSendPingsAutomatically);
	void SetCompression(EWebSocketCompressOptions InCompression);

	EWebSocketServerState GetServerState() const;

	void SetSSLOptions(FString&& InKeyFile, FString&& InCertFile, FString&& InPassPhrase,
		FString&& InDhParamsFile, FString&& InCaFileName);

protected:
	/**
	 * WebSocket options
	*/
	int64 MaxLifetime;
	int64 MaxPayloadLength;
	int64 IdleTimeout;
	int64 MaxBackPressure;
	bool  bCloseOnBackpressureLimit;
	bool  bResetIdleTimeoutOnSend;
	bool  bSendPingsAutomatically;
	EWebSocketCompressOptions Compression;

	/**
	 * SSL options.
	*/
	FString KeyFile;
	FString CertFile;
	FString PassPhrase;
	FString DhParamsFile;
	FString CaFileName;

protected:
	/**
	 * The thread where the server is running.
	*/
	FThreadPtr Thread;

	/**
	 * Ressources to manage thread-safety.
	*/
	const FSharedRessourcesPtr SharedRessources;
};

template<bool bSSL>
class TWebSocketServerInternal final 
	: public TSharedFromThis<TWebSocketServerInternal<bSSL>, ESPMode::ThreadSafe>
	, public IWebSocketServerInternal
{
private:
	DEFINE_TYPES();
	DEFINE_FRIEND(TWebSocketProxy);

public:
	 TWebSocketServerInternal();
	~TWebSocketServerInternal();

	virtual void Listen(FString&& Host, FString&& URI, const uint16 Port, FOnWebSocketServerListening&& Callback, FOnServerClosed&& OnClosed);
	virtual void Close();
	virtual void Publish(FString&& Topic, FString&& Message, EWebSocketOpCode OpCode);

private:
	uWSApp App;
};

/**
 * Helper methods for WebSocket
*/
namespace NWebSocketUtils
{
	uWS::CompressOptions Convert(const EWebSocketCompressOptions Option);
	EWebSocketOpCode     Convert(const uWS::OpCode Code);
	uWS::OpCode          Convert(const EWebSocketOpCode Code);
	FString              Convert(const std::string_view& Value);
}

#if CPP
#	include "WebSocketServerInternal.inl"
#endif

#undef DEFINE_FRIEND
#undef DEFINE_TYPES


