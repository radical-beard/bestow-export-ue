// The Bestow tab — the whole workflow in one panel, in order, numbered.
// Anything that can go wrong is said in plain words next to a disabled
// button instead of a popup after the fact. If using this needs
// documentation beyond "press the buttons top to bottom", fix it here.

#include "SBestowExportPanel.h"

#include "BestowExportCore.h"
#include "BestowExporters.h"
#include "DesktopPlatformModule.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IDesktopPlatform.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "BestowExport"

void SBestowExportPanel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot().Padding(8)
		[
			SNew(SVerticalBox)

			// ── 1. game link ─────────────────────────────────────────────
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Step1", "1. Your bestow game"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
			[
				SNew(SButton)
				.Text_Lambda([this] { return FText::FromString(GameButtonLabel()); })
				.OnClicked(this, &SBestowExportPanel::OnPickGameFolder)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
			[
				SNew(STextBlock)
				.Text_Lambda([this] { return FText::FromString(GameStatus()); })
				.AutoWrapText(true)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 6) [ SNew(SSeparator) ]

			// ── 2. name ──────────────────────────────────────────────────
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Step2", "2. Name this export"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
			[
				SAssignNew(NameBox, SEditableTextBox)
				.HintText(LOCTEXT("NameHint", "e.g. gloom_isle"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 6) [ SNew(SSeparator) ]

			// ── 3. export buttons ────────────────────────────────────────
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Step3", "3. Export"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
			[
				ExportButton(LOCTEXT("Terrain", "Export Terrain (Landscape)"),
					&SBestowExportPanel::OnExportTerrain, &SBestowExportPanel::CanExportTerrain)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
			[
				ExportButton(LOCTEXT("Mesh", "Export Selected as Mesh (.glb)"),
					&SBestowExportPanel::OnExportMesh, &SBestowExportPanel::CanExportMesh)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
			[
				ExportButton(LOCTEXT("Scene", "Export Scene Layout"),
					&SBestowExportPanel::OnExportScene, &SBestowExportPanel::CanExportAlways)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
			[
				ExportButton(LOCTEXT("Anim", "Export Animation Metadata"),
					&SBestowExportPanel::OnExportAnim, &SBestowExportPanel::CanExportAlways)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
			[
				ExportButton(LOCTEXT("Splines", "Export Splines"),
					&SBestowExportPanel::OnExportSplines, &SBestowExportPanel::CanExportAlways)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
			[
				SNew(STextBlock)
				.Text_Lambda([this] { return FText::FromString(ReadinessNote()); })
				.AutoWrapText(true)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 6) [ SNew(SSeparator) ]

			// ── result log + copy ────────────────────────────────────────
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4)
			[
				SAssignNew(LogText, STextBlock)
				.AutoWrapText(true)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2)
			[
				SAssignNew(CopyButton, SButton)
				.Visibility(EVisibility::Collapsed)
				.Text(LOCTEXT("Copy", "Copy snippet"))
				.OnClicked_Lambda([this] {
					FPlatformApplicationMisc::ClipboardCopy(*LastSnippet);
					return FReply::Handled();
				})
			]
		]
	];
}

TSharedRef<SWidget> SBestowExportPanel::ExportButton(const FText& Label,
	FReply (SBestowExportPanel::*Handler)(), bool (SBestowExportPanel::*Enabled)() const)
{
	return SNew(SButton)
		.Text(Label)
		.IsEnabled_Lambda([this, Enabled] { return (this->*Enabled)(); })
		.OnClicked_Lambda([this, Handler] { return (this->*Handler)(); });
}

// ── state helpers ──────────────────────────────────────────────────────────

FString SBestowExportPanel::GameButtonLabel() const
{
	const FString Root = BestowSettings::GetGameRoot();
	if (Root.IsEmpty())
	{
		return TEXT("Choose game folder…");
	}
	return BestowSettings::GameRootValid(Root)
		? TEXT("Change game folder…")
		: TEXT("Choose a different folder…");
}

FString SBestowExportPanel::GameStatus() const
{
	const FString Root = BestowSettings::GetGameRoot();
	if (Root.IsEmpty())
	{
		return TEXT("No game linked yet — every export needs to know where to land.");
	}
	if (!BestowSettings::GameRootValid(Root))
	{
		return FString::Printf(
			TEXT("⚠ %s\nThat folder has no game.toml — pick the game folder itself."), *Root);
	}
	return FString::Printf(TEXT("✓ %s"), *Root);
}

bool SBestowExportPanel::GameLinked() const
{
	return BestowSettings::GameRootValid(BestowSettings::GetGameRoot());
}

UWorld* SBestowExportPanel::EditorWorld()
{
	return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

bool SBestowExportPanel::CanExportAlways() const
{
	return GameLinked();
}

bool SBestowExportPanel::CanExportTerrain() const
{
	if (!GameLinked())
	{
		return false;
	}
	UWorld* World = EditorWorld();
	if (!World)
	{
		return false;
	}
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		return true;
	}
	return false;
}

bool SBestowExportPanel::CanExportMesh() const
{
	return GameLinked() && GEditor && GEditor->GetSelectedActorCount() > 0;
}

FString SBestowExportPanel::ReadinessNote() const
{
	if (!GameLinked())
	{
		return TEXT("Link your bestow game above first — every button waits on it.");
	}
	if (!CanExportTerrain())
	{
		return TEXT("Terrain needs a Landscape in the level. Mesh export needs selected actors.");
	}
	return TEXT("Heights, painted layers, placements and animation data export exactly as authored.");
}

FString SBestowExportPanel::ExportName() const
{
	FString Name = NameBox.IsValid() ? NameBox->GetText().ToString() : FString();
	if (Name.TrimStartAndEnd().IsEmpty())
	{
		if (const UWorld* World = const_cast<SBestowExportPanel*>(this)->EditorWorld())
		{
			Name = World->GetMapName();
			Name.RemoveFromStart(World->StreamingLevelsPrefix);
		}
	}
	return BestowToml::Stem(Name);
}

// ── actions ────────────────────────────────────────────────────────────────

FReply SBestowExportPanel::OnPickGameFolder()
{
	IDesktopPlatform* Desktop = FDesktopPlatformModule::Get();
	if (!Desktop)
	{
		return FReply::Handled();
	}
	const void* Parent = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	FString Dir;
	if (Desktop->OpenDirectoryDialog(const_cast<void*>(Parent),
			TEXT("Pick your bestow game folder (the one with game.toml in it)"),
			BestowSettings::GetGameRoot(), Dir))
	{
		BestowSettings::SetGameRoot(Dir);
	}
	return FReply::Handled();
}

void SBestowExportPanel::ShowResult(const FBestowExportResult& Result)
{
	if (!Result.bOk)
	{
		LogText->SetText(FText::FromString(Result.Error));
		CopyButton->SetVisibility(EVisibility::Collapsed);
		return;
	}
	FString Text = FString::Printf(TEXT("Exported ✓  %d files → %s\n")
		TEXT("Paste the snippet (Copy button) into your game and a running bestow hot-reloads it."),
		Result.Files.Num(), *Result.Folder);
	for (const FString& W : Result.Warnings)
	{
		Text += FString::Printf(TEXT("\n⚠ %s"), *W);
	}
	LogText->SetText(FText::FromString(Text));
	LastSnippet = Result.EntityToml;
	CopyButton->SetVisibility(EVisibility::Visible);
}

FReply SBestowExportPanel::OnExportTerrain()
{
	ShowResult(BestowExporters::ExportTerrain(
		EditorWorld(), BestowSettings::GetGameRoot(), ExportName()));
	return FReply::Handled();
}

FReply SBestowExportPanel::OnExportMesh()
{
	TSet<AActor*> Selected;
	if (GEditor)
	{
		for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
		{
			if (AActor* Actor = Cast<AActor>(*It))
			{
				Selected.Add(Actor);
			}
		}
	}
	ShowResult(BestowExporters::ExportMesh(
		EditorWorld(), Selected, BestowSettings::GetGameRoot(), ExportName()));
	return FReply::Handled();
}

FReply SBestowExportPanel::OnExportScene()
{
	ShowResult(BestowExporters::ExportScene(
		EditorWorld(), BestowSettings::GetGameRoot(), ExportName()));
	return FReply::Handled();
}

FReply SBestowExportPanel::OnExportAnim()
{
	ShowResult(BestowExporters::ExportAnimationMeta(BestowSettings::GetGameRoot()));
	return FReply::Handled();
}

FReply SBestowExportPanel::OnExportSplines()
{
	ShowResult(BestowExporters::ExportSplines(EditorWorld(), BestowSettings::GetGameRoot()));
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
