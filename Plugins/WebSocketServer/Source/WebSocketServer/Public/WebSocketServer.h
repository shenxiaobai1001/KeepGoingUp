// Copyright Pandores Marketplace 2025. All Righst Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WebSocketServer.generated.h"

class UWebSocket;
class UWebSocketServer;

/**
 * How we want to compress data.
*/
UENUM(BlueprintType)
enum class EWebSocketCompressOptions : uint8
{
    /* No compression, recommended option. */
    DISABLED = 0                UMETA(DisplayName = "Disabled"),

    SHARED_COMPRESSOR = 1       UMETA(DisplayName = "Shared Compressor"),
    DEDICATED_COMPRESSOR_3KB    UMETA(DisplayName = "Shared Compressor 3kb"),
    DEDICATED_COMPRESSOR_4KB    UMETA(DisplayName = "Shared Compressor 4kb"),
    DEDICATED_COMPRESSOR_8KB    UMETA(DisplayName = "Shared Compressor 8kb"),
    DEDICATED_COMPRESSOR_16KB   UMETA(DisplayName = "Shared Compressor 16kb"),
    DEDICATED_COMPRESSOR_32KB   UMETA(DisplayName = "Shared Compressor 32kb"),
    DEDICATED_COMPRESSOR_64KB   UMETA(DisplayName = "Shared Compressor 64kb"),
    DEDICATED_COMPRESSOR_128KB  UMETA(DisplayName = "Shared Compressor 128kb"),
    DEDICATED_COMPRESSOR_256KB  UMETA(DisplayName = "Shared Compressor 256kb"),

    /* Same as 256kb */
    DEDICATED_COMPRESSOR UMETA(DisplayName = "Dedicated Compressor")
};

/**
 *  The operation code used for a WebSocket message.
*/
UENUM(BlueprintType)
enum class EWebSocketOpCode : uint8
{
    /* Do not use. Used for UBT compliance. */
    None UMETA(hidden),

    /* The message's content is text. */
    TEXT    =  1    UMETA(DisplayName = "Text"),

    /* The message's content is binary. */
    BINARY  =  2    UMETA(DisplayName = "Binary"),

    /* The message's is a close message. Connection will be closed. */
    CLOSE   =  8    UMETA(DisplayName = "Close"),

    /* The message's is a ping message. */
    PING    =  9    UMETA(DisplayName = "Ping"),

    /* The message's is a pong message. */
    PONG    = 10    UMETA(DisplayName = "Pong")
};

/**
 * The current state of a WebSocket server.
*/
UENUM(BlueprintType)
enum class EWebSocketServerState : uint8
{
    /* We are closing the server */
    Closing, 

    /* The server is not running */
    Closed,

    /* The server is starting but can't receive clients yet. */
    Starting,

    /* The client is running and handle clients */
    Running
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnWebSocketOpened, 
        class UWebSocket*, Socket
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FOnWebSocketMessage, 
        class UWebSocket*, Socket, 
        const FString&, Message, 
        EWebSocketOpCode, OpCode
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FOnWebSocketRawMessage, 
        class UWebSocket*, Socket, 
        const TArray<uint8>&, Message, 
        EWebSocketOpCode, OpCode
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FOnWebSocketClosed, 
        class UWebSocket*, Socket, 
        const int32, Code, 
        const FString&, Message
);

DECLARE_DELEGATE_OneParam(
    FOnWebSocketServerListening,
        bool /* bSuccess */
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWebSocketServerClosed);


DECLARE_DELEGATE_TwoParams(
	FOnWebSocketSubscribed,
		bool /* bSuccess */,
		int32 /* Subscriber Count */
);

DECLARE_DELEGATE_OneParam(
	FOnWebSocketPublished,
		bool /* bSuccess */
);

/**
 * A WebSocket used to communicate with a client.
*/
UCLASS(BlueprintType)
class WEBSOCKETSERVER_API UWebSocket : public UObject
{
	GENERATED_BODY()
private:
	template<bool bSSL>
	friend class TWebSocketProxy;

public:
	 UWebSocket() = default;
	~UWebSocket() = default;

	/**
	 * Sends a message through the WebSocket.
	 * @param Message The message to send.
	*/
	UFUNCTION(BlueprintCallable, Category = "WebSocket|Server|WebSocket")
	void Send(const FString& Message);
	void Send(FString &&Message);
	void Send(const TArray<uint8>& Data);
	void Send(TArray<uint8> &&Data);

	/**
	 * Sends data through the WebSocket
	 * @param Data The data to send.
	*/
	UFUNCTION(BlueprintCallable, Category = "WebSocket|Server|WebSocket", meta = (DisplayName = "Send Binary"))
	void Send_Blueprint(const TArray<uint8>& Data); /* Avoid UBT name collision */

	/**
	 * Pings the client. 
	 * @param Message The optional data sent with the ping.
	*/
	UFUNCTION(BlueprintCallable, Category = "WebSocket|Server|WebSocket")
	void Ping(const FString& Message = TEXT(""));
	void Ping(FString &&Message);

	/**
	 * Pongs the client. 
	 * @param Message The optional data sent with the pong.
	*/
	UFUNCTION(BlueprintCallable, Category = "WebSocket|Server|WebSocket")
	void Pong(const FString& Message = TEXT(""));
	void Pong(FString &&Message);

	/**
	 * Closes the socket immediatly server-side.
	*/
	UFUNCTION(BlueprintCallable, Category = "WebSocket|Server|WebSocket")
	void Close();

	/**
	 * Ends the socket gracefully, sending a reason and an end code to the client.
	*/
	UFUNCTION(BlueprintCallable, Category = "WebSocket|Server|WebSocket")
	void End(const int32 Code = 0, const FString& Message = TEXT(""));
	void End(const int32 Code, FString&& Message);
	
	/**
	 * Subscribes this WebSocket to a Topic.
	 * @param Topic The topic to subscribe to.
	*/
	void Subscribe(const FString&  Topic, const FOnWebSocketSubscribed&  Callback,                            bool bNonStrict = false);
	void Subscribe(const FString&  Topic,       FOnWebSocketSubscribed&& Callback = FOnWebSocketSubscribed(), bool bNonStrict = false);
	void Subscribe(      FString&& Topic, const FOnWebSocketSubscribed&  Callback                           , bool bNonStrict = false);
	void Subscribe(      FString&& Topic,       FOnWebSocketSubscribed&& Callback = FOnWebSocketSubscribed(), bool bNonStrict = false);

	/**
	 * Unubscribes this WebSocket to a Topic.
	 * @param Topic The topic to unsubscribe to.
	*/
	void Unsubscribe(const FString&  Topic, const FOnWebSocketSubscribed&  Callback,                            bool bNonStrict = false);
	void Unsubscribe(const FString&  Topic,       FOnWebSocketSubscribed&& Callback = FOnWebSocketSubscribed(), bool bNonStrict = false);
	void Unsubscribe(      FString&& Topic, const FOnWebSocketSubscribed&  Callback,                            bool bNonStrict = false);
	void Unsubscribe(      FString&& Topic,       FOnWebSocketSubscribed&& Callback = FOnWebSocketSubscribed(), bool bNonStrict = false);

	/**
	 * Publishes a message to all WebSocket subscribed to a topic.
	 * @param Topic    The topic to publish to.
	 * @param Message  The message to publish.
	 * @param Callback Callback called when the publication has been done.
	*/
	void Publish(const FString&  Topic, const FString&  Message, const FOnWebSocketPublished&  Callback = FOnWebSocketPublished());
	void Publish(      FString&& Topic,       FString&& Message,       FOnWebSocketPublished&& Callback = FOnWebSocketPublished());

	/**
	 * Checks if the WebSocket is connected to a client.
	 * @return If the WebSocket is connected to a client.
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WebSocket|Server|WebSocket")
	UPARAM(DisplayName = "Is Connected") bool IsConnected() const;

private:
	TWeakPtr<class IWebSocketProxy, ESPMode::ThreadSafe> SocketProxy;
};

/**
 * Base class for a WebSocket server.
 * Can be either WSS or WS depending on how it was created.
 **/
UCLASS(BlueprintType)
class WEBSOCKETSERVER_API UWebSocketServer : public UObject
{
    GENERATED_BODY()
public:
    /**
     * Called when a new client successfully upgraded the connection from HTTP(S)
     * And established a WebSocket connection.
    */
    UPROPERTY(BlueprintAssignable, Category = "WebSocket|Server")
    FOnWebSocketOpened OnWebSocketOpened;

    /**
     * Called when a WebSocket client sent us a message.
    */
    UPROPERTY(BlueprintAssignable, Category = "WebSocket|Server")
    FOnWebSocketMessage OnWebSocketMessage;

    /**
     * Called when a WebSocket client sent us a message.
    */
    UPROPERTY(BlueprintAssignable, Category = "WebSocket|Server")
    FOnWebSocketRawMessage OnWebSocketRawMessage;

    /**
     * Called when a connection with a client has been closed.
    */
    UPROPERTY(BlueprintAssignable, Category = "WebSocket|Server")
    FOnWebSocketClosed OnWebSocketClosed;
    
    /**
     * Called when we received a ping.
    */
    UPROPERTY(BlueprintAssignable, Category = "WebSocket|Server")
    FOnWebSocketMessage OnWebSocketPing;
    
    /**
     * Called when we received a pong.
    */
    UPROPERTY(BlueprintAssignable, Category = "WebSocket|Server")
    FOnWebSocketMessage OnWebSocketPong;

    /**
     * Called when the WebSocket server has been closed.
    */
    UPROPERTY(BlueprintAssignable, Category = "WebSocket|Server")
    FOnWebSocketServerClosed OnWebSocketServerClosed;

public:
    /**
     * Creates a new WebSocket Server without encryption.
     * @return A new WebSocket Server.
    */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WebSocket|Server")
    static UPARAM(DisplayName = "WebSocket Server") UWebSocketServer* CreateWebSocketServer();

    /**
     * Creates a new WebSocket Server with SSL/TLS support.
     * @param KeyFile       The key file for SSL/TLS.
     * @param CertFile      The cert file for SSL/TLS.
     * @param PassPhrase    The passphrase for the server. Empty for none.
     * @param DhParamsFile  The DH parameters file. Empty for none.
     * @param CaFile        The CA file. Empty for none.
     * @return A new WebSocket Server.
    */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WebSocket|Server")
    static UPARAM(DisplayName = "WebSocket Server") UWebSocketServer* CreateSecureWebSocketServer(
        FString KeyFile,
        FString CertFile,
        FString PassPhrase,
        FString DhParamsFile,
        FString CaFile);

public:

    /**
     * Converts a string to binary data.
     * @param InData The string to convert.
     * @param OutData The converted string.
    */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WebSocket|Server")
    static void StringToBinary(const FString& InData, TArray<uint8>& OutData);

public:
     UWebSocketServer();
    ~UWebSocketServer();

    /**
     * Starts listening for new clients.
     * @param URI The URI where to listen to.
     * @param Port The port where to listen to.
     * @param Callback Callback called when the server started
     *                 or failed to listen.
    */
    void Listen(FString Host, FString URI, const int32 Port, FOnWebSocketServerListening Callback);
    void Listen(FString URI, const int32 Port, FOnWebSocketServerListening Callback);

    /**
     * Stop listening for new clients. The Server will still handle current clients
     * but will close as soon as all clients are disconnected.
    */
    UFUNCTION(BlueprintCallable, Category = "WebSocket|Server", meta=(KeyWords="Close Stop Kill"))
    void StopListening();
    
    /**
     * Sets the idle timeout before disconnecting a client.
     * 0 for disabled. Must be greater or equal to 8 and a multiple of 4.
     * Default is 120 seconds.
     * @param InIdleTimeout The idle timeout.
    */
    UFUNCTION(BlueprintCallable, Category = "WebSocket|Server")
    void SetIdleTimeout(UPARAM(DisplayName = "Idle Timeout") const int64 InIdleTimeout);
    
    /**
     * Sets the maximum lifetime of a client. 0 for disabled. 
     * Default is disabled.
     * @param InMaxLifetime The WebSocket max lifetime.
    */
    UFUNCTION(BlueprintCallable, Category = "WebSocket|Server")
    void SetMaxLifetime(UPARAM(DisplayName = "Max Lifetime") const int64 InMaxLifetime);

    /**
     * Sets the max payload length in bytes.
     * Default is 256KB.
     * @param InMaxPayloadLength The new max payload length.
    */
    UFUNCTION(BlueprintCallable, Category = "WebSocket|Server")
    void SetMaxPayloadLength(const int64 InMaxPayloadLength);

    /**
     * Sets the max back pressure in bytes.
     * Default is 256KB.
     * @param InMaxBackPressure The new max back pressure.
    */
    UFUNCTION(BlueprintCallable, Category = "WebSocket|Server")
    void SetMaxBackPressure(const int64 InMaxBackPressure);

    /**
     * Sets if we should close sockets when we reach back pressure limit.
     * @param bInCloseOnBackpressureLimit If we should close sockets.
    */
    UFUNCTION(BlueprintCallable, Category = "WebSocket|Server")
    void SetCloseOnBackpressureLimit(const bool bInCloseOnBackpressureLimit);

    /**
     * Sets if sending data through a WebSocket resets the idle timeout.
     * @param bInResetIdleTimeoutOnSend If we should reset timeout on send.
    */
    UFUNCTION(BlueprintCallable, Category = "WebSocket|Server")
    void SetResetIdleTimeoutOnSend(const bool bInResetIdleTimeoutOnSend);

    /**
     * Sets if pings should be sent automatically.
     * @param bInSendPingsAutomatically If we should ping automatically clients.
    */
    UFUNCTION(BlueprintCallable, Category = "WebSocket|Server")
    void SetSendPingsAutomatically(const bool bInSendPingsAutomatically);

    /**
     * Sets the compression of data sent through WebSockets.
     * It is recommended to set it to disabled.
     * It literally trades CPU for data size, knowing that you win
     * at most ~30% on data size.
     * @param InCompression The new compression.
    */
    UFUNCTION(BlueprintCallable, Category = "WebSocket|Server")
    void SetCompression(const EWebSocketCompressOptions InCompression);

    /**
     * Publishes a message to a topic. 
     * @param Topic   The topic to publish to.
     * @param Message The message published.
     * @param OpCode  The opcode for the message
    */
    UFUNCTION(BlueprintCallable, Category = "WebSocket|Server")
    void Publish(const FString&  Topic, const FString&  Message, EWebSocketOpCode OpCode = EWebSocketOpCode::TEXT);
    void Publish(const FString&  Topic,       FString&& Message, EWebSocketOpCode OpCode = EWebSocketOpCode::TEXT);
    void Publish(      FString&& Topic, const FString&  Message, EWebSocketOpCode OpCode = EWebSocketOpCode::TEXT);
    void Publish(      FString&& Topic,       FString&& Message, EWebSocketOpCode OpCode = EWebSocketOpCode::TEXT);

    /**
     * Gets the current state of the server.
     * @return The current state of the server.
    */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WebSocket|Server", meta = (KeyWords = "Status State Running Closed Open"))
    UPARAM(DisplayName = "State") EWebSocketServerState GetServerState() const;

private:
    void InternalOnServerClosed();
    void InternalOnWebSocketOpened (UWebSocket*);
    void InternalOnWebSocketMessage(UWebSocket*, TArray<uint8>, EWebSocketOpCode);
    void InternalOnWebSocketClosed (UWebSocket*, const int32, const FString&);
    
private:
    TSharedPtr<class IWebSocketServerInternal, ESPMode::ThreadSafe> Internal;
};

