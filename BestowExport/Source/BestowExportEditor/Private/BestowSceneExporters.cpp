// E2 (mesh via the engine glTF exporter), E3+E6 (level layout + sky),
// E8 (splines). Placement mapping: TargetPoints are bestow's tag-only
// markers; actor Tags cross as bestow tags; a tag `template:<name>`
// instances a bestow template. Behavior never crosses — bestow Lua owns it.

#include "BestowExporters.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/PointLight.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Exporters/GLTFExporter.h"
#include "GameFramework/Actor.h"
#include "Misc/Paths.h"
#include "Options/GLTFExportOptions.h"
#include "UObject/Package.h"

namespace
{
	/// Actor name → unique bestow stem within one scene.
	FString UniqueName(const FString& Raw, TSet<FString>& Taken)
	{
		FString Stem = BestowToml::Stem(Raw);
		if (!Taken.Contains(Stem))
		{
			Taken.Add(Stem);
			return Stem;
		}
		for (int32 I = 2;; I++)
		{
			const FString Candidate = FString::Printf(TEXT("%s_%d"), *Stem, I);
			if (!Taken.Contains(Candidate))
			{
				Taken.Add(Candidate);
				return Candidate;
			}
		}
	}

	/// Transform block (position + Euler XYZ radians) for an actor.
	FString TransformBlock(const AActor* Actor)
	{
		const FVector P = BestowConvert::Pos(Actor->GetActorLocation());
		const FVector E = BestowConvert::EulerXYZ(Actor->GetActorQuat());
		FString S = TEXT("[entities.components.transform]\n");
		S += FString::Printf(TEXT("position = %s\n"), *BestowToml::V3(P.X, P.Y, P.Z));
		if (!E.IsNearlyZero(1e-5))
		{
			S += FString::Printf(TEXT("rotation = %s\n"), *BestowToml::V3(E.X, E.Y, E.Z));
		}
		const FVector Sc = Actor->GetActorScale3D();
		if (!Sc.Equals(FVector::OneVector, 1e-5))
		{
			S += FString::Printf(TEXT("scale = %s\n"), *BestowToml::V3(Sc.X, Sc.Z, Sc.Y));
		}
		return S;
	}

	/// Tags + optional template from AActor::Tags (`template:<x>` wins).
	void TagsOf(const AActor* Actor, TArray<FString>& OutTags, FString& OutTemplate)
	{
		for (const FName& Tag : Actor->Tags)
		{
			const FString T = Tag.ToString();
			if (T.StartsWith(TEXT("template:")))
			{
				OutTemplate = T.Mid(9);
			}
			else if (!T.IsEmpty())
			{
				OutTags.Add(T);
			}
		}
	}

	FString TagList(const TArray<FString>& Tags)
	{
		TArray<FString> Quoted;
		for (const FString& T : Tags)
		{
			Quoted.Add(FString::Printf(TEXT("\"%s\""), *BestowToml::Esc(T)));
		}
		return FString::Join(Quoted, TEXT(", "));
	}
}

// ── E2: selected actors → glb ────────────────────────────────────────────

FBestowExportResult BestowExporters::ExportMesh(
	UWorld* World, const TSet<AActor*>& Selected, const FString& GameRoot, const FString& RawName)
{
	if (!World)
	{
		return FBestowExportResult::Fail(TEXT("No editor world."));
	}
	if (Selected.Num() == 0)
	{
		return FBestowExportResult::Fail(
			TEXT("Select one or more actors in the level first (their meshes become the glb)."));
	}

	const FString Name = BestowToml::Stem(RawName);
	const FString Prefix = FString::Printf(TEXT("assets/models/%s"), *Name);
	const FString Folder = GameRoot / TEXT("assets/models") / Name;
	IFileManager::Get().MakeDirectory(*Folder, true);
	const FString GlbPath = Folder / FString::Printf(TEXT("%s.glb"), *Name);

	UGLTFExportOptions* Options = NewObject<UGLTFExportOptions>();
	Options->ResetToDefault();

	FGLTFExportMessages Messages;
	if (!UGLTFExporter::ExportToGLTF(World, GlbPath, Options, Selected, Messages))
	{
		FString Why = FString::Join(Messages.Errors, TEXT("; "));
		return FBestowExportResult::Fail(FString::Printf(
			TEXT("glb export failed%s%s"), Why.IsEmpty() ? TEXT("") : TEXT(": "), *Why));
	}

	FBestowExportResult Result;
	for (const FString& W : Messages.Warnings)
	{
		Result.Warnings.Add(W);
	}
	Result.Files.Add(GlbPath);
	Result.Files.Add(BestowFiles::EnsureSidecar(GlbPath));

	// The glb bakes world placement (level export), so the entity sits at
	// the origin; move it in bestow via this transform if needed.
	const FString Entity = FString::Printf(
		TEXT("# Paste into a scene to place `%s` (world placement is baked into the glb).\n")
		TEXT("[[entities]]\nname = \"%s\"\n")
		TEXT("[entities.components.transform]\nposition = [0.0, 0.0, 0.0]\n")
		TEXT("[entities.components.render]\nmodel = \"%s/%s.glb\"\n")
		TEXT("[entities.components.physics]\nbody = \"static\"\nshape = \"mesh\"\n"),
		*Name, *BestowToml::Esc(Name), *Prefix, *Name);
	const FString EntityPath = Folder / FString::Printf(TEXT("%s.entity.toml"), *Name);
	FString Err;
	if (!BestowFiles::WriteAtomicText(EntityPath, Entity, Err))
	{
		return FBestowExportResult::Fail(Err);
	}
	Result.Files.Add(EntityPath);
	Result.Files.Add(BestowFiles::EnsureSidecar(EntityPath));

	Result.bOk = true;
	Result.Folder = Folder;
	Result.EntityToml = Entity;
	return Result;
}

// ── E3 + E6: level layout → scene.toml ──────────────────────────────────

FBestowExportResult BestowExporters::ExportScene(
	UWorld* World, const FString& GameRoot, const FString& RawName)
{
	if (!World)
	{
		return FBestowExportResult::Fail(TEXT("No editor world."));
	}

	const FString Name = BestowToml::Stem(RawName);
	FString S = FString::Printf(
		TEXT("# Scene `%s` — exported by BestowExport from the Unreal level.\n")
		TEXT("# Play it:  scene.load(\"scenes/%s.scene.toml\")\n\n"),
		*Name, *Name);

	FBestowExportResult Result;
	TSet<FString> Taken;
	Taken.Add(TEXT("sky"));
	Taken.Add(TEXT("shell"));
	int32 Count = 0;
	int32 Lights = 0;

	// sky entity from the sun + height fog (E6)
	{
		ADirectionalLight* Sun = nullptr;
		AExponentialHeightFog* Fog = nullptr;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (!Sun)
			{
				Sun = Cast<ADirectionalLight>(*It);
			}
			if (!Fog)
			{
				Fog = Cast<AExponentialHeightFog>(*It);
			}
		}
		if (Sun || Fog)
		{
			// lights shine along +X forward; direction TO the sun is the
			// opposite of where it points
			const FVector SunDirUe = Sun ? -Sun->GetActorForwardVector() : FVector(0.3, 0.3, 0.8);
			const FVector D = BestowConvert::Dir(SunDirUe);
			FLinearColor SunColor = FLinearColor(1.0f, 0.97f, 0.9f);
			if (Sun)
			{
				if (const ULightComponent* LC = Sun->GetLightComponent())
				{
					SunColor = LC->GetLightColor();
				}
			}
			S += TEXT("[[entities]]\nname = \"sky\"\n[entities.components.sky]\n");
			S += TEXT("horizon = [0.78, 0.82, 0.85]\nzenith = [0.25, 0.45, 0.72]\n");
			S += FString::Printf(TEXT("sun_dir = %s\n"), *BestowToml::V3(D.X, D.Y, D.Z));
			S += FString::Printf(TEXT("sun_color = %s\n"),
				*BestowToml::V3(SunColor.R, SunColor.G, SunColor.B));
			S += TEXT("sun_size = 0.035\n");
			if (Fog)
			{
				if (const UExponentialHeightFogComponent* FC = Fog->GetComponent())
				{
					const FLinearColor FogColor = FC->FogInscatteringLuminance;
					S += FString::Printf(TEXT("fog_color = %s\n"),
						*BestowToml::V3(FogColor.R, FogColor.G, FogColor.B));
					// UE fog density is per-cm-ish at small magnitudes;
					// scale to bestow's per-meter attenuation
					S += FString::Printf(TEXT("fog_density = %s\n"),
						*BestowToml::F(FC->FogDensity * 0.1));
				}
			}
			S += TEXT("\n");
			Count++;
		}
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		// IsHiddenEd = hidden in the EDITOR viewport; markers like
		// TargetPoint are hidden-in-game by design and must still export
		if (Actor->IsHiddenEd() || Actor->GetActorLabel().StartsWith(TEXT("_")))
		{
			continue;
		}

		TArray<FString> Tags;
		FString Template;
		TagsOf(Actor, Tags, Template);

		const bool bMarker = Actor->IsA<ATargetPoint>();
		const bool bLight = Actor->IsA<APointLight>();
		const bool bCamera = Actor->IsA<ACameraActor>();
		if (!bMarker && !bLight && !bCamera && Template.IsEmpty() && Tags.Num() == 0)
		{
			continue; // plain geometry/utility — not a placement
		}

		const FString EntityName = UniqueName(Actor->GetActorLabel(), Taken);
		S += TEXT("[[entities]]\n");
		S += FString::Printf(TEXT("name = \"%s\"\n"), *BestowToml::Esc(EntityName));
		if (!Template.IsEmpty())
		{
			S += FString::Printf(TEXT("template = \"%s\"\n"), *BestowToml::Esc(Template));
		}
		if (Tags.Num() > 0)
		{
			S += FString::Printf(TEXT("tags = [%s]\n"), *TagList(Tags));
		}
		S += TransformBlock(Actor);

		if (const APointLight* Light = Cast<APointLight>(Actor))
		{
			Lights++;
			if (Lights == 9)
			{
				Result.Warnings.Add(TEXT("More than 8 point lights — bestow renders the first 8 "
					"only (engine cap)."));
			}
			if (const UPointLightComponent* LC = Cast<UPointLightComponent>(Light->GetLightComponent()))
			{
				const FLinearColor C = LC->GetLightColor();
				S += TEXT("[entities.components.light]\n");
				S += FString::Printf(TEXT("color = %s\n"), *BestowToml::V4(C.R, C.G, C.B, 1.0));
				S += FString::Printf(TEXT("intensity = %s\n"), *BestowToml::F(LC->Intensity));
				S += FString::Printf(TEXT("range = %s\n"),
					*BestowToml::F(LC->AttenuationRadius * 0.01));
			}
		}

		if (const ACameraActor* Camera = Cast<ACameraActor>(Actor))
		{
			if (const UCameraComponent* CC = Camera->GetCameraComponent())
			{
				S += TEXT("[entities.components.camera]\n");
				S += FString::Printf(TEXT("fov = %s\n"),
					*BestowToml::F(FMath::DegreesToRadians(CC->FieldOfView)));
			}
		}

		S += TEXT("\n");
		Count++;
	}

	if (Count == 0)
	{
		return FBestowExportResult::Fail(
			TEXT("Nothing exportable found: add Target Point actors (markers), point lights, a "
				 "camera, or set actor tags (a tag `template:<name>` places a bestow template)."));
	}

	const FString Dir = GameRoot / TEXT("scenes");
	const FString ScenePath = Dir / FString::Printf(TEXT("%s.scene.toml"), *Name);
	FString Err;
	if (!BestowFiles::WriteAtomicText(ScenePath, S, Err))
	{
		return FBestowExportResult::Fail(Err);
	}
	Result.Files.Add(ScenePath);
	Result.Files.Add(BestowFiles::EnsureSidecar(ScenePath));
	Result.bOk = true;
	Result.Folder = Dir;
	Result.EntityToml = FString::Printf(TEXT("scene.load(\"scenes/%s.scene.toml\")"), *Name);
	return Result;
}

// ── E8: splines ───────────────────────────────────────────────────────────

FBestowExportResult BestowExporters::ExportSplines(UWorld* World, const FString& GameRoot)
{
	if (!World)
	{
		return FBestowExportResult::Fail(TEXT("No editor world."));
	}

	FBestowExportResult Result;
	const FString Dir = GameRoot / TEXT("assets/splines");
	TSet<FString> Taken;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const USplineComponent* Spline = It->FindComponentByClass<USplineComponent>();
		if (!Spline || Spline->GetNumberOfSplinePoints() < 2)
		{
			continue;
		}
		const FString Name = UniqueName(It->GetActorLabel(), Taken);

		FString S = FString::Printf(
			TEXT("# Spline `%s` — exported from Unreal actor `%s`.\n")
			TEXT("# Engine-pending: bestow has no native spline consumer yet; the baked\n")
			TEXT("# polyline below is directly usable from game Lua.\n\n")
			TEXT("[spline]\nname = \"%s\"\nlength = %s\n\n"),
			*Name, *It->GetActorLabel(), *BestowToml::Esc(Name),
			*BestowToml::F(Spline->GetSplineLength() * 0.01));

		const int32 Points = Spline->GetNumberOfSplinePoints();
		for (int32 I = 0; I < Points; I++)
		{
			const FVector P = BestowConvert::Pos(
				Spline->GetLocationAtSplinePoint(I, ESplineCoordinateSpace::World));
			const FVector TIn = BestowConvert::Pos(
				Spline->GetArriveTangentAtSplinePoint(I, ESplineCoordinateSpace::World)) ;
			const FVector TOut = BestowConvert::Pos(
				Spline->GetLeaveTangentAtSplinePoint(I, ESplineCoordinateSpace::World));
			S += TEXT("[[spline.points]]\n");
			S += FString::Printf(TEXT("position = %s\n"), *BestowToml::V3(P.X, P.Y, P.Z));
			S += FString::Printf(TEXT("in = %s\n"), *BestowToml::V3(TIn.X, TIn.Y, TIn.Z));
			S += FString::Printf(TEXT("out = %s\n\n"), *BestowToml::V3(TOut.X, TOut.Y, TOut.Z));
		}

		S += TEXT("# baked polyline, 1 m spacing\nbaked = [\n");
		const float LengthCm = Spline->GetSplineLength();
		for (float D = 0; D <= LengthCm; D += 100.0f)
		{
			const FVector P = BestowConvert::Pos(
				Spline->GetLocationAtDistanceAlongSpline(D, ESplineCoordinateSpace::World));
			S += FString::Printf(TEXT("  %s,\n"), *BestowToml::V3(P.X, P.Y, P.Z));
		}
		S += TEXT("]\n");

		const FString Path = Dir / FString::Printf(TEXT("%s.spline.toml"), *Name);
		FString Err;
		if (!BestowFiles::WriteAtomicText(Path, S, Err))
		{
			return FBestowExportResult::Fail(Err);
		}
		Result.Files.Add(Path);
		Result.Files.Add(BestowFiles::EnsureSidecar(Path));
	}

	if (Result.Files.Num() == 0)
	{
		return FBestowExportResult::Fail(
			TEXT("No spline actors in this level — add an actor with a Spline component."));
	}
	Result.bOk = true;
	Result.Folder = Dir;
	Result.EntityToml = TEXT("-- read from Lua: assets/splines/<name>.spline.toml");
	Result.Warnings.Add(TEXT("Splines are engine-pending in bestow — the files are data for game "
		"Lua today."));
	return Result;
}
