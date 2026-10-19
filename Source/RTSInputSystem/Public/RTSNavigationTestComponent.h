// Copyright 2024 Jesus Bracho All Rights Reserved.

#pragma once

#include <CoreMinimal.h>
#include "Components/ActorComponent.h"
#include "RTSNavigationTestComponent.generated.h"

class APlayerController;
class URTSInputRouter;
/**
 * 导航通路检查结果。
 *
 * 参数是「NavMesh 名称 + 是否连通」：一次检查会对场景里每一张 RecastNavMesh 分别求值，
 * 每张图各广播一次。名称取该 NavMesh Actor 的名字，用来在多张网的场景里区分结果。
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRTSNavigationTestResult, const FString&, NavMeshName, bool, bHasPath);

/**
 * 导航测试组件：在同一目录下与 URTSSelector 平级。
 *
 * 职责是把「给定两点，在这张场景的所有 NavMesh 上分别能不能走通」这件事做成一个可开关的交互：
 *
 *   1. 输入来自 URTSInputRouter——本类不自己注册输入，只暴露 OnLeftPressed / OnLeftTriggered /
 *      OnLeftReleased 三个落点让 router 在「导航测试」状态下调用（见 URTSInputRouter 的分发）。
 *   2. 起点 / 终点的录入遵循一套点击状态机（见 HandleLeftPressed 上方的长注释）。
 *   3. 每次录入完成后，对场景里每张 RecastNavMesh 各做一次 TestPathSync，把结果经
 *      FOnRTSNavigationTestResult 广播出去（细节面板绑这个委托把文本写进调试框）。
 *   4. 录入与结果用一条持久的 LineBatch 批次渲染（起/终点球体 + 连线 + 可达路线），
 *      关闭测试时整批清掉。
 *
 * 与 URTSSelector 一样：本组件由 URTSInputRouter 在运行时创建并注册到同一个 Owner（PlayerController）
 * 上，所以外部 `PC->FindComponentByClass<URTSNavigationTestComponent>()` 可以取到它。
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RTSINPUTSYSTEM_API URTSNavigationTestComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URTSNavigationTestComponent();

	/** 检查结果广播。每张 NavMesh 一条，参数为 (NavMesh 名称, 是否连通)。 */
	UPROPERTY(BlueprintAssignable, Category = "RTS Navigation Test")
	FOnRTSNavigationTestResult OnNavigationTestResult;

	// ── 开关 ────────────────────────────────────────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "RTS Navigation Test")
	void StartNavigationTest();

	UFUNCTION(BlueprintCallable, Category = "RTS Navigation Test")
	void StopNavigationTest();

	/** 开则关、关则开。细节面板的导航测试按钮走这条。 */
	UFUNCTION(BlueprintCallable, Category = "RTS Navigation Test")
	void ToggleNavigationTest();

	UFUNCTION(BlueprintPure, Category = "RTS Navigation Test")
	bool IsNavigationTestActive() const { return bIsActive; }

	/**
	 * 绑定宿主 router。右键关闭时组件通过它把「导航测试」状态一起退掉——输入模式归 router 所有，
	 * 组件只持有 bIsActive；两者必须同步，所以关闭走 router 而不是组件自己翻标志。
	 */
	void SetOwningRouter(URTSInputRouter* InRouter) { OwningRouter = InRouter; }

	// ── 输入落点（由 URTSInputRouter 在「导航测试」状态下调用） ───────────────────────────────

	void OnLeftPressed();
	void OnLeftTriggered();
	void OnLeftReleased();
	void OnRightPressed();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	APlayerController* GetPlayerController() const;

	/** 鼠标当前是否落在场景几何上；命中则给出世界坐标。 */
	bool QueryCursorWorldLocation(FVector& OutLocation) const;

	/** 鼠标当前屏幕坐标。 */
	bool QueryCursorScreenPosition(FVector2D& OutScreenPosition) const;

	/** 对场景里所有 RecastNavMesh 各做一次通路检查，逐张广播结果。 */
	void RunNavigationChecks();

	/** 清空起点/终点与当前录入手势（不清渲染批次）。 */
	void ResetRecordedPoints();

	/** 重建渲染批次：起/终点球体 + 连线 + （可达时的）路线。 */
	void RebuildDebugDraw();

	/** 清掉本组件自己的渲染批次。 */
	void ClearDebugDraw();

	uint32 GetDebugBatchId() const;

private:
	/** 记录一次检查的结果，供日志/渲染沿用。 */
	struct FNavMeshCheckResult
	{
		FString MeshName;
		bool bHasPath = false;
		TArray<FVector> PathPoints;
	};

	/** 是否处于导航测试状态。 */
	bool bIsActive = false;

	/** 左键是否处于按下（录入手势进行中）。 */
	bool bLeftPressed = false;

	/** 左键按下的屏幕坐标，用于 5 像素判定。 */
	FVector2D LeftPressScreenPosition = FVector2D::ZeroVector;

	/** 起点 / 终点是否已录入。 */
	bool bHasStart = false;
	bool bHasEnd = false;
	FVector StartLocation = FVector::ZeroVector;
	FVector EndLocation = FVector::ZeroVector;

	/** 本批次所有 NavMesh 的检查结果（用于渲染可达路线）。 */
	TArray<FNavMeshCheckResult> CheckResults;

	/** 宿主 router；右键关闭时通过它同步退出「导航测试」输入状态。 */
	TWeakObjectPtr<URTSInputRouter> OwningRouter;

	/** 录入手势中是否存在「已移动超过 5 像素」的记录（决定抬起时是否落实终点）。 */
	bool bMovedBeyondThreshold = false;

	/** 像素阈值：超过它才算「发生了移动」。 */
	static constexpr float ScreenMoveThresholdPixels = 5.0f;
};
