// Copyright Winyunq, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "RTSHUD.h"
#include "RTSMinimalHUD.generated.h"

/**
 * Dependency-free reference HUD assembled entirely from RTSInputSystem native
 * widgets. It deliberately contains no game-specific missions, resources,
 * factions, units, technology trees, or FogOfWar renderer.
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "RTS Minimal HUD Widget"))
class RTSINPUTSYSTEM_API URTSMinimalHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};

/** AHUD entry point that displays URTSMinimalHUDWidget and retains marquee selection. */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "RTS Minimal HUD"))
class RTSINPUTSYSTEM_API ARTSMinimalHUD : public ARTSHUD
{
	GENERATED_BODY()

public:
	ARTSMinimalHUD();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "RTS Minimal HUD")
	TSubclassOf<UUserWidget> MinimalHUDWidgetClass = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> MinimalHUDWidget = nullptr;
};

/** One-class entry point for the public minimal RTS experience. */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "RTS Minimal Game Mode"))
class RTSINPUTSYSTEM_API ARTSMinimalGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARTSMinimalGameMode();
};
