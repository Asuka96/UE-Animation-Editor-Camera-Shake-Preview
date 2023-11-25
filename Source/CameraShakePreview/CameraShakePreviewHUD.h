// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "CameraShakePreviewHUD.generated.h"

UCLASS()
class ACameraShakePreviewHUD : public AHUD
{
	GENERATED_BODY()

public:
	ACameraShakePreviewHUD();

	/** Primary draw call for the HUD */
	virtual void DrawHUD() override;

private:
	/** Crosshair asset pointer */
	class UTexture2D* CrosshairTex;

};

