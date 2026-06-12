#include "BestowExportCore.h"

#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

namespace BestowToml
{
	FString Esc(const FString& S)
	{
		FString Out;
		Out.Reserve(S.Len());
		for (const TCHAR C : S)
		{
			switch (C)
			{
			case TEXT('"'): Out += TEXT("\\\""); break;
			case TEXT('\\'): Out += TEXT("\\\\"); break;
			case TEXT('\n'): Out += TEXT("\\n"); break;
			case TEXT('\r'): Out += TEXT("\\r"); break;
			case TEXT('\t'): Out += TEXT("\\t"); break;
			default:
				if (C < 0x20)
				{
					Out += FString::Printf(TEXT("\\u%04X"), (int32)C);
				}
				else
				{
					Out.AppendChar(C);
				}
			}
		}
		return Out;
	}

	FString F(double V)
	{
		FString S = FString::Printf(TEXT("%.9g"), V);
		if (!S.Contains(TEXT(".")) && !S.Contains(TEXT("e")) && !S.Contains(TEXT("E"))
			&& !S.Contains(TEXT("inf")) && !S.Contains(TEXT("nan")))
		{
			S += TEXT(".0");
		}
		return S;
	}

	FString V3(double X, double Y, double Z)
	{
		return FString::Printf(TEXT("[%s, %s, %s]"), *F(X), *F(Y), *F(Z));
	}

	FString V4(double X, double Y, double Z, double W)
	{
		return FString::Printf(TEXT("[%s, %s, %s, %s]"), *F(X), *F(Y), *F(Z), *F(W));
	}

	FString Stem(const FString& Name)
	{
		FString Out;
		Out.Reserve(Name.Len());
		for (const TCHAR C : Name)
		{
			if ((C >= TEXT('a') && C <= TEXT('z')) || (C >= TEXT('0') && C <= TEXT('9'))
				|| C == TEXT('-') || C == TEXT('_'))
			{
				Out.AppendChar(C);
			}
			else if (C >= TEXT('A') && C <= TEXT('Z'))
			{
				Out.AppendChar(C - TEXT('A') + TEXT('a'));
			}
			else
			{
				Out.AppendChar(TEXT('_'));
			}
		}
		FString Trimmed = Out;
		while (Trimmed.StartsWith(TEXT("_"))) Trimmed.RightChopInline(1);
		while (Trimmed.EndsWith(TEXT("_"))) Trimmed.LeftChopInline(1);
		return Trimmed.IsEmpty() ? TEXT("export") : Trimmed;
	}
}

namespace BestowConvert
{
	FVector Pos(const FVector& Ue)
	{
		// Epic's glTF convention: (X, Z, Y), cm → m.
		return FVector(Ue.X, Ue.Z, Ue.Y) * 0.01;
	}

	FVector Dir(const FVector& Ue)
	{
		return FVector(Ue.X, Ue.Z, Ue.Y).GetSafeNormal();
	}

	FVector EulerXYZ(const FQuat& Ue)
	{
		// Epic's glTF quaternion conversion (handedness flip + Y/Z swap):
		const FQuat B(-Ue.X, -Ue.Z, -Ue.Y, Ue.W);

		// Decompose as R = Rx(a) * Ry(b) * Rz(c)  (glam EulerRot::XYZ,
		// column-vector convention) from the bestow-space basis images.
		const FVector Cx = B.RotateVector(FVector(1, 0, 0)); // R * x̂ = column 0
		const FVector Cy = B.RotateVector(FVector(0, 1, 0));
		const FVector Cz = B.RotateVector(FVector(0, 0, 1));
		// m[r][c]: rows from the column vectors
		const double R02 = Cz.X;
		const double R12 = Cz.Y;
		const double R22 = Cz.Z;
		const double R01 = Cy.X;
		const double R00 = Cx.X;
		const double R10 = Cx.Y;
		const double R11 = Cy.Y;

		const double B_ = FMath::Asin(FMath::Clamp(R02, -1.0, 1.0));
		double A, C;
		if (FMath::Abs(R02) < 0.99999)
		{
			A = FMath::Atan2(-R12, R22);
			C = FMath::Atan2(-R01, R00);
		}
		else
		{
			// gimbal: fold everything into A
			A = FMath::Atan2(R10, R11);
			C = 0.0;
		}
		return FVector(A, B_, C);
	}
}

namespace BestowFiles
{
	bool WriteAtomic(const FString& Path, const TArray<uint8>& Bytes, FString& OutError)
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		const FString Tmp = Path + TEXT(".tmp~");
		if (!FFileHelper::SaveArrayToFile(Bytes, *Tmp))
		{
			OutError = FString::Printf(TEXT("cannot write %s"), *Tmp);
			return false;
		}
		if (!IFileManager::Get().Move(*Path, *Tmp, /*Replace*/ true))
		{
			OutError = FString::Printf(TEXT("cannot move into %s"), *Path);
			return false;
		}
		return true;
	}

	bool WriteAtomicText(const FString& Path, const FString& Text, FString& OutError)
	{
		FTCHARToUTF8 Utf8(*Text);
		TArray<uint8> Bytes((const uint8*)Utf8.Get(), Utf8.Length());
		return WriteAtomic(Path, Bytes, OutError);
	}

	FString UuidV7()
	{
		uint8 B[16];
		for (int32 I = 0; I < 16; I++)
		{
			B[I] = (uint8)FMath::RandRange(0, 255);
		}
		const uint64 Ms = (uint64)(FDateTime::UtcNow() - FDateTime(1970, 1, 1)).GetTotalMilliseconds();
		B[0] = (uint8)(Ms >> 40);
		B[1] = (uint8)(Ms >> 32);
		B[2] = (uint8)(Ms >> 24);
		B[3] = (uint8)(Ms >> 16);
		B[4] = (uint8)(Ms >> 8);
		B[5] = (uint8)Ms;
		B[6] = (B[6] & 0x0F) | 0x70;
		B[8] = (B[8] & 0x3F) | 0x80;
		return FString::Printf(
			TEXT("%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x"),
			B[0], B[1], B[2], B[3], B[4], B[5], B[6], B[7],
			B[8], B[9], B[10], B[11], B[12], B[13], B[14], B[15]);
	}

	FString EnsureSidecar(const FString& Path)
	{
		const FString Sidecar = Path + TEXT(".import.toml");
		if (!FPaths::FileExists(Sidecar))
		{
			FString Err;
			WriteAtomicText(Sidecar, FString::Printf(TEXT("id = \"%s\"\n"), *UuidV7()), Err);
		}
		return Sidecar;
	}

	static bool WritePng(const FString& Path, int32 Width, int32 Height, const void* Data,
		int64 Size, ERGBFormat Format, int32 BitDepth, FString& OutError)
	{
		IImageWrapperModule& Module =
			FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
		const TSharedPtr<IImageWrapper> Wrapper = Module.CreateImageWrapper(EImageFormat::PNG);
		if (!Wrapper.IsValid() || !Wrapper->SetRaw(Data, Size, Width, Height, Format, BitDepth))
		{
			OutError = TEXT("PNG encoder rejected the pixel data");
			return false;
		}
		const TArray64<uint8> Png = Wrapper->GetCompressed();
		TArray<uint8> Bytes(Png.GetData(), (int32)Png.Num());
		return WriteAtomic(Path, Bytes, OutError);
	}

	bool WritePng16(const FString& Path, int32 Width, int32 Height,
		const TArray<uint16>& Samples, FString& OutError)
	{
		check(Samples.Num() == Width * Height);
		return WritePng(Path, Width, Height, Samples.GetData(),
			(int64)Samples.Num() * 2, ERGBFormat::Gray, 16, OutError);
	}

	bool WritePngRgba8(const FString& Path, int32 Width, int32 Height,
		const TArray<uint8>& Rgba, FString& OutError)
	{
		check(Rgba.Num() == Width * Height * 4);
		return WritePng(Path, Width, Height, Rgba.GetData(),
			(int64)Rgba.Num(), ERGBFormat::RGBA, 8, OutError);
	}

	bool WritePngGray8(const FString& Path, int32 Width, int32 Height,
		const TArray<uint8>& Gray, FString& OutError)
	{
		check(Gray.Num() == Width * Height);
		return WritePng(Path, Width, Height, Gray.GetData(),
			(int64)Gray.Num(), ERGBFormat::Gray, 8, OutError);
	}
}

namespace BestowSettings
{
	static const TCHAR* Section = TEXT("BestowExport");
	static const TCHAR* Key = TEXT("GameRoot");

	FString GetGameRoot()
	{
		FString Root;
		GConfig->GetString(Section, Key, Root, GEditorPerProjectIni);
		return Root;
	}

	void SetGameRoot(const FString& Root)
	{
		GConfig->SetString(Section, Key, *Root, GEditorPerProjectIni);
		GConfig->Flush(false, GEditorPerProjectIni);
	}

	bool GameRootValid(const FString& Root)
	{
		return !Root.IsEmpty() && FPaths::FileExists(Root / TEXT("game.toml"));
	}
}
