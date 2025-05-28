#include "PhysicsPreviewUI.h"
#include <LevelEditor/SLevelEditor.h>
#include "PropertyEditor/PhysicsDetailPreviewEditorPanel.h"
#include "PropertyEditor/PhysicsTreePreviewEditorPanel.h"
#include "PropertyEditor/PhysicsGraphPreviewEditorPanel.h"
#include <PropertyEditor/PreviewControlEditorPanel.h>
void FPhysicsPreviewUI::Initialize(SLevelEditor* LevelEditor, float Width, float Height)
{
    auto ControlPanel = std::make_shared<PreviewControlEditorPanel>();
    ControlPanel->Initialize(LevelEditor, Width, Height);
    Panels["PreviewControlPanel"] = ControlPanel;

    auto PhysicsTreePanel = std::make_shared<PhysicsTreePreviewEditorPanel>();
    PhysicsTreePanel->Initialize(LevelEditor, Width, Height);
    Panels["PhysicsTreePanel"] = PhysicsTreePanel;
    
    /*auto PhysicsGraphPanel = std::make_shared<PhysicsGraphPreviewEditorPanel>();
    PhysicsGraphPanel->Initialize(LevelEditor, Width, Height);
    Panels["PhysicsGraphPanel"] = PhysicsGraphPanel;*/

    auto PhysicsDetailPanel = std::make_shared<PhysicsDetailPreviewEditorPanel>();
    PhysicsDetailPanel->Initialize(LevelEditor, Width, Height);
    Panels["PhysicsPropertyPanel"] = PhysicsDetailPanel;
}

void FPhysicsPreviewUI::Render() const
{
    for (const auto& Panel : Panels)
    {
        if (Panel.Value->bIsVisible)
        {
            Panel.Value->Render();
        }
    }
}

void FPhysicsPreviewUI::OnResize(HWND hWnd) const
{
    if (ActiveHandle != hWnd)
    {
        return;
    }
    
    for (auto& Panel : Panels)
    {
        Panel.Value->OnResize(hWnd);
    }
}

void FPhysicsPreviewUI::SetWorld(UWorld* InWorld)
{
    PreviewWorld = InWorld;
    for (auto& [_, Panel] : Panels)
    {
        Panel->SetWorld(PreviewWorld);
    }
}
