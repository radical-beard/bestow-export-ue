#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "BestowSelfTestCommandlet.generated.h"

/// Headless CI for the exporters: builds fixtures (actors, a montage with
/// sections, notifies, sockets) in a transient world, runs every exporter
/// against a target directory, and exits non-zero on any failure.
///
///   UnrealEditor-Cmd <project> -run=BestowSelfTest -targetdir=/tmp/x
UCLASS()
class UBestowSelfTestCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
};
