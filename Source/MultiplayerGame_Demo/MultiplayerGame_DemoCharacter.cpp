// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiplayerGame_DemoCharacter.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"
#include "ThirdPersonMPProjectile.h"



//////////////////////////////////////////////////////////////////////////
// AMultiplayerGame_DemoCharacter

AMultiplayerGame_DemoCharacter::AMultiplayerGame_DemoCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f; // The camera follows at this distance behind the character
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)

	/*-------------------New content----------------------*/
	// The player's maximum health
	MaxHealth = 100.0f;

	// The player's Current health
	CurrentHealth = MaxHealth;

	// 初始化投射物类
	ProjectileClass = AThirdPersonMPProjectile::StaticClass();
	// 初始化射速
	FireRate = 0.25f;
	bIsFiringWeapon = false;


}

//////////////////////////////////////////////////////////////////////////
// Input


void AMultiplayerGame_DemoCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAxis("MoveForward", this, &AMultiplayerGame_DemoCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AMultiplayerGame_DemoCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &AMultiplayerGame_DemoCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AMultiplayerGame_DemoCharacter::LookUpAtRate);

	// handle touch devices
	PlayerInputComponent->BindTouch(IE_Pressed, this, &AMultiplayerGame_DemoCharacter::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &AMultiplayerGame_DemoCharacter::TouchStopped);

	// VR headset functionality
	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &AMultiplayerGame_DemoCharacter::OnResetVR);

	// 处理发射投射物
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &AMultiplayerGame_DemoCharacter::StartFire);
}

void AMultiplayerGame_DemoCharacter::OnResetVR()
{
	// If MultiplayerGame_Demo is added to a project via 'Add Feature' in the Unreal Editor the dependency on HeadMountedDisplay in MultiplayerGame_Demo.Build.cs is not automatically propagated
	// and a linker error will result.
	// You will need to either:
	//		Add "HeadMountedDisplay" to [YourProject].Build.cs PublicDependencyModuleNames in order to build successfully (appropriate if supporting VR).
	// or:
	//		Comment or delete the call to ResetOrientationAndPosition below (appropriate if not supporting VR)
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void AMultiplayerGame_DemoCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
		Jump();
}

void AMultiplayerGame_DemoCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
		StopJumping();
}

void AMultiplayerGame_DemoCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AMultiplayerGame_DemoCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void AMultiplayerGame_DemoCharacter::MoveForward(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void AMultiplayerGame_DemoCharacter::MoveRight(float Value)
{
	if ( (Controller != nullptr) && (Value != 0.0f) )
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
	
		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}

/*-------------------New content----------------------*/

void AMultiplayerGame_DemoCharacter::OnRep_CurrentHealth()
{
	OnHealthUpdate();
}

// 启用开火
void AMultiplayerGame_DemoCharacter::StartFire()
{
	if(!bIsFiringWeapon)
	{
		bIsFiringWeapon = true;
		UWorld* World = GetWorld();
		World->GetTimerManager().SetTimer(FiringTimer, this, &AMultiplayerGame_DemoCharacter::StopFire, FireRate, false);
		HandleFire();
	}
}

// 禁用开火
void AMultiplayerGame_DemoCharacter::StopFire()
{
	bIsFiringWeapon = false;
}

// 控制开火指令的实施
void AMultiplayerGame_DemoCharacter::HandleFire_Implementation()  // 因为 HandleFire 是服务器RPC，其在CPP文件中的实现必须在函数名后面添加后缀 _Implementation。
{
	FVector spawnLocation = GetActorLocation() + (GetControlRotation().Vector()* 100.0f) + (GetActorUpVector()* 50.0f);
	FRotator SpawnRotator = GetControlRotation();  // 根据控制器的旋转而旋转

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Instigator = GetInstigator();
	SpawnParameters.Owner = this;

	AThirdPersonMPProjectile* SpawnedProjectile = GetWorld()->SpawnActor<AThirdPersonMPProjectile>(spawnLocation, SpawnRotator,SpawnParameters);
}

//////////////////////////////////////////////////////////////////////////
// replicated attribute
//GetLifetimeReplicatedProps 函数负责复制我们使用 Replicated 说明符指派的任何属性，并可用于配置属性的复制方式。
//这里使用 CurrentHealth 的最基本实现。一旦添加更多需要复制的属性，也必须添加到此函数。
void AMultiplayerGame_DemoCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);  // 必须调用 GetLifetimeReplicatedProps 的 Super 版本，否则从Actor父类继承的属性不会复制，即便该父类指定要复制。

	// replicate the current health.
	DOREPLIFETIME(AMultiplayerGame_DemoCharacter, CurrentHealth);
}

// OnHealthUpdate 不复制，需要在所有设备上手动调用。
void AMultiplayerGame_DemoCharacter::OnHealthUpdate()
{
	//客户端特定的功能
	if (IsLocallyControlled())
	{
		FString healthMessage = FString::Printf(TEXT("您现在的生命值剩余为 %f。"), CurrentHealth);
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Black, healthMessage);  // 添加屏幕调试消息
	}

	// 服务器特定的功能
	if(GetLocalRole() == ROLE_Authority)
	{
		FString healthMessage = FString::Printf(TEXT("%s 现在的生命值剩余为 %f。"), *GetFName().ToString(),CurrentHealth);
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, healthMessage);
	}

	
	//在所有机器上都执行的函数。 
	/*  
		因任何因伤害或死亡而产生的特殊功能都应放在这里。 
	*/
	// 角色死亡提示及销毁角色Actor
	if (CurrentHealth == 0)
	{
		FString deathMessage = FString::Printf(TEXT("%s 被杀死了"), *GetFName().ToString());
		GEngine->AddOnScreenDebugMessage(-1, 5.f,FColor::Red,deathMessage);
		Destroy();
	}
}

void AMultiplayerGame_DemoCharacter::SetCurrentHealth(float healrhValue)
{
	if(GetLocalRole() == ROLE_Authority)
	{
		CurrentHealth = FMath::Clamp(healrhValue, 0.f, MaxHealth);  // Clamp(x, min, max) 在min, max区间取值，x的值在区间时返回 x；x<min 时返回min；x>max 时返回max
		OnHealthUpdate();
	}
}

float AMultiplayerGame_DemoCharacter::TakeDamage(float DamageTaken, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	//return Super::TakeDamage(DamageTaken, DamageEvent, EventInstigator, DamageCauser);
	float damageApplied = CurrentHealth - DamageTaken;
	SetCurrentHealth(damageApplied);
	return damageApplied;
}

