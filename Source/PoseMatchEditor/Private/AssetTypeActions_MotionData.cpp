#include "AssetTypeActions_MotionData.h"
#include "ToolMenu.h"

#include "MotionData.h"

#include "MotionCacheBuilder.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FText FAssetTypeActions_MotionData::GetName() const
{
	return LOCTEXT("MotionDataAssetName", "Motion Data Asset");
//	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_MotionData, "Motion Data");
}

FColor FAssetTypeActions_MotionData::GetTypeColor() const
{
	return FColor(255, 128, 0);
}

UClass* FAssetTypeActions_MotionData::GetSupportedClass() const
{
	return UMotionData::StaticClass();
}

uint32 FAssetTypeActions_MotionData::GetCategories()
{
	return EAssetTypeCategories::Animation;
}


void FAssetTypeActions_MotionData::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
    FAssetTypeActions_Base::GetActions(InObjects, MenuBuilder);

    auto motionDataAssets = GetTypedWeakObjectPtrs<UMotionData>(InObjects);

    
    
    MenuBuilder.AddMenuEntry(
        LOCTEXT("TextAsset_Rebuild", "Rebuild Motion Cache"),
        LOCTEXT("TextAsset_RebuildToolTip", "Generates the necessary motion trajectories for motion matching."),
        FSlateIcon(),
        FUIAction(
            FExecuteAction::CreateLambda([=] {
                for (auto& MotionDataAsset : motionDataAssets)
                {
                    if (MotionDataAsset.IsValid())
                    {
                        BuildMotionCache(MotionDataAsset);
                    }
                }
            }),
            FCanExecuteAction::CreateLambda([=] {
                /*for (auto& TextAsset : TextAssets)
                {
                    if (TextAsset.IsValid() && !TextAsset->Text.IsEmpty())
                    {
                        return true;
                    }
                }*/
                return true;
            })
        )
    );

    MenuBuilder.AddSeparator("Sep");

    MenuBuilder.AddMenuEntry(
        LOCTEXT("TextAsset_AddAnim", "Add All Compatible Animations"),
        LOCTEXT("TextAsset_AddAnimToolTip", "Add animations to the state pool."),
        FSlateIcon(),
        FUIAction(
            FExecuteAction::CreateLambda([=] {
        for (auto& MotionDataAsset : motionDataAssets)
        {
            if (MotionDataAsset.IsValid())
            {
                AddAnimations(MotionDataAsset);
            }
        }
    }),
            FCanExecuteAction::CreateLambda([=] {
        /*for (auto& TextAsset : TextAssets)
        {
            if (TextAsset.IsValid() && !TextAsset->Text.IsEmpty())
            {
                return true;
            }
        }*/
        return true;
    })
        )
    );

    MenuBuilder.AddMenuEntry(
        LOCTEXT("TextAsset_Convert", "Convert Frame Rate"),
        LOCTEXT("TextAsset_ConvertToolTip", "Speed up from 24fps to 30fps."),
        FSlateIcon(),
        FUIAction(
            FExecuteAction::CreateLambda([=] {
                for (auto& MotionDataAsset : motionDataAssets)
                {
                    if (MotionDataAsset.IsValid())
                    {
                        ChangeAnimationRate(MotionDataAsset, false);
                    }
                }
            }),
                FCanExecuteAction::CreateLambda([=] {
                    return true;
            })
        )
    );

    MenuBuilder.AddMenuEntry(
        LOCTEXT("TextAsset_Convert2", "Convert Frame Rate (reset)"),
        LOCTEXT("TextAsset_Convert2ToolTip", "Speed down from 30fps to 24fps."),
        FSlateIcon(),
        FUIAction(
            FExecuteAction::CreateLambda([=] {
        for (auto& MotionDataAsset : motionDataAssets)
        {
            if (MotionDataAsset.IsValid())
            {
                ChangeAnimationRate(MotionDataAsset, true);
            }
        }
    }),
            FCanExecuteAction::CreateLambda([=] {
        return true;
    })
        )
    );
}

void FAssetTypeActions_MotionData::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
    //Section.AddSeparator("Sup");
    /*Section.AddEntry(FToolMenuEntry::InitToolBarButton(
        "MakeStaticMesh",
        (FToolMenuExecuteAction)NULL,
        LOCTEXT("MakeStaticMesh", "Make Static Mesh"),
        LOCTEXT("MakeStaticMeshTooltip", "Make a new static mesh out of the preview's current pose."),
        FSlateIcon("EditorStyle", "Persona.ConvertToStaticMesh")
    ));*/

    /*Section.AddMenuEntry(
        "DataTable_ExportAsJSON",
        LOCTEXT("DataTable_ExportAsJSON", "Export as JSON"),
        LOCTEXT("DataTable_ExportAsJSONTooltip", "Export the data table as a file containing JSON data."),
        FSlateIcon(),
        FUIAction(
            //FExecuteAction::CreateSP(this, &FAssetTypeActions_DataTable::ExecuteExportAsJSON, Tables),
            (FExecuteAction)NULL,
            FCanExecuteAction()
        )
    );*/

}
