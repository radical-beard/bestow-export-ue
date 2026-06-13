#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

struct FBestowExportResult;
class AActor;
class SButton;
class SEditableTextBox;
class STextBlock;
class USkeletalMeshComponent;
class UWorld;

/// The Bestow dock: link game → name → export buttons → result + copy.
class SBestowExportPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBestowExportPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> ExportButton(const FText& Label,
		FReply (SBestowExportPanel::*Handler)(), bool (SBestowExportPanel::*Enabled)() const);

	FString GameButtonLabel() const;
	FString GameStatus() const;
	FString ReadinessNote() const;
	FString ExportName() const;
	bool GameLinked() const;
	bool CanExportAlways() const;
	bool CanExportTerrain() const;
	bool CanExportMesh() const;
	bool CanExportAttachment() const;
	static UWorld* EditorWorld();

	/// The selected actor IF it's one item attached to a socket on a skeletal
	/// mesh — the precondition for an attachment export. Out params are only
	/// valid when this returns true.
	static bool AttachedItemInfo(AActor*& OutItem, USkeletalMeshComponent*& OutSkel, FName& OutSocket);

	void ShowResult(const FBestowExportResult& Result);
	FReply OnPickGameFolder();
	FReply OnExportTerrain();
	FReply OnExportMesh();
	FReply OnExportAttachment();
	FReply OnExportScene();
	FReply OnExportAnim();
	FReply OnExportSplines();

	TSharedPtr<SEditableTextBox> NameBox;
	TSharedPtr<STextBlock> LogText;
	TSharedPtr<SButton> CopyButton;
	FString LastSnippet;
};
