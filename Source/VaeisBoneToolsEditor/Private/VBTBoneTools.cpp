// Copyright (c) 2019-2023 Jared Taylor. All Rights Reserved.


#include "VBTBoneTools.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "MeshUtilities.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "PackageTools.h"
#include "FileHelpers.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SkeletalMeshSocket.h"

DEFINE_LOG_CATEGORY_STATIC(LogBoneTools, All, All);

#define LOCTEXT_NAMESPACE "VBTBoneTools"


void UVBTBoneTools::SaveAndReloadAsset(const UObject* Asset) const
{
	// Dirty the package (needs saving)
	Asset->MarkPackageDirty();

	// Save the package
	FEditorFileUtils::PromptForCheckoutAndSave({ Asset->GetOutermost() }, false, false);

	// Reload the package with the new bones
	FText DummyText = FText::GetEmpty();
	UPackageTools::ReloadPackages({ Asset->GetOutermost() }, DummyText, UPackageTools::EReloadPackagesInteractionMode::AssumePositive);
}

void UVBTBoneTools::ForceCloseAsset(UObject* Asset)
{
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	AssetEditorSubsystem->CloseAllEditorsForAsset(Asset);
}

TArray<USkeletalMesh*> UVBTBoneTools::FindCompatibleMeshes(USkeleton* const Skeleton) const
{
	FARFilter Filter;
	Filter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());

	const FString SkeletonString = FAssetData(Skeleton).GetExportTextName();
	Filter.TagsAndValues.Add(GET_MEMBER_NAME_CHECKED(USkeletalMesh, GetSkeleton()), SkeletonString);

	TArray<FAssetData> AssetList;
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().GetAssets(Filter, AssetList);

	TArray<USkeletalMesh*> Result;

	if (AssetList.Num() > 0)
	{
		for (const FAssetData& AssetDataList : AssetList)
		{
			if (USkeletalMesh* const SkelMesh = Cast<USkeletalMesh>(AssetDataList.GetAsset()))
			{
				Result.Add(SkelMesh);
			}
		}
	}

	return Result;
}

void UVBTBoneTools::AddBoneToMesh(const USkeletalMesh* const Mesh, int32 LODIndex, int32 BoneIndex)
{
	Mesh->GetImportedModel()->LODModels[LODIndex].RequiredBones.AddUnique(BoneIndex);
	Mesh->GetImportedModel()->LODModels[LODIndex].ActiveBoneIndices.AddUnique(BoneIndex);

	for (int32 i = 0; i < Mesh->GetImportedModel()->LODModels[LODIndex].Sections.Num(); i++)
	{
		Mesh->GetImportedModel()->LODModels[LODIndex].Sections[i].BoneMap.AddUnique(BoneIndex);
	}
}

void UVBTBoneTools::AddBoneToSkeleton(const FName& SocketName, const FName& ParentBoneName, const FTransform& TM, USkeletalMesh* Mesh)
{
	// Check new bone doesn't already exist first
	const FString NewBoneName = SocketName.ToString() + "_bone";
	const int32 NewBoneIndex = Mesh->GetRefSkeleton().FindRawBoneIndex(*NewBoneName);
	if (NewBoneIndex == INDEX_NONE)
	{
		const int32 ParentIndex = Mesh->GetRefSkeleton().FindRawBoneIndex(ParentBoneName);
		if (ParentIndex == INDEX_NONE)
		{
			UE_LOG(LogBoneTools, Error, TEXT("Target mesh does not have the parent bone { %s }. The parent must exist on both, this tool is not built to resolve this. Aborting."), *ParentBoneName.ToString());
			// const FString ErrorString = FString::Printf(TEXT("Target mesh does not have the parent bone { %s }. The parent must exist on both, this tool is not built to resolve this. Aborting."), *BoneName.ToString());
			return;
		}

		FReferenceSkeletonModifier Mod = FReferenceSkeletonModifier(Mesh->GetRefSkeleton(), Mesh->GetSkeleton());
		Mod.Add(FMeshBoneInfo(*NewBoneName, NewBoneName, ParentIndex), TM);
	}
}

void UVBTBoneTools::ConvertSocketsToBones(USkeletalMesh* SkeletalMesh, USkeletalMesh* TargetSkeletalMesh)
{
	if (!TargetSkeletalMesh)
	{
		const FString ErrorString = "TargetSkeletalMesh provided is not valid. Aborting.";
		const FText ErrorText = FText::FromString(ErrorString);

		FMessageLog Log = FMessageLog("AssetCheck");
		Log.Open(EMessageSeverity::Error, true);
		Log.Message(EMessageSeverity::Error, ErrorText);
		return;
	}
	
	if (!SkeletalMesh)
	{
		const FString ErrorString = "SkeletalMesh provided is not valid. Aborting.";
		const FText ErrorText = FText::FromString(ErrorString);

		FMessageLog Log = FMessageLog("AssetCheck");
		Log.Open(EMessageSeverity::Error, true);
		Log.Message(EMessageSeverity::Error, ErrorText);
		return;
	}

	if (SkeletalMesh == TargetSkeletalMesh)
	{
		const FString ErrorString = "SkeletalMesh cannot be the same as TargetSkeletalMesh. Aborting.";
		const FText ErrorText = FText::FromString(ErrorString);

		FMessageLog Log = FMessageLog("AssetCheck");
		Log.Open(EMessageSeverity::Error, true);
		Log.Message(EMessageSeverity::Error, ErrorText);
		return;
	}

	USkeleton* Skeleton = SkeletalMesh->GetSkeleton();
	USkeleton* TargetSkeleton = TargetSkeletalMesh->GetSkeleton();

	if (!Skeleton)
	{
		const FString ErrorString = FString::Printf(TEXT("No skeleton found for %s. Aborting."), *SkeletalMesh->GetName());
		const FText ErrorText = FText::FromString(ErrorString);

		FMessageLog Log = FMessageLog("AssetCheck");
		Log.Open(EMessageSeverity::Error, true);
		Log.Message(EMessageSeverity::Error, ErrorText);
		return;
	}
	
	// Close the assets to avoid issues
	ForceCloseAsset(SkeletalMesh);
	ForceCloseAsset(Skeleton);
	ForceCloseAsset(TargetSkeletalMesh);
	ForceCloseAsset(TargetSkeleton);

	// Find all sockets and convert them to bones
	const TArray<TObjectPtr<USkeletalMeshSocket>>& Sockets = Skeleton->Sockets;
	for (const TObjectPtr<USkeletalMeshSocket>& S : Sockets)
	{
		AddBoneToSkeleton(S->SocketName, S->BoneName, S->GetSocketLocalTransform(), TargetSkeletalMesh);

		// Find index of new bone
		const FString NewBoneName = S->SocketName.ToString() + "_bone";
		const int32 NewBoneIndex = TargetSkeletalMesh->GetRefSkeleton().FindRawBoneIndex(*NewBoneName);
		
		// Add bones to every LOD of the skeletal mesh
		for (int32 i = 0; i < TargetSkeletalMesh->GetImportedModel()->LODModels.Num(); i++)
		{
			AddBoneToMesh(TargetSkeletalMesh, i, NewBoneIndex);

			// Reload mesh LODs
			TargetSkeletalMesh->AddBoneToReductionSetting(i, TEXT(""));
		}
	}

	// Add all new bones from the mesh to the skeleton then save and reload it
	// Reloading it refreshes bones, so without this step you can't observe the changes regardless
	TargetSkeleton->MergeAllBonesToBoneTree(TargetSkeletalMesh);

	SaveAndReloadAsset(TargetSkeletalMesh);
	SaveAndReloadAsset(TargetSkeleton);
}

void UVBTBoneTools::AddBonesToMesh(USkeletalMesh* SkeletalMesh, FString BoneName, FString ParentBoneName, FTransform BoneLocalTM)
{
	if (!SkeletalMesh)
	{
		const FString ErrorString = "SkeletalMesh provided is not valid. Aborting.";
		const FText ErrorText = FText::FromString(ErrorString);

		FMessageLog Log = FMessageLog("AssetCheck");
		Log.Open(EMessageSeverity::Error, true);
		Log.Message(EMessageSeverity::Error, ErrorText);
		return;
	}

	if (BoneName.IsEmpty())
	{
		const FString ErrorString = "No BoneName provided. Aborting.";
		const FText ErrorText = FText::FromString(ErrorString);

		FMessageLog Log = FMessageLog("AssetCheck");
		Log.Open(EMessageSeverity::Error, true);
		Log.Message(EMessageSeverity::Error, ErrorText);

		return;
	}

	USkeleton* Skeleton = SkeletalMesh->GetSkeleton();

	if (!Skeleton)
	{
		const FString ErrorString = FString::Printf(TEXT("No skeleton found for %s. Aborting."), *SkeletalMesh->GetName());
		const FText ErrorText = FText::FromString(ErrorString);

		FMessageLog Log = FMessageLog("AssetCheck");
		Log.Open(EMessageSeverity::Error, true);
		Log.Message(EMessageSeverity::Error, ErrorText);
		return;
	}

	if (SkeletalMesh->GetRefSkeleton().FindRawBoneIndex(*ParentBoneName) == INDEX_NONE)
	{
		const FString ErrorString = FString::Printf(TEXT("Skeleton { %s } has no parent bone { %s }. Aborting."), *Skeleton->GetName(), *ParentBoneName);
		const FText ErrorText = FText::FromString(ErrorString);

		FMessageLog Log = FMessageLog("AssetCheck");
		Log.Open(EMessageSeverity::Error, true);
		Log.Message(EMessageSeverity::Error, ErrorText);
		return;
	}
	
	// Close the assets to avoid issues
	ForceCloseAsset(SkeletalMesh);
	ForceCloseAsset(Skeleton);

	// Add the bone
	AddBoneToSkeleton(*BoneName, *ParentBoneName, BoneLocalTM, SkeletalMesh);

	// Find index of new bone
	const int32 NewBoneIndex = SkeletalMesh->GetRefSkeleton().FindRawBoneIndex(*BoneName);
	
	// Add bones to every LOD of the skeletal mesh
	for (int32 i = 0; i < SkeletalMesh->GetImportedModel()->LODModels.Num(); i++)
	{
		AddBoneToMesh(SkeletalMesh, i, NewBoneIndex);

		// Reload mesh LODs
		SkeletalMesh->AddBoneToReductionSetting(i, TEXT(""));
	}

	// Add all new bones from the mesh to the skeleton then save and reload it
	// Reloading it refreshes bones, so without this step you can't observe the changes regardless
	Skeleton->MergeAllBonesToBoneTree(SkeletalMesh);

	SaveAndReloadAsset(SkeletalMesh);
	SaveAndReloadAsset(Skeleton);
}

void UVBTBoneTools::RemoveBonesFromMesh(USkeletalMesh* SkeletalMesh, const FName& BoneName, bool bAllMeshes)
{
	if (BoneName.IsNone())
	{
		const FString ErrorString = "No BoneName provided. Aborting.";
		const FText ErrorText = FText::FromString(ErrorString);

		FMessageLog Log = FMessageLog("AssetCheck");
		Log.Open(EMessageSeverity::Error, true);
		Log.Message(EMessageSeverity::Error, ErrorText);

		return;
	}

	if (!SkeletalMesh)
	{
		const FString ErrorString = "SkeletalMesh provided is not valid. Aborting.";
		const FText ErrorText = FText::FromString(ErrorString);

		FMessageLog Log = FMessageLog("AssetCheck");
		Log.Open(EMessageSeverity::Error, true);
		Log.Message(EMessageSeverity::Error, ErrorText);
		return;
	}

	USkeleton* Skeleton = SkeletalMesh->GetSkeleton();

	if (!Skeleton)
	{
		const FString ErrorString = FString::Printf(TEXT("No skeleton found for %s. Aborting."), *SkeletalMesh->GetName());
		const FText ErrorText = FText::FromString(ErrorString);

		FMessageLog Log = FMessageLog("AssetCheck");
		Log.Open(EMessageSeverity::Error, true);
		Log.Message(EMessageSeverity::Error, ErrorText);
		return;
	}
	
	// Close the assets to avoid issues
	ForceCloseAsset(Skeleton);

	// Get all meshes compatible with skeleton (to add bones to their LODs)
	TArray<USkeletalMesh*> Meshes = bAllMeshes ? FindCompatibleMeshes(Skeleton) : TArray{ SkeletalMesh };

	// Close any open meshes
	for (USkeletalMesh* Mesh : Meshes)
	{
		ForceCloseAsset(Mesh);

		Mesh->GetRefSkeleton().RemoveBonesByName(Skeleton, { BoneName });
		Skeleton->RemoveBonesFromSkeleton({ BoneName }, true);

		// Add all new bones from the mesh to the skeleton then save and reload it (reloading it refreshes bones)
		Skeleton->MergeAllBonesToBoneTree(Mesh);
		SaveAndReloadAsset(Mesh);
	}

	// Add all new bones from the mesh to the skeleton then save and reload it (reloading it refreshes bones)
	SaveAndReloadAsset(Skeleton);
}

#undef LOCTEXT_NAMESPACE
