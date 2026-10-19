// Copyright 2024 Jesus Bracho All Rights Reserved.

#include "RTSInputRouter.h"

#include "RTSSelector.h"
#include "RTSNavigationTestComponent.h"
#include "RTSInputPanelSettings.h"
#include "RTSSelectionSubsystem.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Components/InputComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Widgets/SViewport.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————//
// Slate 输入预处理器：全局鼠标移动与 Tab
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————//

/**
 * 挂在 Slate 上的输入预处理器。作用两个：
 *   - HandleMouseMoveEvent：把全局鼠标移动转给 selector 的 HandlePointerMoved()。
 *     恒返回 false（不消费），这是刻意的——相机与 UMG 控件仍要收到同一条移动事件。
 *   - HandleKeyDownEvent：接管 Tab（选择分组循环）。返回 true 表示消费掉，
 *     避免 Slate 把 Tab 变成焦点导航。
 *
 * 持 TWeakObjectPtr 而不是裸指针：预处理器挂在 SlateApplication 上，其生命周期可能长于组件，
 * 弱引用 + 每次 Get() 判空是防止组件先销毁后访问的护栏。
 */
class FRTSSelectorInputProcessor final : public IInputProcessor
{
public:
	explicit FRTSSelectorInputProcessor(URTSInputRouter* InRouter)
		: Router(InRouter)
	{
	}

	virtual void Tick(
		const float DeltaTime,
		FSlateApplication& SlateApp,
		TSharedRef<ICursor> Cursor) override
	{
		// IInputProcessor requires Tick. Intentionally no business work here:
		// selector feedback changes only in discrete input-event callbacks.
		(void)DeltaTime;
		(void)SlateApp;
		(void)Cursor;
	}

	virtual bool HandleMouseMoveEvent(
		FSlateApplication& SlateApp,
		const FPointerEvent& MouseEvent) override
	{
		(void)SlateApp;
		(void)MouseEvent;
		if (URTSInputRouter* RouterPtr = Router.Get())
		{
			if (URTSSelector* SelectorComponent = RouterPtr->GetSelector())
			{
				SelectorComponent->HandlePointerMoved();
			}
		}
		return false;
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& KeyEvent) override
	{
		(void)SlateApp;
		if (KeyEvent.GetKey() != EKeys::Tab || KeyEvent.IsShiftDown()
			|| KeyEvent.IsControlDown() || KeyEvent.IsAltDown() || KeyEvent.IsCommandDown())
		{
			return false;
		}
		URTSInputRouter* RouterPtr = Router.Get();
		APlayerController* PC = RouterPtr ? RouterPtr->GetPlayerController() : nullptr;
		ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
		UGameViewportClient* ViewportClient = LocalPlayer ? LocalPlayer->ViewportClient : nullptr;
		if (!ViewportClient || ViewportClient->IgnoreInput() || PC->IsPaused())
		{
			return false;
		}
		const TSharedPtr<FSlateUser> User = LocalPlayer->GetSlateUser();
		const TSharedPtr<SViewport> Viewport = ViewportClient->GetGameViewportWidget();
		if (!User || User->GetUserIndex() != KeyEvent.GetUserIndex()
			|| !Viewport || !User->IsWidgetInFocusPath(Viewport))
		{
			// A PIE selector must not handle keys aimed at editor windows or another local player.
			return false;
		}
		const TSharedPtr<SWidget> Focused = User->GetFocusedWidget();
		if (Focused && Focused->GetTypeAsString().Contains(TEXT("EditableText")))
		{
			// Chat and text fields keep their own Tab handling.
			return false;
		}
		if (URTSSelectionSubsystem* Selection = LocalPlayer->GetSubsystem<URTSSelectionSubsystem>())
		{
			// Selection owns Tab before Slate turns it into focus navigation. A held key stays consumed.
			if (!KeyEvent.IsRepeat()) Selection->CycleGroup();
			return true;
		}
		return false;
	}

	virtual const TCHAR* GetDebugName() const override
	{
		return TEXT("RTSSelectorInput");
	}

private:
	TWeakObjectPtr<URTSInputRouter> Router;
};

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————//
// 软件光标
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————//

namespace
{
	enum class ERTSStrategyCursorStyle : uint8
	{
		Normal,
		Selectable,
		Targeting
	};

	class SRTSStrategyCursor final : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SRTSStrategyCursor)
			: _Style(ERTSStrategyCursorStyle::Normal)
		{
			_Visibility = EVisibility::HitTestInvisible;
		}
			SLATE_ARGUMENT(ERTSStrategyCursorStyle, Style)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Style = InArgs._Style;
		}

		virtual FVector2D ComputeDesiredSize(float) const override
		{
			// SViewport centers software cursor widgets on the actual cursor
			// position. The pointer tip below is therefore drawn at this
			// widget's exact center so selection rays and clicks land there.
			return FVector2D(44.0f, 44.0f);
		}

		virtual int32 OnPaint(
			const FPaintArgs& Args,
			const FGeometry& AllottedGeometry,
			const FSlateRect& MyCullingRect,
			FSlateWindowElementList& OutDrawElements,
			int32 LayerId,
			const FWidgetStyle& InWidgetStyle,
			bool bParentEnabled) const override
		{
			(void)Args;
			(void)MyCullingRect;
			(void)InWidgetStyle;
			(void)bParentEnabled;

			const FPaintGeometry PaintGeometry = AllottedGeometry.ToPaintGeometry();
			const FVector2f Hotspot(22.0f, 22.0f);
			const FLinearColor Accent =
				Style == ERTSStrategyCursorStyle::Selectable
					? FLinearColor(0.24f, 1.0f, 0.44f, 1.0f)
					: (Style == ERTSStrategyCursorStyle::Targeting
						? FLinearColor(1.0f, 0.34f, 0.10f, 1.0f)
						: FLinearColor(0.78f, 0.90f, 0.98f, 1.0f));
			const FLinearColor EdgeColor(0.015f, 0.025f, 0.045f, 0.98f);

			// Compact asymmetric RTS pointer. The first/last point is the exact
			// input hotspot; the body grows down-right like a conventional
			// cursor without visually displacing the click point.
			const TArray<FVector2f> Arrow =
			{
				Hotspot,
				FVector2f(23.2f, 38.0f),
				FVector2f(27.6f, 34.0f),
				FVector2f(32.3f, 42.0f),
				FVector2f(36.2f, 39.8f),
				FVector2f(31.5f, 31.8f),
				FVector2f(40.0f, 30.4f),
				Hotspot
			};
			TArray<FVector2f> ShadowArrow;
			ShadowArrow.Reserve(Arrow.Num());
			for (const FVector2f& Point : Arrow)
			{
				ShadowArrow.Add(Point + FVector2f(1.25f, 1.5f));
			}

			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId,
				PaintGeometry,
				MoveTemp(ShadowArrow),
				ESlateDrawEffect::None,
				FLinearColor(0.0f, 0.0f, 0.0f, 0.72f),
				true,
				7.0f);
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId + 1,
				PaintGeometry,
				Arrow,
				ESlateDrawEffect::None,
				EdgeColor,
				true,
				5.5f);
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId + 2,
				PaintGeometry,
				Arrow,
				ESlateDrawEffect::None,
				Accent,
				true,
				2.7f);

			if (Style != ERTSStrategyCursorStyle::Normal)
			{
				// Three restrained acquisition corners make the selectable
				// state readable while leaving the down-right pointer body
				// and its exact hotspot unobstructed.
				const TArray<TArray<FVector2f>> AcquisitionCorners =
				{
					{ Hotspot + FVector2f(-5.0f, -10.0f), Hotspot + FVector2f(-10.0f, -10.0f), Hotspot + FVector2f(-10.0f, -5.0f) },
					{ Hotspot + FVector2f(5.0f, -10.0f), Hotspot + FVector2f(10.0f, -10.0f), Hotspot + FVector2f(10.0f, -5.0f) },
					{ Hotspot + FVector2f(-10.0f, 5.0f), Hotspot + FVector2f(-10.0f, 10.0f), Hotspot + FVector2f(-5.0f, 10.0f) }
				};
				for (const TArray<FVector2f>& Corner : AcquisitionCorners)
				{
					FSlateDrawElement::MakeLines(
						OutDrawElements,
						LayerId + 3,
						PaintGeometry,
						Corner,
						ESlateDrawEffect::None,
						EdgeColor,
						true,
						4.5f);
					FSlateDrawElement::MakeLines(
						OutDrawElements,
						LayerId + 4,
						PaintGeometry,
						Corner,
						ESlateDrawEffect::None,
						Accent,
						true,
						1.8f);
				}
			}
			return LayerId + 4;
		}

	private:
		ERTSStrategyCursorStyle Style = ERTSStrategyCursorStyle::Normal;
	};
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————//
// 组件生命周期
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————//

URTSInputRouter::URTSInputRouter()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// Add defaults for input actions
	static ConstructorHelpers::FObjectFinder<UInputAction>
		MouseLeftActionFinder(TEXT("/RTSInputSystem/Inputs/IA_MouseLeftBtn"));
	static ConstructorHelpers::FObjectFinder<UInputMappingContext>
		InputMappingContextFinder(TEXT("/RTSInputSystem/Inputs/RTSInputSystemInputs"));
	this->IA_MouseLeftBtn = MouseLeftActionFinder.Object;
	this->InputMappingContext = InputMappingContextFinder.Object;

	static ConstructorHelpers::FObjectFinder<UInputAction>
		MouseRightActionFinder(TEXT("/RTSInputSystem/Inputs/IA_MouseRightBtn"));
	if (MouseRightActionFinder.Succeeded())
	{
		this->IA_MouseRightBtn = MouseRightActionFinder.Object;
	}
}

void URTSInputRouter::BeginPlay()
{
	Super::BeginPlay();

	this->ResolvePlayerController();

	const auto NetMode = this->GetNetMode();
	if (NetMode != NM_DedicatedServer)
	{
		this->EnsureSelector();
		this->EnsureNavigationTest();
		this->RegisterSelectionInputProcessor();
		this->InstallStrategyMouseCursors();
		this->BindInputMappingContext();
	}
}

void URTSInputRouter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterSelectionInputProcessor();
	RestoreStrategyMouseCursors();

	Super::EndPlay(EndPlayReason);
}

URTSSelector* URTSInputRouter::GetSelector() const
{
	return Selector.Get();
}

URTSNavigationTestComponent* URTSInputRouter::GetNavigationTest() const
{
	return NavigationTest.Get();
}

APlayerController* URTSInputRouter::GetPlayerController() const
{
	return PlayerController.Get();
}

APlayerController* URTSInputRouter::ResolvePlayerController()
{
	if (!PlayerController.IsValid())
	{
		PlayerController = Cast<APlayerController>(GetOwner());
	}
	return PlayerController.Get();
}

void URTSInputRouter::EnsureSelector()
{
	if (IsValid(Selector))
	{
		return;
	}

	// Owner 是 PlayerController：注册之后 URTSSelector 就是 PC 上的一个普通组件，
	// 项目里所有 FindComponentByClass<URTSSelector>() 的取用点都照旧能查到它。
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	URTSSelector* NewSelector = NewObject<URTSSelector>(Owner, TEXT("RTSSelector"));
	if (!NewSelector)
	{
		return;
	}

	NewSelector->RegisterComponent();
	Selector = NewSelector;
}

void URTSInputRouter::EnsureNavigationTest()
{
	if (IsValid(NavigationTest))
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	URTSNavigationTestComponent* NewNavigationTest =
		NewObject<URTSNavigationTestComponent>(Owner, TEXT("RTSNavigationTest"));
	if (!NewNavigationTest)
	{
		return;
	}

	NewNavigationTest->RegisterComponent();
	NavigationTest = NewNavigationTest;
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————//
// Enhanced Input 绑定
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————//

void URTSInputRouter::SetupPlayerInputComponent_Outer(UInputComponent* PlayerInputComponent)
{
	// PC 的 SetupInputComponent 可能早于本组件的 BeginPlay 执行，所以这两个入口都要自己能解析出
	// PC 与 selector，避免依赖 BeginPlay 的先后顺序。
	ResolvePlayerController();
	EnsureSelector();

	if (!PlayerController.IsValid() || !Selector)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RTSInputRouter] Selection input was not bound: player controller or selector is unavailable."));
		return;
	}

	if (const auto InputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// 两个鼠标动作绑定到本类自己的处理函数，由它按 InputMode 分发；不再直接绑到 selector。
		if (this->IA_MouseLeftBtn)
		{
			InputComponent->BindAction(this->IA_MouseLeftBtn, ETriggerEvent::Started, this, &URTSInputRouter::HandleMouseLeftPressed);
			InputComponent->BindAction(this->IA_MouseLeftBtn, ETriggerEvent::Triggered, this, &URTSInputRouter::HandleMouseLeftTriggered);
			InputComponent->BindAction(this->IA_MouseLeftBtn, ETriggerEvent::Completed, this, &URTSInputRouter::HandleMouseLeftReleased);
			InputComponent->BindAction(this->IA_MouseLeftBtn, ETriggerEvent::Canceled, this, &URTSInputRouter::HandleMouseLeftReleased);
		}
		if (this->IA_MouseRightBtn)
		{
			InputComponent->BindAction(this->IA_MouseRightBtn, ETriggerEvent::Started, this, &URTSInputRouter::HandleMouseRightPressed);
			InputComponent->BindAction(this->IA_MouseRightBtn, ETriggerEvent::Triggered, this, &URTSInputRouter::HandleMouseRightTriggered);
			InputComponent->BindAction(this->IA_MouseRightBtn, ETriggerEvent::Completed, this, &URTSInputRouter::HandleMouseRightReleased);
			InputComponent->BindAction(this->IA_MouseRightBtn, ETriggerEvent::Canceled, this, &URTSInputRouter::HandleMouseRightReleased);
		}

		RegisterControlGroupHotkeys(InputComponent);
	}
}

void URTSInputRouter::BindInputActions()
{
	if (!PlayerController.IsValid() || !Selector || !IA_MouseLeftBtn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RTSInputRouter] Selection input was not bound because the player controller or input action is unavailable."));
		return;
	}

	if (const auto EnhancedInputComponent = Cast<UEnhancedInputComponent>(this->PlayerController->InputComponent))
	{
		EnhancedInputComponent->BindAction(
			this->IA_MouseLeftBtn,
			ETriggerEvent::Started,
			this,
			&URTSInputRouter::HandleMouseLeftPressed
		);

		EnhancedInputComponent->BindAction(
			this->IA_MouseLeftBtn,
			ETriggerEvent::Triggered,
			this,
			&URTSInputRouter::HandleMouseLeftTriggered
		);

		EnhancedInputComponent->BindAction(
			this->IA_MouseLeftBtn,
			ETriggerEvent::Completed,
			this,
			&URTSInputRouter::HandleMouseLeftReleased
		);

		EnhancedInputComponent->BindAction(
			this->IA_MouseLeftBtn,
			ETriggerEvent::Canceled,
			this,
			&URTSInputRouter::HandleMouseLeftReleased
		);

		if (!this->IA_MouseRightBtn)
		{
			// Dynamically load it in case the CDO failed to find it during editor startup
			this->IA_MouseRightBtn = Cast<UInputAction>(StaticLoadObject(UInputAction::StaticClass(), nullptr, TEXT("/Script/EnhancedInput.InputAction'/RTSInputSystem/Inputs/IA_MouseRightBtn.IA_MouseRightBtn'")));
		}

		if (this->IA_MouseRightBtn)
		{
			UE_LOG(LogTemp, Warning, TEXT("[RTSInputRouter] IA_MouseRightBtn BOUND SUCCESSFULLY!"));
			EnhancedInputComponent->BindAction(
				this->IA_MouseRightBtn,
				ETriggerEvent::Started,
				this,
				&URTSInputRouter::HandleMouseRightPressed
			);
			EnhancedInputComponent->BindAction(
				this->IA_MouseRightBtn,
				ETriggerEvent::Triggered,
				this,
				&URTSInputRouter::HandleMouseRightTriggered
			);
			EnhancedInputComponent->BindAction(
				this->IA_MouseRightBtn,
				ETriggerEvent::Completed,
				this,
				&URTSInputRouter::HandleMouseRightReleased
			);
			EnhancedInputComponent->BindAction(
				this->IA_MouseRightBtn,
				ETriggerEvent::Canceled,
				this,
				&URTSInputRouter::HandleMouseRightReleased
			);
		}
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————//
// 输入状态管理与鼠标分发
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————//

void URTSInputRouter::SetInputMode(ERTSInputMode NewMode)
{
	if (InputMode == NewMode)
	{
		return;
	}

	// 切换状态时把"进行中"的输入收尾，避免跨越状态的半截手势泄漏到另一个状态。
	// 例：正在框选时切走，selector 仍停在 bIsSelecting；这里主动补一次抬起。
	if (InputMode == ERTSInputMode::EntityManipulation && Selector)
	{
		FInputActionValue Empty;
		Selector->OnSelectionEnd(Empty);
	}

	InputMode = NewMode;
}

void URTSInputRouter::SetNavigationTestActive(bool bActive)
{
	EnsureNavigationTest();

	const ERTSInputMode NewMode = bActive
		? ERTSInputMode::NavigationTest
		: ERTSInputMode::EntityManipulation;

	// 先切换输入模式（它会负责把进行中的 selector 手势收尾），再驱动组件起停，
	// 这样两边的开关始终一致。
	SetInputMode(NewMode);

	if (NavigationTest)
	{
		NavigationTest->SetOwningRouter(this);
		if (bActive)
		{
			NavigationTest->StartNavigationTest();
		}
		else
		{
			NavigationTest->StopNavigationTest();
		}
	}
}

void URTSInputRouter::ToggleNavigationTest()
{
	SetNavigationTestActive(!IsNavigationTestActive());
}

bool URTSInputRouter::IsNavigationTestActive() const
{
	return InputMode == ERTSInputMode::NavigationTest;
}

// 鼠标输入响应入口。左右键对称，各含按下 / 拖动 / 释放；每个入口内部按当前 InputMode
// 直接分发给对应状态的处理器。

void URTSInputRouter::HandleMouseLeftPressed(const FInputActionValue& Value)
{
	switch (InputMode)
	{
	case ERTSInputMode::EntityManipulation:
		HandleEntityManipulationLeftPressed(Value);
		break;
	case ERTSInputMode::NavigationTest:
		HandleNavigationTestLeftPressed(Value);
		break;
	default:
		break;
	}
}

void URTSInputRouter::HandleMouseLeftTriggered(const FInputActionValue& Value)
{
	switch (InputMode)
	{
	case ERTSInputMode::EntityManipulation:
		HandleEntityManipulationLeftTriggered(Value);
		break;
	case ERTSInputMode::NavigationTest:
		HandleNavigationTestLeftTriggered(Value);
		break;
	default:
		break;
	}
}

void URTSInputRouter::HandleMouseLeftReleased(const FInputActionValue& Value)
{
	switch (InputMode)
	{
	case ERTSInputMode::EntityManipulation:
		HandleEntityManipulationLeftReleased(Value);
		break;
	case ERTSInputMode::NavigationTest:
		HandleNavigationTestLeftReleased(Value);
		break;
	default:
		break;
	}
}

void URTSInputRouter::HandleMouseRightPressed(const FInputActionValue& Value)
{
	switch (InputMode)
	{
	case ERTSInputMode::EntityManipulation:
		HandleEntityManipulationRightPressed(Value);
		break;
	case ERTSInputMode::NavigationTest:
		HandleNavigationTestRightPressed(Value);
		break;
	default:
		break;
	}
}

void URTSInputRouter::HandleMouseRightTriggered(const FInputActionValue& Value)
{
	switch (InputMode)
	{
	case ERTSInputMode::EntityManipulation:
		HandleEntityManipulationRightTriggered(Value);
		break;
	case ERTSInputMode::NavigationTest:
		HandleNavigationTestRightTriggered(Value);
		break;
	default:
		break;
	}
}

void URTSInputRouter::HandleMouseRightReleased(const FInputActionValue& Value)
{
	switch (InputMode)
	{
	case ERTSInputMode::EntityManipulation:
		HandleEntityManipulationRightReleased(Value);
		break;
	case ERTSInputMode::NavigationTest:
		HandleNavigationTestRightReleased(Value);
		break;
	default:
		break;
	}
}

// ── 实体操纵状态：原样转发给 URTSSelector ───────────────────────────────────────────────────

void URTSInputRouter::HandleEntityManipulationLeftPressed(const FInputActionValue& Value)
{
	if (Selector)
	{
		Selector->OnSelectionStart(Value);
	}
}

void URTSInputRouter::HandleEntityManipulationLeftTriggered(const FInputActionValue& Value)
{
	if (Selector)
	{
		Selector->OnUpdateSelection(Value);
	}
}

void URTSInputRouter::HandleEntityManipulationLeftReleased(const FInputActionValue& Value)
{
	if (Selector)
	{
		Selector->OnSelectionEnd(Value);
	}
}

void URTSInputRouter::HandleEntityManipulationRightPressed(const FInputActionValue& Value)
{
	if (Selector)
	{
		Selector->OnIssueCommand(Value);
	}
}

void URTSInputRouter::HandleEntityManipulationRightTriggered(const FInputActionValue& Value)
{
	(void)Value;
	// 实体操纵状态下右键暂无拖动语义。
}

void URTSInputRouter::HandleEntityManipulationRightReleased(const FInputActionValue& Value)
{
	(void)Value;
	// 实体操纵状态下右键暂无释放语义。
}

// ── 导航测试状态：转发给 URTSNavigationTestComponent ────────────────────────────────────────
//
// 具体的录入状态机与通路检查都在导航测试组件里；router 只负责把输入转过去。

void URTSInputRouter::HandleNavigationTestLeftPressed(const FInputActionValue& Value)
{
	(void)Value;
	if (NavigationTest)
	{
		NavigationTest->OnLeftPressed();
	}
}

void URTSInputRouter::HandleNavigationTestLeftTriggered(const FInputActionValue& Value)
{
	(void)Value;
	if (NavigationTest)
	{
		NavigationTest->OnLeftTriggered();
	}
}

void URTSInputRouter::HandleNavigationTestLeftReleased(const FInputActionValue& Value)
{
	(void)Value;
	if (NavigationTest)
	{
		NavigationTest->OnLeftReleased();
	}
}

void URTSInputRouter::HandleNavigationTestRightPressed(const FInputActionValue& Value)
{
	(void)Value;
	if (NavigationTest)
	{
		NavigationTest->OnRightPressed();
	}
}

void URTSInputRouter::HandleNavigationTestRightTriggered(const FInputActionValue& Value)
{
	(void)Value;
	// 导航测试状态下右键暂无拖动语义。
}

void URTSInputRouter::HandleNavigationTestRightReleased(const FInputActionValue& Value)
{
	(void)Value;
	// 导航测试状态下右键暂无释放语义（关闭在按下时已完成）。
}

void URTSInputRouter::BindInputMappingContext()
{
	ResolvePlayerController();
	if (PlayerController.IsValid() && PlayerController->GetLocalPlayer())
	{
		if (const auto Input = PlayerController->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			PlayerController->bShowMouseCursor = true;

			// Keep camera, UI, and project-level contexts that may already be active.
			if (!Input->HasMappingContext(this->InputMappingContext))
			{
				Input->AddMappingContext(this->InputMappingContext, 0);
			}
		}
	}
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————//
// Slate 预处理器的注册 / 反注册
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————//

void URTSInputRouter::RegisterSelectionInputProcessor()
{
	if (SelectionInputProcessor.IsValid()
		|| !FSlateApplication::IsInitialized())
	{
		return;
	}
	SelectionInputProcessor =
		MakeShared<FRTSSelectorInputProcessor>(this);
	FSlateApplication::Get().RegisterInputPreProcessor(
		SelectionInputProcessor);
}

void URTSInputRouter::UnregisterSelectionInputProcessor()
{
	if (SelectionInputProcessor.IsValid()
		&& FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(
			SelectionInputProcessor);
	}
	SelectionInputProcessor.Reset();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————//
// 控制组热键
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————//

void URTSInputRouter::RegisterControlGroupHotkeys(UInputComponent* InputComponent)
{
	if (!InputComponent)
	{
		const URTSInputPanelSettings* Settings = GetDefault<URTSInputPanelSettings>();
		if (!PlayerController.IsValid() || !PlayerController->InputComponent || (Settings && !Settings->bEnableControlGroupHotkeys))
		{
			return;
		}

		UnregisterControlGroupHotkeys();
	}

	UWorld* InputWorld = GetWorld();
	if (!InputWorld)
	{
		return;
	}

	if (InputComponent)
	{
		ControlGroupInputComponent = InputComponent;
	}
	else
	{
		ControlGroupInputComponent = NewObject<UInputComponent>(PlayerController.Get());
		if (!ControlGroupInputComponent)
		{
			return;
		}

		ControlGroupInputComponent->Priority = 10;
		ControlGroupInputComponent->bBlockInput = false;
		ControlGroupInputComponent->RegisterComponentWithWorld(InputWorld);
		PlayerController->PushInputComponent(ControlGroupInputComponent);
		ControlGroupInputOwner = PlayerController;
	}

	const FKey NumberKeys[] =
	{
		EKeys::Zero, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four,
		EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine
	};
	const FKey NumpadKeys[] =
	{
		EKeys::NumPadZero, EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour,
		EKeys::NumPadFive, EKeys::NumPadSix, EKeys::NumPadSeven, EKeys::NumPadEight, EKeys::NumPadNine
	};

	auto AddBinding = [this](const FKey& Key, int32 GroupIndex, bool bShift, bool bControl, bool bAlt)
	{
		FInputKeyBinding Binding(FInputChord(Key, bShift, bControl, bAlt, false), IE_Pressed);
		Binding.bConsumeInput = true;
		Binding.KeyDelegate.GetDelegateForManualSet().BindLambda([WeakThis = TWeakObjectPtr<URTSInputRouter>(this), GroupIndex]()
		{
			if (URTSInputRouter* Router = WeakThis.Get())
			{
				if (URTSSelector* SelectorComponent = Router->GetSelector())
				{
					SelectorComponent->HandleControlGroupHotkey(GroupIndex);
				}
			}
		});
		ControlGroupInputComponent->KeyBindings.Add(MoveTemp(Binding));
	};

	for (int32 GroupIndex = 0; GroupIndex <= 9; ++GroupIndex)
	{
		for (const FKey Key : { NumberKeys[GroupIndex], NumpadKeys[GroupIndex] })
		{
			AddBinding(Key, GroupIndex, false, false, false);
			AddBinding(Key, GroupIndex, false, true, false);
			AddBinding(Key, GroupIndex, true, false, false);
			AddBinding(Key, GroupIndex, false, false, true);
		}
	}
}

void URTSInputRouter::UnregisterControlGroupHotkeys()
{
	if (!ControlGroupInputComponent)
	{
		return;
	}

	if (ControlGroupInputOwner.IsValid())
	{
		ControlGroupInputOwner->PopInputComponent(ControlGroupInputComponent);
	}
	ControlGroupInputComponent->DestroyComponent();
	ControlGroupInputComponent = nullptr;
	ControlGroupInputOwner.Reset();
}

//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————//
// 软件光标
//——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————//

void URTSInputRouter::InstallStrategyMouseCursors()
{
	const URTSInputPanelSettings* Settings = GetDefault<URTSInputPanelSettings>();
	ULocalPlayer* LocalPlayer = PlayerController.IsValid()
		? PlayerController->GetLocalPlayer()
		: nullptr;
	UGameViewportClient* ViewportClient = LocalPlayer
		? LocalPlayer->ViewportClient
		: nullptr;
	if (!Settings
		|| !Settings->bEnableStrategyMouseCursor
		|| !ViewportClient)
	{
		return;
	}

	// 启用软件光标并为三种状态设置 RTS 指针。
	ViewportClient->SetUseSoftwareCursorWidgets(true);
	ViewportClient->SetSoftwareCursorWidget(
		EMouseCursor::Default,
		SNew(SRTSStrategyCursor)
		.Style(ERTSStrategyCursorStyle::Normal));
	ViewportClient->SetSoftwareCursorWidget(
		EMouseCursor::GrabHand,
		SNew(SRTSStrategyCursor)
		.Style(ERTSStrategyCursorStyle::Selectable));
	ViewportClient->SetSoftwareCursorWidget(
		EMouseCursor::Crosshairs,
		SNew(SRTSStrategyCursor)
		.Style(ERTSStrategyCursorStyle::Targeting));

	PlayerController->DefaultMouseCursor = EMouseCursor::Default;
	PlayerController->CurrentMouseCursor = EMouseCursor::Default;
}

void URTSInputRouter::RestoreStrategyMouseCursors()
{
	ULocalPlayer* LocalPlayer = PlayerController.IsValid()
		? PlayerController->GetLocalPlayer()
		: nullptr;
	if (UGameViewportClient* ViewportClient =
		LocalPlayer ? LocalPlayer->ViewportClient : nullptr)
	{
		// 清空三种状态的软件光标并重建。
		ViewportClient->SetSoftwareCursorWidget(
			EMouseCursor::Default,
			TSharedPtr<SWidget>());
		ViewportClient->SetSoftwareCursorWidget(
			EMouseCursor::GrabHand,
			TSharedPtr<SWidget>());
		ViewportClient->SetSoftwareCursorWidget(
			EMouseCursor::Crosshairs,
			TSharedPtr<SWidget>());
		ViewportClient->RebuildCursors();
	}
}
