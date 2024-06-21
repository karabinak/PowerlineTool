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
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SNew(SImage)
				.ColorAndOpacity(FColor::Black)
				.RenderOpacity(.3f)
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					[
						AssetPicker
					]

					+ SVerticalBox::Slot()
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("Generate Meshes")))
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.OnClicked_Raw(this, &FSimplePowerlineToolModule::CreateMeshClicked)
					]

					+ SVerticalBox::Slot()
					[
						SNew(SButton)
						.Text(FText::FromString(TEXT("Regenerate Selected Meshes")))
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.OnClicked_Raw(this, &FSimplePowerlineToolModule::RegenerateMeshClicked)
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

FReply FSimplePowerlineToolModule::CreateMeshClicked()
{
	if (!SelectedMesh) return FReply::Handled();
	if (!GetSelectedActors()) return FReply::Handled();

	bAttachToSocket = CanOperateOnSockets();
	if (ActorLocation.IsEmpty()) return FReply::Handled();
	SpawnPowerlineActors();

	ActorSelection.Empty();
	ActorLocation.Empty();
	return FReply::Handled();
}

FReply FSimplePowerlineToolModule::RegenerateMeshClicked()
{
	RegenerateMesh();

	return FReply::Handled();
}

void FSimplePowerlineToolModule::RegenerateMesh()
{
	USelection* SelectedActors = GEditor->GetSelectedActors();
	SelectedActors->GetSelectedObjects<AActor>(ActorSelection);

	if (ActorSelection.Num() > 0)
	{
		for (AActor* Object : ActorSelection)
		{
			if (Object)
			{
				TSet<UActorComponent*> Components = Object->GetComponents();
				USplineComponent* SplineComponents = nullptr;
				bool bChanging = false;
				int32 HowManyChanges = 0;
				for (UActorComponent* ActorComponent : Components)
				{
					if (USplineComponent* SplineComp = Cast<USplineComponent>(ActorComponent))
					{
						SplineComponents = SplineComp;
						bChanging = true;
						HowManyChanges = 0;
						continue;
					}
					if (bChanging)
					{
						if (HowManyChanges < SplineSegments)
						{
							if (USplineMeshComponent* MeshComp = Cast<USplineMeshComponent>(ActorComponent))
							{
								FVector StartLocation, StartTangent, EndLocation, EndTangent;
								SplineComponents->GetLocationAndTangentAtSplinePoint(HowManyChanges, StartLocation, StartTangent, ESplineCoordinateSpace::Local);
								SplineComponents->GetLocationAndTangentAtSplinePoint(HowManyChanges + 1, EndLocation, EndTangent, ESplineCoordinateSpace::Local);

								MeshComp->SetStartAndEnd(StartLocation, StartTangent, EndLocation, EndTangent);
								HowManyChanges++;
							}
						}
						else
						{
							bChanging = false;
						}
					}
				}
			}
		}
	}
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
			CreateSplineMeshComponents(SplineComp, CableActor);
		}
	}
}

void FSimplePowerlineToolModule::SetSplinePointsLocation(USplineComponent* SplineComp, int32 Index)
{
	FVector DistanceVector = ActorLocation[Index] - ActorLocation[Index + NumSocketPerActor];
	FVector PointDistance = DistanceVector / SplineSegments;

	FVector StartLocation = ActorLocation[Index + NumSocketPerActor] - PointDistance;
	FVector BaseStartLocation = StartLocation;
	for (int32 SplinePoint = 0; SplinePoint < SplineSegments + 1; SplinePoint++)
	{
		FVector NewLocation = LineBendZCalculator(SplinePoint, SplineComp, StartLocation);
		SplineComp->SetLocationAtSplinePoint(SplinePoint, NewLocation + PointDistance, ESplineCoordinateSpace::World);
		StartLocation += PointDistance;
	}
}

FVector FSimplePowerlineToolModule::LineBendZCalculator(int32 Index, USplineComponent* SplineComp, FVector Location)
{

	bool bZeroValue = LineBend < 0.f;
	bool bFirstOrLastIndex = Index == 0 || Index == SplineSegments;
	if (bZeroValue || bFirstOrLastIndex) return Location;

	int32 NumOfPoints = SplineSegments + 1;
	int32 HalfOfPoints = (NumOfPoints - 2) / 2; // -2 because without first and last point
	float OneLineBend = LineBend / HalfOfPoints;

	FVector OutVector = Location;
	if (NumOfPoints % 2 != 0)
	{
		if (Index <= HalfOfPoints)
		{
			float ZValue = -OneLineBend * Index;
			OutVector.Z += ZValue;
			return OutVector;
		}
		else
		{
			float ZValue = -OneLineBend * (NumOfPoints - Index - 1);
			OutVector.Z += ZValue;
			return OutVector;
		}
	}
	else
	{
		if (Index <= HalfOfPoints)
		{
			float ZValue = -OneLineBend * Index;
			OutVector.Z += ZValue;
			return OutVector;
		}
		else
		{
			float ZValue = -OneLineBend * (NumOfPoints - Index - 1);
			OutVector.Z += ZValue;
			return OutVector;
		}
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
				if (LastMeshComponent == nullptr) LastMeshComponent = MeshComponent;
	
				bool bNoSockets = MeshComponent->GetAllSocketNames().IsEmpty();
				bool bSameNumSockets = LastMeshComponent->GetAllSocketNames().Num() == MeshComponent->GetAllSocketNames().Num();
				if (bNoSockets)
				{
					ActorLocation.Add(MeshComponent->GetComponentLocation());
					NumSocketPerActor = 2;
				}
				else if (LastMeshComponent->GetAllSocketNames().Num() == MeshComponent->GetAllSocketNames().Num())
				{
					NumSocketPerActor = MeshComponent->GetAllSocketNames().Num();
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("One of selected actors have different amount of sockets"));
					ActorLocation.Empty();
					return false;
				}
				SaveSocketsLocation(MeshComponent);
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
	if (Names.IsEmpty())
	{
		ActorLocation.Add(MeshComponent->GetComponentLocation());
	}
	else
	{
		for (FName Name : Names)
		{
			ActorLocation.Add(MeshComponent->GetSocketLocation(Name));
		}
	}
}

void FSimplePowerlineToolModule::CreateSplineMeshComponents(USplineComponent* SplineComp, AActor* CableActor)
{
	for (int32 Point = 0; Point <= SplineComp->GetNumberOfSplinePoints() - 2; Point++)
	{
		FVector StartLocation, StartTangent, EndLocation, EndTangent;
		SplineComp->GetLocationAndTangentAtSplinePoint(Point, StartLocation, StartTangent, ESplineCoordinateSpace::Local);
		SplineComp->GetLocationAndTangentAtSplinePoint(Point + 1, EndLocation, EndTangent, ESplineCoordinateSpace::Local);

		USplineMeshComponent* SplineMeshComp = NewObject<USplineMeshComponent>(CableActor, USplineMeshComponent::StaticClass());
		if (SplineMeshComp)
		{
			SplineMeshComp->AttachToComponent(CableActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
			SplineMeshComp->RegisterComponent();
			SplineMeshComp->SetStartAndEnd(StartLocation, StartTangent, EndLocation, EndTangent);
			CableActor->AddInstanceComponent(SplineMeshComp);
			if (SelectedMesh)
			{
				SplineMeshComp->SetStaticMesh(SelectedMesh);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("No CableMesh"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("SplineMeshComp not valid"));
		}
	}
}

void FSimplePowerlineToolModule::OnAssetSelected(const FAssetData& AssetData)
{
	SelectedMesh = Cast<UStaticMesh>(AssetData.GetAsset());
	if (SelectedMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("Asset Selected: %s"), *SelectedMesh->GetName());
	}
}
	
IMPLEMENT_MODULE(FSimplePowerlineToolModule, SimplePowerlineTool)