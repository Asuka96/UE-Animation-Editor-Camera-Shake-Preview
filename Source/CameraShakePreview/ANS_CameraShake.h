// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MatineeCameraShake.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraAnim.h"
#include "Camera/CameraAnimInst.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/CameraComponent.h"
#include "TickableEditorObject.h"
#include "Camera/CameraModifier_CameraShake.h"
#include "EditorViewportClient.h"
#include "Camera/PlayerCameraManager.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"

#include "ANS_CameraShake.generated.h"


#if WITH_EDITOR
UCLASS()
class AViewportPreviewPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

public:
	void ResetPostProcessSettings()
	{
		ClearCachedPPBlends();
	}

	void MergePostProcessSettings(TArray<FPostProcessSettings>& InSettings, TArray<float>& InBlendWeights)
	{
		InSettings.Append(PostProcessBlendCache);
		InBlendWeights.Append(PostProcessBlendCacheWeights);
	}
};

class FViewportCameraShakePreviewer : public FTickableEditorObject, public FGCObject
{
public:
	FViewportCameraShakePreviewer();

	// FTickableObject Interface
	virtual ETickableTickType GetTickableTickType() const { return ETickableTickType::Always; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(FViewportCameraShakePreviewer, STATGROUP_Tickables); }
	virtual void Tick(float DeltaTime) override;

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("SCameraShakePreviewer"); }

	UCameraShakeBase* AddCameraShake(TSubclassOf<UCameraShakeBase> ShakeClass, const FAddCameraShakeParams& Params);
	void ModifyCamera(FEditorViewportViewModifierParams& Params);

private:
	ACameraActor* GetTempCameraActor();
	void UpdateCameraAnimInstance(UCameraAnimInst& CameraAnimInstance, float DeltaTime, FMinimalViewInfo& InOutPOV);
	void CleanUpCameraAnimInstances();

	void AddPostProcessBlend(const FPostProcessSettings& Settings, float Weight);

private:
	AViewportPreviewPlayerCameraManager* PreviewCamera;
	UCameraModifier_CameraShake* PreviewCameraShake;

	/** Hidden camera actor and active camera anims for Matinee shakes specifically */
	TWeakObjectPtr<ACameraActor> TempCameraActor;
	TArray<UCameraAnimInst*> ActiveAnims;

	TOptional<float> LastDeltaTime;

	FVector LastLocationModifier;
	FRotator LastRotationModifier;
	float LastFOVModifier;

	TArray<FPostProcessSettings> LastPostProcessSettings;
	TArray<float> LastPostProcessBlendWeights;
};
#endif


UCLASS(meta = (DisplayName = "CameraShake"))
class UANS_CameraShake : public UAnimNotifyState
{
	GENERATED_BODY()
public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration) override;
	//virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
	
private:
	UPROPERTY(EditAnywhere)
	TSubclassOf<class UCameraShakeBase> Shake;
	UPROPERTY(EditAnywhere)
	float Scale = 1.f;
	UPROPERTY(EditAnywhere)
	ECameraShakePlaySpace PlaySpace = ECameraShakePlaySpace::CameraLocal;
	UPROPERTY(EditAnywhere)
	FRotator UserPlaySpaceRot = FRotator::ZeroRotator;
	UPROPERTY(EditAnywhere)
	bool bEnablePreview = false;
#if WITH_EDITOR
	TUniquePtr<FViewportCameraShakePreviewer> CameraShakePreviewUpdater;
	void OnModifyView(FEditorViewportViewModifierParams& Params);
#endif
};
