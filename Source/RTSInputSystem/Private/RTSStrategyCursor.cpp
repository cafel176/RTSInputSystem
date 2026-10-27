// Copyright 2024 Jesus Bracho All Rights Reserved.

#include "RTSStrategyCursor.h"

#include "Rendering/DrawElements.h"

void SRTSStrategyCursor::Construct(const FArguments& InArgs)
{
	m_Style = InArgs._Style;
}

FVector2D SRTSStrategyCursor::ComputeDesiredSize(float) const
{
	// SViewport centers software cursor widgets on the actual cursor
	// position. The pointer tip below is therefore drawn at this
	// widget's exact center so selection rays and clicks land there.
	return FVector2D(44.0f, 44.0f);
}

int32 SRTSStrategyCursor::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	(void)Args;
	(void)MyCullingRect;
	(void)InWidgetStyle;
	(void)bParentEnabled;

	const FPaintGeometry PaintGeometry = AllottedGeometry.ToPaintGeometry();
	const FVector2f Hotspot(22.0f, 22.0f);
	FLinearColor Accent = FLinearColor(0.24f, 1.0f, 0.44f, 1.0f);
	const FLinearColor EdgeColor(0.015f, 0.025f, 0.045f, 0.98f);

	// 跟踪/瞄准模式：绘制瞄准光标，之后直接返回，不再执行常规箭头光标逻辑。
	// 构成：中心一个点，四周四个尖三角形朝向中心，沿 45° 斜线分布（而非水平/垂直），整体红色。
	if (m_Style == ERTSStrategyCursorStyle::Aim)
	{
		const FLinearColor AimColor(1.0f, 0.10f, 0.06f, 1.0f);
		const float DotRadius = 2.0f;      // 中心点半径
		const float ApexRadius = 8.0f;     // 三角形尖端到中心的距离
		const float BaseRadius = 22.0f;    // 三角形底边到中心的距离
		const float HalfWidth = 4.5f;      // 底边半宽，越小三角形越尖

		// 四条对角线方向，三角形底边沿斜线的垂线张开，尖端指向中心。
		const FVector2f Diagonals[4] =
		{
			FVector2f(0.7071f, 0.7071f),
			FVector2f(0.7071f, -0.7071f),
			FVector2f(-0.7071f, 0.7071f),
			FVector2f(-0.7071f, -0.7071f)
		};

		TArray<TArray<FVector2f>> AimShapes;

		// 中心点：一个小菱形，与线条绘制风格保持一致。
		AimShapes.Add(
			{
				Hotspot + FVector2f(0.0f, -DotRadius),
				Hotspot + FVector2f(DotRadius, 0.0f),
				Hotspot + FVector2f(0.0f, DotRadius),
				Hotspot + FVector2f(-DotRadius, 0.0f),
				Hotspot + FVector2f(0.0f, -DotRadius)
			});

		for (const FVector2f& Dir : Diagonals)
		{
			const FVector2f Perp(-Dir.Y, Dir.X);
			AimShapes.Add(
				{
					Hotspot + Dir * ApexRadius,
					Hotspot + Dir * BaseRadius + Perp * HalfWidth,
					Hotspot + Dir * BaseRadius - Perp * HalfWidth,
					Hotspot + Dir * ApexRadius
				});
		}

		for (const TArray<FVector2f>& Shape : AimShapes)
		{
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId,
				PaintGeometry,
				Shape,
				ESlateDrawEffect::None,
				EdgeColor,
				true,
				5.0f);
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId + 1,
				PaintGeometry,
				Shape,
				ESlateDrawEffect::None,
				AimColor,
				true,
				2.0f);
		}

		return LayerId + 2;
	}

	TArray<TArray<FVector2f>> lines =
	{
		{
		Hotspot,
		FVector2f(23.2f, 38.0f),
		FVector2f(27.6f, 34.0f),
		FVector2f(32.3f, 42.0f),
		FVector2f(36.2f, 39.8f),
		FVector2f(31.5f, 31.8f),
		FVector2f(40.0f, 30.4f),
		Hotspot
		}
	};

	switch (m_Style)
	{
	case ERTSStrategyCursorStyle::Normal:
		Accent = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
		break;
	case ERTSStrategyCursorStyle::Selectable:
		Accent = FLinearColor(0.24f, 1.0f, 0.44f, 1.0f);
		lines.Append(
			{
				{ Hotspot + FVector2f(-5.0f, -10.0f), Hotspot + FVector2f(-10.0f, -10.0f), Hotspot + FVector2f(-10.0f, -5.0f) },
				{ Hotspot + FVector2f(5.0f, -10.0f), Hotspot + FVector2f(10.0f, -10.0f), Hotspot + FVector2f(10.0f, -5.0f) },
				{ Hotspot + FVector2f(-10.0f, 5.0f), Hotspot + FVector2f(-10.0f, 10.0f), Hotspot + FVector2f(-5.0f, 10.0f) }
			}
		);
		break;
	case ERTSStrategyCursorStyle::Targeting:
		Accent = FLinearColor(1.0f, 0.34f, 0.10f, 1.0f);

		lines.Append(
			{
				{ Hotspot + FVector2f(-5.0f, -10.0f), Hotspot + FVector2f(-10.0f, -10.0f), Hotspot + FVector2f(-10.0f, -5.0f) },
				{ Hotspot + FVector2f(5.0f, -10.0f), Hotspot + FVector2f(10.0f, -10.0f), Hotspot + FVector2f(10.0f, -5.0f) },
				{ Hotspot + FVector2f(-10.0f, 5.0f), Hotspot + FVector2f(-10.0f, 10.0f), Hotspot + FVector2f(-5.0f, 10.0f) }
			}
		);
		break;
	default:
		break;
	}


	for(auto line : lines)
	{
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			PaintGeometry,
			line,
			ESlateDrawEffect::None,
			EdgeColor,
			true,
			5.0f);
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId + 1,
			PaintGeometry,
			line,
			ESlateDrawEffect::None,
			Accent,
			true,
			2.0f);
	}


	return LayerId + 2;
}
