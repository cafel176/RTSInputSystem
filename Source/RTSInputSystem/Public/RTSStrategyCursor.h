// Copyright 2024 Jesus Bracho All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

/**
 * 软件光标样式。对应 URTSInputRouter 注册到视口的三种 EMouseCursor 映射：
 *   - Normal：默认状态（EMouseCursor::Default）
 *   - Selectable：指向可选中目标（EMouseCursor::GrabHand）
 *   - Targeting：处于目标选择/施法状态（EMouseCursor::Crosshairs）
 *   - Aim：处于实体跟踪，瞄准状态（EMouseCursor::Crosshairs）
 */
enum class ERTSStrategyCursorStyle : uint8
{
	Normal,
	Selectable,
	Targeting,
	Aim
};

/**
 * 纯代码绘制的 RTS 鼠标指针。不依赖任何贴图资源，全部通过 Slate 线条实时绘制，
 * 由 URTSInputRouter 通过 UGameViewportClient::SetSoftwareCursorWidget 挂载。
 * 尺寸固定 44x44，指针尖端落在 widget 中心以对齐实际光标热区。
 */
class RTSINPUTSYSTEM_API SRTSStrategyCursor final : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SRTSStrategyCursor)
		: _Style(ERTSStrategyCursorStyle::Normal)
	{
		_Visibility = EVisibility::HitTestInvisible;
	}
		SLATE_ARGUMENT(ERTSStrategyCursorStyle, Style)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float) const override;

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	ERTSStrategyCursorStyle m_Style = ERTSStrategyCursorStyle::Normal;
};
