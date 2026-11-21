#include "ABFPNetworkClient.h"

#include "Dom/JsonObject.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "SocketSubsystem.h"
#include "Containers/StringConv.h"

namespace
{
    int32 ReadInt(const TSharedPtr<FJsonValue>& Value)
    {
        return Value.IsValid() ? static_cast<int32>(Value->AsNumber()) : 0;
    }

    int64 ReadInt64(const TSharedPtr<FJsonValue>& Value)
    {
        return Value.IsValid() ? static_cast<int64>(Value->AsNumber()) : 0;
    }

    int64 ReadId(const TSharedPtr<FJsonValue>& Value)
    {
        if (!Value.IsValid()) return 0;
        if (Value->Type == EJson::String) return FCString::Atoi64(*Value->AsString());
        return static_cast<int64>(Value->AsNumber());
    }

    bool ReadRow(const TSharedPtr<FJsonValue>& Value,
        TArray<TSharedPtr<FJsonValue>>& OutValues)
    {
        if (!Value.IsValid() || Value->Type != EJson::Array) return false;
        OutValues = Value->AsArray();
        return true;
    }
}

bool FABFPNetworkClient::Connect(const FString& Host, int32 Port)
{
    Disconnect();
    ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!Subsystem || Port <= 0 || Port > 65535) return false;

    Socket = Subsystem->CreateSocket(NAME_Stream, TEXT("ABFP TCP Client"), false);
    if (!Socket) return false;

    bool bValidAddress = false;
    TSharedRef<FInternetAddr> Address = Subsystem->CreateInternetAddr();
    Address->SetIp(*Host, bValidAddress);
    Address->SetPort(Port);
    if (!bValidAddress || !Socket->Connect(*Address))
    {
        Disconnect();
        return false;
    }

    Socket->SetNonBlocking(true);
    Socket->SetNoDelay(true);
    RawReceiveBuffer.Reset();
    return true;
}

void FABFPNetworkClient::Disconnect()
{
    if (Socket)
    {
        Socket->Close();
        if (ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
            Subsystem->DestroySocket(Socket);
        Socket = nullptr;
    }
    RawReceiveBuffer.Reset();
}

bool FABFPNetworkClient::IsConnected() const
{
    return Socket != nullptr &&
        Socket->GetConnectionState() == ESocketConnectionState::SCS_Connected;
}

bool FABFPNetworkClient::SendCommand(const FString& Command)
{
    if (!Socket || Command.IsEmpty()) return false;
    const FString Wire = Command + TEXT("\n");
    FTCHARToUTF8 Utf8(*Wire);
    int32 Offset = 0;
    while (Offset < Utf8.Length())
    {
        int32 Sent = 0;
        if (!Socket->Send(reinterpret_cast<const uint8*>(Utf8.Get()) + Offset,
            Utf8.Length() - Offset, Sent) || Sent <= 0)
        {
            Disconnect();
            return false;
        }
        Offset += Sent;
    }
    return true;
}

bool FABFPNetworkClient::PollSnapshot(FABFPSnapshot& OutSnapshot)
{
    if (!Socket) return false;

    /*
     * HasPendingData() reports payload bytes, but an orderly TCP close can
     * make the socket readable with zero payload. Probe that case as well so
     * the pawn's reconnect loop can recover after a host restart.
     */
    while (true)
    {
        uint32 Pending = 0;
        const bool bHasPending = Socket->HasPendingData(Pending) && Pending > 0;
        if (!bHasPending &&
            !Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::Zero()))
        {
            break;
        }

        const int32 ToRead = bHasPending ?
            static_cast<int32>(FMath::Min<uint32>(Pending, 8192)) : 8192;
        const int32 OldNum = RawReceiveBuffer.Num();
        RawReceiveBuffer.SetNumUninitialized(OldNum + ToRead);
        int32 Read = 0;
        if (!Socket->Recv(RawReceiveBuffer.GetData() + OldNum, ToRead, Read) || Read <= 0)
        {
            RawReceiveBuffer.SetNum(OldNum, EAllowShrinking::No);
            Disconnect();
            return false;
        }
        RawReceiveBuffer.SetNum(OldNum + Read, EAllowShrinking::No);
        if (RawReceiveBuffer.Num() > 4 * 1024 * 1024)
        {
            Disconnect();
            return false;
        }
    }

    bool bUpdated = false;
    int32 ConsumeOffset = 0;
    while (ConsumeOffset < RawReceiveBuffer.Num())
    {
        int32 NewlineIndex = INDEX_NONE;
        for (int32 Idx = ConsumeOffset; Idx < RawReceiveBuffer.Num(); ++Idx)
        {
            if (RawReceiveBuffer[Idx] == static_cast<uint8>('\n'))
            {
                NewlineIndex = Idx;
                break;
            }
        }
        if (NewlineIndex == INDEX_NONE) break;

        const int32 LineLength = NewlineIndex - ConsumeOffset;
        if (LineLength > 0)
        {
            FUTF8ToTCHAR Converter(
                reinterpret_cast<const ANSICHAR*>(RawReceiveBuffer.GetData() + ConsumeOffset),
                LineLength);
            const FString Line(Converter.Length(), Converter.Get());
            FABFPSnapshot Parsed;
            if (ParseStateLine(Line, Parsed))
            {
                OutSnapshot = MoveTemp(Parsed);
                bUpdated = true;
            }
        }
        ConsumeOffset = NewlineIndex + 1;
    }

    if (ConsumeOffset > 0)
    {
        RawReceiveBuffer.RemoveAt(0, ConsumeOffset, EAllowShrinking::No);
    }

    return bUpdated;
}

bool FABFPNetworkClient::ParseStateLine(const FString& Line,
    FABFPSnapshot& OutSnapshot) const
{
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Line);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return false;

    FString Type;
    if (!Root->TryGetStringField(TEXT("type"), Type) || Type != TEXT("state"))
        return false;

    double Turn = 0.0;
    double Depth = 0.0;
    if (!Root->TryGetNumberField(TEXT("turn"), Turn) ||
        !Root->TryGetNumberField(TEXT("depth"), Depth)) return false;

    FABFPSnapshot Snapshot;
    Snapshot.Turn = static_cast<int64>(Turn);
    Root->TryGetBoolField(TEXT("initial"), Snapshot.bInitialState);
    Snapshot.DungeonDepth = static_cast<int32>(Depth);

    const TArray<TSharedPtr<FJsonValue>>* Player = nullptr;
    if (!Root->TryGetArrayField(TEXT("player"), Player) || !Player || Player->Num() < 2)
        return false;
    Snapshot.PlayerX = ReadInt((*Player)[0]);
    Snapshot.PlayerY = ReadInt((*Player)[1]);

    const TArray<TSharedPtr<FJsonValue>>* PlayerStats = nullptr;
    if (Root->TryGetArrayField(TEXT("player_stats"), PlayerStats) && PlayerStats &&
        PlayerStats->Num() >= 4)
    {
        Snapshot.PlayerHitPoints = ReadInt((*PlayerStats)[0]);
        Snapshot.PlayerMaxHitPoints = ReadInt((*PlayerStats)[1]);
        Snapshot.PlayerLevel = ReadInt((*PlayerStats)[2]);
        Snapshot.PlayerGold = ReadInt((*PlayerStats)[3]);
        if (PlayerStats->Num() >= 6)
        {
            Snapshot.PlayerMana = ReadInt((*PlayerStats)[4]);
            Snapshot.PlayerMaxMana = ReadInt((*PlayerStats)[5]);
        }
        if (PlayerStats->Num() >= 7)
        {
            Snapshot.bPlayerDead = ReadInt((*PlayerStats)[6]) != 0;
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* Cells = nullptr;
    if (!Root->TryGetArrayField(TEXT("cells"), Cells) || !Cells) return false;
    Snapshot.Cells.Reserve(Cells->Num());
    for (const TSharedPtr<FJsonValue>& Value : *Cells)
    {
        TArray<TSharedPtr<FJsonValue>> Row;
        if (!ReadRow(Value, Row) || Row.Num() < 7) return false;
        FABFPCell Cell;
        Cell.X = ReadInt(Row[0]);
        Cell.Y = ReadInt(Row[1]);
        const int32 TerrainValue = ReadInt(Row[2]);
        Cell.Terrain = (TerrainValue >= 0 && TerrainValue <= static_cast<int32>(EABFPTerrain::Vein))
            ? static_cast<EABFPTerrain>(TerrainValue)
            : EABFPTerrain::Wall;
        Cell.bVisible = ReadInt(Row[3]) != 0;
        Cell.bLit = ReadInt(Row[4]) != 0;
        Cell.bOccupiedByMonster = ReadInt(Row[5]) != 0;
        Cell.bOccupiedByObject = ReadInt(Row[6]) != 0;
        Snapshot.Cells.Add(Cell);
    }

    const TArray<TSharedPtr<FJsonValue>>* Entities = nullptr;
    if (!Root->TryGetArrayField(TEXT("entities"), Entities) || !Entities) return false;
    Snapshot.Entities.Reserve(Entities->Num());
    for (const TSharedPtr<FJsonValue>& Value : *Entities)
    {
        TArray<TSharedPtr<FJsonValue>> Row;
        if (!ReadRow(Value, Row) || Row.Num() < 10) return false;
        FABFPEntity Entity;
        Entity.Id = ReadId(Row[0]);
        Entity.X = ReadInt(Row[1]);
        Entity.Y = ReadInt(Row[2]);
        Entity.Type = static_cast<EABFPEntityType>(ReadInt(Row[3]));
        Entity.KindId = ReadInt(Row[4]);
        Entity.CategoryId = ReadInt(Row[5]);
        Entity.VariantId = ReadInt(Row[6]);
        Entity.HitPoints = ReadInt(Row[7]);
        Entity.MaxHitPoints = ReadInt(Row[8]);
        Entity.Quantity = ReadInt(Row[9]);
        if (Row.Num() > 10 && Row[10].IsValid() &&
            Row[10]->Type == EJson::String)
            Entity.Name = Row[10]->AsString();
        Snapshot.Entities.Add(Entity);
    }

    const TArray<TSharedPtr<FJsonValue>>* Events = nullptr;
    if (Root->TryGetArrayField(TEXT("events"), Events) && Events)
    {
        Snapshot.Events.Reserve(Events->Num());
        for (const TSharedPtr<FJsonValue>& Value : *Events)
        {
            TArray<TSharedPtr<FJsonValue>> Row;
            if (!ReadRow(Value, Row) || Row.Num() < 9) return false;
            FABFPEvent Event;
            Event.Type = static_cast<EABFPEventType>(ReadInt(Row[0]));
            Event.Turn = ReadInt64(Row[1]);
            Event.SubjectId = ReadId(Row[2]);
            Event.TargetId = ReadId(Row[3]);
            Event.FromX = ReadInt(Row[4]);
            Event.FromY = ReadInt(Row[5]);
            Event.ToX = ReadInt(Row[6]);
            Event.ToY = ReadInt(Row[7]);
            Event.Value = ReadInt(Row[8]);
            Snapshot.Events.Add(Event);
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* Messages = nullptr;
    if (Root->TryGetArrayField(TEXT("messages"), Messages) && Messages)
    {
        Snapshot.Messages.Reserve(Messages->Num());
        for (const TSharedPtr<FJsonValue>& Value : *Messages)
        {
            TArray<TSharedPtr<FJsonValue>> Row;
            if (!ReadRow(Value, Row) || Row.Num() < 2) continue;
            if (!Row[1].IsValid() || Row[1]->Type != EJson::String) continue;
            FABFPMessage Msg;
            Msg.Type = ReadInt(Row[0]);
            Msg.Text = Row[1]->AsString();
            if (!Msg.Text.IsEmpty())
            {
                Snapshot.Messages.Add(MoveTemp(Msg));
            }
        }
    }

    OutSnapshot = MoveTemp(Snapshot);
    return true;
}
