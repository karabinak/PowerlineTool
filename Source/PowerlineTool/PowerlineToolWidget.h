///////////////////////////////////////////
////	
////	Author: Kamil Mamcarczyk
////	Date: 03.06.24
////
///////////////////////////////////////////

#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"
#include "PowerlineToolWidget.generated.h"

class UEditorUtilityButton;
class UEditorUtilityCheckBox;
class UEditorUtilitySlider;
class USplineComponent;

UCLASS()
class POWERLINETOOL_API UPowerlineToolWidget : public UEditorUtilityWidget
{
	GENERATED_BODY()

public:
	UPowerlineToolWidget();

	UPROPERTY(meta = (BindWidget))
	UEditorUtilityButton* CreatePowerlinesButton;

	UPROPERTY(meta = (BindWidget))
	UEditorUtilityButton* RegenerateSelectedMeshButton;

	UPROPERTY(meta = (BindWidget))
	UEditorUtilityCheckBox* AttachCheckBox;

	UPROPERTY(meta = (BindWidget))
	UEditorUtilitySlider* SegmentsSlider;

	UPROPERTY(meta = (BindWidget))
	UEditorUtilitySlider* CurveSlider;
	

protected:
	UFUNCTION()
	void OnSegmentSliderChanged(float Value);

	UFUNCTION(BlueprintImplementableEvent)
	void ChangeSegmentText(int32 Value);

	UFUNCTION()
	void OnCurveSliderChanged(float Value);

	UFUNCTION(BlueprintImplementableEvent)
	void ChangeCurveText(float Value);


	UFUNCTION()
	void OnCheckboxChanged(bool bIsChecked);


	UFUNCTION()
	void RegenerateSelectedMesh();

	UFUNCTION()
	void CreatePowerlines();

	void ClearData();

	bool SelectedObjects();
	bool SocketAmount(AActor* Object);
	void SocketLocation(FPermissionListOwners& SocketsName, UStaticMeshComponent* MeshComponent);
	USplineComponent* CreateSplineComponent(AActor* CableActor);
	void CreateSplineMeshComponents(USplineComponent* SplineComp, AActor* CableActor);
	void SetPointsCurve(USplineComponent* SplineComp);
	void SetSplinePointsLocation(USplineComponent* SplineComp, int32 Index);
	void CreateSplinePoints(USplineComponent* SplineComp);
	void CreateRootComponent(AActor* CableActor);

	virtual void NativeConstruct() override;

private:
	UPROPERTY(VisibleAnywhere)
	TArray<FVector> ObjectsLocations;
	UPROPERTY(VisibleAnywhere)
	TArray<AActor*> ObjectSelection;
	UPROPERTY(VisibleAnywhere)
	int32 Segments = 10;
	UPROPERTY(VisibleAnywhere)
	float Elevation = 20.f;
	UPROPERTY(EditAnywhere)
	UStaticMesh* CableMesh;
	UPROPERTY(VisibleAnywhere)
	bool bAttachToSockets = false;

	UPROPERTY(VisibleAnywhere)
	int32 AmountOfSockets = 0;
	UPROPERTY(VisibleAnywhere)
	TArray<FVector> SocketsLocations;
	
};
