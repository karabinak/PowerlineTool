// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "SimplePowerlineToolStyle.h"

class FSimplePowerlineToolCommands : public TCommands<FSimplePowerlineToolCommands>
{
public:

	FSimplePowerlineToolCommands()
		: TCommands<FSimplePowerlineToolCommands>(TEXT("SimplePowerlineTool"), NSLOCTEXT("Contexts", "SimplePowerlineTool", "SimplePowerlineTool Plugin"), NAME_None, FSimplePowerlineToolStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenPluginWindow;
};