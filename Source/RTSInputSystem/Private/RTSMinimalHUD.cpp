// Copyright Winyunq, 2026. All Rights Reserved.

#include "RTSMinimalHUD.h"

#include "RTSInputRouter.h"
#include "UI/RTSActiveGroupWidget.h"
#include "UI/RTSCommanderGridWidget.h"
#include "UI/RTSMinimapJumpWidget.h"
#include "UI/RTSSelectionQueryButton.h"
#include "UI/RTSUnitPanelWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"

namespace
{
	const FLinearColor PanelColor(0.008f, 0.024f, 0.038f, 0.94f);
	const FLinearColor AccentColor(0.1f, 0.72f, 0.95f, 1.0f);

	URTSSelectionQueryButton* AddSelectionButton(
		UWidgetTree* WidgetTree,
		UHorizontalBox* Parent,
		TSubclassOf<URTSSelectionQueryButton> ButtonClass,
		const FName Name,
		const FText& Label,
		const FName RequiredTag = NAME_None)
	{
		URTSSelectionQueryButton* Button = WidgetTree->ConstructWidget<URTSSelectionQueryButton>(
			ButtonClass, Name);
		Button->SetBackgroundColor(FLinearColor(0.025f, 0.09f, 0.13f, 1.0f));
		if (!RequiredTag.IsNone())
		{
			Button->SelectionQuery.RequiredSelectionTag =
				FGameplayTag::RequestGameplayTag(RequiredTag, false);
		}

		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), FName(*FString::Printf(TEXT("%sLabel"), *Name.ToString())));
		Text->SetText(Label);
		Text->SetJustification(ETextJustify::Center);
		Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Button->AddChild(Text);

		if (UHorizontalBoxSlot* Slot = Parent->AddChildToHorizontalBox(Button))
		{
			Slot->SetPadding(FMargin(4.0f));
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Fill);
		}
		return Button;
	}

	UScaleBox* AddScaledPanel(
		UWidgetTree* WidgetTree,
		UHorizontalBox* Parent,
		UWidget* Content,
		const FName Name,
		const float Width,
		const float Height)
	{
		USizeBox* Host = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), FName(*FString::Printf(TEXT("%sHost"), *Name.ToString())));
		Host->SetWidthOverride(Width);
		Host->SetHeightOverride(Height);

		UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>(
			UScaleBox::StaticClass(), Name);
		Scale->SetStretch(EStretch::ScaleToFit);
		Scale->SetStretchDirection(EStretchDirection::DownOnly);
		Scale->SetContent(Content);
		Host->AddChild(Scale);

		if (UHorizontalBoxSlot* Slot = Parent->AddChildToHorizontalBox(Host))
		{
			Slot->SetPadding(FMargin(5.0f, 0.0f));
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Bottom);
		}
		return Scale;
	}
}

TSharedRef<SWidget> URTSMinimalHUDWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), TEXT("RTSMinimalHUDRoot"));

		UBorder* QuickSelectionFrame = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("QuickSelectionFrame"));
		QuickSelectionFrame->SetBrushColor(PanelColor);
		QuickSelectionFrame->SetPadding(FMargin(6.0f));
		UHorizontalBox* QuickSelection = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("QuickSelection"));
		QuickSelectionFrame->SetContent(QuickSelection);

		AddSelectionButton(
			WidgetTree, QuickSelection, URTSSelectAllArmyButton::StaticClass(),
			TEXT("SelectArmy"), NSLOCTEXT("RTSInputSystem", "MinimalHUDArmy", "ARMY"));
		AddSelectionButton(
			WidgetTree, QuickSelection, URTSSelectAllIdleArmyButton::StaticClass(),
			TEXT("SelectIdle"), NSLOCTEXT("RTSInputSystem", "MinimalHUDIdle", "IDLE"));
		AddSelectionButton(
			WidgetTree, QuickSelection, URTSSelectionQueryButton::StaticClass(),
			TEXT("SelectInfantry"), NSLOCTEXT("RTSInputSystem", "MinimalHUDInfantry", "INFANTRY"),
			TEXT("RTS.Selection.Army.Ground.Infantry"));
		AddSelectionButton(
			WidgetTree, QuickSelection, URTSSelectionQueryButton::StaticClass(),
			TEXT("SelectArmor"), NSLOCTEXT("RTSInputSystem", "MinimalHUDArmor", "ARMOR"),
			TEXT("RTS.Selection.Army.Ground.Armor"));
		AddSelectionButton(
			WidgetTree, QuickSelection, URTSSelectionQueryButton::StaticClass(),
			TEXT("SelectStructures"), NSLOCTEXT("RTSInputSystem", "MinimalHUDStructures", "STRUCTURES"),
			TEXT("RTS.Selection.Structure"));

		if (UCanvasPanelSlot* QuickSlot = Root->AddChildToCanvas(QuickSelectionFrame))
		{
			QuickSlot->SetAnchors(FAnchors(0.5f, 0.0f));
			QuickSlot->SetAlignment(FVector2D(0.5f, 0.0f));
			QuickSlot->SetPosition(FVector2D(0.0f, 20.0f));
			QuickSlot->SetSize(FVector2D(760.0f, 54.0f));
		}

		UBorder* BottomFrame = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("BottomFrame"));
		BottomFrame->SetBrushColor(PanelColor);
		BottomFrame->SetPadding(FMargin(8.0f));
		UHorizontalBox* BottomPanels = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("BottomPanels"));
		BottomFrame->SetContent(BottomPanels);

		USizeBox* MinimapHost = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), TEXT("MinimapHost"));
		MinimapHost->SetWidthOverride(250.0f);
		MinimapHost->SetHeightOverride(250.0f);
		UOverlay* MinimapOverlay = WidgetTree->ConstructWidget<UOverlay>(
			UOverlay::StaticClass(), TEXT("MinimapOverlay"));
		MinimapHost->AddChild(MinimapOverlay);

		UBorder* MinimapBackground = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("MinimapBackground"));
		MinimapBackground->SetBrushColor(FLinearColor(0.025f, 0.07f, 0.08f, 1.0f));
		UTextBlock* MinimapLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("MinimapLabel"));
		MinimapLabel->SetText(NSLOCTEXT(
			"RTSInputSystem", "MinimalHUDMinimap",
			"MINIMAP INPUT\nLeft: camera jump\nRight: move command\nRenderer is optional"));
		MinimapLabel->SetJustification(ETextJustify::Center);
		MinimapLabel->SetColorAndOpacity(FSlateColor(AccentColor));
		MinimapBackground->SetContent(MinimapLabel);
		MinimapOverlay->AddChildToOverlay(MinimapBackground);

		URTSMinimapJumpWidget* MinimapInput = WidgetTree->ConstructWidget<URTSMinimapJumpWidget>(
			URTSMinimapJumpWidget::StaticClass(), TEXT("MinimapInput"));
		if (UOverlaySlot* MinimapInputSlot = MinimapOverlay->AddChildToOverlay(MinimapInput))
		{
			MinimapInputSlot->SetHorizontalAlignment(HAlign_Fill);
			MinimapInputSlot->SetVerticalAlignment(VAlign_Fill);
		}
		if (UHorizontalBoxSlot* MinimapSlot = BottomPanels->AddChildToHorizontalBox(MinimapHost))
		{
			MinimapSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
			MinimapSlot->SetVerticalAlignment(VAlign_Bottom);
		}

		URTSUnitPanelWidget* UnitPanel = WidgetTree->ConstructWidget<URTSUnitPanelWidget>(
			URTSUnitPanelWidget::StaticClass(), TEXT("UnitPanel"));
		AddScaledPanel(WidgetTree, BottomPanels, UnitPanel, TEXT("UnitPanelScale"), 650.0f, 300.0f);

		URTSActiveGroupWidget* ActiveGroup = WidgetTree->ConstructWidget<URTSActiveGroupWidget>(
			URTSActiveGroupWidget::StaticClass(), TEXT("ActiveGroup"));
		AddScaledPanel(WidgetTree, BottomPanels, ActiveGroup, TEXT("ActiveGroupScale"), 150.0f, 150.0f);

		URTSCommanderGridWidget* CommandGrid = WidgetTree->ConstructWidget<URTSCommanderGridWidget>(
			URTSCommanderGridWidget::StaticClass(), TEXT("CommandGrid"));
		AddScaledPanel(WidgetTree, BottomPanels, CommandGrid, TEXT("CommandGridScale"), 520.0f, 300.0f);

		if (UCanvasPanelSlot* BottomSlot = Root->AddChildToCanvas(BottomFrame))
		{
			BottomSlot->SetAnchors(FAnchors(0.5f, 1.0f));
			BottomSlot->SetAlignment(FVector2D(0.5f, 1.0f));
			BottomSlot->SetPosition(FVector2D(0.0f, -18.0f));
			BottomSlot->SetSize(FVector2D(1600.0f, 318.0f));
		}

		WidgetTree->RootWidget = Root;
	}

	return Super::RebuildWidget();
}

ARTSMinimalHUD::ARTSMinimalHUD()
{
	MinimalHUDWidgetClass = URTSMinimalHUDWidget::StaticClass();
}

void ARTSMinimalHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PlayerController = GetOwningPlayerController();
	if (!PlayerController || MinimalHUDWidget)
	{
		return;
	}

	if (!MinimalHUDWidgetClass)
	{
		return;
	}

	MinimalHUDWidget = CreateWidget<UUserWidget>(
		PlayerController, MinimalHUDWidgetClass);
	if (MinimalHUDWidget)
	{
		MinimalHUDWidget->AddToViewport(0);
	}
}

ARTSMinimalPlayerController::ARTSMinimalPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	RTSInputRouter = CreateDefaultSubobject<URTSInputRouter>(TEXT("RTSInputRouter"));
}

void ARTSMinimalPlayerController::SetNavigationTestActive(bool bActive)
{
	if (RTSInputRouter)
	{
		RTSInputRouter->SetNavigationTestActive(bActive);
	}
}

void ARTSMinimalPlayerController::ToggleNavigationTest()
{
	if (RTSInputRouter)
	{
		RTSInputRouter->ToggleNavigationTest();
	}
}

bool ARTSMinimalPlayerController::IsNavigationTestActive() const
{
	return RTSInputRouter && RTSInputRouter->IsNavigationTestActive();
}

ARTSMinimalGameMode::ARTSMinimalGameMode()
{
	HUDClass = ARTSMinimalHUD::StaticClass();
	PlayerControllerClass = ARTSMinimalPlayerController::StaticClass();
}
