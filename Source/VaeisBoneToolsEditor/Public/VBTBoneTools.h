// Copyright (c) 2019-2023 Jared Taylor. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blutility/Classes/EditorUtilityWidget.h"
#include "VBTBoneTools.generated.h"

/**
 *
 */
UCLASS()
class VAEISBONETOOLSEDITOR_API UVBTBoneTools : public UEditorUtilityWidget
{
	GENERATED_BODY()

public:
	/** Reloading the asset will force it to refresh changes to bones */
	void SaveAndReloadAsset(const UObject* Asset) const;

	/** Closing open editors prevents issues from forming */
	static void ForceCloseAsset(UObject* Asset);

	/** @return Every mesh compatible with the given skeleton */
	TArray<USkeletalMesh*> FindCompatibleMeshes(USkeleton* const Skeleton) const;
	
	static void AddBoneToMesh(const USkeletalMesh* const Mesh, int32 LODIndex, int32 BoneIndex);
	static void AddBoneToSkeleton(const FName& SocketName, const FName& ParentBoneName, const FTransform& TM, USkeletalMesh* Mesh);

	UFUNCTION(BlueprintCallable, Category="Development|Editor")
	void ConvertSocketsToBones(USkeletalMesh* SkeletalMesh, USkeletalMesh* OptionalTargetSkeletalMesh = nullptr);

	UFUNCTION(BlueprintCallable, Category="Development|Editor")
	void AddBonesToMesh(USkeletalMesh* SkeletalMesh, FString BoneName, FString ParentBoneName, FTransform BoneLocalTM);
	
	UFUNCTION(BlueprintCallable, Category = "Development|Editor")
	void RemoveBonesFromMesh(USkeletalMesh* SkeletalMesh, const FName& BoneName, bool bAllMeshes = false);
};
