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
    ReceiveBuffer.Reset();
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
    ReceiveBuffer.Reset();
}

bool FABFPNetworkClient::IsConnected() const
{
    return Socket != nullptr;
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
            return false;
        }
        Offset += Sent;
    }
    return true;
}

bool FABFPNetworkClient::PollSnapshot(FABFPSnapshot& OutSnapshot)
{
    if (!Socket) return false;

    uint32 Pending = 0;
    while (Socket->HasPendingData(Pending) && Pending > 0)
    {
        const int32 ToRead = static_cast<int32>(FMath::Min<uint32>(Pending, 8192));
        TArray<uint8> Bytes;
        Bytes.SetNumUninitialized(ToRead + 1);
        int32 Read = 0;
        if (!Socket->Recv(Bytes.GetData(), ToRead, Read) || Read <= 0) break;
        Bytes[Read] = 0;
        ReceiveBuffer.Append(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Bytes.GetData())));
        if (ReceiveBuffer.Len() > 4 * 1024 * 1024)
        {
            ReceiveBuffer.Reset();
            return false;
        }
    }

    bool bUpdated = false;
    while (true)
    {
        const int32 Newline = ReceiveBuffer.Find(TEXT("\n"));
        if (Newline == INDEX_NONE) break;
        const FString Line = ReceiveBuffer.Left(Newline);
        ReceiveBuffer = ReceiveBuffer.Mid(Newline + 1);
        FABFPSnapshot Parsed;
        if (ParseStateLine(Line, Parsed))
        {
            OutSnapshot = MoveTemp(Parsed);
            bUpdated = true;
        }
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
    Snapshot.DungeonDepth = static_cast<int32>(Depth);

    const TArray<TSharedPtr<FJsonValue>>* Player = nullptr;
    if (!Root->TryGetArrayField(TEXT("player"), Player) || !Player || Player->Num() < 2)
        return false;
    Snapshot.PlayerX = ReadInt((*Player)[0]);
    Snapshot.PlayerY = ReadInt((*Player)[1]);

    const TArray<TSharedPtr<FJsonValue>>* Cells = nullptr;
    if (!Root->TryGetArrayField(TEXT("cells"), Cells) || !Cells) return false;
    for (const TSharedPtr<FJsonValue>& Value : *Cells)
    {
        TArray<TSharedPtr<FJsonValue>> Row;
        if (!ReadRow(Value, Row) || Row.Num() < 7) return false;
        FABFPCell Cell;
        Cell.X = ReadInt(Row[0]);
        Cell.Y = ReadInt(Row[1]);
        Cell.Terrain = static_cast<EABFPTerrain>(ReadInt(Row[2]));
        Cell.bVisible = ReadInt(Row[3]) != 0;
        Cell.bLit = ReadInt(Row[4]) != 0;
        Cell.bOccupiedByMonster = ReadInt(Row[5]) != 0;
        Cell.bOccupiedByObject = ReadInt(Row[6]) != 0;
        Snapshot.Cells.Add(Cell);
    }

    const TArray<TSharedPtr<FJsonValue>>* Entities = nullptr;
    if (!Root->TryGetArrayField(TEXT("entities"), Entities) || !Entities) return false;
    for (const TSharedPtr<FJsonValue>& Value : *Entities)
    {
        TArray<TSharedPtr<FJsonValue>> Row;
        if (!ReadRow(Value, Row) || Row.Num() < 11) return false;
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
        Snapshot.Entities.Add(Entity);
    }

    const TArray<TSharedPtr<FJsonValue>>* Events = nullptr;
    if (Root->TryGetArrayField(TEXT("events"), Events) && Events)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Events)
        {
            TArray<TSharedPtr<FJsonValue>> Row;
            if (!ReadRow(Value, Row) || Row.Num() < 9) return false;
            FABFPEvent Event;
            Event.Type = ReadInt(Row[0]);
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

    OutSnapshot = MoveTemp(Snapshot);
    return true;
}
