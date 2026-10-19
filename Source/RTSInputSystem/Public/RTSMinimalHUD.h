// Copyright Winyunq, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "RTSHUD.h"
#include "RTSMinimalHUD.generated.h"

class URTSInputRouter;

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

/** Minimal controller that installs the input router (which in turn owns the selector) and plugin input actions. */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "RTS Minimal Player Controller"))
class RTSINPUTSYSTEM_API ARTSMinimalPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARTSMinimalPlayerController();

	/**
	 * 输入注册与路由组件。它负责创建并托管 URTSSelector（注册在同一个 PC 上，所以
	 * `PC->FindComponentByClass<URTSSelector>()` 仍然可用），并注册鼠标/键盘输入。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RTS Input")
	TObjectPtr<URTSInputRouter> RTSInputRouter;

	// ── 导航测试开关（需求 4：给用户提供开启/关闭入口） ──────────────────────────────────────
	//
	// 这两个接口转发给 RTSInputRouter，由它统一维护输入模式与导航测试组件的一致性。

	/** 开启导航测试：鼠标输入转入导航测试逻辑（录起终点、检查通路）。 */
	UFUNCTION(BlueprintCallable, Category = "RTS Input|Navigation Test")
	void SetNavigationTestActive(bool bActive);

	/** 开则关、关则开。 */
	UFUNCTION(BlueprintCallable, Category = "RTS Input|Navigation Test")
	void ToggleNavigationTest();

	UFUNCTION(BlueprintPure, Category = "RTS Input|Navigation Test")
	bool IsNavigationTestActive() const;
};

/** One-class entry point for the public minimal RTS experience. */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "RTS Minimal Game Mode"))
class RTSINPUTSYSTEM_API ARTSMinimalGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARTSMinimalGameMode();
};
