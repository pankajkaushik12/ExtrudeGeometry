// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SplineExtrudeGeometryCreater.generated.h"

UCLASS()
class EXTRUDEGEOMETRY_API ASplineExtrudeGeometryCreater : public AActor
{
	GENERATED_BODY()
	
public:	
	ASplineExtrudeGeometryCreater();

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;


private: 
	float WidthOfExtrudeGeometry;
	TArray<FVector> SplinePoints;
	TArray<FVector> SplineTangents;

	UMaterialInterface* BaseMaterial;
	UTexture2D* LoadedTexture;

	void CreateSplinePointsFromControlPoints(const TArray<FVector>& controlPoints, int32 SegmentsPerPoint);

	void CreateExtrudeGeometrySegment(const TArray<FVector>& SplinePoints, int32 Index);

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	class UProceduralMeshComponent* ProceduralMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UMaterialInstanceDynamic* DynamicMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExtrudeGeometry")
	int32 SidesPerSegment = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExtrudeGeometry")
	int32 SegmentsPerPoint = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExtrudeGeometry")
	TArray<FVector> ControlPoints;

	UFUNCTION(BlueprintCallable, Category = "ExtrudeGeometry")
	void SetExtrudeGeometryProperties(FString SplineControlPointsFilePath, FString TextureFilePath, float WidthOfExtrudeGeometry);

	UFUNCTION(BlueprintCallable, Category = "ExtrudeGeometry")
	void CreateExtrudeGeometry();

	UFUNCTION(BlueprintCallable, Category = "Update Geometry")
	void UpdateControlPoints(const TArray<FVector>& UpdatedControlPoints);

	UFUNCTION(BlueprintCallable, Category = "Update Geometry")
	void UpdateExtrudeGeometryWidth(float Width);
};
