// Copyright Open World Server, PLLC.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h" // Required for FJsonObjectConverter
#include "OWSPlayerStateData.generated.h"

// Mirrors OWSShared.Messages.S2SVector
USTRUCT(BlueprintType)
struct FS2SVector
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    float X;
    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    float Y;
    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    float Z;

    FS2SVector() : X(0.f), Y(0.f), Z(0.f) {}
    FS2SVector(float InX, float InY, float InZ) : X(InX), Y(InY), Z(InZ) {}
    FS2SVector(const FVector& InVector) : X(InVector.X), Y(InVector.Y), Z(InVector.Z) {}

    FVector ToFVector() const { return FVector(X, Y, Z); }
};

// Mirrors OWSShared.Messages.InventoryItemData
USTRUCT(BlueprintType)
struct FInventoryItemData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    FString ItemName;
    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    int32 Quantity;
    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    FString CustomData; // Serialized JSON or other custom format

    FInventoryItemData() : ItemName(TEXT("")), Quantity(0), CustomData(TEXT("")) {}
};

// Mirrors OWSShared.Messages.PlayerStateDataForHandoff
USTRUCT(BlueprintType)
struct FPlayerStateDataForHandoff
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    FString UserSessionGUID; // Using FString for GUID to simplify JSON
    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    FString CharacterName;
    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    FString SourceServerS2SEndpoint;
    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    FString TargetGridCellID; // The specific cell ID player is moving to on target server

    // Player Transform
    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    FS2SVector PlayerPosition;
    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    FS2SVector PlayerRotation; // Using FS2SVector for FRotator for simplicity in JSON
    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    FS2SVector PlayerVelocity;

    // Snapshots
    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    TArray<uint8> PlayerStateSnapshot; // For UE's FMemoryWriter/Reader snapshot
    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    TArray<uint8> PawnStateSnapshot;   // For UE's FMemoryWriter/Reader snapshot for the pawn

    // Dynamic Character Data (example, expand as needed)
    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    float CurrentHealth;
    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    float CurrentMana;
    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    float CurrentEnergy;
    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    float CurrentStamina;

    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    TMap<FString, FString> CustomCharacterData; // For any other game-specific data
    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    TArray<FInventoryItemData> Inventory;
    //UPROPERTY(BlueprintReadWrite, Category = "OWS")
    //TArray<FActiveAbilityData> ActiveAbilities; // Define FActiveAbilityData if needed
    //UPROPERTY(BlueprintReadWrite, Category = "OWS")
    //TArray<FAppliedEffectData> AppliedEffects;  // Define FAppliedEffectData if needed


    FPlayerStateDataForHandoff() :
        UserSessionGUID(TEXT("")),
        CharacterName(TEXT("")),
        SourceServerS2SEndpoint(TEXT("")),
        TargetGridCellID(TEXT("")),
        CurrentHealth(0.f),
        CurrentMana(0.f),
        CurrentEnergy(0.f),
        CurrentStamina(0.f)
    {}

    // Method to serialize to JSON string
    bool ToJsonString(FString& OutJsonString) const
    {
        TSharedPtr<FJsonObject> JsonObject = FJsonObjectConverter::UStructToJsonObject(*this);
        if (!JsonObject.IsValid())
        {
            return false;
        }
        return FJsonSerializer::Serialize(JsonObject.ToSharedRef(), TJsonWriterFactory<>::Create(&OutJsonString, 0));
    }

    // Method to deserialize from JSON string
    bool FromJsonString(const FString& InJsonString)
    {
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InJsonString);
        if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
        {
            return false;
        }
        return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), FPlayerStateDataForHandoff::StaticStruct(), this, 0, 0);
    }
};
