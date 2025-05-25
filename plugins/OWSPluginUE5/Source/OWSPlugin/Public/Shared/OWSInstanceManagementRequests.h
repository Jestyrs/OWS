// Copyright Open World Server Org. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h" // For USTRUCT
#include "Misc/Guid.h" // For FGuid
#include "OWSInstanceManagementRequests.generated.h" // UHT

USTRUCT(BlueprintType)
struct OWSPLUGIN_API FHandoffPreparationRequest_UE
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    FGuid PlayerUserSessionGUID;

    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    FString CharacterName;

    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    int32 SourceWorldServerID = 0;

    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    FString CurrentGridCellID;

    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    FString TargetGridCellID;
};

USTRUCT(BlueprintType)
struct OWSPLUGIN_API FHandoffPreparationResponse_UE
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    bool CanProceed = false;

    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    FString TargetWorldServerS2SEndpoint;

    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    FString ErrorMessage;
};
