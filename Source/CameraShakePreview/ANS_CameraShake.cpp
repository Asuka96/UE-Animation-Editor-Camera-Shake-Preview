// Fill out your copyright notice in the Description page of Project Settings.


#include "ANS_CameraShake.h"

#include "Engine/World.h"
#include "GameFrameWork/Pawn.h"
#include "GameFrameWork/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#if WITH_EDITOR
namespace UE
{
namespace MovieScene
{
constexpr auto AnimationViewportMapName = "Transient";
	
UWorld* FindCameraShakePreviewerWorld()
{
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Editor)
		{
			return Context.World();
		}
	}
	return nullptr;
}

}
}

/**
 * Data struct for each entry in the panel's main list.
 */
struct FViewportCameraShakeData
	: public TSharedFromThis<FViewportCameraShakeData>
	, public FGCObject
{
	TSubclassOf<UCameraShakeBase> ShakeClass;
	UCameraShakeBase* ShakeInstance = nullptr;
	bool bIsPlaying = false;
	bool bIsHidden = false;

	TWeakObjectPtr<UCameraShakeSourceComponent> SourceComponent;

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(ShakeInstance);
	}
	virtual FString GetReferencerName() const override
	{
		return TEXT("FViewportCameraShakeData");
	}
};

FViewportCameraShakePreviewer::FViewportCameraShakePreviewer()
	: PreviewCamera(NewObject<AViewportPreviewPlayerCameraManager>())
	, LastLocationModifier(FVector::ZeroVector)
	, LastRotationModifier(FRotator::ZeroRotator)
	, LastFOVModifier(0.f)
{
	PreviewCameraShake = CastChecked<UCameraModifier_CameraShake>(
			PreviewCamera->AddNewCameraModifier(UCameraModifier_CameraShake::StaticClass()));
}

void FViewportCameraShakePreviewer::Tick(float DeltaTime)
{
	// Accumulate the deltas in case we get ticked several times before we are 
	// asked to modify a viewport.
	LastDeltaTime = LastDeltaTime.Get(0.f) + DeltaTime; 
}

void FViewportCameraShakePreviewer::ModifyCamera(FEditorViewportViewModifierParams& Params)
{
	const float DeltaTime = LastDeltaTime.Get(-1.f);
	if (DeltaTime > 0.f)
	{
		LastPostProcessSettings.Reset();
		LastPostProcessBlendWeights.Reset();
		PreviewCamera->ResetPostProcessSettings();

		FMinimalViewInfo OriginalPOV(Params.ViewInfo);

		PreviewCameraShake->ModifyCamera(DeltaTime, Params.ViewInfo);

		PreviewCamera->MergePostProcessSettings(LastPostProcessSettings, LastPostProcessBlendWeights);

		for (UCameraAnimInst* ActiveAnim : ActiveAnims)
		{
			UpdateCameraAnimInstance(*ActiveAnim, DeltaTime, Params.ViewInfo);
		}

		LastLocationModifier = Params.ViewInfo.Location - OriginalPOV.Location;
		LastRotationModifier = Params.ViewInfo.Rotation - OriginalPOV.Rotation;
		LastFOVModifier = Params.ViewInfo.FOV - OriginalPOV.FOV;

		LastDeltaTime.Reset();
	}
	else
	{                                      
		Params.ViewInfo.Location += LastLocationModifier;
		Params.ViewInfo.Rotation += LastRotationModifier;
		Params.ViewInfo.FOV += LastFOVModifier;
	}

	for (int32 PPIndex = 0; PPIndex < LastPostProcessSettings.Num(); ++PPIndex)
	{
		Params.AddPostProcessBlend(LastPostProcessSettings[PPIndex], LastPostProcessBlendWeights[PPIndex]);
	}

	// Clean-up finished camera anims.
	for (int32 Index = ActiveAnims.Num() - 1; Index >= 0; --Index)
	{
		UCameraAnimInst* ActiveAnim(ActiveAnims[Index]);
		if (ActiveAnim->bFinished)
		{
			ActiveAnims.RemoveAt(Index);
		}
	}
}

void FViewportCameraShakePreviewer::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(PreviewCamera);
	Collector.AddReferencedObjects(ActiveAnims);
}

UCameraShakeBase* FViewportCameraShakePreviewer::AddCameraShake(TSubclassOf<UCameraShakeBase> ShakeClass, const FAddCameraShakeParams& Params)
{
	FAddCameraShakeParams ActualParams(Params);
	if (ShakeClass.Get()->IsChildOf<UMatineeCameraShake>())
	{
		ActualParams.Initializer = FOnInitializeCameraShake::CreateLambda([this](UCameraShakeBase* ShakeInstance)
			{
				UMatineeCameraShake* MatineeShakeInstance = CastChecked<UMatineeCameraShake>(ShakeInstance);
				MatineeShakeInstance->SetTempCameraAnimActor(GetTempCameraActor());
				UE_LOG(LogTemp, Warning, TEXT("The Camera Actor's name is %s"), *GetTempCameraActor()->GetName());
			});
	}

	UCameraShakeBase* ShakeInstance = PreviewCameraShake->AddCameraShake(ShakeClass, ActualParams);
	UE_LOG(LogTemp, Warning, TEXT("The Shake Instance's name is %s"), *ShakeInstance->GetName());
	if (UMatineeCameraShake* MatineeShakeInstance = Cast<UMatineeCameraShake>(ShakeInstance))
	{
		if (UCameraAnimInst* CameraAnimInstance = MatineeShakeInstance->AnimInst)
		{
			ActiveAnims.Add(CameraAnimInstance);
		}
	}
	return ShakeInstance;
}

ACameraActor* FViewportCameraShakePreviewer::GetTempCameraActor()
{
	if (!TempCameraActor.IsValid())
	{
		UWorld* World = UE::MovieScene::FindCameraShakePreviewerWorld();
		if (!ensure(World))
		{
			return nullptr;
		}

		FActorSpawnParameters SpawnInfo;
		SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		// Don't save this actor into a map.
		SpawnInfo.ObjectFlags |= RF_Transient;
		TempCameraActor = World->SpawnActor<ACameraActor>(SpawnInfo);
		if (TempCameraActor.IsValid())
		{
			TempCameraActor.Get()->SetIsTemporarilyHiddenInEditor(true);
		}
	}

	check(TempCameraActor.IsValid());
	return TempCameraActor.Get();
}

void FViewportCameraShakePreviewer::UpdateCameraAnimInstance(UCameraAnimInst& CameraAnimInstance, float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	ACameraActor* Actor = TempCameraActor.Get();
	if (!ensure(Actor != nullptr))
	{
		return;
	}
	if (CameraAnimInstance.bFinished)
	{
		return;
	}

	Actor->SetActorLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);

	const ACameraActor* DefaultActor = GetDefault<ACameraActor>();
	if (DefaultActor)
	{
		Actor->GetCameraComponent()->AspectRatio = DefaultActor->GetCameraComponent()->AspectRatio;
		Actor->GetCameraComponent()->FieldOfView = CameraAnimInstance.CamAnim->BaseFOV;
		Actor->GetCameraComponent()->PostProcessSettings = CameraAnimInstance.CamAnim->BasePostProcessSettings;
		Actor->GetCameraComponent()->PostProcessBlendWeight = CameraAnimInstance.CamAnim->BasePostProcessBlendWeight;
	}

	CameraAnimInstance.AdvanceAnim(DeltaTime, false);

	if (CameraAnimInstance.CurrentBlendWeight > 0.f)
	{
		CameraAnimInstance.ApplyToView(InOutPOV);

		if (Actor->GetCameraComponent()->PostProcessBlendWeight > 0.f)
		{
			AddPostProcessBlend(
					Actor->GetCameraComponent()->PostProcessSettings, 
					Actor->GetCameraComponent()->PostProcessBlendWeight * CameraAnimInstance.CurrentBlendWeight);
		}
	}
}

void FViewportCameraShakePreviewer::CleanUpCameraAnimInstances()
{
	for (int32 Index = ActiveAnims.Num() - 1; Index >= 0; --Index)
	{
		UCameraAnimInst* ActiveAnim(ActiveAnims[Index]);
		if (!ActiveAnim || ActiveAnim->bFinished)
		{
			ActiveAnims.RemoveAt(Index);
		}
	}
}

void FViewportCameraShakePreviewer::AddPostProcessBlend(const FPostProcessSettings& Settings, float Weight)
{
	check(LastPostProcessSettings.Num() == LastPostProcessBlendWeights.Num());
	LastPostProcessSettings.Add(Settings);
	LastPostProcessBlendWeights.Add(Weight);
}

void UANS_CameraShake::OnModifyView(FEditorViewportViewModifierParams& Params)
{
	CameraShakePreviewUpdater->ModifyCamera(Params);
}

#endif

void UANS_CameraShake::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration){
	Super::NotifyBegin(MeshComp, Animation, TotalDuration);
#if WITH_EDITOR
	if(GEditor && bEnablePreview)
	{
		const TArray<FEditorViewportClient*>& ViewportClients = GEditor->GetAllViewportClients();
		for (FEditorViewportClient* ViewportClient : ViewportClients)
		{
			if(ViewportClient->GetWorld()->GetMapName() == TEXT("Transient"))//动画视口场景名称
				{
				ViewportClient->ViewModifiers.AddUObject(this, &UANS_CameraShake::OnModifyView);//增加委托后，下面的shake才会生效
				}
		}
		CameraShakePreviewUpdater = TUniquePtr<FViewportCameraShakePreviewer>(new FViewportCameraShakePreviewer());
		CameraShakePreviewUpdater->AddCameraShake(Shake, FAddCameraShakeParams(Scale, PlaySpace, UserPlaySpaceRot));

	}
#endif
}

void UANS_CameraShake::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	Super::NotifyEnd(MeshComp, Animation);
	
}