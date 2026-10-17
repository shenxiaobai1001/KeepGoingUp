// Copyright Pandores Marketplace 2025. All Righst Reserved.

#include "WebSocketServer.h"
#include "WebSocketServerInternal.h"

void UWebSocket::Send(const FString& Message)
{
	Send(FString(Message));
}

void UWebSocket::Send(FString&& Message)
{
	auto Proxy = SocketProxy.Pin();
	if (Proxy)
	{
		Proxy->SendMessage(MoveTemp(Message));
	}
}

void UWebSocket::Close()
{
	auto Proxy = SocketProxy.Pin();
	if (Proxy)
	{
		Proxy->Close();
	}
}

void UWebSocket::End(const int32 Code, const FString& Message)
{
	End(Code, FString(Message));
}

void UWebSocket::End(const int32 Code, FString&& Message)
{
	auto Proxy = SocketProxy.Pin();
	if (Proxy)
	{
		Proxy->End(Code, MoveTemp(Message));
	}
}

void UWebSocket::Ping(const FString& Message)
{
	Ping(FString(Message));
}

void UWebSocket::Ping(FString&& Message)
{
	auto Proxy = SocketProxy.Pin();
	if (Proxy)
	{
		Proxy->Ping(MoveTemp(Message));
	}
}

void UWebSocket::Pong(const FString& Message)
{
	Pong(FString(Message));
}

void UWebSocket::Pong(FString&& Message)
{
	auto Proxy = SocketProxy.Pin();
	if (Proxy)
	{
		Proxy->Pong(MoveTemp(Message));
	}
}

bool UWebSocket::IsConnected() const
{
	if (!SocketProxy.IsValid())
	{
		return false;
	}

	auto Proxy = SocketProxy.Pin();
	return Proxy && Proxy->IsSocketValid();
}

void UWebSocket::Subscribe(const FString& Topic, const FOnWebSocketSubscribed& Callback, bool bNonStrict)
{
	Subscribe(FString(Topic), FOnWebSocketSubscribed(Callback), bNonStrict);
}

void UWebSocket::Subscribe(const FString& Topic, FOnWebSocketSubscribed&& Callback, bool bNonStrict)
{
	Subscribe(FString(Topic), MoveTemp(Callback), bNonStrict);
}

void UWebSocket::Subscribe(FString&& Topic, const FOnWebSocketSubscribed& Callback, bool bNonStrict)
{
	Subscribe(MoveTemp(Topic), FOnWebSocketSubscribed(Callback), bNonStrict);
}

void UWebSocket::Subscribe(FString&& Topic, FOnWebSocketSubscribed&& Callback, bool bNonStrict)
{
	auto Proxy = SocketProxy.Pin();
	if (Proxy)
	{
		Proxy->Subscribe(MoveTemp(Topic), MoveTemp(Callback), bNonStrict);
	}
	else
	{
		Callback.ExecuteIfBound(false, 0);
	}
}

void UWebSocket::Unsubscribe(const FString& Topic, const FOnWebSocketSubscribed& Callback, bool bNonStrict)
{
	Unsubscribe(FString(Topic), FOnWebSocketSubscribed(Callback), bNonStrict);
}

void UWebSocket::Unsubscribe(const FString& Topic, FOnWebSocketSubscribed&& Callback, bool bNonStrict)
{
	Unsubscribe(FString(Topic), MoveTemp(Callback), bNonStrict);
}

void UWebSocket::Unsubscribe(FString&& Topic, const FOnWebSocketSubscribed& Callback, bool bNonStrict)
{
	Unsubscribe(MoveTemp(Topic), FOnWebSocketSubscribed(Callback), bNonStrict);
}

void UWebSocket::Unsubscribe(FString&& Topic, FOnWebSocketSubscribed&& Callback, bool bNonStrict)
{
	auto Proxy = SocketProxy.Pin();
	if (Proxy)
	{
		Proxy->Unsubscribe(MoveTemp(Topic), MoveTemp(Callback), bNonStrict);
	}
	else
	{
		Callback.ExecuteIfBound(false, 0);
	}
}

void UWebSocket::Publish(const FString& Topic, const FString& Message, const FOnWebSocketPublished& Callback)
{
	Publish(FString(Topic), FString(Message), FOnWebSocketPublished(Callback));
}

void UWebSocket::Publish(FString&& Topic, FString&& Message, FOnWebSocketPublished&& Callback)
{
	auto Proxy = SocketProxy.Pin();
	if (Proxy)
	{
		Proxy->Publish(MoveTemp(Topic), MoveTemp(Message), MoveTemp(Callback));
	}
	else
	{
		Callback.ExecuteIfBound(false);
	}
}

void UWebSocket::Send_Blueprint(const TArray<uint8>& Data)
{
	Send(Data);
}

void UWebSocket::Send(const TArray<uint8>& Data)
{
	Send(CopyTemp(Data));
}

void UWebSocket::Send(TArray<uint8>&& Data)
{
	auto Proxy = SocketProxy.Pin();
	if (Proxy)
	{
		Proxy->SendData(MoveTemp(Data));
	}
}
