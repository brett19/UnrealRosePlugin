#pragma once

#include "Slate.h"
#include "Commands.h"

class FRosePluginCommands : public TCommands < FRosePluginCommands >
{
public:

    FRosePluginCommands()
        : TCommands<FRosePluginCommands>(TEXT("BrettPlugin"), NSLOCTEXT("Contexts", "BrettPlugin", "ROSE Plugin"), NAME_None, FEditorStyle::GetStyleSetName())
    {
    }

    virtual void RegisterCommands() override;

    TSharedPtr< FUICommandInfo > StartButton;

};