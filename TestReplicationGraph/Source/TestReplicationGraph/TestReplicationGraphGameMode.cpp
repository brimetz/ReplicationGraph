// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "TestReplicationGraphGameMode.h"
#include "TestReplicationGraphCharacter.h"
#include "UObject/ConstructorHelpers.h"

ATestReplicationGraphGameMode::ATestReplicationGraphGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPersonCPP/Blueprints/ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
