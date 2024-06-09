///////////////////////////////////////////
////	
////	Author: Kamil Mamcarczyk
////	Date: 03.06.24
////
///////////////////////////////////////////

#include "PowerlineToolWidget.h"
#include "EditorUtilityLibrary.h"
#include "EditorUtilityWidgetComponents.h"
#include "Engine/Selection.h"
#include "Engine/StaticMeshSocket.h"
#include "Editor/EditorEngine.h"

#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/TextBlock.h"

UPowerlineToolWidget::UPowerlineToolWidget()
{


}

void UPowerlineToolWidget::NativeConstruct()
{
	Super::NativeConstruct();

	CreatePowerlinesButton->OnClicked.AddDynamic(this, &ThisClass::CreatePowerlines);
	RegenerateSelectedMeshButton->OnClicked.AddDynamic(this, &ThisClass::RegenerateSelectedMesh);
	AttachCheckBox->OnCheckStateChanged.AddDynamic(this, &ThisClass::OnCheckboxChanged);
	SegmentsSlider->OnValueChanged.AddDynamic(this, &ThisClass::OnSegmentSliderChanged);
	CurveSlider->OnValueChanged.AddDynamic(this, &ThisClass::OnCurveSliderChanged);
}

void UPowerlineToolWidget::OnSegmentSliderChanged(float Value)
{
	Segments = Value;
	ChangeSegmentText(Segments);
}

void UPowerlineToolWidget::OnCurveSliderChanged(float Value)
{
	Elevation = Value;
	ChangeCurveText(Elevation);
}

void UPowerlineToolWidget::OnCheckboxChanged(bool bIsChecked)
{
	bAttachToSockets = bIsChecked;
	AttachCheckBox->SetIsChecked(bIsChecked);
}

void UPowerlineToolWidget::RegenerateSelectedMesh()
{
	USelection* SelectedActors = GEditor->GetSelectedActors();
	SelectedActors->GetSelectedObjects<AActor>(ObjectSelection);

	if (ObjectSelection.Num() > 0)
	{
		for (AActor* Object : ObjectSelection)
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
						if (HowManyChanges < SplineComponents->GetNumberOfSplinePoints() - 2)
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

void UPowerlineToolWidget::CreatePowerlines()
{
	if (!SelectedObjects()) return;

	FActorSpawnParameters SpawnParameters;
	AActor* CableActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FVector(0.f, 0.f, 0.f), FRotator(0.f, 0.f, 0.f), SpawnParameters);

	if (CableActor)
	{
		// Initialize Root Component if doesn't exist
		CreateRootComponent(CableActor);
		CableActor->SetActorLocation(ObjectSelection[0]->GetActorLocation());
		CableActor->SetIsSpatiallyLoaded(true);

		// Calculate how much spline components to spawn
		int32 AmountOfSplineComponents;
		if (bAttachToSockets)
		{
			AmountOfSplineComponents = AmountOfSockets / 2;
		}
		else
		{
			AmountOfSplineComponents = 1;
		}

		for (int32 IntComponent = 0; IntComponent < AmountOfSplineComponents; IntComponent++)
		{
			// Create SplineComponents
			USplineComponent* SplineComp = CreateSplineComponent(CableActor);
			if (SplineComp)
			{
				CreateSplinePoints(SplineComp);
				SetSplinePointsLocation(SplineComp, IntComponent);
				SetPointsCurve(SplineComp);
				CreateSplineMeshComponents(SplineComp, CableActor);
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("Object Created Successfully!"));
	ClearData();
}

void UPowerlineToolWidget::ClearData()
{
	ObjectsLocations.Empty();
	SocketsLocations.Empty();
	ObjectSelection.Empty();
	AmountOfSockets = 0;
}

bool UPowerlineToolWidget::SelectedObjects()
{
	USelection* SelectedActors = GEditor->GetSelectedActors();
	SelectedActors->GetSelectedObjects<AActor>(ObjectSelection);

	// Only 2 Actors
	if (ObjectSelection.Num() == 2)
	{
		// Add selected objects to array
		for (AActor* Object : ObjectSelection)
		{
			if (Object)
			{
				if (bAttachToSockets)
				{
					if (SocketAmount(Object))
					{
						continue;
					}
					else
					{
						return false;
					}
				}
				else
				{
					// Getting object location without sockets
					ObjectsLocations.Add(Object->GetActorLocation());
				}
			}
		}
		return true;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Select Only 2 objects"));
		return false;
	}
}

bool UPowerlineToolWidget::SocketAmount(AActor* Object)
{
	if (UStaticMeshComponent* MeshComponent = Object->GetComponentByClass<UStaticMeshComponent>())
	{
		AmountOfSockets += MeshComponent->GetAllSocketNames().Num();
		if (AmountOfSockets % 2 == 0)
		{
			TArray<FName> SocketNames = MeshComponent->GetAllSocketNames();
			if (SocketNames.IsEmpty())
			{
				UE_LOG(LogTemp, Warning, TEXT("No sockets!"));
				return false;
			}
			SocketLocation(SocketNames, MeshComponent);
			return true;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Amount of sockets is not even!"));
			return false;
		}
	}
	return false;
}

void UPowerlineToolWidget::SocketLocation(FPermissionListOwners& SocketsName, UStaticMeshComponent* MeshComponent)
{
	for (FName Name : SocketsName)
	{
		ObjectsLocations.Add(MeshComponent->GetSocketLocation(Name));
	}
}

USplineComponent* UPowerlineToolWidget::CreateSplineComponent(AActor* CableActor)
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

void UPowerlineToolWidget::CreateSplineMeshComponents(USplineComponent* SplineComp, AActor* CableActor)
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
			if (CableMesh)
			{
				SplineMeshComp->SetStaticMesh(CableMesh);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("No CableMesh"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("SplineMEshCOmp not valid"));
		}
	}
}

void UPowerlineToolWidget::SetPointsCurve(USplineComponent* SplineComp)
{
	if (Elevation > 0.f)
	{
		float ElevationStep = Elevation / (SplineComp->GetNumberOfSplinePoints() / 2);
		for (int i = 1; i <= SplineComp->GetNumberOfSplinePoints() / 2 - 1; i++)
		{
			FVector OldLocation;
			OldLocation = SplineComp->GetSplinePointAt(i, ESplineCoordinateSpace::World).Position;
			SplineComp->SetLocationAtSplinePoint(i, OldLocation - FVector(0.f, 0.f, ElevationStep * i), ESplineCoordinateSpace::World);

			OldLocation = SplineComp->GetSplinePointAt(SplineComp->GetNumberOfSplinePoints() - i - 1, ESplineCoordinateSpace::World).Position;
			SplineComp->SetLocationAtSplinePoint(SplineComp->GetNumberOfSplinePoints() - i - 1, OldLocation - FVector(0.f, 0.f, ElevationStep * i), ESplineCoordinateSpace::World);
		}

		if (SplineComp->GetNumberOfSplinePoints() % 2 != 0)
		{
			int32 MiddlePoint = SplineComp->GetNumberOfSplinePoints() / 2;
			FVector OldLocation = SplineComp->GetSplinePointAt(MiddlePoint, ESplineCoordinateSpace::World).Position;
			SplineComp->SetLocationAtSplinePoint(MiddlePoint, OldLocation - FVector(0.f, 0.f, Elevation), ESplineCoordinateSpace::World);
		}
	}
}

void UPowerlineToolWidget::SetSplinePointsLocation(USplineComponent* SplineComp, int32 Index)
{
	int32 HalfOfSockets;
	if (bAttachToSockets)
	{
		HalfOfSockets = AmountOfSockets / 2;
	}
	else
	{
		HalfOfSockets = 1;
	}

	FVector VectorDistance = ObjectsLocations[Index] - ObjectsLocations[Index + HalfOfSockets];
	FVector PointToPointDistance = VectorDistance / Segments;

	FVector StartLocation = ObjectsLocations[Index + HalfOfSockets] - PointToPointDistance;
	for (int32 Iterator = 0; Iterator < SplineComp->GetNumberOfSplinePoints(); Iterator++)
	{
		// Actually I don't know why that is happening, but it's working :D
		if (Iterator == 0 || Iterator == SplineComp->GetNumberOfSplinePoints() - 1)
		{
			SplineComp->SetLocationAtSplinePoint(Iterator, StartLocation + PointToPointDistance, ESplineCoordinateSpace::World);
		}
		else
		{
			SplineComp->SetLocationAtSplinePoint(Iterator, StartLocation + PointToPointDistance, ESplineCoordinateSpace::Local);
		}
		StartLocation += PointToPointDistance;
	}
}

void UPowerlineToolWidget::CreateSplinePoints(USplineComponent* SplineComp)
{
	for (int32 i = 0; i < Segments - 1; i++)
	{
		FVector PointPosition;
		SplineComp->AddSplinePoint(PointPosition, ESplineCoordinateSpace::Local);
	}
}

void UPowerlineToolWidget::CreateRootComponent(AActor* CableActor)
{
	if (!CableActor->GetRootComponent())
	{
		USceneComponent* RootComp = NewObject<USceneComponent>(CableActor, TEXT("RootComponent"));
		CableActor->SetRootComponent(RootComp);
		RootComp->RegisterComponent();
		CableActor->GetRootComponent()->SetMobility(EComponentMobility::Static);
	}
}