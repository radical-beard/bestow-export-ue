// Bestow Export — registers the "Bestow" nomad tab (Tools menu → Bestow)
// hosting the export panel. Everything else lives in the exporters.

#include "Framework/Docking/TabManager.h"
#include "Modules/ModuleManager.h"
#include "SBestowExportPanel.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "BestowExport"

static const FName BestowTabName("BestowExport");

class FBestowExportEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGlobalTabmanager::Get()
			->RegisterNomadTabSpawner(BestowTabName,
				FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&) {
					return SNew(SDockTab)
						.TabRole(ETabRole::NomadTab)
						[
							SNew(SBestowExportPanel)
						];
				}))
			.SetDisplayName(LOCTEXT("TabTitle", "Bestow"))
			.SetTooltipText(LOCTEXT("TabTooltip", "Export this project's content to the bestow engine"))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Convert"));
	}

	virtual void ShutdownModule() override
	{
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(BestowTabName);
		}
	}
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBestowExportEditorModule, BestowExportEditor)
