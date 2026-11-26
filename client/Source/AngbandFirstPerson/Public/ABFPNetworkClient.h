#pragma once

#include "CoreMinimal.h"
#include "Sockets.h"
#include "ABFPProtocol.h"

enum class EABFPConnectionState : uint8
{
    Disconnected,
    Connecting,
    Connected
};

class FABFPNetworkClient
{
public:
    FABFPNetworkClient() = default;
    ~FABFPNetworkClient();
    FABFPNetworkClient(const FABFPNetworkClient&) = delete;
    FABFPNetworkClient& operator=(const FABFPNetworkClient&) = delete;

    /** Starts an asynchronous non-blocking TCP connect so the GameThread never stalls. */
    bool BeginConnect(const FString& Host, int32 Port);

    /** Polls an in-flight non-blocking connect attempt and returns the updated state. */
    EABFPConnectionState TickConnect();

    /** Backward-compatible alias for BeginConnect(). */
    bool Connect(const FString& Host, int32 Port) { return BeginConnect(Host, Port); }

    void Disconnect();
    bool IsConnected() const { return ConnectionState == EABFPConnectionState::Connected && Socket != nullptr; }
    bool IsConnecting() const { return ConnectionState == EABFPConnectionState::Connecting && Socket != nullptr; }
    EABFPConnectionState GetConnectionState() const { return ConnectionState; }
    bool SendCommand(const FString& Command);
    bool PollSnapshot(FABFPSnapshot& OutSnapshot);

private:
    bool ParseStateLine(const FString& Line, FABFPSnapshot& OutSnapshot) const;

    FSocket* Socket = nullptr;
    EABFPConnectionState ConnectionState = EABFPConnectionState::Disconnected;
    double ConnectStartTimeSeconds = 0.0;
    double LastConnectPollTimeSeconds = 0.0;
    TArray<uint8> RawReceiveBuffer;
};
