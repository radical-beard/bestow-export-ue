// Shared plumbing for every Bestow exporter: TOML emission discipline
// (escaping, decimal-point floats), hot-reload-safe writes + UUIDv7
// .import.toml sidecars (bestow D-011: ids never churn), PNG encoding,
// and the UE→bestow coordinate convention.
//
// Coordinates: Unreal is left-handed Z-up centimeters; bestow is
// right-handed Y-up meters (−Z forward). We use the SAME mapping as
// Epic's glTF exporter so TOML transforms agree with exported glb
// contents: position (X, Z, Y) * 0.01, quaternion (−X, −Z, −Y, W).
// Rotations land in bestow TOML as Euler XYZ radians (what
// bestow-ecs/world.rs parses).

#pragma once

#include "CoreMinimal.h"

struct FBestowExportResult
{
	bool bOk = false;
	FString Error;
	TArray<FString> Files;
	FString Folder;
	FString EntityToml;
	TArray<FString> Warnings;

	static FBestowExportResult Fail(const FString& InError)
	{
		FBestowExportResult R;
		R.Error = InError;
		return R;
	}
};

namespace BestowToml
{
	/// Escape for a TOML basic (double-quoted) string.
	FString Esc(const FString& S);

	/// Float with a guaranteed decimal point, shortest round-trip-ish.
	FString F(double V);

	FString V3(double X, double Y, double Z);
	FString V4(double X, double Y, double Z, double W);

	/// File/entity stem: lowercase ascii, '_' elsewhere.
	FString Stem(const FString& Name);
}

namespace BestowConvert
{
	/// UE world position (cm, LH Z-up) → bestow meters (RH Y-up).
	FVector Pos(const FVector& Ue);

	/// UE direction (unit) → bestow direction.
	FVector Dir(const FVector& Ue);

	/// UE rotation → bestow Euler XYZ radians (glam from_euler(XYZ) order).
	FVector EulerXYZ(const FQuat& Ue);
}

namespace BestowFiles
{
	/// tmp + rename so a watching bestow never half-reads.
	bool WriteAtomic(const FString& Path, const TArray<uint8>& Bytes, FString& OutError);
	bool WriteAtomicText(const FString& Path, const FString& Text, FString& OutError);

	/// Create `<Path>.import.toml` with a fresh UUIDv7 — only if missing.
	FString EnsureSidecar(const FString& Path);

	/// RFC 9562 UUIDv7.
	FString UuidV7();

	/// 16-bit grayscale PNG from row-major samples.
	bool WritePng16(const FString& Path, int32 Width, int32 Height,
		const TArray<uint16>& Samples, FString& OutError);

	/// 8-bit RGBA PNG from row-major RGBA bytes.
	bool WritePngRgba8(const FString& Path, int32 Width, int32 Height,
		const TArray<uint8>& Rgba, FString& OutError);

	/// 8-bit grayscale PNG.
	bool WritePngGray8(const FString& Path, int32 Width, int32 Height,
		const TArray<uint8>& Gray, FString& OutError);
}

namespace BestowSettings
{
	/// The linked bestow game root (per-project editor setting).
	FString GetGameRoot();
	void SetGameRoot(const FString& Root);
	bool GameRootValid(const FString& Root);
}
