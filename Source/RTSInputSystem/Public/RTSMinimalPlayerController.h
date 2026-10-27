// Copyright Winyunq, 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MassAPIStructs.h"
#include "InputAction.h"
#include "RTSMinimalPlayerController.generated.h"

class URTSInputRouter;

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

	// ── 实体跟随接口（默认空实现，供子类按需转发给具体 Pawn） ──────────────────────────────

	/** 相机当前是否锁定并跟随某个实体。默认返回 false。 */
	UFUNCTION(BlueprintCallable, Category = "RTS Input|Entity Follow")
	virtual bool IsCameraLockedToEntity();

	/** 让相机跟随指定实体。默认不做事，子类可覆盖为转发给具体 Pawn。 */
	UFUNCTION(BlueprintCallable, Category = "RTS Input|Entity Follow")
	virtual void LockCameraToEntity(const FEntityHandle& entity);

	/** 解除相机对实体的跟随。默认不做事。 */
	UFUNCTION(BlueprintCallable, Category = "RTS Input|Entity Follow")
	virtual void UnLockCamera();
	//实体跟随时的鼠标消息
	virtual void HandleMouseLeftPressed(const FInputActionValue& Value);
	virtual void HandleMouseLeftTriggered(const FInputActionValue& Value);
	virtual void HandleMouseLeftReleased(const FInputActionValue& Value);
};
