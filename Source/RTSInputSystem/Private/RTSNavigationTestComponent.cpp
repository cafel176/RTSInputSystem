// Copyright 2024 Jesus Bracho All Rights Reserved.

#include "RTSNavigationTestComponent.h"

#include "RTSInputRouter.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Components/LineBatchComponent.h"

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————//

URTSNavigationTestComponent::URTSNavigationTestComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void URTSNavigationTestComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 组件销毁时把自己那一批渲染清干净。
	ClearDebugDraw();

	Super::EndPlay(EndPlayReason);
}

APlayerController* URTSNavigationTestComponent::GetPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————//
// 开关
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————//

void URTSNavigationTestComponent::StartNavigationTest()
{
	bIsActive = true;
	ResetRecordedPoints();
	RebuildDebugDraw();
}

void URTSNavigationTestComponent::StopNavigationTest()
{
	bIsActive = false;
	ResetRecordedPoints();
	ClearDebugDraw();
}

void URTSNavigationTestComponent::ToggleNavigationTest()
{
	if (bIsActive)
	{
		StopNavigationTest();
	}
	else
	{
		StartNavigationTest();
	}
}

void URTSNavigationTestComponent::ResetRecordedPoints()
{
	bLeftPressed = false;
	bHasStart = false;
	bHasEnd = false;
	bMovedBeyondThreshold = false;
	StartLocation = FVector::ZeroVector;
	EndLocation = FVector::ZeroVector;
	CheckResults.Reset();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————//
// 录入手势状态机
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————//
//
// 规则（对应需求 6）：
//
//   按下 LeftPressed：
//     · 若「已记录起点、未记录终点」→ 本次按下进入**终点录入**（记录终点，随后可拖动调整，抬起才计算）。
//     · 否则（没有起点，或上一次测试的两点都在）→ 本次按下**重新记录起点**，并清掉旧终点。
//
//   LeftTriggered（拖动中）：
//     · 与按下点的屏幕距离 > 5 像素 → 记为「发生了移动」。
//     · 终点录入中 → 持续把终点更新到当前光标（拖动调整）。
//     · 起点录入中且已越过阈值 → 把当前光标记成终点（一次按下-拖动-抬起即可完成录入）。
//
//   LeftReleased：
//     · 终点录入中 → 用当前光标定稿终点并执行检查（无论是否拖动过）。
//     · 起点录入中且发生过移动 → 用当前光标定稿终点并执行检查。
//     · 起点录入中且未移动（纯点击）→ 只留起点，等待第二次点击录终点。
//
// 这套状态机让「点一下再点一下」与「按下拖动抬起」两种录入方式都成立，且能反复进行多次测试
// （每次测试完成后，下一次按下都从「重新记录起点」开始）。

void URTSNavigationTestComponent::OnLeftPressed()
{
	if (!bIsActive)
	{
		return;
	}

	FVector CursorWorld;
	if (!QueryCursorWorldLocation(CursorWorld))
	{
		return;
	}

	FVector2D CursorScreen;
	QueryCursorScreenPosition(CursorScreen);

	bLeftPressed = true;
	LeftPressScreenPosition = CursorScreen;
	bMovedBeyondThreshold = false;

	if (bHasStart && !bHasEnd)
	{
		// 第二次点击：录入终点（可拖动调整，抬起才计算）。
		EndLocation = CursorWorld;
		bHasEnd = true;
	}
	else
	{
		// 首次点击（或上一轮测试已结束）：重新开始，记录起点。
		StartLocation = CursorWorld;
		bHasStart = true;
		bHasEnd = false;
		EndLocation = FVector::ZeroVector;
		CheckResults.Reset();
	}

	RebuildDebugDraw();
}

void URTSNavigationTestComponent::OnLeftTriggered()
{
	if (!bIsActive || !bLeftPressed)
	{
		return;
	}

	FVector2D CursorScreen;
	if (!QueryCursorScreenPosition(CursorScreen))
	{
		return;
	}

	if (!bMovedBeyondThreshold
		&& FVector2D::Distance(CursorScreen, LeftPressScreenPosition) > ScreenMoveThresholdPixels)
	{
		bMovedBeyondThreshold = true;
	}

	FVector CursorWorld;
	if (!QueryCursorWorldLocation(CursorWorld))
	{
		return;
	}

	//if (bHasStart && !bHasEnd)
	//{
	//	// 终点录入中的拖动调整。
	//	EndLocation = CursorWorld;
	//	RebuildDebugDraw();
	//}
	//else 
	if (bMovedBeyondThreshold)
	{
		// 起点录入中的拖动：越过阈值后把当前光标记成终点。
		EndLocation = CursorWorld;
		bHasEnd = true;
		RebuildDebugDraw();
	}
}

void URTSNavigationTestComponent::OnLeftReleased()
{
	if (!bIsActive || !bLeftPressed)
	{
		return;
	}

	bLeftPressed = false;

	// 终点录入：抬起时定稿并计算（拖动过则以抬起点为准）。
	if (bHasEnd)
	{
		FVector CursorWorld;
		if (bMovedBeyondThreshold && QueryCursorWorldLocation(CursorWorld))
		{
			EndLocation = CursorWorld;
		}
		RebuildDebugDraw();
		RunNavigationChecks();
		return;
	}

	// 起点录入：只有移动超过阈值才算终点录入完成；纯点击则只留起点。
	if (bMovedBeyondThreshold)
	{
		FVector CursorWorld;
		if (QueryCursorWorldLocation(CursorWorld))
		{
			EndLocation = CursorWorld;
			bHasEnd = true;
			RebuildDebugDraw();
			RunNavigationChecks();
		}
	}
}

void URTSNavigationTestComponent::OnRightPressed()
{
	//// 右键：关闭导航测试（需求 5）。走 router 关闭，让 InputMode 与组件状态一起复位。
	//if (!bIsActive)
	//{
	//	return;
	//}

	//if (URTSInputRouter* Router = OwningRouter.Get())
	//{
	//	Router->SetNavigationTestActive(false);
	//}
	//else
	//{
	//	StopNavigationTest();
	//}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————//
// 光标查询
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————//

bool URTSNavigationTestComponent::QueryCursorScreenPosition(FVector2D& OutScreenPosition) const
{
	APlayerController* PC = GetPlayerController();
	if (!PC)
	{
		return false;
	}
	float X = 0.0f;
	float Y = 0.0f;
	if (!PC->GetMousePosition(X, Y))
	{
		return false;
	}
	OutScreenPosition = FVector2D(X, Y);
	return true;
}

bool URTSNavigationTestComponent::QueryCursorWorldLocation(FVector& OutLocation) const
{
	APlayerController* PC = GetPlayerController();
	if (!PC)
	{
		return false;
	}

	FHitResult Hit;
	PC->GetHitResultUnderCursor(ECC_Visibility, /*bTraceComplex=*/false, Hit);
	if (!Hit.bBlockingHit)
	{
		return false;
	}

	OutLocation = Hit.Location;
	return true;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————//
// 通路检查
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————//

void URTSNavigationTestComponent::RunNavigationChecks()
{
	CheckResults.Reset();

	if (!bHasStart || !bHasEnd)
	{
		return;
	}

	UWorld* World = GetWorld();
	UNavigationSystemV1* NavSys = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (!NavSys)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RTSNavigationTest] 找不到 NavigationSystem，无法执行通路检查。"));
		RebuildDebugDraw();
		return;
	}

	// 场景里每一张 RecastNavMesh 各检查一次（需求 7），逐张广播结果（需求 3）。
	// 显式带 NavData：多张网的场景里，不指定就会静默回落到"默认那张"（见 FlowField 的同一条注释）。
	int32 MeshCount = 0;
	for (TActorIterator<ARecastNavMesh> It(World); It; ++It)
	{
		ARecastNavMesh* Mesh = *It;
		if (!Mesh)
		{
			continue;
		}
		++MeshCount;

		FPathFindingQuery Query(nullptr, *Mesh, StartLocation, EndLocation);
		Query.SetAllowPartialPaths(false);
		FPathFindingResult Result = NavSys->FindPathSync(Query);


		bool bHasPath = Result.IsSuccessful() && Result.Path.IsValid();
		//如果不通，则交换起点和终点，再算一次，在上楼、远距离导航的情况下很容易出现A->B不通，但是B->A可通的情况
		if (!bHasPath)
		{
			FPathFindingQuery Query2(nullptr, *Mesh, EndLocation, StartLocation);
			Query2.SetAllowPartialPaths(false);
			Result = NavSys->FindPathSync(Query2);
			bHasPath = Result.IsSuccessful() && Result.Path.IsValid();
		}

		FNavMeshCheckResult CheckResult;
		CheckResult.MeshName = Mesh->GetName();
		CheckResult.bHasPath = bHasPath;
		if (bHasPath)
		{
			const TArray<FNavPathPoint>& PathPoints = Result.Path->GetPathPoints();
			CheckResult.PathPoints.Reserve(PathPoints.Num());
			for (const FNavPathPoint& Point : PathPoints)
			{
				CheckResult.PathPoints.Add(Point.Location);
			}
		}
		CheckResults.Add(MoveTemp(CheckResult));

		OnNavigationTestResult.Broadcast(Mesh->GetName(), bHasPath);
	}

	if (MeshCount == 0)
	{
		OnNavigationTestResult.Broadcast(TEXT("(无 RecastNavMesh)"), false);
	}

	RebuildDebugDraw();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————//
// 渲染
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————//

uint32 URTSNavigationTestComponent::GetDebugBatchId() const
{
	// 与其它系统（URTSSelector / MassBattleAgentCustomSubsystem）互不干扰的批次号。
	// 0 是引擎的 INVALID_ID，ClearBatch(0) 是空操作，所以末位必须置 1。
	return PointerHash(this, 0x52544E56u) | 1u;
}

namespace
{
	/** 往一组线里追加一个线框球（三个正交大圆），避免依赖 LineBatcher 的 DrawSphere 签名。 */
	void AppendWireframeSphere(
		TArray<FBatchedLine>& Out,
		const FVector& Center,
		float Radius,
		const FLinearColor& Color,
		float Thickness,
		uint32 BatchId,
		int32 Segments = 24)
	{
		const int32 SafeSegments = FMath::Max(8, Segments);
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			FVector Previous;
			for (int32 Segment = 0; Segment <= SafeSegments; ++Segment)
			{
				const float Angle = (2.0f * PI) * static_cast<float>(Segment) / static_cast<float>(SafeSegments);
				const float C = FMath::Cos(Angle) * Radius;
				const float S = FMath::Sin(Angle) * Radius;

				FVector Point;
				switch (Axis)
				{
				case 0: Point = Center + FVector(C, S, 0.0f); break; // XY
				case 1: Point = Center + FVector(C, 0.0f, S); break; // XZ
				default: Point = Center + FVector(0.0f, C, S); break; // YZ
				}

				if (Segment > 0)
				{
					Out.Emplace(Previous, Point, Color, -1.0f, Thickness, 1, BatchId);
				}
				Previous = Point;
			}
		}
	}
}

void URTSNavigationTestComponent::RebuildDebugDraw()
{
	UWorld* World = GetWorld();
	ULineBatchComponent* LineBatcher = World
		? World->GetLineBatcher(UWorld::ELineBatcherType::WorldPersistent)
		: nullptr;
	if (!LineBatcher)
	{
		return;
	}

	const uint32 BatchId = GetDebugBatchId();
	LineBatcher->ClearBatch(BatchId);

	if (!bIsActive)
	{
		return;
	}

	TArray<FBatchedLine> Lines;

	constexpr float PointRadius = 60.0f;
	constexpr float LineThickness = 0.0f;

	if (bHasStart)
	{
		AppendWireframeSphere(Lines, StartLocation, PointRadius, FLinearColor(0.2f, 1.0f, 0.3f, 1.0f), LineThickness, BatchId);
	}
	if (bHasEnd)
	{
		AppendWireframeSphere(Lines, EndLocation, PointRadius, FLinearColor(1.0f, 0.35f, 0.2f, 1.0f), LineThickness, BatchId);
	}
	if (bHasStart && bHasEnd)
	{
		// 起终点连线。
		Lines.Emplace(StartLocation, EndLocation, FLinearColor(1.0f, 1.0f, 0.3f, 1.0f), -1.0f, LineThickness, 1, BatchId);

		// 可达时的路线：取第一张有路的 NavMesh 的路径点连线。
		for (const FNavMeshCheckResult& CheckResult : CheckResults)
		{
			if (!CheckResult.bHasPath || CheckResult.PathPoints.Num() < 2)
			{
				continue;
			}
			for (int32 PointIndex = 1; PointIndex < CheckResult.PathPoints.Num(); ++PointIndex)
			{
				Lines.Emplace(
					CheckResult.PathPoints[PointIndex - 1],
					CheckResult.PathPoints[PointIndex],
					FLinearColor(0.3f, 0.8f, 1.0f, 1.0f),
					-1.0f,
					LineThickness,
					1,
					BatchId);
			}
			//break;
		}
	}

	if (!Lines.IsEmpty())
	{
		LineBatcher->DrawLines(Lines);
	}
}

void URTSNavigationTestComponent::ClearDebugDraw()
{
	UWorld* World = GetWorld();
	if (ULineBatchComponent* LineBatcher = World
		? World->GetLineBatcher(UWorld::ELineBatcherType::WorldPersistent)
		: nullptr)
	{
		LineBatcher->ClearBatch(GetDebugBatchId());
	}
}
