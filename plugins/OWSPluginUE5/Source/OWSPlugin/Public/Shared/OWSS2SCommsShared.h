// Copyright Open World Server Org. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h" // For USTRUCT
#include "Misc/Guid.h" // For FGuid
#include "OWSS2SCommsShared.generated.h" // UHT

USTRUCT(BlueprintType)
struct OWSPLUGIN_API FS2SVector_UE
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") float X = 0.f;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") float Y = 0.f;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") float Z = 0.f;
};

USTRUCT(BlueprintType)
struct OWSPLUGIN_API FS2SRotator_UE
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") float Pitch = 0.f;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") float Yaw = 0.f;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") float Roll = 0.f;
};

USTRUCT(BlueprintType)
struct OWSPLUGIN_API FS2SActiveGameplayEffectInfo_UE
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") FString EffectName;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") float RemainingDuration = 0.f;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") int32 StackCount = 0;
};

USTRUCT(BlueprintType)
struct OWSPLUGIN_API FCustomCharacterDataPair_UE
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") FString CustomFieldName;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") FString FieldValue;
};

USTRUCT(BlueprintType)
struct OWSPLUGIN_API FPlayerStateDataForHandoff_UE
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") FGuid UserSessionGUID;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") FString CharacterName;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") FString ClassName;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") int32 CharacterLevel = 0;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") double X_DB = 0.0;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") double Y_DB = 0.0;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") double Z_DB = 0.0;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") double RX_DB = 0.0;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") double RY_DB = 0.0;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") double RZ_DB = 0.0;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") double MaxHealth_DB = 0.0;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") double MaxMana_DB = 0.0;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") double MaxEnergy_DB = 0.0;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") double MaxStamina_DB = 0.0;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") double CurrentHealth = 0.0;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") double CurrentMana = 0.0;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") double CurrentEnergy = 0.0;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") double CurrentStamina = 0.0;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") FS2SVector_UE LivePosition;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") FS2SRotator_UE LiveRotation;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") FS2SVector_UE LiveVelocity;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") TArray<uint8> PlayerStateSnapshot;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") TArray<uint8> CharacterSnapshot;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") TMap<FString, TArray<uint8>> ActorComponentSnapshots;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") TMap<FString, float> ActiveAbilityCooldowns;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") TArray<FS2SActiveGameplayEffectInfo_UE> ActiveGameplayEffects;
    UPROPERTY(BlueprintReadWrite, Category = "OWS | S2S") TArray<FCustomCharacterDataPair_UE> LiveCustomCharacterData;
};

USTRUCT(BlueprintType)
struct OWSPLUGIN_API FClientHandoffInfo_UE
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite, Category = "OWS") FString TargetServerIP;
    UPROPERTY(BlueprintReadWrite, Category = "OWS") int32 TargetServerPort = 0;
    UPROPERTY(BlueprintReadWrite, Category = "OWS") FString HandoffSessionToken;
    UPROPERTY(BlueprintReadWrite, Category = "OWS") FString ForCharacterName;
};
