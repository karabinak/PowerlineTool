// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimplePowerlineToolCommands.h"

#define LOCTEXT_NAMESPACE "FSimplePowerlineToolModule"

void FSimplePowerlineToolCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "SimplePowerlineTool", "Bring up SimplePowerlineTool window", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
