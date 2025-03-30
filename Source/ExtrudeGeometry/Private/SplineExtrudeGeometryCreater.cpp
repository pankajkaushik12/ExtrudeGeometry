
#include "SplineExtrudeGeometryCreater.h"
#include "ProceduralMeshComponent.h"
#include "ImageUtils.h"

ASplineExtrudeGeometryCreater::ASplineExtrudeGeometryCreater()
{
	PrimaryActorTick.bCanEverTick = true;

	ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
	RootComponent = ProceduralMesh;

	SplinePoints.Empty();

	static ConstructorHelpers::FObjectFinder<UMaterial> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
	if (MaterialFinder.Succeeded())
	{
		BaseMaterial = MaterialFinder.Object;
	}
}

void ASplineExtrudeGeometryCreater::BeginPlay()
{
	Super::BeginPlay();

}

void ASplineExtrudeGeometryCreater::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ASplineExtrudeGeometryCreater::SetExtrudeGeometryProperties(FString SplineControlPointsFilePath, FString TextureFilePath, float WidthOfExtrudeGeometry_)
{
	SplineControlPointsFilePath = FPaths::Combine(FPaths::ProjectContentDir(), SplineControlPointsFilePath);
	WidthOfExtrudeGeometry = WidthOfExtrudeGeometry_;

	FString SplineControlPointsFileContent;
	if (!FFileHelper::LoadFileToString(SplineControlPointsFileContent, *SplineControlPointsFilePath)) {
		UE_LOG(LogTemp, Error, TEXT("Failed to load file: %s"), *SplineControlPointsFilePath);
		return;
	}
	TArray<FString> SplineControlPoints;
	SplineControlPointsFileContent.ParseIntoArray(SplineControlPoints, TEXT("\n"), true);

	if (SplineControlPoints.Num() == 0) {
		UE_LOG(LogTemp, Warning, TEXT("No control points found in file"));
		return;
	}

	FVector ControlPoint;
	for (int i = 0; i < SplineControlPoints.Num(); i++)
	{
		FString SplineControlPoint = SplineControlPoints[i];
		TArray<FString> SplineControlPointCoordinates;
		SplineControlPoint.ParseIntoArray(SplineControlPointCoordinates, TEXT(","), true);
		if (SplineControlPointCoordinates.Num() < 3) {
			ControlPoint = FVector(FCString::Atof(*SplineControlPointCoordinates[0]), FCString::Atof(*SplineControlPointCoordinates[1]), 100.f);
		}
		else {
			ControlPoint = FVector(FCString::Atof(*SplineControlPointCoordinates[0]), FCString::Atof(*SplineControlPointCoordinates[1]), FCString::Atof(*SplineControlPointCoordinates[2]));
		}
		ControlPoint.X *= 50.0f;
		ControlPoint.Y *= 50.0f;

		ControlPoints.Add(ControlPoint);
	}

	TextureFilePath = FPaths::Combine(FPaths::ProjectContentDir(), TextureFilePath);
	LoadedTexture = FImageUtils::ImportFileAsTexture2D(TextureFilePath);
	if (!LoadedTexture)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load texture file: %s"), *TextureFilePath);
		return;
	}

	if (BaseMaterial && LoadedTexture)
	{
		DynamicMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		DynamicMaterial->SetTextureParameterValue(FName("Param"), LoadedTexture);

		UE_LOG(LogTemp, Warning, TEXT("Material is created with texture from file: %s"), *TextureFilePath);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("BaseMaterial or LoadedTexture is null"));
	}
}

void ASplineExtrudeGeometryCreater::CreateSplinePointsFromControlPoints(const TArray<FVector>& controlPoints, int32 segmentsPerPoint)
{
	if (controlPoints.Num() < 4) return ;

	FVector P0, P1, P2, P3;

	for (int32 i = 0; i < controlPoints.Num() - 1; i++)
	{
		if (i == 0) {
			P0 = 2.0f * controlPoints[i] - controlPoints[i + 1];
			P1 = controlPoints[i];
			P2 = controlPoints[i + 1];
			P3 = controlPoints[i + 2];
		}
		else if (i == controlPoints.Num() - 2) {
			P0 = controlPoints[i - 1];
			P1 = controlPoints[i];
			P2 = controlPoints[i + 1];
			P3 = 2.0f * controlPoints[i + 1] - controlPoints[i];
		}
		else {
			P0 = controlPoints[i - 1];
			P1 = controlPoints[i];
			P2 = controlPoints[i + 1];
			P3 = controlPoints[i + 2];
		}

		for (int32 j = 0; j < segmentsPerPoint; j++)
		{
			float t = static_cast<float>(j) / segmentsPerPoint;
			float t2 = t * t;
			float t3 = t2 * t;

			FVector Point = 0.5f * (
				2.0f * P1 +
				(-P0 + P2) * t +
				(2.0f * P0 - 5.0f * P1 + 4.0f * P2 - P3) * t2 +
				(-P0 + 3.0f * P1 - 3.0f * P2 + P3) * t3
				);
			SplinePoints.Add(Point);
			SplineTangents.Add(0.5f * (
				(-P0 + P2) +
				(2.0f * P0 - 5.0f * P1 + 4.0f * P2 - P3) * 2.0f * t +
				(-P0 + 3.0f * P1 - 3.0f * P2 + P3) * 3.0f * t2
				));
		}
	}
}

void ASplineExtrudeGeometryCreater::CreateExtrudeGeometrySegment(const TArray<FVector>& splinePoints, int32 Index)
{
	FVector StartPoint = splinePoints[Index];
	FVector EndPoint = splinePoints[Index + 1];

	FVector SegmentAxis1 = SplineTangents[Index].GetSafeNormal();
	FVector RightVector1 = FVector::CrossProduct(SegmentAxis1, FVector::UpVector).GetSafeNormal();
	FVector UpVector1 = FVector::CrossProduct(RightVector1, SegmentAxis1).GetSafeNormal();

	FVector SegmentAxis2 = SplineTangents[Index + 1].GetSafeNormal();
	FVector RightVector2 = FVector::CrossProduct(SegmentAxis2, FVector::UpVector).GetSafeNormal();
	FVector UpVector2 = FVector::CrossProduct(RightVector2, SegmentAxis2).GetSafeNormal();

	TArray<FVector> Disc1Vertices, Disc2Vertices;
	TArray<FVector> Disc1Normals, Disc2Normals;

	TArray<FVector> StartDiscVertices;
	TArray<FVector> StartDiscNormals;
	TArray<int32> StartDiscTriangles;
	for (int32 i = 0; i < SidesPerSegment; i++) {
		float Angle = FMath::DegreesToRadians(i * 360.0f / SidesPerSegment);

		FVector Offset1 = (RightVector1 * FMath::Cos(Angle) + UpVector1 * FMath::Sin(Angle)) * (WidthOfExtrudeGeometry / 2.f);
		FVector Offset2 = (RightVector2 * FMath::Cos(Angle) + UpVector2 * FMath::Sin(Angle)) * (WidthOfExtrudeGeometry / 2.f);
		Disc1Vertices.Add(StartPoint + Offset1);
		Disc2Vertices.Add(EndPoint + Offset2);

		FVector Normal1 = (Offset1).GetSafeNormal();
		FVector Normal2 = (Offset2).GetSafeNormal();
		Disc1Normals.Add(Normal1);
		Disc2Normals.Add(Normal2);
	}

	if (Index == 0 || Index == SplinePoints.Num() - 2) {

		FVector Normal = Index == 0 ? -SegmentAxis1 : SegmentAxis2;
		FVector Vertex1, Vertex2, Vertex3;

		for (int32 i = 0; i < SidesPerSegment; i++) {
			int32 Next = (i + 1) % SidesPerSegment;

			Vertex1 = Index == 0 ? Disc1Vertices[i] : EndPoint;
			Vertex2 = Index == 0 ? StartPoint : Disc2Vertices[i];
			Vertex3 = Index == 0 ? Disc1Vertices[Next] : Disc2Vertices[Next];

			StartDiscVertices.Add(Vertex1);
			StartDiscVertices.Add(Vertex2);
			StartDiscVertices.Add(Vertex3);

			StartDiscNormals.Add(Normal);
			StartDiscNormals.Add(Normal);
			StartDiscNormals.Add(Normal);

			StartDiscTriangles.Add(i * 3);
			StartDiscTriangles.Add(i * 3 + 1);
			StartDiscTriangles.Add(i * 3 + 2);
		}
	}


	// Joining the two discs to create the extrude geometry
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FProcMeshTangent> Tangents;

	for (int32 i = 0; i < Disc1Vertices.Num(); i++) {
		int32 Next = (i + 1) % Disc1Vertices.Num();

		// Vertices
		Vertices.Add(Disc1Vertices[i]);
		Vertices.Add(Disc2Vertices[i]);
		Vertices.Add(Disc1Vertices[Next]);
		Vertices.Add(Disc2Vertices[Next]);

		// UVs
		float U1 = static_cast<float>(i) / Disc1Vertices.Num();
		float U2 = static_cast<float>(Next) / Disc1Vertices.Num();
		UVs.Add(FVector2D(U1, 0.0f));
		UVs.Add(FVector2D(U1, 1.f));
		UVs.Add(FVector2D(U2, 0.0f));
		UVs.Add(FVector2D(U2, 1.f));

		// Normals
		Normals.Add(Disc1Normals[i]);
		Normals.Add(Disc2Normals[i]);
		Normals.Add(Disc1Normals[Next]);
		Normals.Add(Disc2Normals[Next]);

		// Tangents
		Tangents.Add(FProcMeshTangent(SegmentAxis1, false));
		Tangents.Add(FProcMeshTangent(SegmentAxis1, false));
		Tangents.Add(FProcMeshTangent(SegmentAxis2, false));
		Tangents.Add(FProcMeshTangent(SegmentAxis2, false));

		// Triangles
		int32 BaseIndex = i * 4;
		Triangles.Add(BaseIndex);
		Triangles.Add(BaseIndex + 2);
		Triangles.Add(BaseIndex + 1);

		Triangles.Add(BaseIndex + 1);
		Triangles.Add(BaseIndex + 2);
		Triangles.Add(BaseIndex + 3);
	}

	if (Index == 0 || Index == SplinePoints.Num() - 2) {
		ProceduralMesh->CreateMeshSection(Index == 0 ? Index : Index + 2, StartDiscVertices, StartDiscTriangles, StartDiscNormals, TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), true);
		ProceduralMesh->SetMaterial(Index == 0 ? Index : Index + 2, DynamicMaterial);
	}
	ProceduralMesh->CreateMeshSection(Index + 1, Vertices, Triangles, Normals, UVs, TArray<FColor>(), Tangents, true);
	ProceduralMesh->SetMaterial(Index + 1, DynamicMaterial);
}

void ASplineExtrudeGeometryCreater::CreateExtrudeGeometry()
{
	CreateSplinePointsFromControlPoints(ControlPoints, SegmentsPerPoint);

	for (int32 i = 0; i < SplinePoints.Num() - 1; i++)
	{
		CreateExtrudeGeometrySegment(SplinePoints, i);
	}
}

void ASplineExtrudeGeometryCreater::UpdateControlPoints(const TArray<FVector>& UpdatedControlPoints)
{
	ControlPoints = UpdatedControlPoints;

	SplinePoints.Empty();
	SplineTangents.Empty();
	ProceduralMesh->ClearAllMeshSections();

	CreateExtrudeGeometry();
}
