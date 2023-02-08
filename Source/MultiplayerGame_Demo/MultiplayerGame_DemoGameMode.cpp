// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiplayerGame_DemoGameMode.h"
#include "MultiplayerGame_DemoCharacter.h"
#include "UObject/ConstructorHelpers.h"

AMultiplayerGame_DemoGameMode::AMultiplayerGame_DemoGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPersonCPP/Blueprints/ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
