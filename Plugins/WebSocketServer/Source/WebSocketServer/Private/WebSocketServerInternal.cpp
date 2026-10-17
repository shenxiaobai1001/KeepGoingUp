// Copyright Pandores Marketplace 2025. All Rights Reserved.

#include "WebSocketServerInternal.h"

FWebSocketServerSharedRessourcesManager::FWebSocketServerSharedRessourcesManager()
	: ListenSocket(nullptr)
	, ServerStatus(EWebSocketServerState::Closed)
	, Loop(nullptr)
{
}

IWebSocketServerInternal::IWebSocketServerInternal()
	: MaxLifetime(DefaultMaxLifetime)
	, MaxPayloadLength(DefaultMaxPayloadLength)
	, IdleTimeout(DefaultIdleTimeout)
	, MaxBackPressure(DefaultMaxBackPressure)
	, bCloseOnBackpressureLimit(false)
	, bResetIdleTimeoutOnSend(false)
	, bSendPingsAutomatically(true)
	, Compression(EWebSocketCompressOptions::DISABLED)
	, SharedRessources(MakeShared<FWebSocketServerSharedRessourcesManager, ESPMode::ThreadSafe>())
{
}


void IWebSocketServerInternal::SetMaxLifetime(const int64 InMaxLifetime)
{
	MaxLifetime = InMaxLifetime;
}

void IWebSocketServerInternal::SetMaxPayloadLength(const int64 InMaxLifetime)
{
	MaxPayloadLength = InMaxLifetime;
}

void IWebSocketServerInternal::SetIdleTimeout(const int64 InIdleTimeout)
{
	IdleTimeout = InIdleTimeout;
}

void IWebSocketServerInternal::SetMaxBackPressure(const int64 InMaxBackPressure)
{
	MaxBackPressure = InMaxBackPressure;
}

void  IWebSocketServerInternal::SetCloseOnBackpressureLimit(const bool bInCloseOnBackpressureLimit)
{
	bCloseOnBackpressureLimit = bInCloseOnBackpressureLimit;
}

void  IWebSocketServerInternal::SetResetIdleTimeoutOnSend(const bool bInResetIdleTimeoutOnSend)
{
	bResetIdleTimeoutOnSend = bInResetIdleTimeoutOnSend;
}

void  IWebSocketServerInternal::SetSendPingsAutomatically(const bool bInSendPingsAutomatically)
{
	bSendPingsAutomatically = bInSendPingsAutomatically;
}

void IWebSocketServerInternal::SetCompression(EWebSocketCompressOptions InCompression)
{
	Compression = InCompression;
}

EWebSocketServerState IWebSocketServerInternal::GetServerState() const
{
	return SharedRessources->ServerStatus;
}

void IWebSocketServerInternal::SetSSLOptions(
	FString&& InKeyFile,
	FString&& InCertFile,
	FString&& InPassPhrase,
	FString&& InDhParamsFile,
	FString&& InCaFileName)
{
	KeyFile = MoveTemp(InKeyFile);
	CertFile = MoveTemp(InCertFile);
	PassPhrase = MoveTemp(InPassPhrase);
	DhParamsFile = MoveTemp(InDhParamsFile);
	CaFileName = MoveTemp(InCaFileName);
}

namespace NWebSocketUtils
{
	uWS::CompressOptions Convert(const EWebSocketCompressOptions Option)
	{
		switch(Option)
		{
		#define MakeCase(type) \
			case EWebSocketCompressOptions:: type: return uWS:: type;
			MakeCase(DISABLED)
			MakeCase(SHARED_COMPRESSOR)
			MakeCase(DEDICATED_COMPRESSOR_3KB)
			MakeCase(DEDICATED_COMPRESSOR_4KB)
			MakeCase(DEDICATED_COMPRESSOR_8KB)
			MakeCase(DEDICATED_COMPRESSOR_16KB)
			MakeCase(DEDICATED_COMPRESSOR_32KB)
			MakeCase(DEDICATED_COMPRESSOR_64KB)
			MakeCase(DEDICATED_COMPRESSOR_128KB)
			MakeCase(DEDICATED_COMPRESSOR_256KB)
			MakeCase(DEDICATED_COMPRESSOR)
		#undef MakeCase
		}

		return uWS::CompressOptions::DISABLED;
	}

	EWebSocketOpCode Convert(const uWS::OpCode Code)
	{
		switch (Code)
		{
		#define MakeCase(type) \
			case uWS::OpCode:: type : return EWebSocketOpCode:: type ; 
			MakeCase(TEXT)
			MakeCase(BINARY)
			MakeCase(CLOSE)
			MakeCase(PING)
			MakeCase(PONG)
		#undef MakeCase
		}

		return EWebSocketOpCode::None;
	}
	
	uWS::OpCode Convert(const EWebSocketOpCode Code)
	{
		switch (Code)
		{
		#define MakeCase(type) \
			case EWebSocketOpCode:: type : return uWS::OpCode:: type ; 
			MakeCase(TEXT)
			MakeCase(BINARY)
			MakeCase(CLOSE)
			MakeCase(PING)
			MakeCase(PONG)
		#undef MakeCase
		}

		return uWS::OpCode::TEXT;
	}
	
	FString Convert(const std::string_view& Value)
	{		
		const FUTF8ToTCHAR Converter(Value.data(), Value.size());
		return FString(Converter.Length(), Converter.Get());
	}
};
