#include "PhysicsDetailPreviewEditorPanel.h"
#include "EditorEngine.h"
#include "LevelEditor/SLevelEditor.h"
#include "PhysicsCore/PhysicsAssetTypes.h"
#include "PhysicsTreePreviewEditorPanel.h"
#include <Components/PrimitiveComponents/MeshComponents/SkeletalMeshComponent.h>
#include <Actors/SkeletalMeshActor.h>
#include "Physics/PhysicsAsset.h"
#include "Physics/PhysicsConstraintTemplate.h"
#include "Animation/Skeleton.h"
void PhysicsDetailPreviewEditorPanel::Initialize(SLevelEditor* levelEditor, float InWidth, float InHeight)
{
    activeLevelEditor = levelEditor;
    Width = InWidth;
    Height = InHeight;
}

void PhysicsDetailPreviewEditorPanel::Render()
{
    UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine);

    if (EditorEngine == nullptr)
    {
        return;
    }

    AActor* PickedActor = nullptr;

    if (!World->GetSelectedActors().IsEmpty())
    {
        PickedActor = *World->GetSelectedActors().begin();
    }

   /* USkeletalMeshComponent* SkeletalMeshComp = nullptr;
    ASkeletalMeshActor* SkeletalActor = Cast<ASkeletalMeshActor>(PickedActor);
    UPhysicsAsset* PhysicsAsset = nullptr;
    

    if (SkeletalActor) {
        SkeletalMeshComp = SkeletalActor->GetSkeletalMeshComponent();
        PhysicsAsset = SkeletalMeshComp->GetSkeletalMesh()->GetPhysicsAsset();
    }*/
    
    /*if (!SkeletalActor || !SkeletalMeshComp || !PhysicsAsset) {
        UE_LOG(LogLevel::Warning, "[PhysicsDetailPreviewEditorPanel::Render()] Invalid Actor or Component or PhysicsAsset");
        return;
    }*/

    ASkeletalMeshActor* SkeletalActor = Cast<ASkeletalMeshActor>(PickedActor);
    if (!SkeletalActor)
    {
        UE_LOG(LogLevel::Warning, "Invalid SkeletalActor");
        return;
    }

    USkeletalMeshComponent* SkeletalMeshComp = SkeletalActor->GetSkeletalMeshComponent();
    if (!SkeletalMeshComp || !SkeletalMeshComp->GetSkeletalMesh())
    {
        UE_LOG(LogLevel::Warning, "Invalid SkeletalMeshComponent");
        return;
    }

    UPhysicsAsset* PhysicsAsset = SkeletalMeshComp->GetSkeletalMesh()->GetPhysicsAsset();
    if (!PhysicsAsset)
    {
        UE_LOG(LogLevel::Warning, "Invalid PhysicsAsset");
        return;
    }
   
    TArray<UBodySetup*> BodySetups;
    PhysicsAsset->GetBodySetups(BodySetups);

    
    /* Pre Setup */
    float PanelWidth = (Width) * 0.2f - 6.0f;
    float PanelHeight = (Height) * 0.9f;

    float PanelPosX = (Width) * 0.8f + 5.0f;
    float PanelPosY = 45.0f;

    ImVec2 MinSize(140, 370);
    ImVec2 MaxSize(FLT_MAX, 1500);

    /* Min, Max Size */
    ImGui::SetNextWindowSizeConstraints(MinSize, MaxSize);

    /* Panel Position */
    ImGui::SetNextWindowPos(ImVec2(PanelPosX, PanelPosY), ImGuiCond_Always);

    /* Panel Size */
    ImGui::SetNextWindowSize(ImVec2(PanelWidth, PanelHeight), ImGuiCond_Always);

    /* Panel Flags */
    ImGuiWindowFlags PanelFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

    /* Render Start */
    ImGui::Begin("Detail", nullptr, PanelFlags);

    using EType = EPhysicsNodeType;
    EType Type = PhysicsTreePreviewEditorPanel::SelectedType;

    
    switch (PhysicsTreePreviewEditorPanel::SelectedType) {
    case EType::Bone:
    {
        DrawBoneDetails(PhysicsTreePreviewEditorPanel::SelectedBoneIndex, SkeletalMeshComp, PhysicsAsset);
        break;
    }
    case EType::Constraint:
    {
        UPhysicsConstraintTemplate* Constraint = PhysicsAsset->ConstraintSetup[PhysicsTreePreviewEditorPanel::SelectedConstraintIndex];
        DrawPropertiesForObject(Constraint);
        break;
    }
    case EType::Body:
    {
        UBodySetup* Body = BodySetups[PhysicsTreePreviewEditorPanel::SelectedBodyIndex];
        DrawPropertiesForObject(Body);
        break;
    }
    /*case EType::Primitive:
        int BI = PhysicsTreePreviewEditorPanel::SelectedBodyIndex;
        auto PrimType = PhysicsTreePreviewEditorPanel::SelectedPrimType;
        int Pi = PhysicsTreePreviewEditorPanel::SelectedPrimIndex;
        break;*/
    default:
        ImGui::Text("Select an element on the left to edit its properties.");
        break;
    }

    ImGui::End();
}

void PhysicsDetailPreviewEditorPanel::OnResize(HWND hWnd)
{
    RECT clientRect;
    GetClientRect(hWnd, &clientRect);
    Width = clientRect.right - clientRect.left;
    Height = clientRect.bottom - clientRect.top;
}

void PhysicsDetailPreviewEditorPanel::DrawBoneDetails(int32 BoneIndex, USkeletalMeshComponent* SkelComp, UPhysicsAsset* PhysicsAsset)
{

    // 본 이름 표시
    const FRefSkeletal& RefSkel = SkelComp->GetSkeletalMesh()->GetSkeleton()->GetRefSkeletal();
    FName BoneName1 = RefSkel.GetBoneName(BoneIndex);
    ImGui::SeparatorText(*BoneName1.ToString());

    // 타겟 Bone 선택 콤보
    static int32 TargetBone = INDEX_NONE;
    if (ImGui::BeginCombo("Target Bone",
        TargetBone >= 0 ?
        *(RefSkel.GetBoneName(TargetBone).ToString())
        : "None"))
    {
        for (int32 i = 0; i < RefSkel.RawBones.Num(); ++i)
        {
            std::string BoneNameStr = RefSkel.GetBoneName(i).ToString().ToAnsiString();
            const char* Name = BoneNameStr.c_str();
            bool bSel = (i == TargetBone);
            if (ImGui::Selectable(Name, bSel))
            {
                TargetBone = i;
            }
            if (bSel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (TargetBone != INDEX_NONE && TargetBone != BoneIndex)
    {
        if (ImGui::Button("Add Constraint"))
        {
            UPhysicsConstraintTemplate* NewC = FObjectFactory::ConstructObject<UPhysicsConstraintTemplate>(PhysicsAsset);
            NewC->ConstraintBone1 = BoneName1;
            NewC->ConstraintBone2 = FName(*(BoneName1.ToString() + TEXT("_") + RefSkel.GetBoneName(TargetBone).ToString()));

            NewC->TwistLimit = 45.f;
            NewC->SwingLimit1 = 45.f;
            NewC->SwingLimit2 = 45.f;

            PhysicsAsset->ConstraintSetup.Add(NewC);
            TargetBone = INDEX_NONE;
        }
    }
}

void PhysicsDetailPreviewEditorPanel::DrawPropertiesForObject(UObject* Obj)
{
    if(!Obj)
    {
        ImGui::Text("No object to display.");
        return;
    }

    UClass* Class = Obj->GetClass();
    ImGui::SeparatorText(*Class->GetName());

    for (const FProperty* Prop : Class->GetProperties())
    {        
        Prop->DisplayInImGui(Obj);

    }

}
