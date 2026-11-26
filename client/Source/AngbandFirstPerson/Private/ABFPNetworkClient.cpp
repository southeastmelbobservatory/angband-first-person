#include "ABFPNetworkClient.h"

#include "Dom/JsonObject.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "SocketSubsystem.h"
#include "SocketTypes.h"
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

FABFPNetworkClient::~FABFPNetworkClient()
{
    Disconnect();
}

bool FABFPNetworkClient::BeginConnect(const FString& Host, int32 Port)
{
    Disconnect();
    ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!Subsystem || Port <= 0 || Port > 65535) return false;

    Socket = Subsystem->CreateSocket(NAME_Stream, TEXT("ABFP TCP Client"), false);
    if (!Socket) return false;

    /* Set non-blocking BEFORE Connect() so Winsock never freezes the GameThread for
     * ~2.04 seconds of synchronous TCP SYN retransmissions when no host is listening. */
    if (!Socket->SetNonBlocking(true))
    {
        Disconnect();
        return false;
    }
    Socket->SetNoDelay(true);

    bool bValidAddress = false;
    TSharedRef<FInternetAddr> Address = Subsystem->CreateInternetAddr();
    Address->SetIp(*Host, bValidAddress);
    Address->SetPort(Port);
    if (!bValidAddress)
    {
        Disconnect();
        return false;
    }

    const bool bConnectStarted = Socket->Connect(*Address);
    if (!bConnectStarted)
    {
        const ESocketErrors LastError = Subsystem->GetLastErrorCode();
        if (LastError != SE_NO_ERROR &&
            LastError != SE_EWOULDBLOCK &&
            LastError != SE_EINPROGRESS)
        {
            Disconnect();
            return false;
        }
    }

    RawReceiveBuffer.Reset();
    ConnectStartTimeSeconds = FPlatformTime::Seconds();
    LastConnectPollTimeSeconds = 0.0;

    /* Check once immediately in case loopback completed synchronously. */
    const ESocketConnectionState SockState = Socket->GetConnectionState();
    if (SockState == ESocketConnectionState::SCS_Connected)
    {
        ConnectionState = EABFPConnectionState::Connected;
    }
    else if (SockState == ESocketConnectionState::SCS_ConnectionError)
    {
        Disconnect();
        return false;
    }
    else
    {
        ConnectionState = EABFPConnectionState::Connecting;
    }
    return true;
}

EABFPConnectionState FABFPNetworkClient::TickConnect()
{
    if (ConnectionState != EABFPConnectionState::Connecting || !Socket)
    {
        return ConnectionState;
    }

    const double Now = FPlatformTime::Seconds();
    if ((Now - ConnectStartTimeSeconds) >= 3.0)
    {
        Disconnect();
        return ConnectionState;
    }

    /* Poll immediately on the first tick after BeginConnect(), then every 50ms. */
    if (LastConnectPollTimeSeconds > 0.0 && (Now - LastConnectPollTimeSeconds) < 0.05)
    {
        return ConnectionState;
    }
    LastConnectPollTimeSeconds = Now;

    const ESocketConnectionState SockState = Socket->GetConnectionState();
    if (SockState == ESocketConnectionState::SCS_Connected)
    {
        ConnectionState = EABFPConnectionState::Connected;
    }
    else if (SockState == ESocketConnectionState::SCS_ConnectionError)
    {
        Disconnect();
    }
    return ConnectionState;
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
    ConnectionState = EABFPConnectionState::Disconnected;
    ConnectStartTimeSeconds = 0.0;
    LastConnectPollTimeSeconds = 0.0;
    RawReceiveBuffer.Reset();
}

bool FABFPNetworkClient::SendCommand(const FString& Command)
{
    if (!IsConnected() || Command.IsEmpty()) return false;
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
    if (!IsConnected()) return false;

    /*
     * HasPendingData() reports payload bytes, but an orderly TCP close can
     * make the socket readable with zero payload. Probe that case as well so
     * the pawn's reconnect loop can recover after a host restart.
     */
    int32 RecvChunks = 0;
    while (RecvChunks < 32)
    {
        uint32 Pending = 0;
        const bool bHasPending = Socket->HasPendingData(Pending) && Pending > 0;
        if (!bHasPending &&
            !Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::Zero()))
        {
            break;
        }

        const int32 ToRead = bHasPending ?
            static_cast<int32>(FMath::Min<uint32>(Pending, 16384)) : 16384;
        const int32 OldNum = RawReceiveBuffer.Num();
        RawReceiveBuffer.SetNumUninitialized(OldNum + ToRead);
        int32 Read = 0;
        const bool bRecvOk = Socket->Recv(RawReceiveBuffer.GetData() + OldNum, ToRead, Read);
        if (!bRecvOk)
        {
            RawReceiveBuffer.SetNum(OldNum, EAllowShrinking::No);
            ISocketSubsystem* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
            const ESocketErrors LastError = Subsystem ? Subsystem->GetLastErrorCode() : SE_NO_ERROR;
            if (LastError == SE_EWOULDBLOCK || LastError == SE_EINTR)
            {
                break;
            }
            Disconnect();
            return false;
        }
        if (Read <= 0)
        {
            RawReceiveBuffer.SetNum(OldNum, EAllowShrinking::No);
            Disconnect();
            return false;
        }
        RawReceiveBuffer.SetNum(OldNum + Read, EAllowShrinking::No);
        ++RecvChunks;
        if (RawReceiveBuffer.Num() > 4 * 1024 * 1024)
        {
            Disconnect();
            return false;
        }
    }

    /* If multiple complete snapshots arrived during a hitch, deserialize only the newest one
       (preserving any older lines' discard via ConsumeOffset) so the GameThread never burns
       cycles parsing stale intermediate 45KB JSON frames. */
    int32 LastNewlineIndex = INDEX_NONE;
    int32 PrevNewlineIndex = INDEX_NONE;
    for (int32 Idx = 0; Idx < RawReceiveBuffer.Num(); ++Idx)
    {
        if (RawReceiveBuffer[Idx] == static_cast<uint8>('\n'))
        {
            PrevNewlineIndex = LastNewlineIndex;
            LastNewlineIndex = Idx;
        }
    }

    bool bUpdated = false;
    if (LastNewlineIndex != INDEX_NONE)
    {
        const int32 LineStart = (PrevNewlineIndex == INDEX_NONE) ? 0 : (PrevNewlineIndex + 1);
        const int32 LineLength = LastNewlineIndex - LineStart;
        if (LineLength > 0)
        {
            FUTF8ToTCHAR Converter(
                reinterpret_cast<const ANSICHAR*>(RawReceiveBuffer.GetData() + LineStart),
                LineLength);
            const FString Line(Converter.Length(), Converter.Get());
            FABFPSnapshot Parsed;
            if (ParseStateLine(Line, Parsed))
            {
                OutSnapshot = MoveTemp(Parsed);
                bUpdated = true;
            }
        }
        RawReceiveBuffer.RemoveAt(0, LastNewlineIndex + 1, EAllowShrinking::No);
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
        if (PlayerStats->Num() >= 12)
        {
            Snapshot.PlayerExp = ReadInt((*PlayerStats)[7]);
            Snapshot.PlayerMaxExp = ReadInt((*PlayerStats)[8]);
            Snapshot.PlayerArmorClass = ReadInt((*PlayerStats)[9]);
            Snapshot.PlayerSpeed = ReadInt((*PlayerStats)[10]);
            Snapshot.PlayerFoodPct = ReadInt((*PlayerStats)[11]);
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* PlayerMeta = nullptr;
    if (Root->TryGetArrayField(TEXT("player_meta"), PlayerMeta) && PlayerMeta && PlayerMeta->Num() >= 3)
    {
        Snapshot.PlayerNewSpells = ReadInt((*PlayerMeta)[0]);
        Snapshot.bCanChooseSpells = ReadInt((*PlayerMeta)[1]) != 0;
        Snapshot.bHasLauncher = ReadInt((*PlayerMeta)[2]) != 0;
        if (PlayerMeta->Num() >= 4 && (*PlayerMeta)[3].IsValid() && (*PlayerMeta)[3]->Type == EJson::String)
            Snapshot.PlayerName = (*PlayerMeta)[3]->AsString();
        if (PlayerMeta->Num() >= 5 && (*PlayerMeta)[4].IsValid() && (*PlayerMeta)[4]->Type == EJson::String)
            Snapshot.PlayerRaceName = (*PlayerMeta)[4]->AsString();
        if (PlayerMeta->Num() >= 6 && (*PlayerMeta)[5].IsValid() && (*PlayerMeta)[5]->Type == EJson::String)
            Snapshot.PlayerClassName = (*PlayerMeta)[5]->AsString();
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

    const TArray<TSharedPtr<FJsonValue>>* Inventory = nullptr;
    if (Root->TryGetArrayField(TEXT("inventory"), Inventory) && Inventory)
    {
        Snapshot.Inventory.Reserve(Inventory->Num());
        for (const TSharedPtr<FJsonValue>& Value : *Inventory)
        {
            TArray<TSharedPtr<FJsonValue>> Row;
            if (!ReadRow(Value, Row) || Row.Num() < 10) continue;
            FABFPInventoryItem Item;
            Item.Index = ReadInt(Row[0]);
            if (Row[1].IsValid() && Row[1]->Type == EJson::String)
                Item.Label = Row[1]->AsString();
            Item.bEquipped = ReadInt(Row[2]) != 0;
            Item.Flags = ReadInt(Row[3]);
            Item.Quantity = ReadInt(Row[4]);
            Item.CategoryId = ReadInt(Row[5]);
            Item.VariantId = ReadInt(Row[6]);
            Item.SellPrice = ReadInt(Row[7]);
            if (Row[8].IsValid() && Row[8]->Type == EJson::String)
                Item.SlotName = Row[8]->AsString();
            if (Row[9].IsValid() && Row[9]->Type == EJson::String)
                Item.Name = Row[9]->AsString();
            Snapshot.Inventory.Add(MoveTemp(Item));
        }
    }

    const TSharedPtr<FJsonObject>* ShopObj = nullptr;
    if (Root->TryGetObjectField(TEXT("shop"), ShopObj) && ShopObj && ShopObj->IsValid())
    {
        Snapshot.Shop.bActive = true;
        (*ShopObj)->TryGetBoolField(TEXT("is_home"), Snapshot.Shop.bIsHome);
        (*ShopObj)->TryGetStringField(TEXT("name"), Snapshot.Shop.Name);
        (*ShopObj)->TryGetStringField(TEXT("owner"), Snapshot.Shop.Owner);
        const TArray<TSharedPtr<FJsonValue>>* ShopItems = nullptr;
        if ((*ShopObj)->TryGetArrayField(TEXT("items"), ShopItems) && ShopItems)
        {
            Snapshot.Shop.Items.Reserve(ShopItems->Num());
            for (const TSharedPtr<FJsonValue>& Value : *ShopItems)
            {
                TArray<TSharedPtr<FJsonValue>> Row;
                if (!ReadRow(Value, Row) || Row.Num() < 6) continue;
                FABFPShopItem ShopItem;
                ShopItem.Index = ReadInt(Row[0]);
                ShopItem.Quantity = ReadInt(Row[1]);
                ShopItem.Price = ReadInt(Row[2]);
                ShopItem.CategoryId = ReadInt(Row[3]);
                ShopItem.VariantId = ReadInt(Row[4]);
                if (Row[5].IsValid() && Row[5]->Type == EJson::String)
                    ShopItem.Name = Row[5]->AsString();
                Snapshot.Shop.Items.Add(MoveTemp(ShopItem));
            }
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* SpellsArr = nullptr;
    if (Root->TryGetArrayField(TEXT("spells"), SpellsArr) && SpellsArr)
    {
        Snapshot.Spells.Reserve(SpellsArr->Num());
        for (const TSharedPtr<FJsonValue>& Value : *SpellsArr)
        {
            TArray<TSharedPtr<FJsonValue>> Row;
            if (!ReadRow(Value, Row) || Row.Num() < 8) continue;
            FABFPSpellEntry Spell;
            Spell.Index = ReadInt(Row[0]);
            Spell.BookIndex = ReadInt(Row[1]);
            Spell.Level = ReadInt(Row[2]);
            Spell.Mana = ReadInt(Row[3]);
            Spell.FailPct = ReadInt(Row[4]);
            Spell.Flags = ReadInt(Row[5]);
            if (Row[6].IsValid() && Row[6]->Type == EJson::String)
                Spell.Name = Row[6]->AsString();
            if (Row[7].IsValid() && Row[7]->Type == EJson::String)
                Spell.Info = Row[7]->AsString();
            Snapshot.Spells.Add(MoveTemp(Spell));
        }
    }

    const TSharedPtr<FJsonObject>* BirthObj = nullptr;
    if (Root->TryGetObjectField(TEXT("birth"), BirthObj) && BirthObj && BirthObj->IsValid())
    {
        Snapshot.Birth.bActive = true;
        (*BirthObj)->TryGetBoolField(TEXT("rolled"), Snapshot.Birth.bRolled);
        double NumVal = 0.0;
        if ((*BirthObj)->TryGetNumberField(TEXT("race"), NumVal)) Snapshot.Birth.RaceIndex = static_cast<int32>(NumVal);
        if ((*BirthObj)->TryGetNumberField(TEXT("class"), NumVal)) Snapshot.Birth.ClassIndex = static_cast<int32>(NumVal);
        if ((*BirthObj)->TryGetNumberField(TEXT("points_left"), NumVal)) Snapshot.Birth.PointsLeft = static_cast<int32>(NumVal);
        if ((*BirthObj)->TryGetNumberField(TEXT("hp"), NumVal)) Snapshot.Birth.HitPoints = static_cast<int32>(NumVal);
        if ((*BirthObj)->TryGetNumberField(TEXT("gold"), NumVal)) Snapshot.Birth.Gold = static_cast<int32>(NumVal);
        (*BirthObj)->TryGetStringField(TEXT("name"), Snapshot.Birth.Name);

        auto ParseIntArr = [&](const TCHAR* Field, TArray<int32>& Dest)
        {
            const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
            if ((*BirthObj)->TryGetArrayField(Field, Arr) && Arr)
            {
                Dest.Reserve(Arr->Num());
                for (const TSharedPtr<FJsonValue>& V : *Arr) Dest.Add(ReadInt(V));
            }
        };
        ParseIntArr(TEXT("stats_base"), Snapshot.Birth.StatsBase);
        ParseIntArr(TEXT("stats_total"), Snapshot.Birth.StatsTotal);
        ParseIntArr(TEXT("points_spent"), Snapshot.Birth.PointsSpent);
        ParseIntArr(TEXT("points_inc"), Snapshot.Birth.PointsInc);

        auto ParseOptArr = [&](const TCHAR* Field, TArray<FABFPBirthOption>& Dest)
        {
            const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
            if ((*BirthObj)->TryGetArrayField(Field, Arr) && Arr)
            {
                Dest.Reserve(Arr->Num());
                for (const TSharedPtr<FJsonValue>& V : *Arr)
                {
                    TArray<TSharedPtr<FJsonValue>> Row;
                    if (!ReadRow(V, Row) || Row.Num() < 6) continue;
                    FABFPBirthOption Opt;
                    Opt.Index = ReadInt(Row[0]);
                    if (Row[1].IsValid() && Row[1]->Type == EJson::String) Opt.Name = Row[1]->AsString();
                    TArray<TSharedPtr<FJsonValue>> Adjs;
                    if (ReadRow(Row[2], Adjs))
                    {
                        for (const TSharedPtr<FJsonValue>& A : Adjs) Opt.StatAdj.Add(ReadInt(A));
                    }
                    Opt.HitDie = ReadInt(Row[3]);
                    Opt.ExpFactor = ReadInt(Row[4]);
                    Opt.bHasMagic = ReadInt(Row[5]) != 0;
                    Dest.Add(MoveTemp(Opt));
                }
            }
        };
        ParseOptArr(TEXT("races"), Snapshot.Birth.Races);
        ParseOptArr(TEXT("classes"), Snapshot.Birth.Classes);
    }

    OutSnapshot = MoveTemp(Snapshot);
    return true;
}
