// Copyright Winyunq, 2026. All Rights Reserved.

#include "RTSMinimalPlayerController.h"

#include "RTSInputRouter.h"

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

bool ARTSMinimalPlayerController::IsCameraLockedToEntity()
{
	return false;
}

void ARTSMinimalPlayerController::LockCameraToEntity(const FEntityHandle& entity)
{
}

void ARTSMinimalPlayerController::UnLockCamera()
{
}

void ARTSMinimalPlayerController::HandleMouseLeftPressed(const FInputActionValue& Value)
{
}

void ARTSMinimalPlayerController::HandleMouseLeftTriggered(const FInputActionValue& Value)
{
}

void ARTSMinimalPlayerController::HandleMouseLeftReleased(const FInputActionValue& Value)
{
}
