// Copyright 2024 Jesus Bracho All Rights Reserved.

#pragma once

#include <CoreMinimal.h>
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Components/ActorComponent.h"
#include "RTSInputRouter.generated.h"

class APlayerController;
class FRTSSelectorInputProcessor;
class IInputProcessor;
class UInputComponent;
class URTSNavigationTestComponent;
class URTSSelector;

/**
 * 输入分发状态。当前鼠标左右键的响应按这个状态决定转发给谁。
 *
 *   - EntityManipulation（实体操纵）：转发给 URTSSelector，走原有的框选 / 右键命令逻辑。
 *   - NavigationTest（导航测试）：预留给导航测试交互，响应代码暂留空。
 */
UENUM(BlueprintType)
enum class ERTSInputMode : uint8
{
	EntityManipulation UMETA(DisplayName = "实体操纵"),
	NavigationTest     UMETA(DisplayName = "导航测试")
};

/**
 * RTS 输入的注册与路由层。
 *
 * 这个组件做三件事：
 *   1. 把「鼠标/键盘输入」注册到引擎（Slate 预处理器、Enhanced Input 绑定、软件光标、控制组热键）；
 *   2. 鼠标左右键的响应先进入本类，再按 ERTSInputMode 分发给对应状态的处理器；
 *   3. 实体操纵状态下把输入转发给 URTSSelector 的公开接口，导航测试状态暂时留空。
 *
 * 拆出来的理由是这样：输入「怎么注册、怎么被引擎派发、往哪个状态分发」与选择「被调用后做什么」是
 * 两件不同的事，前者依赖 Slate/EnhancedInput/软件光标的细节，后者依赖 Mass、命令层与选择子系统。
 * 分开之后，换输入后端或换输入策略时不必触碰 3600 行的选择逻辑。
 *
 * 生命周期上的一个关键点：URTSSelector 由本组件**在运行时**创建并注册到同一个 Owner（PlayerController）
 * 上。这样 URTSSelector 仍然是一个「挂在 PC 上的普通组件」，项目里所有
 * `PC->FindComponentByClass<URTSSelector>()` 的取用点（RTSCamera、RTSHUD、命令面板、小地图等）
 * 都不必改动——它们照旧能查到它，只是现在这个组件由本类而非 PC 的构造函数负责创建。
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RTSINPUTSYSTEM_API URTSInputRouter : public UActorComponent
{
	GENERATED_BODY()

public:
	URTSInputRouter();

	/**
	 * 由 PlayerController 的 SetupInputComponent 调用：绑定 Enhanced Input 动作与控制组热键。
	 *
	 * 保留 `_Outer` 这层包装是为了沿用 PC 侧既有的调用形状（PC 不直接覆盖 selector 的 protected
	 * SetupPlayerInputComponent）。本函数会先 EnsureSelector()，因此即便它先于 BeginPlay 执行，
	 * selector 也已经存在。
	 */
	void SetupPlayerInputComponent_Outer(UInputComponent* PlayerInputComponent);

	/** 本组件管理的选择器组件；尚未创建时为 null。 */
	URTSSelector* GetSelector() const;

	/** 本组件管理的导航测试组件；尚未创建时为 null。 */
	URTSNavigationTestComponent* GetNavigationTest() const;

	/** 本组件的宿主 PlayerController（即 Owner）；未就绪时为 null。 */
	APlayerController* GetPlayerController() const;

	// ── 输入状态 ────────────────────────────────────────────────────────────────────────────

	/** 当前输入状态。鼠标左右键的响应按它分发。 */
	UFUNCTION(BlueprintCallable, Category = "RTS Input")
	void SetInputMode(ERTSInputMode NewMode);

	UFUNCTION(BlueprintPure, Category = "RTS Input")
	ERTSInputMode GetInputMode() const { return InputMode; }

	// ── 导航测试开关（权威入口） ─────────────────────────────────────────────────────────────
	//
	// InputMode 与本 router 托管的 URTSNavigationTestComponent 必须同步，所以开关统一从这里走：
	// 置状态的同时 Start/Stop 组件。PlayerController 与细节面板都调这里。

	UFUNCTION(BlueprintCallable, Category = "RTS Input")
	void SetNavigationTestActive(bool bActive);

	UFUNCTION(BlueprintCallable, Category = "RTS Input")
	void ToggleNavigationTest();

	UFUNCTION(BlueprintPure, Category = "RTS Input")
	bool IsNavigationTestActive() const;

	// ── 输入动作与映射上下文（原本挂在 URTSSelector 上，随输入职责一起搬到这里） ──────────────

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RTSCamera - Inputs")
	UInputMappingContext* InputMappingContext;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RTSCamera - Inputs")
	UInputAction* IA_MouseLeftBtn;

	// Action for right clicking to issue a smart command
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RTSCamera - Inputs")
	UInputAction* IA_MouseRightBtn;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/**
	 * 幂等地创建并注册 URTSSelector 到本组件的 Owner 上。
	 *
	 * 同时被 SetupPlayerInputComponent_Outer 与 BeginPlay 调用：前者可能在 BeginPlay 之前执行
	 * （PC 调 SetupInputComponent 的时机与组件 BeginPlay 不保证先后），必须先建出来才能绑输入。
	 */
	void EnsureSelector();

	/** 幂等地创建并注册 URTSNavigationTestComponent 到本组件的 Owner 上（同 EnsureSelector 的规矩）。 */
	void EnsureNavigationTest();

	/**
	 * 幂等地解析并缓存宿主 PlayerController（= Owner）。
	 *
	 * 不能让 BeginPlay 独占这一步：SetupPlayerInputComponent_Outer 可能先于 BeginPlay 执行，
	 * 那两个入口都要能拿到 PC，所以这里按需解析。
	 */
	APlayerController* ResolvePlayerController();

	void BindInputActions();
	void BindInputMappingContext();
	void InstallStrategyMouseCursors();
	void RestoreStrategyMouseCursors();

	void RegisterSelectionInputProcessor();
	void UnregisterSelectionInputProcessor();

	void RegisterControlGroupHotkeys(UInputComponent* InputComponent);
	void UnregisterControlGroupHotkeys();

	// ── 鼠标输入的响应入口（Enhanced Input 直接绑定到这几个函数） ─────────────────────────────
	//
	// 每个入口内部按当前 InputMode 直接分发给对应状态的处理器（不再单设一层 Dispatch 函数）。
	// 左右键对称：都含按下 / 拖动 / 释放三种事件。

	void HandleMouseLeftPressed(const FInputActionValue& Value);
	void HandleMouseLeftTriggered(const FInputActionValue& Value);
	void HandleMouseLeftReleased(const FInputActionValue& Value);

	void HandleMouseRightPressed(const FInputActionValue& Value);
	void HandleMouseRightTriggered(const FInputActionValue& Value);
	void HandleMouseRightReleased(const FInputActionValue& Value);

	/** 实体操纵状态下的响应：原样转发给 URTSSelector。 */
	void HandleEntityManipulationLeftPressed(const FInputActionValue& Value);
	void HandleEntityManipulationLeftTriggered(const FInputActionValue& Value);
	void HandleEntityManipulationLeftReleased(const FInputActionValue& Value);
	void HandleEntityManipulationRightPressed(const FInputActionValue& Value);
	void HandleEntityManipulationRightTriggered(const FInputActionValue& Value);
	void HandleEntityManipulationRightReleased(const FInputActionValue& Value);

	/** 导航测试状态下的响应：暂留空（占位）。 */
	void HandleNavigationTestLeftPressed(const FInputActionValue& Value);
	void HandleNavigationTestLeftTriggered(const FInputActionValue& Value);
	void HandleNavigationTestLeftReleased(const FInputActionValue& Value);
	void HandleNavigationTestRightPressed(const FInputActionValue& Value);
	void HandleNavigationTestRightTriggered(const FInputActionValue& Value);
	void HandleNavigationTestRightReleased(const FInputActionValue& Value);

private:
	/** 本组件创建并托管的 selector。Owner 是 PlayerController，所以外部 FindComponentByClass 仍可发现它。 */
	UPROPERTY(Transient)
	TObjectPtr<URTSSelector> Selector;

	/** 本组件创建并托管的导航测试组件（同样注册在 Owner 上）。 */
	UPROPERTY(Transient)
	TObjectPtr<URTSNavigationTestComponent> NavigationTest;

	UPROPERTY(Transient)
	TObjectPtr<class UInputComponent> ControlGroupInputComponent;

	TWeakObjectPtr<APlayerController> PlayerController;
	TWeakObjectPtr<APlayerController> ControlGroupInputOwner;
	TSharedPtr<IInputProcessor> SelectionInputProcessor;

	/** 当前输入状态。默认进入实体操纵（与拆分前行为一致）。 */
	ERTSInputMode InputMode = ERTSInputMode::EntityManipulation;
};
