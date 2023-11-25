// Copyright Epic Games, Inc. All Rights Reserved.

#include "CameraShakePreviewGameMode.h"
#include "CameraShakePreviewHUD.h"
#include "CameraShakePreviewCharacter.h"
#include "UObject/ConstructorHelpers.h"

ACameraShakePreviewGameMode::ACameraShakePreviewGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPersonCPP/Blueprints/FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

	// use our custom HUD class
	HUDClass = ACameraShakePreviewHUD::StaticClass();
}
