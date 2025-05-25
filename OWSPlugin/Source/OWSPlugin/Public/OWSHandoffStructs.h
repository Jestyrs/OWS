// Copyright Open World Server, PLLC.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h" // Required for FJsonObjectConverter
#include "OWSHandoffStructs.generated.h"

// Mirrors OWSInstanceManagement.Requests.Instance.HandoffPreparationRequest
USTRUCT(BlueprintType)
struct FRequestHandoffPreparation
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    FString PlayerUserSessionGUID; // Using FString for GUID
    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    FString CharacterName;
    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    int32 SourceWorldServerID;
    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    FString CurrentGridCellID; // The cell player is currently in
    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    FString TargetGridCellID;  // The cell player intends to move to

    FRequestHandoffPreparation() :
        PlayerUserSessionGUID(TEXT("")),
        CharacterName(TEXT("")),
        SourceWorldServerID(0),
        CurrentGridCellID(TEXT("")),
        TargetGridCellID(TEXT(""))
    {}

    bool ToJsonString(FString& OutJsonString) const
    {
        TSharedPtr<FJsonObject> JsonObject = FJsonObjectConverter::UStructToJsonObject(*this);
        if (!JsonObject.IsValid()) return false;
        return FJsonSerializer::Serialize(JsonObject.ToSharedRef(), TJsonWriterFactory<>::Create(&OutJsonString, 0));
    }

    // Note: FromJsonString is not typically needed for request structs on the client/sending side
};

// Mirrors OWSInstanceManagement.Requests.Instance.HandoffPreparationResponse
USTRUCT(BlueprintType)
struct FHandoffPreparationResponse
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    bool CanProceed;
    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    FString TargetWorldServerS2SEndpoint;
    UPROPERTY(BlueprintReadWrite, Category = "OWS")
    FString ErrorMessage;

    FHandoffPreparationResponse() :
        CanProceed(false),
        TargetWorldServerS2SEndpoint(TEXT("")),
        ErrorMessage(TEXT(""))
    {}

    bool FromJsonString(const FString& InJsonString)
    {
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InJsonString);
        if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid()) return false;
        
        // Manually deserialize to handle potential missing fields gracefully
        CanProceed = JsonObject->GetBoolField(TEXT("canProceed")); // Adjusted to match C# typical camelCase JSON
        JsonObject->TryGetStringField(TEXT("targetWorldServerS2SEndpoint"), TargetWorldServerS2SEndpoint); // camelCase
        JsonObject->TryGetStringField(TEXT("errorMessage"), ErrorMessage); // camelCase
        
        // Fallback for PascalCase if the above fails (e.g. if JSON naming policy changes)
        if (TargetWorldServerS2SEndpoint.IsEmpty() && JsonObject->HasField(TEXT("TargetWorldServerS2SEndpoint")))
        {
            CanProceed = JsonObject->GetBoolField(TEXT("CanProceed"));
             JsonObject->TryGetStringField(TEXT("TargetWorldServerS2SEndpoint"), TargetWorldServerS2SEndpoint);
             JsonObject->TryGetStringField(TEXT("ErrorMessage"), ErrorMessage);
        }
        return true; // FJsonObjectConverter::JsonObjectToUStruct is stricter, manual allows partial success
    }
};
