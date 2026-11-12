#pragma once

#include "CoreMinimal.h"
#include "Sockets.h"
#include "ABFPProtocol.h"

class FABFPNetworkClient
{
public:
    bool Connect(const FString& Host, int32 Port);
    void Disconnect();
    bool IsConnected() const;
    bool SendCommand(const FString& Command);
    bool PollSnapshot(FABFPSnapshot& OutSnapshot);

private:
    bool ParseStateLine(const FString& Line, FABFPSnapshot& OutSnapshot) const;

    FSocket* Socket = nullptr;
    FString ReceiveBuffer;
};
