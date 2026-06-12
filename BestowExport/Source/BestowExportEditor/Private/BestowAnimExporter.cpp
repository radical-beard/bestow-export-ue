// E5 — Animation metadata. UE's animation tools are the timeline; bestow
// plays the clip FILES, so what crosses is metadata:
//
//   AnimSequence NOTIFIES      → [[events]]   (name + trigger time)
//   Montage SECTIONS           → [[sections]] (a section runs to the next
//                                 one; name ending `!` or `_exit` sets
//                                 can_end — the player may cancel there)
//   Skeletal-mesh SOCKETS      → [[animation.sockets]]
//
// Sidecars are named after the clip: animation asset `idle` →
// `assets/anims/idle.fbx.anim.toml` (the bestow convention where each
// clip ships as its own fbx named after itself). bestow fires the events
// as `anim.event` and plays sections via anim.play{ section = "name" }.

#include "BestowExporters.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

namespace
{
	bool SectionCanEnd(FString& InOutName)
	{
		if (InOutName.EndsWith(TEXT("!")))
		{
			InOutName.LeftChopInline(1);
			return true;
		}
		if (InOutName.EndsWith(TEXT("_exit")))
		{
			InOutName.LeftChopInline(5);
			return true;
		}
		return false;
	}
}

FBestowExportResult BestowExporters::ExportAnimationMeta(const FString& GameRoot)
{
	FBestowExportResult Result;
	const FString Dir = GameRoot / TEXT("assets/anims");

	const FAssetRegistryModule& Registry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	int32 ClipsWithMeta = 0;
	int32 SocketMeshes = 0;

	// ── AnimSequences: notifies → events ─────────────────────────────────
	TArray<FAssetData> Sequences;
	Registry.Get().GetAssetsByClass(UAnimSequence::StaticClass()->GetClassPathName(), Sequences);
	for (const FAssetData& Asset : Sequences)
	{
		if (!Asset.PackageName.ToString().StartsWith(TEXT("/Game/")))
		{
			continue; // project content only, never engine assets
		}
		const UAnimSequence* Seq = Cast<UAnimSequence>(Asset.GetAsset());
		if (!Seq || Seq->Notifies.Num() == 0)
		{
			continue;
		}

		TArray<TPair<FString, double>> Events;
		for (const FAnimNotifyEvent& Notify : Seq->Notifies)
		{
			Events.Emplace(Notify.NotifyName.ToString(), (double)Notify.GetTriggerTime());
		}
		Events.Sort([](const TPair<FString, double>& A, const TPair<FString, double>& B) {
			return A.Value < B.Value;
		});

		FString S = FString::Printf(
			TEXT("# Events for clip `%s` — exported from Unreal anim notifies.\n"),
			*Seq->GetName());
		for (const TPair<FString, double>& E : Events)
		{
			S += FString::Printf(TEXT("\n[[events]]\nname = \"%s\"\ntime = %s\n"),
				*BestowToml::Esc(E.Key), *BestowToml::F(E.Value));
		}

		const FString Path = Dir /
			FString::Printf(TEXT("%s.fbx.anim.toml"), *BestowToml::Stem(Seq->GetName()));
		FString Err;
		if (!BestowFiles::WriteAtomicText(Path, S, Err))
		{
			return FBestowExportResult::Fail(Err);
		}
		Result.Files.Add(Path);
		Result.Files.Add(BestowFiles::EnsureSidecar(Path));
		ClipsWithMeta++;
	}

	// ── Montages: sections (+ montage notifies as events) ────────────────
	TArray<FAssetData> Montages;
	Registry.Get().GetAssetsByClass(UAnimMontage::StaticClass()->GetClassPathName(), Montages);
	for (const FAssetData& Asset : Montages)
	{
		if (!Asset.PackageName.ToString().StartsWith(TEXT("/Game/")))
		{
			continue;
		}
		const UAnimMontage* Montage = Cast<UAnimMontage>(Asset.GetAsset());
		if (!Montage || (Montage->CompositeSections.Num() == 0 && Montage->Notifies.Num() == 0))
		{
			continue;
		}

		FString S = FString::Printf(
			TEXT("# Events/sections for clip `%s` — exported from an Unreal montage.\n"),
			*Montage->GetName());

		TArray<TPair<FString, double>> Events;
		for (const FAnimNotifyEvent& Notify : Montage->Notifies)
		{
			Events.Emplace(Notify.NotifyName.ToString(), (double)Notify.GetTriggerTime());
		}
		Events.Sort([](const TPair<FString, double>& A, const TPair<FString, double>& B) {
			return A.Value < B.Value;
		});
		for (const TPair<FString, double>& E : Events)
		{
			S += FString::Printf(TEXT("\n[[events]]\nname = \"%s\"\ntime = %s\n"),
				*BestowToml::Esc(E.Key), *BestowToml::F(E.Value));
		}

		TArray<TPair<FString, double>> Sections;
		for (const FCompositeSection& Section : Montage->CompositeSections)
		{
			Sections.Emplace(Section.SectionName.ToString(), (double)Section.GetTime());
		}
		Sections.Sort([](const TPair<FString, double>& A, const TPair<FString, double>& B) {
			return A.Value < B.Value;
		});
		const double Length = Montage->GetPlayLength();
		for (int32 I = 0; I < Sections.Num(); I++)
		{
			FString SectionName = Sections[I].Key;
			const bool bCanEnd = SectionCanEnd(SectionName);
			const double Start = Sections[I].Value;
			const double End = I + 1 < Sections.Num() ? Sections[I + 1].Value : Length;
			if (End <= Start)
			{
				continue;
			}
			S += FString::Printf(TEXT("\n[[sections]]\nname = \"%s\"\nstart = %s\nend = %s\n"),
				*BestowToml::Esc(BestowToml::Stem(SectionName)),
				*BestowToml::F(Start), *BestowToml::F(End));
			if (bCanEnd)
			{
				S += TEXT("can_end = true\n");
			}
		}

		const FString Path = Dir /
			FString::Printf(TEXT("%s.fbx.anim.toml"), *BestowToml::Stem(Montage->GetName()));
		FString Err;
		if (!BestowFiles::WriteAtomicText(Path, S, Err))
		{
			return FBestowExportResult::Fail(Err);
		}
		Result.Files.Add(Path);
		Result.Files.Add(BestowFiles::EnsureSidecar(Path));
		ClipsWithMeta++;
	}

	// ── Skeletal meshes: sockets ──────────────────────────────────────────
	TArray<FAssetData> Meshes;
	Registry.Get().GetAssetsByClass(USkeletalMesh::StaticClass()->GetClassPathName(), Meshes);
	for (const FAssetData& Asset : Meshes)
	{
		if (!Asset.PackageName.ToString().StartsWith(TEXT("/Game/")))
		{
			continue;
		}
		const USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset.GetAsset());
		if (!Mesh)
		{
			continue;
		}
		const TArray<USkeletalMeshSocket*> Sockets = Mesh->GetActiveSocketList();
		if (Sockets.Num() == 0)
		{
			continue;
		}

		FString S = FString::Printf(
			TEXT("# Sockets for `%s` — exported from Unreal skeletal-mesh sockets.\n"),
			*Mesh->GetName());
		for (const USkeletalMeshSocket* Socket : Sockets)
		{
			if (!Socket)
			{
				continue;
			}
			const FVector P = BestowConvert::Pos(Socket->RelativeLocation);
			const FQuat Q = Socket->RelativeRotation.Quaternion();
			const FQuat B(-Q.X, -Q.Z, -Q.Y, Q.W); // Epic's glTF quat convention
			S += FString::Printf(
				TEXT("\n[[animation.sockets]]\nname = \"%s\"\nbone = \"%s\"\n")
				TEXT("offset = %s\nrotation = %s\n"),
				*BestowToml::Esc(Socket->SocketName.ToString()),
				*BestowToml::Esc(Socket->BoneName.ToString()),
				*BestowToml::V3(P.X, P.Y, P.Z),
				*BestowToml::V4(B.X, B.Y, B.Z, B.W));
		}

		const FString Path = Dir /
			FString::Printf(TEXT("%s.sockets.toml"), *BestowToml::Stem(Mesh->GetName()));
		FString Err;
		if (!BestowFiles::WriteAtomicText(Path, S, Err))
		{
			return FBestowExportResult::Fail(Err);
		}
		Result.Files.Add(Path);
		Result.Files.Add(BestowFiles::EnsureSidecar(Path));
		SocketMeshes++;
	}

	if (Result.Files.Num() == 0)
	{
		return FBestowExportResult::Fail(
			TEXT("Nothing to export yet: add notifies to animation sequences (events), sections "
				 "to montages (combos), or sockets to skeletal meshes."));
	}

	Result.bOk = true;
	Result.Folder = Dir;
	Result.EntityToml =
		TEXT("anim.play(entity, \"assets/anims/<clip>.fbx\", { section = \"<name>\" })");
	if (ClipsWithMeta == 0)
	{
		Result.Warnings.Add(TEXT("No sequences/montages had notifies or sections — only sockets "
			"were exported."));
	}
	return Result;
}
