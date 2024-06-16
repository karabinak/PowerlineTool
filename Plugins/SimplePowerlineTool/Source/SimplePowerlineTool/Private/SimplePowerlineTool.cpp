// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimplePowerlineTool.h"
#include "SimplePowerlineToolStyle.h"
#include "SimplePowerlineToolCommands.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "ToolMenus.h"

#include "Engine/Selection.h"
#include "Engine/StaticMeshSocket.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"

static const FName SimplePowerlineToolTabName("SimplePowerlineTool");

#define LOCTEXT_NAMESPACE "FSimplePowerlineToolModule"

void FSimplePowerlineToolModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FSimplePowerlineToolStyle::Initialize();
	FSimplePowerlineToolStyle::ReloadTextures();

	FSimplePowerlineToolCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FSimplePowerlineToolCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FSimplePowerlineToolModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FSimplePowerlineToolModule::RegisterMenus));
	
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(SimplePowerlineToolTabName, FOnSpawnTab::CreateRaw(this, &FSimplePowerlineToolModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FSimplePowerlineToolTabTitle", "SimplePowerlineTool"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FSimplePowerlineToolModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FSimplePowerlineToolStyle::Shutdown();

	FSimplePowerlineToolCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SimplePowerlineToolTabName);
}

TSharedRef<SDockTab> FSimplePowerlineToolModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.Filter.ClassPaths.Empty();
	AssetPickerConfig.Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
	AssetPickerConfig.bForceShowEngineContent = false;
	AssetPickerConfig.SelectionMode = ESelectionMode::Single;
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateRaw(this, &FSimplePowerlineToolModule::OnAssetSelected);


	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	TSharedRef<SWidget> AssetPicker = ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig);

	FText WidgetText = FText::Format(
		LOCTEXT("WindowWidgetText", "Add code to {0} in {1} to override this window's contents"),
		FText::FromString(TEXT("FSimplePowerlineToolModule::OnSpawnPluginTab")),
		FText::FromString(TEXT("SimplePowerlineTool.cpp"))
		);

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			// Put your tab content here!
			SNew(SOverlay)
			+SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SNew(SBox)
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Top)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Click Me")))
					.OnClicked_Raw(this, &FSimplePowerlineToolModule::OnButtonClick)
				]
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Top)
			[
				SNew(SBox)
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.WidthOverride(250.f)
				.HeightOverride(400.f)
				[
					AssetPicker
				]
			]
		];
}

void FSimplePowerlineToolModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(SimplePowerlineToolTabName);
}

void FSimplePowerlineToolModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FSimplePowerlineToolCommands::Get().OpenPluginWindow, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Settings");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FSimplePowerlineToolCommands::Get().OpenPluginWindow));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE

FReply FSimplePowerlineToolModule::OnButtonClick()
{

	if (!GetSelectedActors()) return FReply::Handled();
	bAttachToSocket = CanOperateOnSockets();
	SpawnPowerlineActors();


	ActorSelection.Empty();
	ActorLocation.Empty();
	return FReply::Handled();
}

void FSimplePowerlineToolModule::SpawnPowerlineActors()
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	for (int32 ActorNum = 0; ActorNum < ActorSelection.Num() - 1; ActorNum++)
	{
		FActorSpawnParameters SpawnParameters;
		AActor* CableActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector(0.f, 0.f, 0.f), FRotator(0.f, 0.f, 0.f), SpawnParameters);
		CreateRootComponent(CableActor);
		CableActor->SetActorLocation(ActorSelection[ActorNum]->GetActorLocation());
		CableActor->SetIsSpatiallyLoaded(true);

		for (int32 Index = 0; Index < NumSocketPerActor; Index++)
		{
			USplineComponent* SplineComp = CreateSplineComponents(CableActor);
			AddSplinePoints(SplineComp);
			SetSplinePointsLocation(SplineComp, ActorNum * NumSocketPerActor + Index);
			/*CreateSplineMeshComponents(SplineComp, CableActor);*/
		}
	}
}

void FSimplePowerlineToolModule::SetSplinePointsLocation(USplineComponent* SplineComp, int32 Index)
{
	FVector DistanceVector = ActorLocation[Index] - ActorLocation[Index + NumSocketPerActor];
	FVector PointDistance = DistanceVector / SplineSegments - 1;

	FVector StartLocation = ActorLocation[Index + NumSocketPerActor] - PointDistance;
	for (int32 SplinePoint = 0; SplinePoint < SplineComp->GetNumberOfSplinePoints(); SplinePoint++)
	{
		SplineComp->SetLocationAtSplinePoint(SplinePoint, StartLocation + PointDistance, ESplineCoordinateSpace::World);
		StartLocation += PointDistance;
	}
}

void FSimplePowerlineToolModule::AddSplinePoints(USplineComponent* SplineComp)
{
	for (int32 Segment = 0; Segment < SplineSegments - 1; Segment++)
	{
		FVector PointPosition;
		SplineComp->AddSplinePoint(PointPosition, ESplineCoordinateSpace::Local);
	}
}

USplineComponent* FSimplePowerlineToolModule::CreateSplineComponents(AActor* CableActor)
{
	USplineComponent* SplineComp = NewObject<USplineComponent>(CableActor);
	if (SplineComp)
	{
		SplineComp->SetupAttachment(CableActor->GetRootComponent());
		SplineComp->RegisterComponent();
		SplineComp->SetDrawDebug(false);
		CableActor->AddInstanceComponent(SplineComp);
		return SplineComp;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Creating USplineComponent FAILED!"));
	}
	return nullptr;
}

void FSimplePowerlineToolModule::CreateRootComponent(AActor* CableActor)
{
	if (!CableActor->GetRootComponent())
	{
		USceneComponent* RootComp = NewObject<USceneComponent>(CableActor, TEXT("RootComponent"));
		CableActor->SetRootComponent(RootComp);
		CableActor->GetRootComponent()->SetMobility(EComponentMobility::Static);
		RootComp->RegisterComponent();
	}
}

bool FSimplePowerlineToolModule::GetSelectedActors()
{
	USelection* EditorSelection = GEditor->GetSelectedActors();
	EditorSelection->GetSelectedObjects<AActor>(ActorSelection);
	if (ActorSelection.Num() > 1)
	{
		NumSelectedActors = ActorSelection.Num();
		return true;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Select atleast 2 actors"));
		return false;
	}
}

bool FSimplePowerlineToolModule::CanOperateOnSockets()
{
	UStaticMeshComponent* LastMeshComponent = nullptr;
	for (AActor* Actor : ActorSelection)
	{
		if (Actor)
		{
			if (UStaticMeshComponent* MeshComponent = Actor->GetComponentByClass<UStaticMeshComponent>())
			{
				if (LastMeshComponent)
				{
					bool SameNumSockets = LastMeshComponent->GetAllSocketNames().Num() == MeshComponent->GetAllSocketNames().Num();
					if (SameNumSockets)
					{
						LastMeshComponent = MeshComponent;
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("One of selected actors have different amount of sockets"));
						ActorLocation.Empty();
						NumSocketPerActor = 1;
						return false;
					}
				}
				SaveSocketsLocation(MeshComponent);
				NumSocketPerActor = MeshComponent->GetAllSocketNames().Num();
				LastMeshComponent = MeshComponent;
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("One of selected actors, don't have MeshComponent"));
				return false;
			}
		}
	}
	return true;
}

void FSimplePowerlineToolModule::SaveSocketsLocation(UStaticMeshComponent* MeshComponent)
{
	TArray<FName> Names = MeshComponent->GetAllSocketNames();
	for (FName Name : Names)
	{
		ActorLocation.Add(MeshComponent->GetSocketLocation(Name));
	}
}

void FSimplePowerlineToolModule::CreateSplineMeshComponents(USplineComponent* SplineComp, AActor* CableActor)
{
	//for (int32 Point = 0; Point <= SplineComp->GetNumberOfSplinePoints() - 2; Point++)
	//{
	//	FVector StartLocation, StartTangent, EndLocation, EndTangent;
	//	SplineComp->GetLocationAndTangentAtSplinePoint(Point, StartLocation, StartTangent, ESplineCoordinateSpace::Local);
	//	SplineComp->GetLocationAndTangentAtSplinePoint(Point + 1, EndLocation, EndTangent, ESplineCoordinateSpace::Local);

	//	USplineMeshComponent* SplineMeshComp = NewObject<USplineMeshComponent>(CableActor, USplineMeshComponent::StaticClass());
	//	if (SplineMeshComp)
	//	{
	//		SplineMeshComp->AttachToComponent(CableActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	//		SplineMeshComp->RegisterComponent();
	//		SplineMeshComp->SetStartAndEnd(StartLocation, StartTangent, EndLocation, EndTangent);
	//		CableActor->AddInstanceComponent(SplineMeshComp);
	//		if (CableMesh)
	//		{
	//			SplineMeshComp->SetStaticMesh(CableMesh);
	//		}
	//		else
	//		{
	//			UE_LOG(LogTemp, Warning, TEXT("No CableMesh"));
	//		}
	//	}
	//	else
	//	{
	//		UE_LOG(LogTemp, Warning, TEXT("SplineMEshCOmp not valid"));
	//	}
	//}
}

void FSimplePowerlineToolModule::OnAssetSelected(const FAssetData& AssetData)
{
	SelectedAsset = Cast<UStaticMesh>(AssetData.GetAsset());
	if (SelectedAsset)
	{
		UE_LOG(LogTemp, Warning, TEXT("Asset Selected: %s"), *SelectedAsset->GetName());
	}
}
	
IMPLEMENT_MODULE(FSimplePowerlineToolModule, SimplePowerlineTool)