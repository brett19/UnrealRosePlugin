#include "BrettPluginPrivatePCH.h"

#include "FRosePluginCommands.h"

PRAGMA_DISABLE_OPTIMIZATION
void FRosePluginCommands::RegisterCommands()
{
    UI_COMMAND(StartButton, "Import ROSE", "Begins importing ROSE data", EUserInterfaceActionType::Button, FInputGesture());
}
PRAGMA_ENABLE_OPTIMIZATION
