// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FToolBarBuilder;
class FMenuBuilder;
class USplineComponent;

class FSimplePowerlineToolModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/** This function will be bound to Command (by default it will bring up plugin window) */
	void PluginButtonClicked();
	
private:

	void RegisterMenus();

	TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);

	FReply OnButtonClick();

	void SpawnPowerlineActors();

	void AddSplinePoints(USplineComponent* SplineComp);

	void SetSplinePointsLocation(USplineComponent* SplineComp, int32 Index);

	USplineComponent* CreateSplineComponents(AActor* CableActor);

	void CreateRootComponent(AActor* CableActor);

	bool GetSelectedActors();
	bool CanOperateOnSockets();

	void SaveSocketsLocation(UStaticMeshComponent* MeshComponent);

	void CreateSplineMeshComponents(USplineComponent* SplineComp, AActor* CableActor);

	TArray<AActor*> ActorSelection;
	TArray<FVector> ActorLocation;
	int32 NumSelectedActors = 0;

	int32 NumSocketPerActor = 0;
	bool bAttachToSocket = false;

	int32 SplineSegments = 7;

	UStaticMesh* SelectedAsset;

	void OnAssetSelected(const FAssetData& AssetData);


private:
	TSharedPtr<class FUICommandList> PluginCommands;
};
