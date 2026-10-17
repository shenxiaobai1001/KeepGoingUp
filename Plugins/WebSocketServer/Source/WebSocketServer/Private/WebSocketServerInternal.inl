// Copyright Pandores Marketplace 2025. All Rights Reserved.

// Default 
static constexpr int64 DefaultMaxLifetime		= 0;
static constexpr int64 DefaultMaxPayloadLength	= 256 * 1024;
static constexpr int64 DefaultIdleTimeout		= 120;
static constexpr int64 DefaultMaxBackPressure	= 256 * 1024;

///////////////////////////////////////////////////////////////
// FWebSocketData

template<bool bSSL>
TWebSocketData<bSSL>::TWebSocketData()
	: Proxy(MakeShared<TWebSocketProxy<bSSL>, ESPMode::ThreadSafe>())
	, WebSocket(nullptr)
{
}

template<bool bSSL>
TWebSocketData<bSSL>::TWebSocketData(TWebSocketData&& Other)
	: Proxy(MoveTemp(Other.Proxy))
	, WebSocket(Other.WebSocket)
{
	Other.WebSocket = nullptr;
}

template<bool bSSL>
TWebSocketProxyPtr<bSSL> TWebSocketData<bSSL>::GetProxy()
{
	return Proxy;
}

template<bool bSSL>
TWebSocketData<bSSL>::~TWebSocketData()
{
	// Close wasn't graceful.
	if (Proxy)
	{
		Proxy->OnClosed(-1, "Server Closed", FOnClosed());
		OnClosed();
	}
}

template<bool bSSL>
void TWebSocketData<bSSL>::SetSocket(FWebSocket* const InWebSocket)
{
	// Must be called once and only once.
	check(!WebSocket);

	WebSocket = InWebSocket;
}

template<bool bSSL>
void TWebSocketData<bSSL>::OnClosed()
{
	Proxy->bIsSocketValid = false;
	Proxy     = nullptr;
	WebSocket = nullptr;
}


///////////////////////////////////////////////////////////////
// FWebSocketProxy

template<bool bSSL>
TWebSocketProxy<bSSL>::TWebSocketProxy()
	: RawWebSocket(nullptr)
	, bIsSocketValid(true)
{
}

template<bool bSSL>
void TWebSocketProxy<bSSL>::OnOpen(FWebSocketServerInternalPtr InServer, TWebSocket<bSSL>* InRawWebSocket,
	FOnOpened UserCallback)
{
	RawWebSocket    = InRawWebSocket;
	WebSocketServer = InServer;

	AsyncTask(ENamedThreads::GameThread,
		[
			Self = this->AsShared(),
			UserCallback = MoveTemp(UserCallback)
		]() -> void
	{
		UWebSocket* const NewWebSocket = NewObject<UWebSocket>();

		Self->WebSocket.Reset(NewWebSocket);

		NewWebSocket->SocketProxy = Self;

		UserCallback.ExecuteIfBound(NewWebSocket);
	});
}


template<bool bSSL>
void TWebSocketProxy<bSSL>::OnMessage(std::string_view Message, uWS::OpCode Code, FOnMessage UserCallback)
{
	AsyncTask(ENamedThreads::GameThread,
		[
			Self         = this->AsShared(),
			BinMessage   = TArray<uint8>((uint8*)Message.data(), Message.size()),
			UserCallback = MoveTemp(UserCallback),
			Code
		]() -> void
	{
		check(Self->WebSocket.IsValid());

		UserCallback.ExecuteIfBound(Self->WebSocket.Get(), BinMessage, NWebSocketUtils::Convert(Code));
	});
}

template<bool bSSL>
void TWebSocketProxy<bSSL>::OnClosed(const int Code, const std::string_view Message, FOnClosed UserCallback)
{
	bIsSocketValid = false;
	RawWebSocket = nullptr;

	AsyncTask(ENamedThreads::GameThread,
		[
			Self         = this->AsShared(),
			StrMessage   = NWebSocketUtils::Convert(Message),
			UserCallback = MoveTemp(UserCallback),
			Code
		]() -> void
	{		
		check(Self->WebSocket.IsValid());

		UserCallback.ExecuteIfBound(Self->WebSocket.Get(), Code, StrMessage);

		if (Self->WebSocket)
		{
			Self->WebSocket->SocketProxy = nullptr;
		}
	});
}

template<bool bSSL>
void TWebSocketProxy<bSSL>::OnPing(const std::string_view Message, FOnMessage UserCallback)
{
	OnMessage(Message, uWS::OpCode::PING, MoveTemp(UserCallback));
}

template<bool bSSL>
void TWebSocketProxy<bSSL>::OnPong(const std::string_view Message, FOnMessage UserCallback)
{
	OnMessage(Message, uWS::OpCode::PONG, MoveTemp(UserCallback));
}

template<bool bSSL>
void TWebSocketProxy<bSSL>::ExecuteOnServerThread(TUniqueFunction<void(TWebSocket<bSSL>*)> Function)
{
	if (!WebSocketServer.IsValid() || !bIsSocketValid)
	{
		return;
	}

	FWebSocketServerInternalPtr Internal = WebSocketServer.Pin();
	if (!Internal)
	{
		return;
	}

	auto SharedRessources = Internal->SharedRessources;

	auto ServerThreadWork = [Self = this->AsShared(), Function = MoveTemp(Function)]() -> void
	{
		if (Self->RawWebSocket)
		{
			Function(Self->RawWebSocket);
		}
	};

	{
		FScopeLock Lock(&SharedRessources->LoopAccess);

		if (SharedRessources->Loop)
		{
			SharedRessources->Loop->defer(MoveTemp(ServerThreadWork));
		}
	}
}

template<bool bSSL>
void TWebSocketProxy<bSSL>::SendInternal(FString&& Message, const uWS::OpCode Code)
{
	ExecuteOnServerThread([Message = MoveTemp(Message), Code](FWebSocket* Socket) -> void
	{
		Socket->send(TCHAR_TO_UTF8(*Message), Code); 
	});
}

template<bool bSSL>
void TWebSocketProxy<bSSL>::SendMessage(FString&& Message)
{
	SendInternal(MoveTemp(Message), uWS::OpCode::TEXT);
}

template<bool bSSL>
void TWebSocketProxy<bSSL>::SendData(TArray<uint8>&& Data)
{
	ExecuteOnServerThread([Data = MoveTemp(Data)](FWebSocket* Socket) -> void
	{
		std::string_view StrData(reinterpret_cast<const char*>(Data.GetData()), Data.Num());
		Socket->send(StrData, uWS::OpCode::BINARY);
	});
}

template<bool bSSL>
void TWebSocketProxy<bSSL>::Close()
{
	ExecuteOnServerThread([](FWebSocket* Socket) -> void
	{
		Socket->close();
	});
}

template<bool bSSL>
void TWebSocketProxy<bSSL>::End(const int32 Code, FString&& Message)
{
	ExecuteOnServerThread([Code, Message = MoveTemp(Message)](FWebSocket* Socket) -> void
	{
		Socket->end(Code, TCHAR_TO_UTF8(*Message));
	});
}

template<bool bSSL>
void TWebSocketProxy<bSSL>::Ping(FString&& Message)
{
	SendInternal(MoveTemp(Message), uWS::OpCode::PING);
}

template<bool bSSL>
void TWebSocketProxy<bSSL>::Pong(FString&& Message)
{
	SendInternal(MoveTemp(Message), uWS::OpCode::PONG);
}

template<bool bSSL>
void TWebSocketProxy<bSSL>::Subscribe(FString&& Topic, FOnWebSocketSubscribed&& Callback, bool bNonStrict)
{
	ExecuteOnServerThread([
		Topic    = MoveTemp(Topic),
		Callback = MoveTemp(Callback),
		bNonStrict
	](FWebSocket* Socket) mutable -> void
	{
		const auto Result = Socket->subscribe(TCHAR_TO_UTF8(*Topic), bNonStrict);

		if (Callback.IsBound())
		{
			AsyncTask(ENamedThreads::GameThread, 
				[Callback = MoveTemp(Callback), bSuccess = Result.second, NumSubscribers = Result.first]() -> void
			{
				Callback.ExecuteIfBound(bSuccess, static_cast<int32>(NumSubscribers));
			});
		}
	});
}

template<bool bSSL>
void TWebSocketProxy<bSSL>::Unsubscribe(FString&& Topic, FOnWebSocketSubscribed&& Callback, bool bNonStrict)
{
	ExecuteOnServerThread([
		Topic    = MoveTemp(Topic),
		Callback = MoveTemp(Callback),
		bNonStrict
	](FWebSocket* Socket) mutable -> void
	{
		const auto Result = Socket->unsubscribe(TCHAR_TO_UTF8(*Topic), bNonStrict);

		if (Callback.IsBound())
		{
			AsyncTask(ENamedThreads::GameThread, 
				[Callback = MoveTemp(Callback), bSuccess = Result.second, NumSubscribers = Result.first]() -> void
			{
				Callback.ExecuteIfBound(bSuccess, static_cast<int32>(NumSubscribers));
			});
		}
	});
}

template<bool bSSL>
void TWebSocketProxy<bSSL>::Publish(FString&& Topic, FString&& Message, FOnWebSocketPublished&& Callback)
{
	ExecuteOnServerThread([
		Topic    = MoveTemp(Topic),
		Message  = MoveTemp(Message),
		Callback = MoveTemp(Callback)
	](FWebSocket* Socket) mutable -> void
	{
		const bool bSuccess = Socket->publish(TCHAR_TO_UTF8(*Topic), TCHAR_TO_UTF8(*Message));

		if (Callback.IsBound())
		{
			AsyncTask(ENamedThreads::GameThread, [Callback = MoveTemp(Callback), bSuccess]() -> void
			{
				Callback.ExecuteIfBound(bSuccess);
			});
		}
	});
}

template<bool bSSL>
bool TWebSocketProxy<bSSL>::IsSocketValid() const
{
	return bIsSocketValid;
}

template<bool bSSL>
TWebSocketProxy<bSSL>::~TWebSocketProxy()
{
	// This proxy should always be destroyed from Game Thread
	// as it holds a strong reference to an UObject.
	check(IsInGameThread());
}


///////////////////////////////////////////////////////////////
// TWebSocketServerInternal

template<bool bSSL>
TWebSocketServerInternal<bSSL>::TWebSocketServerInternal() 
	: IWebSocketServerInternal()
{
}

template<bool bSSL>
TWebSocketServerInternal<bSSL>::~TWebSocketServerInternal()
{
	if (SharedRessources->ServerStatus == EWebSocketServerState::Running)
	{
		UE_LOG(LogWebSocketServer, Warning, TEXT("WebSocket Server wasn't closed. Closing it now."));
		Close();
	}
}

template<bool bSSL>
void TWebSocketServerInternal<bSSL>::Listen(FString&& Host, FString&& URI, const uint16 Port,
	FOnWebSocketServerListening&& Callback, FOnServerClosed&& OnServerClosed)
{
	using FWebSocketBehavior = typename uWS::template TemplatedApp<bSSL>::template WebSocketBehavior<FWebSocketData>;

	EWebSocketServerState Expected = EWebSocketServerState::Closed;
	if (!SharedRessources->ServerStatus.CompareExchange(Expected, EWebSocketServerState::Starting))
	{
		return;
	}

	UE_LOG(LogWebSocketServer, Log, TEXT("Starting server on %s:%d..."), *URI, Port);

	Thread.Reset(new std::thread(
	[
		// Settings
		Compression					= this->Compression,
		MaxPayloadLength			= this->MaxPayloadLength,
		IdleTimeout					= this->IdleTimeout,
		MaxBackPressure				= this->MaxBackPressure,
		bCloseOnBackpressureLimit	= this->bCloseOnBackpressureLimit,
		bResetIdleTimeoutOnSend		= this->bResetIdleTimeoutOnSend,
		bSendPingsAutomatically		= this->bSendPingsAutomatically,
		MaxLifetime					= this->MaxLifetime,

		// SSL options
		KeyFile			= std::string(TCHAR_TO_UTF8(*KeyFile)),
		CertFile		= std::string(TCHAR_TO_UTF8(*CertFile)),
		PassPhrase		= std::string(TCHAR_TO_UTF8(*PassPhrase)),
		DhParamsFile	= std::string(TCHAR_TO_UTF8(*DhParamsFile)),
		CaFileName		= std::string(TCHAR_TO_UTF8(*CaFileName)),

		// Events
		OnOpened		= MoveTemp(this->OnOpenedEvent),
		OnMessage		= MoveTemp(this->OnMessageEvent),
		OnClosed		= MoveTemp(this->OnClosedEvent),
		OnPing			= MoveTemp(this->OnPingEvent),
		OnPong			= MoveTemp(this->OnPongEvent),
		OnServerClosed	= MoveTemp(OnServerClosed),

		// Parameters
		Port,
		URI			= MoveTemp(URI),
		Host		= MoveTemp(Host),
		Callback	= MoveTemp(Callback),

		SharedRessources = this->SharedRessources,
		Server			 = this->AsShared()
	]() mutable -> void
	{			
		FWebSocketBehavior Behavior;

		Behavior.compression				= NWebSocketUtils::Convert(Compression);
		Behavior.maxPayloadLength			= MaxPayloadLength;
		Behavior.idleTimeout				= IdleTimeout;
		Behavior.maxBackpressure			= MaxBackPressure;
		Behavior.closeOnBackpressureLimit	= bCloseOnBackpressureLimit;
		Behavior.resetIdleTimeoutOnSend		= bResetIdleTimeoutOnSend;
		Behavior.sendPingsAutomatically		= bSendPingsAutomatically;
		Behavior.maxLifetime				= MaxLifetime;

		// We let default upgrade.
		//Behavior.upgrade = [](uWS::HttpResponse<bSSL>* Response, uWS::HttpRequest* Request, struct us_socket_context_t* Context) -> void
		//{
		//	UE_LOG(LogWebSocketServer, Verbose, TEXT("New connection received for upgrade."));
		//
		//	Response->upgrade<FWebSocketData>(FWebSocketData(),
		//		Request->getHeader("sec-websocket-key"),
		//		Request->getHeader("sec-websocket-protocol"),
		//		Request->getHeader("sec-websocket-extensions"),
		//		Context);
		//};

		Behavior.open = [&Server, &OnOpened](FWebSocket* Socket) -> void
		{
			UE_LOG(LogWebSocketServer, Verbose, TEXT("New WebSocket connection opened."));

			FWebSocketData* const SocketData = Socket->getUserData();
			SocketData->SetSocket(Socket);
			SocketData->GetProxy()->OnOpen(Server, Socket, OnOpened);
		};

		Behavior.message = [&OnMessage](FWebSocket* Socket, std::string_view Message, uWS::OpCode Code) -> void
		{
			FWebSocketData* const SocketData = Socket->getUserData();
			SocketData->GetProxy()->OnMessage(Message, Code, OnMessage);
		};

		//Behavior.drain = [](FWebSocket* Socket) -> void
		//{
		//
		//};

		if (!bSendPingsAutomatically)
		{
			Behavior.ping = [&OnPing](FWebSocket* Socket, std::string_view Data) -> void
			{
				FWebSocketData* const SocketData = Socket->getUserData();
				SocketData->GetProxy()->OnPing(Data, OnPing);
			};

			Behavior.pong = [&OnPong](FWebSocket* Socket, std::string_view Data) -> void
			{
				FWebSocketData* const SocketData = Socket->getUserData();
				SocketData->GetProxy()->OnPong(Data, OnPong);
			};
		}

		Behavior.close = [&OnClosed](FWebSocket* Socket, int Code, std::string_view Message) -> void
		{
			UE_LOG(LogWebSocketServer, Verbose, TEXT("WebSocket closed."));

			FWebSocketData* const SocketData = Socket->getUserData();
			
			auto Proxy = SocketData->GetProxy();

			SocketData->OnClosed();
			Proxy     ->OnClosed(Code, Message, OnClosed);
		};

		if constexpr (bSSL == true)
		{
			Server->App = uWSApp(
			{
				/* .key_file_name		 = */ KeyFile     .size() == 0 ? nullptr : KeyFile     .c_str(),
				/* .cert_file_name		 = */ CertFile    .size() == 0 ? nullptr : CertFile    .c_str(),
				/* .passphrase			 = */ PassPhrase  .size() == 0 ? nullptr : PassPhrase  .c_str(),
				/* .dh_params_file_name  = */ DhParamsFile.size() == 0 ? nullptr : DhParamsFile.c_str(),
				/* .ca_file_name		 = */ CaFileName  .size() == 0 ? nullptr : CaFileName  .c_str()					
			});
		}
		else
		{
			Server->App = uWSApp();
		}
		
		Server->App
		
		.template ws<FWebSocketData>(TCHAR_TO_UTF8(*URI), MoveTemp(Behavior))

		.listen(TCHAR_TO_UTF8(*Host), Port, [&SharedRessources, &Callback, _URI = URI](us_listen_socket_t* ListenSocket) -> void
		{
			check(SharedRessources->ServerStatus == EWebSocketServerState::Starting);

			const bool bServerStarted = ([&]() -> bool
			{
				if (!ListenSocket)
				{
					UE_LOG(LogWebSocketServer, Error, TEXT("Failed to start WebSocket server."));

					return false;
				}

				UE_LOG(LogWebSocketServer, Log, TEXT("WebSocket Server started at %s."), *_URI);

				SharedRessources->Loop         = uWS::Loop::get();
				SharedRessources->ListenSocket = ListenSocket;
				SharedRessources->ServerStatus = EWebSocketServerState::Running;

				return true;
			}());

			if (Callback.IsBound())
			{
				AsyncTask(ENamedThreads::GameThread, [Callback = MoveTemp(Callback), bServerStarted]() -> void
				{
					Callback.ExecuteIfBound(bServerStarted);
				});
			}
		})
			
		.run();

		UE_LOG(LogWebSocketServer, Log, TEXT("WebSocket loop exited."));
		
		{
			FScopeLock Lock(&SharedRessources->LoopAccess);
			if (SharedRessources->Loop)
			{
				SharedRessources->Loop->free();
				SharedRessources->Loop = nullptr;
			}
		}

		SharedRessources->ListenSocket = nullptr;
		SharedRessources->ServerStatus.Exchange(EWebSocketServerState::Closed);

		UE_LOG(LogWebSocketServer, Log, TEXT("WebSocket Server closed."));

		AsyncTask(ENamedThreads::GameThread, [OnServerClosed = MoveTemp(OnServerClosed)]() -> void
		{
			OnServerClosed.ExecuteIfBound();
		});
	}));

	Thread->detach();
}

template<bool bSSL>
void TWebSocketServerInternal<bSSL>::Close()
{
	EWebSocketServerState Expected = EWebSocketServerState::Running;
	if (SharedRessources->ServerStatus.CompareExchange(Expected, EWebSocketServerState::Closing))
	{
		auto LoopWork = [SharedRessources = this->SharedRessources]() -> void
		{
			if (SharedRessources->ListenSocket)
			{
				us_listen_socket_close(0, SharedRessources->ListenSocket);
				SharedRessources->ListenSocket = nullptr;

				UE_LOG(LogWebSocketServer, Verbose, TEXT("Closed listening socket."));
			}
			else
			{
				UE_LOG(LogWebSocketServer, Warning, TEXT("Failed to close server: ListenSocket was nullptr."));
			}
		};

		{
			FScopeLock Lock(&SharedRessources->LoopAccess);

			if (SharedRessources->Loop)
			{
				SharedRessources->Loop->defer(MoveTemp(LoopWork));
			}
			else
			{
				UE_LOG(LogWebSocketServer, Warning, TEXT("Failed to close server: Loop was nullptr."));
			}
		}
	}
	else
	{
		UE_LOG(LogWebSocketServer, Warning, TEXT("Called Close() on WebSocket Server but the server wasn't running."));
	}
}

template<bool bSSL>
void TWebSocketServerInternal<bSSL>::Publish(FString&& Topic, FString&& Message, EWebSocketOpCode OpCode)
{
	auto LoopWork = 
	[
		Self	= this->AsShared(), 
		Topic	= MoveTemp(Topic), 
		Message = MoveTemp(Message), 
		Code    = NWebSocketUtils::Convert(OpCode)
	]() -> void
	{
		Self->App.publish(TCHAR_TO_UTF8(*Topic), TCHAR_TO_UTF8(*Message), static_cast<uWS::OpCode>(Code));
	};

	{
		FScopeLock Lock(&SharedRessources->LoopAccess);

		if (SharedRessources->Loop)
		{
			SharedRessources->Loop->defer(MoveTemp(LoopWork));
		}
	}
}

