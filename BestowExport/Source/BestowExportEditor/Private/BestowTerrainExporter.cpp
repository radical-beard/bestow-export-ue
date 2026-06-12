// E1 — Landscape → bestow terrain. Reads the live editor Landscape
// through FLandscapeEditDataInterface (heights + paint-layer weights),
// converts UE's height encoding ((h16 - 32768) / 128 * scaleZ, cm) into
// bestow's normalized 16-bit window over [height_min, height_max] meters,
// and writes the same self-contained folder the Godot addon produces.

#include "BestowExporters.h"

#include "EngineUtils.h"
#include "Interfaces/IPluginManager.h"
#include "Landscape.h"
#include "LandscapeDataAccess.h"
#include "LandscapeEdit.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeProxy.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	FString ShaderTemplate(FString& OutError)
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BestowExport"));
		if (!Plugin.IsValid())
		{
			OutError = TEXT("BestowExport plugin not found by the plugin manager");
			return FString();
		}
		// Resources/ because Epic's BuildPlugin packaging stages only the
		// standard plugin folders — a bare templates/ dir would be dropped.
		const FString Path = Plugin->GetBaseDir() / TEXT("Resources/templates/terrain_baked.slang");
		FString Template;
		if (!FFileHelper::LoadFileToString(Template, *Path))
		{
			OutError = FString::Printf(TEXT("missing shader template at %s — reinstall the plugin"), *Path);
		}
		return Template;
	}
}

FBestowExportResult BestowExporters::ExportTerrain(
	UWorld* World, const FString& GameRoot, const FString& RawName)
{
	if (!World)
	{
		return FBestowExportResult::Fail(TEXT("No editor world."));
	}

	ALandscapeProxy* Proxy = nullptr;
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		Proxy = *It;
		break;
	}
	if (!Proxy)
	{
		return FBestowExportResult::Fail(
			TEXT("This level has no Landscape — open a level with terrain (Landscape mode creates one)."));
	}
	ULandscapeInfo* Info = Proxy->GetLandscapeInfo();
	if (!Info)
	{
		return FBestowExportResult::Fail(TEXT("The Landscape has no LandscapeInfo yet — save the level once."));
	}

	int32 MinX = 0, MinY = 0, MaxX = 0, MaxY = 0;
	if (!Info->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
	{
		return FBestowExportResult::Fail(TEXT("The Landscape has no components yet — sculpt something first."));
	}

	const int32 UeW = MaxX - MinX + 1; // vertices along UE X
	const int32 UeH = MaxY - MinY + 1; // vertices along UE Y

	// ── heights ───────────────────────────────────────────────────────────
	TArray<uint16> UeHeights;
	UeHeights.SetNumZeroed(UeW * UeH);
	{
		FLandscapeEditDataInterface EditData(Info, /*bUploadTextureChangesToGPU*/ false);
		int32 X1 = MinX, Y1 = MinY, X2 = MaxX, Y2 = MaxY;
		EditData.GetHeightData(X1, Y1, X2, Y2, UeHeights.GetData(), UeW);
	}

	const FTransform LandscapeXf = Proxy->LandscapeActorToWorld();
	const FVector Scale = LandscapeXf.GetScale3D();
	const FVector OriginUe = LandscapeXf.TransformPosition(FVector(MinX, MinY, 0));

	// bestow grid: x ↔ UE X, z ↔ UE Y (Epic's (X, Z, Y) axis convention),
	// square padded to the larger side; padding reads height_min.
	const int32 Side = FMath::Max(UeW, UeH);
	const double SpacingM = Scale.X * 0.01; // assume uniform XY (warned below)
	const double SizeM = (Side - 1) * SpacingM;

	double HMin = TNumericLimits<double>::Max();
	double HMax = -TNumericLimits<double>::Max();
	TArray<double> HeightsM;
	HeightsM.SetNumZeroed(Side * Side);
	TArray<bool> Valid;
	Valid.Init(false, Side * Side);
	for (int32 Uy = 0; Uy < UeH; Uy++)
	{
		for (int32 Ux = 0; Ux < UeW; Ux++)
		{
			const uint16 H16 = UeHeights[Uy * UeW + Ux];
			const double LocalZ = LandscapeDataAccess::GetLocalHeight(H16);
			const double WorldZCm = OriginUe.Z + LocalZ * Scale.Z;
			const double M = WorldZCm * 0.01;
			const int32 Bi = Uy * Side + Ux; // bz = Uy, bx = Ux
			HeightsM[Bi] = M;
			Valid[Bi] = true;
			HMin = FMath::Min(HMin, M);
			HMax = FMath::Max(HMax, M);
		}
	}
	if (HMax - HMin < 1e-3)
	{
		HMax = HMin + 1.0;
	}

	TArray<uint16> Quantized;
	Quantized.SetNumZeroed(Side * Side);
	const double Range = HMax - HMin;
	for (int32 I = 0; I < HeightsM.Num(); I++)
	{
		const double H = Valid[I] ? HeightsM[I] : HMin;
		Quantized[I] = (uint16)FMath::RoundToInt(FMath::Clamp((H - HMin) / Range, 0.0, 1.0) * 65535.0);
	}

	// ── paint layers: rockness collapse + lossless per-layer maps ────────
	FBestowExportResult Result;
	TArray<uint8> Rockness;
	Rockness.SetNumZeroed(Side * Side);
	TArray<uint8> Painted; // any-layer-painted mask (autoshader = NOT painted)
	Painted.SetNumZeroed(Side * Side);
	TArray<TPair<FName, TArray<uint8>>> LayerMaps;
	{
		FLandscapeEditDataInterface EditData(Info, false);
		for (const FLandscapeInfoLayerSettings& Layer : Info->Layers)
		{
			if (!Layer.LayerInfoObj)
			{
				continue;
			}
			TArray<uint8> Weights;
			Weights.SetNumZeroed(UeW * UeH);
			EditData.GetWeightDataFast(Layer.LayerInfoObj, MinX, MinY, MaxX, MaxY,
				Weights.GetData(), UeW);

			const bool bRock = Layer.LayerName.ToString().Contains(TEXT("rock"), ESearchCase::IgnoreCase);
			TArray<uint8> Resampled;
			Resampled.SetNumZeroed(Side * Side);
			for (int32 Uy = 0; Uy < UeH; Uy++)
			{
				for (int32 Ux = 0; Ux < UeW; Ux++)
				{
					const uint8 W = Weights[Uy * UeW + Ux];
					const int32 Bi = Uy * Side + Ux;
					Resampled[Bi] = W;
					if (W > 0)
					{
						Painted[Bi] = 255;
						if (bRock)
						{
							Rockness[Bi] = (uint8)FMath::Min(255, Rockness[Bi] + W);
						}
					}
				}
			}
			LayerMaps.Emplace(Layer.LayerName, MoveTemp(Resampled));
		}
	}
	if (LayerMaps.Num() == 0)
	{
		Result.Warnings.Add(TEXT("No paint layers on the Landscape — the whole terrain exports as "
			"slope-driven material (autoshader)."));
	}
	else if (!LayerMaps.ContainsByPredicate([](const TPair<FName, TArray<uint8>>& L) {
				 return L.Key.ToString().Contains(TEXT("rock"), ESearchCase::IgnoreCase);
			 }))
	{
		Result.Warnings.Add(TEXT("No paint layer name contains \"rock\" — painted areas export as "
			"grass. Name a layer e.g. `Rock` to drive the rock material."));
	}

	// ── write the folder ──────────────────────────────────────────────────
	const FString Name = BestowToml::Stem(RawName);
	const FString Prefix = FString::Printf(TEXT("assets/terrain/%s"), *Name);
	const FString Folder = GameRoot / TEXT("assets/terrain") / Name;
	FString Err;

	auto Track = [&Result](const FString& Path) {
		Result.Files.Add(Path);
		Result.Files.Add(BestowFiles::EnsureSidecar(Path));
	};

	const FString HgtPath = Folder / FString::Printf(TEXT("%s.hgt.png"), *Name);
	if (!BestowFiles::WritePng16(HgtPath, Side, Side, Quantized, Err))
	{
		return FBestowExportResult::Fail(Err);
	}
	Track(HgtPath);

	// bestow world placement: bestow.x = UE.X m, bestow.z = UE.Y m
	const double BOriginX = OriginUe.X * 0.01;
	const double BOriginZ = OriginUe.Y * 0.01;
	const FString Meta = FString::Printf(
		TEXT("# Generated by BestowExport (Unreal) — placement metadata for %s.hgt.png\n")
		TEXT("# (16-bit grayscale heightfield sampled from the live Landscape).\n\n")
		TEXT("width = %d\nheight = %d\n")
		TEXT("world_origin_x = %s\nworld_origin_z = %s\n")
		TEXT("world_size_x = %s\nworld_size_z = %s\n")
		TEXT("height_min = %s\nheight_max = %s\n"),
		*Name, Side, Side,
		*BestowToml::F(BOriginX), *BestowToml::F(BOriginZ),
		*BestowToml::F(SizeM), *BestowToml::F(SizeM),
		*BestowToml::F(HMin), *BestowToml::F(HMax));
	const FString MetaPath = Folder / FString::Printf(TEXT("%s.hgt.toml"), *Name);
	if (!BestowFiles::WriteAtomicText(MetaPath, Meta, Err))
	{
		return FBestowExportResult::Fail(Err);
	}
	Track(MetaPath);

	// ctl.png: RGBA8 (R = rockness, G = autoshader flag where unpainted)
	TArray<uint8> Ctl;
	Ctl.SetNumZeroed(Side * Side * 4);
	for (int32 I = 0; I < Side * Side; I++)
	{
		Ctl[I * 4 + 0] = Rockness[I];
		Ctl[I * 4 + 1] = Painted[I] ? 0 : 255;
		Ctl[I * 4 + 3] = 255;
	}
	const FString CtlPath = Folder / FString::Printf(TEXT("%s.ctl.png"), *Name);
	if (!BestowFiles::WritePngRgba8(CtlPath, Side, Side, Ctl, Err))
	{
		return FBestowExportResult::Fail(Err);
	}
	Track(CtlPath);

	// lossless per-layer weightmaps
	for (const TPair<FName, TArray<uint8>>& Layer : LayerMaps)
	{
		const FString LayerPath = Folder /
			FString::Printf(TEXT("%s.layer.%s.png"), *Name, *BestowToml::Stem(Layer.Key.ToString()));
		if (BestowFiles::WritePngGray8(LayerPath, Side, Side, Layer.Value, Err))
		{
			Track(LayerPath);
		}
	}

	// flat detail textures (UE landscape textures live in materials —
	// replace these PNGs with your real grass/rock albedos any time)
	{
		auto Flat = [&](const TCHAR* File, uint8 R, uint8 G, uint8 B) {
			TArray<uint8> Px;
			Px.SetNumZeroed(4 * 4 * 4);
			for (int32 I = 0; I < 16; I++)
			{
				Px[I * 4] = R;
				Px[I * 4 + 1] = G;
				Px[I * 4 + 2] = B;
				Px[I * 4 + 3] = 255;
			}
			const FString P = Folder / File;
			if (!FPaths::FileExists(P)) // user replacements survive re-export
			{
				FString E2;
				if (BestowFiles::WritePngRgba8(P, 4, 4, Px, E2))
				{
					Track(P);
				}
			}
			else
			{
				Track(P);
			}
		};
		Flat(TEXT("grass.png"), 77, 133, 56);
		Flat(TEXT("rock.png"), 115, 107, 102);
		Result.Warnings.Add(TEXT("grass.png / rock.png are flat placeholders (Landscape detail "
			"textures live inside materials) — drop real albedos over them; re-exports keep yours."));
	}

	// shader: footprint from params, shared with the Godot exporter
	const FString Shader = ShaderTemplate(Err);
	if (Shader.IsEmpty())
	{
		return FBestowExportResult::Fail(Err);
	}
	const FString ShaderPath = Folder / TEXT("terrain_baked.slang");
	if (!BestowFiles::WriteAtomicText(ShaderPath, Shader, Err))
	{
		return FBestowExportResult::Fail(Err);
	}
	Track(ShaderPath);

	// entity snippet: centred placement matching the authored UE world spot
	const double CenterX = BOriginX + SizeM * 0.5;
	const double CenterZ = BOriginZ + SizeM * 0.5;
	const int32 Resolution = FMath::Min(1024, Side);
	const FString Entity = FString::Printf(
		TEXT("# Paste into a scene to place `%s` exactly where it sat in Unreal.\n")
		TEXT("[[entities]]\nname = \"%s\"\n")
		TEXT("[entities.components.transform]\nposition = %s\n")
		TEXT("[entities.components.render]\nmodel = \"terrain\"\n")
		TEXT("shader = \"%s/terrain_baked.slang\"\n")
		TEXT("texture = \"%s/%s.ctl.png\"\n")
		TEXT("texture2 = \"%s/grass.png\"\n")
		TEXT("texture3 = \"%s/rock.png\"\n")
		TEXT("color = [1.0, 1.0, 1.0, 1.0]\n")
		TEXT("params = %s\n")
		TEXT("[entities.components.terrain]\n")
		TEXT("heightmap = \"%s/%s.hgt.png\"\n")
		TEXT("size = %s\nheight_min = %s\nheight_max = %s\nresolution = %d\n"),
		*Name, *BestowToml::Esc(Name),
		*BestowToml::V3(CenterX, 0.0, CenterZ),
		*Prefix, *Prefix, *Name, *Prefix, *Prefix,
		*BestowToml::V4(BOriginX, BOriginZ, SizeM, SizeM),
		*Prefix, *Name,
		*BestowToml::F(SizeM), *BestowToml::F(HMin), *BestowToml::F(HMax), Resolution);
	const FString EntityPath = Folder / FString::Printf(TEXT("%s.entity.toml"), *Name);
	if (!BestowFiles::WriteAtomicText(EntityPath, Entity, Err))
	{
		return FBestowExportResult::Fail(Err);
	}
	Track(EntityPath);

	if (!FMath::IsNearlyEqual(Scale.X, Scale.Y, 0.01))
	{
		Result.Warnings.Add(TEXT("Landscape X/Y scales differ — bestow terrain is square-celled; "
			"the X scale was used."));
	}

	Result.bOk = true;
	Result.Folder = Folder;
	Result.EntityToml = Entity;
	return Result;
}
