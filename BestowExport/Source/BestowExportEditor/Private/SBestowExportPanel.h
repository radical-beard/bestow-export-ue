#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

struct FBestowExportResult;
class SButton;
class SEditableTextBox;
class STextBlock;
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
	static UWorld* EditorWorld();

	void ShowResult(const FBestowExportResult& Result);
	FReply OnPickGameFolder();
	FReply OnExportTerrain();
	FReply OnExportMesh();
	FReply OnExportScene();
	FReply OnExportAnim();
	FReply OnExportSplines();

	TSharedPtr<SEditableTextBox> NameBox;
	TSharedPtr<STextBlock> LogText;
	TSharedPtr<SButton> CopyButton;
	FString LastSnippet;
};
