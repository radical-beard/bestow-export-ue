// The Bestow tab — the whole workflow in one panel, in order, numbered.
// Anything that can go wrong is said in plain words next to a disabled
// button instead of a popup after the fact. If using this needs
// documentation beyond "press the buttons top to bottom", fix it here.

#include "SBestowExportPanel.h"

#include "BestowExportCore.h"
#include "BestowExporters.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "DesktopPlatformModule.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
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
				ExportButton(LOCTEXT("Attach", "Export Attachment (grip)"),
					&SBestowExportPanel::OnExportAttachment, &SBestowExportPanel::CanExportAttachment)
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

bool SBestowExportPanel::AttachedItemInfo(
	AActor*& OutItem, USkeletalMeshComponent*& OutSkel, FName& OutSocket)
{
	OutItem = nullptr;
	OutSkel = nullptr;
	OutSocket = NAME_None;
	if (!GEditor || GEditor->GetSelectedActorCount() != 1)
	{
		return false;
	}
	AActor* Actor = Cast<AActor>(GEditor->GetSelectedActors()->GetSelectedObject(0));
	USceneComponent* Root = Actor ? Actor->GetRootComponent() : nullptr;
	if (!Root)
	{
		return false;
	}
	USkeletalMeshComponent* Skel = Cast<USkeletalMeshComponent>(Root->GetAttachParent());
	const FName Socket = Root->GetAttachSocketName();
	if (!Skel || Socket.IsNone())
	{
		return false;
	}
	OutItem = Actor;
	OutSkel = Skel;
	OutSocket = Socket;
	return true;
}

bool SBestowExportPanel::CanExportAttachment() const
{
	if (!GameLinked())
	{
		return false;
	}
	AActor* Item = nullptr;
	USkeletalMeshComponent* Skel = nullptr;
	FName Socket = NAME_None;
	return AttachedItemInfo(Item, Skel, Socket);
}

FString SBestowExportPanel::ReadinessNote() const
{
	if (!GameLinked())
	{
		return TEXT("Link your bestow game above first — every button waits on it.");
	}
	// every grey button gets its reason here — the guide promises that
	FString Notes;
	if (!CanExportTerrain())
	{
		Notes += TEXT("Terrain needs a Landscape in the level. ");
	}
	if (!CanExportMesh())
	{
		Notes += TEXT("Mesh export needs selected actors — click something in the level first. ");
	}
	if (!CanExportAttachment())
	{
		Notes += TEXT("Attachment export needs ONE item selected that's attached to a socket on the "
					  "character's skeletal mesh — drag the item onto the character, pick a socket, nudge it. ");
	}
	if (Notes.IsEmpty())
	{
		return TEXT("Heights, painted layers, placements and animation data export exactly as authored.");
	}
	return Notes.TrimEnd();
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

FReply SBestowExportPanel::OnExportAttachment()
{
	AActor* Item = nullptr;
	USkeletalMeshComponent* Skel = nullptr;
	FName Socket = NAME_None;
	if (!AttachedItemInfo(Item, Skel, Socket))
	{
		return FReply::Handled();
	}
	// Name after the item, not the level — the grip travels with the item.
	FString Name = NameBox.IsValid() ? NameBox->GetText().ToString() : FString();
	if (Name.TrimStartAndEnd().IsEmpty())
	{
		Name = Item->GetActorLabel();
	}
	ShowResult(BestowExporters::ExportAttachment(
		Item, BestowSettings::GetGameRoot(), BestowToml::Stem(Name)));
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
