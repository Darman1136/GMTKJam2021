// Copyright Epic Games, Inc. All Rights Reserved.

#include "GMTK2021Pawn.h"
#include "GMTK2021Projectile.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

const FName AGMTK2021Pawn::MoveForwardBinding("MoveForward");
const FName AGMTK2021Pawn::MoveRightBinding("MoveRight");
const FName AGMTK2021Pawn::FireForwardBinding("FireForward");
const FName AGMTK2021Pawn::FireRightBinding("FireRight");

AGMTK2021Pawn::AGMTK2021Pawn() {
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ShipMesh(TEXT("/Game/TwinStick/Meshes/TwinStickUFO.TwinStickUFO"));
	// Create the mesh component
	ShipMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShipMesh"));
	RootComponent = ShipMeshComponent;
	ShipMeshComponent->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	ShipMeshComponent->SetStaticMesh(ShipMesh.Object);

	// Cache our sound effect
	static ConstructorHelpers::FObjectFinder<USoundBase> FireAudio(TEXT("/Game/TwinStick/Audio/TwinStickFire.TwinStickFire"));
	FireSound = FireAudio.Object;

	// Create a camera boom...
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetUsingAbsoluteRotation(true); // Don't want arm to rotate when ship does
	CameraBoom->TargetArmLength = 1200.f;
	CameraBoom->SetRelativeRotation(FRotator(-80.f, 0.f, 0.f));
	CameraBoom->bDoCollisionTest = false; // Don't want to pull camera in when it collides with level

	// Create a camera...
	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	CameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	CameraComponent->bUsePawnControlRotation = false;	// Camera does not rotate relative to arm

	// Movement
	MoveSpeed = 1000.0f;
	// Weapon
	GunOffset = FVector(90.f, 0.f, 0.f);
	FireRate = 0.1f;
	bCanFire = true;
}

void AGMTK2021Pawn::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) {
	check(PlayerInputComponent);

	// set up gameplay key bindings
	PlayerInputComponent->BindAxis(MoveForwardBinding);
	PlayerInputComponent->BindAxis(MoveRightBinding);
	PlayerInputComponent->BindAxis(FireForwardBinding);
	PlayerInputComponent->BindAxis(FireRightBinding);
}

void AGMTK2021Pawn::Tick(float DeltaSeconds) {
	// Find movement direction
	const float ForwardValue = GetInputAxisValue(MoveForwardBinding);
	const float RightValue = GetInputAxisValue(MoveRightBinding);

	// Clamp max size so that (X=1, Y=1) doesn't cause faster movement in diagonal directions
	const FVector MoveDirection = FVector(ForwardValue, RightValue, 0.f).GetClampedToMaxSize(1.0f);

	// Calculate  movement
	const FVector Movement = MoveDirection * MoveSpeed * DeltaSeconds;

	// If non-zero size, move this actor
	if (Movement.SizeSquared() > 0.0f) {
		const FRotator NewRotation = Movement.Rotation();
		FHitResult Hit(1.f);
		RootComponent->MoveComponent(Movement, NewRotation, true, &Hit);
		MirrorPawn->GetRootComponent()->MoveComponent(Movement * -1, NewRotation * -1, true, &Hit);

		if (Hit.IsValidBlockingHit()) {
			const FVector Normal2D = Hit.Normal.GetSafeNormal2D();
			const FVector Deflection = FVector::VectorPlaneProject(Movement, Normal2D) * (1.f - Hit.Time);
			RootComponent->MoveComponent(Deflection, NewRotation, true);
			MirrorPawn->GetRootComponent()->MoveComponent(Deflection * -1, NewRotation * -1, true);
		}
	}

	// Create fire direction vector
	const float FireForwardValue = GetInputAxisValue(FireForwardBinding);
	const float FireRightValue = GetInputAxisValue(FireRightBinding);
	const FVector FireDirection = FVector(FireForwardValue, FireRightValue, 0.f);

	// Try and fire a shot
	bool success = FireShot(GetActorLocation(), FireDirection);


	// Create fire direction vector
	const FVector MirrorFireDirection = FVector(FireForwardValue * -1, FireRightValue * -1, 0.f);

	// Try and fire a shot
	FireShot(MirrorPawn->GetActorLocation(), MirrorFireDirection, true);

	// if success = true we fired a shot and need to wait
	if (success)
		bCanFire = false;
}

bool AGMTK2021Pawn::FireShot(FVector ActorLocation, FVector FireDirection, bool mirror) {
	// If it's ok to fire again
	if (bCanFire == true) {
		// If we are pressing fire stick in a direction
		if (FireDirection.SizeSquared() > 0.0f) {
			const FRotator FireRotation = FireDirection.Rotation();
			// Spawn projectile at an offset from this pawn
			const FVector SpawnLocation = ActorLocation + FireRotation.RotateVector(GunOffset);

			UWorld* const World = GetWorld();
			if (World != nullptr) {
				// spawn the projectile
				World->SpawnActor<AGMTK2021Projectile>(SpawnLocation, FireRotation);
			}

			if (!mirror) {
				World->GetTimerManager().SetTimer(TimerHandle_ShotTimerExpired, this, &AGMTK2021Pawn::ShotTimerExpired, FireRate);

				// try and play the sound if specified
				if (FireSound != nullptr) {
					UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation());
				}
			}

			return true;
		}
	}
	return false;
}

void AGMTK2021Pawn::ShotTimerExpired() {
	bCanFire = true;
}

