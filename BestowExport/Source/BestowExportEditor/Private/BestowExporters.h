// The five exporters. Each takes editor-side objects plus the linked
// bestow game root and writes engine-consumable files (formats verified
// against bestow's parsing code — see the spec in the argh repo,
// docs/bestow-export/spec.md, which this plugin mirrors for Unreal).

#pragma once

#include "CoreMinimal.h"
#include "BestowExportCore.h"

class AActor;
class UWorld;

namespace BestowExporters
{
	/// E1 — Landscape → self-contained `assets/terrain/<name>/` folder:
	/// 16-bit hgt.png + hgt.toml + ctl.png (rockness/autoshader) +
	/// per-layer lossless weightmaps + terrain_baked.slang + entity TOML.
	/// Rock = any paint layer whose name contains "rock" (case-insensitive).
	FBestowExportResult ExportTerrain(UWorld* World, const FString& GameRoot, const FString& Name);

	/// E2 — selected actors → `assets/models/<name>/<name>.glb` via the
	/// engine glTF exporter + an entity snippet (render.model + trimesh
	/// collider). The glb bakes world placement; the entity sits at origin.
	FBestowExportResult ExportMesh(UWorld* World, const TSet<AActor*>& Selected,
		const FString& GameRoot, const FString& Name);

	/// E3+E6 — level layout → `scenes/<name>.scene.toml`: TargetPoints →
	/// tag markers, point lights, camera, DirectionalLight + fog → sky
	/// entity, any tagged actor (tag `template:<x>` instances a template).
	FBestowExportResult ExportScene(UWorld* World, const FString& GameRoot, const FString& Name);

	/// E5 — project-wide animation metadata: AnimSequence notifies →
	/// `assets/anims/<clip>.fbx.anim.toml` events, Montage sections →
	/// sections (name ending `!` or `_exit` → can_end), skeletal-mesh
	/// sockets → `assets/anims/<mesh>.sockets.toml`.
	FBestowExportResult ExportAnimationMeta(const FString& GameRoot);

	/// E8 — every actor with a spline component → `assets/splines/*.toml`
	/// (control points + 1 m baked polyline; engine-pending in bestow,
	/// directly readable from game Lua today).
	FBestowExportResult ExportSplines(UWorld* World, const FString& GameRoot);
}
