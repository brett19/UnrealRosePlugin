// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "BrettPluginPrivatePCH.h"
#include <vector>
#include <algorithm>
#include "FRosePluginCommands.h"
#include "Common.h"
#include "Zmd.h"
#include "Zms.h"
#include "Zmo.h"
#include "Zsc.h"
#include "Chr.h"
#include "Him.h"
#include "Ifo.h"

DEFINE_LOG_CATEGORY(RosePlugin);

const FString RosePackageName(TEXT("/Game/ROSEImp"));
const FString RoseBasePath(TEXT("D:/zz_test_evo/"));

class FBrettPlugin : public IBrettPlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void StartButton_Clicked();

private:
	void AddToolbarExtension(FToolBarBuilder &);

	TSharedPtr<FUICommandList> RosePluginCommands;
	TSharedPtr<FExtensibilityManager> MyExtensionManager;
	TSharedPtr< const FExtensionBase > ToolbarExtension;
	TSharedPtr<FExtender> ToolbarExtender;
};

IMPLEMENT_MODULE( FBrettPlugin, BrettPlugin )



void FBrettPlugin::StartupModule()
{
	// This code will execute after your module is loaded into memory (but after global variables are initialized, of course.)
	UE_LOG(RosePlugin, Log, TEXT("ROSE plugin started!"));

	FRosePluginCommands::Register();

	RosePluginCommands = MakeShareable(new FUICommandList);

	RosePluginCommands->MapAction(
		FRosePluginCommands::Get().StartButton,
		FExecuteAction::CreateRaw(this, &FBrettPlugin::StartButton_Clicked),
		FCanExecuteAction());


	ToolbarExtender = MakeShareable(new FExtender);
	ToolbarExtension = ToolbarExtender->AddToolBarExtension("Settings", EExtensionHook::After, RosePluginCommands, FToolBarExtensionDelegate::CreateRaw(this, &FBrettPlugin::AddToolbarExtension));
	
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(FName("LevelEditor"));
	LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);

	MyExtensionManager = LevelEditorModule.GetToolBarExtensibilityManager();
}

void FBrettPlugin::AddToolbarExtension(FToolBarBuilder &builder)
{
#define LOCTEXT_NAMESPACE "LevelEditorToolBar"

	UE_LOG(RosePlugin, Log, TEXT("Starting Extension logic"));

	FSlateIcon IconBrush = FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.ViewOptions", "LevelEditor.ViewOptions.Small");

	builder.AddToolBarButton(FRosePluginCommands::Get().StartButton, NAME_None, LOCTEXT("StartButton_Override", "Import ROSE"), LOCTEXT("StartButton_ToolTipOverride", "Begins importing ROSE data"), IconBrush, NAME_None);

#undef LOCTEXT_NAMESPACE
}

void FBrettPlugin::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	if (MyExtensionManager.IsValid())
	{
		FRosePluginCommands::Unregister();

		ToolbarExtender->RemoveExtension(ToolbarExtension.ToSharedRef());

		MyExtensionManager->RemoveExtender(ToolbarExtender);
	} else
	{
		MyExtensionManager.Reset();
	}
}

void RefreshCollisionChange(const UStaticMesh * StaticMesh)
{
	for (FObjectIterator Iter(UStaticMeshComponent::StaticClass()); Iter; ++Iter)
	{
		UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(*Iter);
		if (StaticMeshComponent->StaticMesh == StaticMesh)
		{
			// it needs to recreate IF it already has been created
			if (StaticMeshComponent->IsPhysicsStateCreated())
			{
				StaticMeshComponent->RecreatePhysicsState();
			}
		}
	}

	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
}

void BuildAssetPath(FString& PackageName, FString& AssetName, const FString& RosePath, const FString& Postfix = TEXT(""))
{
	FString NormPath = RosePath.ToUpper();
	FPaths::NormalizeFilename(NormPath);
	TArray<FString> PathParts;
	NormPath.ParseIntoArray(&PathParts, TEXT("/"), true);
	FString FileName = PathParts.Pop();

	AssetName = FPaths::GetBaseFilename(FileName);
	if (!Postfix.IsEmpty()) {
		AssetName.Append(Postfix);
	}

	if (PathParts.Num() <= 0) {
		DebugBreak();
	}

	if (PathParts[0].Compare(TEXT("3DDATA")) != 0) {
		DebugBreak();
	}
	PathParts.RemoveAt(0);

	if (PathParts[0].Compare(TEXT("JUNON")) == 0 ||
		PathParts[0].Compare(TEXT("LUNAR")) == 0 ||
		PathParts[0].Compare(TEXT("ORO")) == 0) {
		PathParts.Insert(TEXT("MAPS"), 0);
	}

	PackageName = FString(TEXT("/")) + FString::Join(PathParts, TEXT("/"));
}

UPackage* GetOrMakePackage(const FString& PackageName, FString& AssetName) {
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	FString FinalPackageName;
	FString BasePackageName = RosePackageName + PackageName / AssetName;
	BasePackageName = PackageTools::SanitizePackageName(BasePackageName);

	AssetToolsModule.Get().CreateUniqueAssetName(BasePackageName, TEXT(""), FinalPackageName, AssetName);

	UE_LOG(RosePlugin, Log, TEXT("Making Package - %s, %s, %s, %s"),
		*RosePackageName, *PackageName, *AssetName, *BasePackageName, *FinalPackageName);
	UPackage* Package = CreatePackage(NULL, *FinalPackageName);
	if (Package == NULL) {
		UE_LOG(RosePlugin, Error, TEXT("Failed to create package - %s"), *FinalPackageName);
	}
	return Package;
}

template<typename T>
T* GetExistingAsset(const FString& PackageName, const FString& AssetName) {
	FString BasePackageName = RosePackageName + PackageName / AssetName;
	BasePackageName = PackageTools::SanitizePackageName(BasePackageName);
	BasePackageName.Append(TEXT("."));
	BasePackageName.Append(AssetName);
	T* ExistingAsset = LoadObject<T>(NULL, *BasePackageName);
	if (ExistingAsset) {
		return ExistingAsset;
	}
	return NULL;
}

UTexture* ImportTexture(const FString& PackageName, FString& AssetName, const FString& SourcePath)
{
	UTexture* ExistingTexture = GetExistingAsset<UTexture>(PackageName, AssetName);
	if (ExistingTexture != NULL) {
		return ExistingTexture;
	}

	UPackage* Package = GetOrMakePackage(PackageName, AssetName);
	if (Package == NULL) {
		return NULL;
	}

	TArray<uint8> DataBinary;
	if (!FFileHelper::LoadFileToArray(DataBinary, *SourcePath)) {
		UE_LOG(RosePlugin, Warning, TEXT("Unable to read texture from source."));
		return NULL;
	}
	const uint8* PtrTexture = DataBinary.GetTypedData();

	UTextureFactory* TextureFact = new UTextureFactory(FPostConstructInitializeProperties());
	TextureFact->AddToRoot();

	UTexture* Texture = (UTexture*)TextureFact->FactoryCreateBinary(
		UTexture2D::StaticClass(), Package, *AssetName,
		RF_Standalone | RF_Public, NULL, TEXT("dds"),
		PtrTexture, PtrTexture + DataBinary.Num(), GWarn);

	if (Texture != NULL)
	{
		// Notify the asset registry
		FAssetRegistryModule::AssetCreated(Texture);

		// Set the dirty flag so this package will get saved later
		Texture->MarkPackageDirty();
	}

	TextureFact->RemoveFromRoot();

	return Texture;
}

UMaterial* GetOrMakeBaseMaterial(const Zsc::Texture& MatInfo) {
	FString MaterialName;
	if (MatInfo.alphaTestEnabled) {
		MaterialName = "AlphaRefMaterial";
	} else if (MatInfo.alphaEnabled) {
		MaterialName = "AlphaMaterial";
	} else {
		MaterialName = "BaseMaterial";
	}

	if (MatInfo.twoSided) {
		MaterialName.Append("_DS");
	}

	FString MaterialFullName = FString::Printf(TEXT("%s/%s.%s"), *RosePackageName, *MaterialName, *MaterialName);
	UMaterial* Material = LoadObject<UMaterial>(NULL, *MaterialFullName, NULL, LOAD_None, NULL);
	if (Material != NULL) {
		UE_LOG(RosePlugin, Log, TEXT("Skipped Base Creation - Found It!"));
		return Material;
	}

	UE_LOG(RosePlugin, Log, TEXT("Creating Base Material!"));

	UMaterialFactoryNew* MaterialFactory = new UMaterialFactoryNew(FPostConstructInitializeProperties());

	UPackage *Package = GetOrMakePackage(TEXT("/"), MaterialName);

	Material = (UMaterial*)MaterialFactory->FactoryCreateNew(
		UMaterial::StaticClass(), Package, *MaterialName, RF_Standalone | RF_Public, NULL, GWarn);
	if (Material == NULL) {
		return NULL;
	}

	// Notify the asset registry
	FAssetRegistryModule::AssetCreated(Material);

	// Set the dirty flag so this package will get saved later
	Material->MarkPackageDirty();

	// make sure that any static meshes, etc using this material will stop using the FMaterialResource of the original 
	// material, and will use the new FMaterialResource created when we make a new UMaterial in place
	FGlobalComponentReregisterContext RecreateComponents;

	// let the material update itself if necessary
	Material->PreEditChange(NULL);

	if (MatInfo.alphaTestEnabled) {
		Material->BlendMode = BLEND_Masked;
		Material->OpacityMaskClipValue = 0.5f;
	} else if (MatInfo.alphaEnabled) {
		Material->BlendMode = BLEND_Translucent;
	} else {
		Material->BlendMode = BLEND_Opaque;
	}

	if (MatInfo.twoSided) {
		Material->TwoSided = 1;
	}

	UMaterialExpressionTextureSampleParameter2D* UnrealTextureExpression =
		ConstructObject<UMaterialExpressionTextureSampleParameter2D>(UMaterialExpressionTextureSampleParameter2D::StaticClass(), Material);
	Material->Expressions.Add(UnrealTextureExpression);
	UnrealTextureExpression->ConnectExpression(&Material->BaseColor, 0);

	if (MatInfo.alphaTestEnabled) {
		UnrealTextureExpression->ConnectExpression(&Material->OpacityMask, 4);
	}

	UnrealTextureExpression->ParameterName = TEXT("Texture");
	UnrealTextureExpression->SetDefaultTexture();
	UnrealTextureExpression->SamplerType = SAMPLERTYPE_Color;
	UnrealTextureExpression->MaterialExpressionEditorX = -320;
	UnrealTextureExpression->MaterialExpressionEditorY = 240;

	Material->bUsedWithSkeletalMesh = true;

	Material->PostEditChange();

	return Material;
}

UMaterialInterface* ImportMaterial(const FString& PackageName, FString& MaterialName, const Zsc::Texture& TexData, UTexture *Texture) {
	UPackage* Package = GetOrMakePackage(PackageName, MaterialName);
	if (Package == NULL) {
		return NULL;
	}

	UMaterialInstanceConstant* Material = CastChecked<UMaterialInstanceConstant>(
		StaticConstructObject(UMaterialInstanceConstant::StaticClass(), Package, *MaterialName, RF_Standalone | RF_Public));
	if (Material == NULL) {
		return NULL;
	}

	// Notify the asset registry
	FAssetRegistryModule::AssetCreated(Material);

	// Set the dirty flag so this package will get saved later
	Material->MarkPackageDirty();

	UMaterial* BaseMaterial = GetOrMakeBaseMaterial(TexData);

	// make sure that any static meshes, etc using this material will stop using the FMaterialResource of the original 
	// material, and will use the new FMaterialResource created when we make a new UMaterial in place
	FGlobalComponentReregisterContext RecreateComponentsX;

	// let the material update itself if necessary
	Material->PreEditChange(NULL);

	Material->SetParentEditorOnly(BaseMaterial);
	Material->SetTextureParameterValueEditorOnly(TEXT("Texture"), Texture);

	if (TexData.alphaTestEnabled && TexData.alphaReference != 128) {
		Material->bOverrideBaseProperties = true;
		Material->BasePropertyOverrides.bOverride_OpacityMaskClipValue = true;
		Material->BasePropertyOverrides.OpacityMaskClipValue = (float)TexData.alphaReference / 255.0f;
	}

	Material->PostEditChange();

	return Material;
}

struct ImportSkelData {
	ImportSkelData(Zmd& _data, const FString& _SkelPackage, FString& _SkelName)
		: data(_data), skeleton(0), SkelPackage(_SkelPackage), SkelName(_SkelName) {}
	Zmd& data;

	USkeleton *skeleton;
	const FString& SkelPackage;
	FString& SkelName;
};

struct ImportMeshData {
	struct Item {
		Item(Zms& _data, uint32 _matIdx)
			: data(_data), matIdx(_matIdx), 
			vertOffset(0), indexOffset(0), faceOffset(0) {}

		Zms& data;
		uint32 matIdx;

		uint32 vertOffset;
		uint32 indexOffset;
		uint32 faceOffset;
	};

	TArray<Item> meshes;
	TArray<UMaterialInterface*> materials;
};

USkeleton* ApplySkeletonToMesh(const FString& PackageName, FString& SkeletonName, USkeletalMesh* Mesh, ImportSkelData& skelData) {
	FReferenceSkeleton& RefSkeleton = Mesh->RefSkeleton;
	for (int i = 0; i < skelData.data.bones.Num(); ++i) {
		const Zmd::Bone& bone = skelData.data.bones[i];

		int32 ueParent = (i > 0) ? bone.parent : INDEX_NONE;
		const FMeshBoneInfo BoneInfo(FName(bone.name, FNAME_Add, true), bone.name, ueParent);
		const FTransform BoneTransform(bone.rotation, bone.translation);

		RefSkeleton.Add(BoneInfo, BoneTransform);
	}
	Mesh->CalculateInvRefMatrices();

	if (!skelData.skeleton) {
		UPackage* Package = GetOrMakePackage(PackageName, SkeletonName);
		if (Package == NULL) {
			return NULL;
		}

		USkeleton* Skeleton = CastChecked<USkeleton>(StaticConstructObject(USkeleton::StaticClass(), Package, *SkeletonName, RF_Standalone | RF_Public));
		if (Skeleton == NULL) {
			return NULL;
		}

		// Notify the asset registry
		FAssetRegistryModule::AssetCreated(Skeleton);

		// Set the dirty flag so this package will get saved later
		Skeleton->MarkPackageDirty();

		skelData.skeleton = Skeleton;
	}

	if (skelData.skeleton) {
		skelData.skeleton->MergeAllBonesToBoneTree(Mesh);
	}

	return Mesh->Skeleton;
}

USkeletalMesh* ImportSkeletalMesh(const FString& PackageName, FString& MeshName, ImportMeshData meshData, ImportSkelData& skelData) {
	UPackage* Package = GetOrMakePackage(PackageName, MeshName);
	if (Package == NULL) {
		return NULL;
	}

	USkeletalMesh* SkeletalMesh = CastChecked<USkeletalMesh>(
		StaticConstructObject(USkeletalMesh::StaticClass(), Package, *MeshName, RF_Standalone | RF_Public));
	if (SkeletalMesh == NULL) {
		return NULL;
	}

	// Notify the asset registry
	FAssetRegistryModule::AssetCreated(SkeletalMesh);

	// Set the dirty flag so this package will get saved later
	SkeletalMesh->MarkPackageDirty();

	SkeletalMesh->PreEditChange(NULL);

	for (int i = 0; i < meshData.materials.Num(); ++i) {
		SkeletalMesh->Materials.Add(FSkeletalMaterial(meshData.materials[i]));
	}

	FString SkeletonName = MeshName + "_Skeleton";
	USkeleton *Skeleton = ApplySkeletonToMesh(PackageName, SkeletonName, SkeletalMesh, skelData);
	if (Skeleton == NULL) {
		return NULL;
	}

	FSkeletalMeshResource* ImportedResource = SkeletalMesh->GetImportedResource();
	check(ImportedResource->LODModels.Num() == 0);
	ImportedResource->LODModels.Empty();
	new(ImportedResource->LODModels)FStaticLODModel();

	SkeletalMesh->LODInfo.Empty();
	SkeletalMesh->LODInfo.AddZeroed();
	SkeletalMesh->LODInfo[0].LODHysteresis = 0.02f;
	FSkeletalMeshOptimizationSettings Settings;
	// set default reduction settings values
	SkeletalMesh->LODInfo[0].ReductionSettings = Settings;

	SkeletalMesh->bHasVertexColors = false;

	FStaticLODModel& LODModel = ImportedResource->LODModels[0];
	LODModel.NumTexCoords = 1;

	IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");

	auto meshList = meshData.meshes;

	TArray<FVector> LODPoints;
	TArray<FMeshWedge> LODWedges;
	TArray<FMeshFace> LODFaces;
	TArray<FVertInfluence> LODInfluences;
	TArray<int32> LODPointToRawMap;

	int32 totalVertCount = 0;
	int32 totalIndexCount = 0;
	int32 totalFaceCount = 0;
	for (int i = 0; i < meshList.Num(); ++i) {
		meshList[i].vertOffset = totalVertCount;
		meshList[i].indexOffset = totalIndexCount;
		meshList[i].faceOffset = totalFaceCount;
		totalVertCount += meshList[i].data.vertexPositions.Num();
		totalIndexCount += meshList[i].data.indexes.Num();
		totalFaceCount += meshList[i].data.indexes.Num() / 3;
	}

	LODPoints.AddZeroed(totalVertCount);
	LODPointToRawMap.AddZeroed(totalVertCount);
	LODWedges.AddZeroed(totalIndexCount);
	LODFaces.AddZeroed(totalFaceCount);

	bool hasNormals = true;

	for (int i = 0; i < meshList.Num(); ++i) {
		Zms& tmesh = meshList[i].data;

		if (tmesh.vertexNormals.Num() == 0) {
			hasNormals = false;
		}

		for (int j = 0; j < tmesh.vertexPositions.Num(); ++j) {
			int32 vertIdx = meshList[i].vertOffset + j;
			LODPoints[vertIdx] = tmesh.vertexPositions[j];
			LODPointToRawMap[vertIdx] = vertIdx;
		}

		for (int j = 0; j < tmesh.indexes.Num(); ++j) {
			int32 wedgeIdx = meshList[i].indexOffset + j;
			LODWedges[wedgeIdx].iVertex = meshList[i].vertOffset + tmesh.indexes[j];
			LODWedges[wedgeIdx].UVs[0] = tmesh.vertexUvs[0][tmesh.indexes[j]];

			if (LODWedges[wedgeIdx].iVertex >= (uint32)totalVertCount) {
				DebugBreak();
			}
		}

		int32 faceCount = tmesh.indexes.Num() / 3;
		for (int j = 0; j < faceCount; ++j) {
			int32 faceIdx = meshList[i].faceOffset + j;
			LODFaces[faceIdx].iWedge[0] = meshList[i].indexOffset + (j * 3 + 0);
			LODFaces[faceIdx].iWedge[1] = meshList[i].indexOffset + (j * 3 + 1);
			LODFaces[faceIdx].iWedge[2] = meshList[i].indexOffset + (j * 3 + 2);
			if (hasNormals) {
				LODFaces[faceIdx].TangentZ[0] = tmesh.vertexNormals[tmesh.indexes[j * 3 + 0]];
				LODFaces[faceIdx].TangentZ[1] = tmesh.vertexNormals[tmesh.indexes[j * 3 + 1]];
				LODFaces[faceIdx].TangentZ[2] = tmesh.vertexNormals[tmesh.indexes[j * 3 + 2]];
			}
			LODFaces[faceIdx].MeshMaterialIndex = meshList[i].matIdx;
		}

		for (int j = 0; j < tmesh.vertexPositions.Num(); ++j) {
			int32 infBaseIdx = (meshList[i].vertOffset + j) * 4;
			for (int k = 0; k < 4; ++k) {
				FVertInfluence vi;
				vi.VertIndex = meshList[i].vertOffset + j;
				vi.BoneIndex = tmesh.boneWeights[j].boneIdx[k];
				vi.Weight = tmesh.boneWeights[j].weight[k];
				if (vi.Weight < 0.0001f) {
					continue;
				}
				if (vi.VertIndex >= (uint32)totalVertCount) {
					DebugBreak();
				}
				if (vi.BoneIndex >= skelData.data.bones.Num()) {
					DebugBreak();
				}
				LODInfluences.Add(vi);
			}
		}
	}

	TArray<FText> WarningMessages;
	TArray<FName> WarningNames;
	// Create actual rendering data.
	if (!MeshUtilities.BuildSkeletalMesh(
		ImportedResource->LODModels[0], 
		SkeletalMesh->RefSkeleton, 
		LODInfluences, LODWedges, LODFaces, LODPoints, LODPointToRawMap, 
		false, !hasNormals, true, &WarningMessages, &WarningNames))
	{
		DebugBreak();
	}
	else if (WarningMessages.Num() > 0)
	{
		DebugBreak();
	}

	const int32 NumSections = LODModel.Sections.Num();
	for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
	{
		SkeletalMesh->LODInfo[0].TriangleSortSettings.AddZeroed();
	}
	
	
	SkeletalMesh->PostEditChange();

	FString PhysName = MeshName + "_PhysicsAsset";
	UPhysicsAsset* PhysicsAsset = CastChecked<UPhysicsAsset>(
		StaticConstructObject(UPhysicsAsset::StaticClass(), Package, *PhysName, RF_Standalone | RF_Public));
	if (PhysicsAsset) {
		// Notify the asset registry
		FAssetRegistryModule::AssetCreated(PhysicsAsset);

		// Set the dirty flag so this package will get saved later
		PhysicsAsset->MarkPackageDirty();

		// Create the data!
		FPhysAssetCreateParams NewBodyData;
		NewBodyData.Initialize();
		FText CreationErrorMessage;
		FPhysicsAssetUtils::CreateFromSkeletalMesh(PhysicsAsset, SkeletalMesh, NewBodyData, CreationErrorMessage);
	}

	return NULL;
}

UAnimSequence *ImportSkeletalAnim(const FString& PackageName, FString& AnimName, ImportSkelData& skelData, const Zmo& anim) {
	UPackage* Package = GetOrMakePackage(PackageName, AnimName);
	if (Package == NULL) {
		return NULL;
	}

	UAnimSequence* AnimSeq = CastChecked<UAnimSequence>(
		StaticConstructObject(UAnimSequence::StaticClass(), Package, *AnimName, RF_Standalone | RF_Public));
	if (AnimSeq == NULL) {
		return NULL;
	}

	// Notify the asset registry
	FAssetRegistryModule::AssetCreated(AnimSeq);

	// Set the dirty flag so this package will get saved later
	AnimSeq->MarkPackageDirty();

	AnimSeq->PreEditChange(NULL);

	AnimSeq->SetSkeleton(skelData.skeleton);

	AnimSeq->SequenceLength = (float)anim.frameCount / (float)anim.framesPerSecond;
	AnimSeq->NumFrames = anim.frameCount;

	TArray<FRawAnimSequenceTrack> tracks;
	for (int i = 0; i < skelData.data.bones.Num(); ++i) {
		const Zmd::Bone& bone = skelData.data.bones[i];
		FRawAnimSequenceTrack track;
		// All keys must be ABSOLUTE for Unreal!
		track.PosKeys.Add(bone.translation);
		track.RotKeys.Add(bone.rotation);
		track.ScaleKeys.Add(FVector(1, 1, 1));
		tracks.Add(track);
	}

	for (int i = 0; i < anim.channels.Num(); ++i) {
		Zmo::Channel *channel = anim.channels[i];
		FRawAnimSequenceTrack& track = tracks[channel->index];
		if (channel->type() == Zmo::ChannelType::Position) {
			auto posChannel = (Zmo::PositionChannel*)channel;
			track.PosKeys.Empty();
			for (int j = 0; j < posChannel->frames.Num(); ++j) {
				track.PosKeys.Add(posChannel->frames[j]);
			}
		} else if (channel->type() == Zmo::ChannelType::Rotation) {
			auto rotChannel = (Zmo::RotationChannel*)channel;
			track.RotKeys.Empty();
			for (int j = 0; j < rotChannel->frames.Num(); ++j) {
				track.RotKeys.Add(rotChannel->frames[j]);
			}
		} else if (channel->type() == Zmo::ChannelType::Scale) {
			auto scaleChannel = (Zmo::ScaleChannel*)channel;
			track.ScaleKeys.Empty();
			for (int j = 0; j < scaleChannel->frames.Num(); ++j) {
				track.ScaleKeys.Add(scaleChannel->frames[j]);
			}
		} else {
			DebugBreak();
		}
	}

	AnimSeq->RawAnimationData.Empty();
	AnimSeq->AnimationTrackNames.Empty();
	AnimSeq->TrackToSkeletonMapTable.Empty();
	for (int i = 0; i < tracks.Num(); ++i) {
		AnimSeq->RawAnimationData.Add(tracks[i]);
		AnimSeq->AnimationTrackNames.Add(skelData.data.bones[i].name);
		AnimSeq->TrackToSkeletonMapTable.Add(FTrackToSkeletonMap(i));
	}

	AnimSeq->PostProcessSequence();

	AnimSeq->PostEditChange();

	return AnimSeq;
}

const TCHAR* animNames[] = {
	TEXT("Stop"),
	TEXT("Walk"),
	TEXT("Attack"),
	TEXT("Hit"),
	TEXT("Die"),
	TEXT("Run"),
	TEXT("Casting1"),
	TEXT("SkillAction1"),
	TEXT("Casting2"),
	TEXT("SkillAction2"),
	TEXT("Etc")
};
const int MaxAnims = 11;

void ImportChar(const Chr& chars, const Zsc& meshs, uint32 charIdx) {
	FString CharName = FString::Printf(TEXT("Char_%d"), charIdx);
	FString PackageName = FString(TEXT("/")) + CharName;

	const Chr::Character& mchar = chars.characters[charIdx];

	ImportMeshData meshData;
	Zmd meshZmd(*(RoseBasePath + chars.skeletons[mchar.skeletonIdx]));
	FString SkelPackage;
	FString SkelName;
	DebugBreak();
	// ToDo Names
	ImportSkelData skelData(meshZmd, SkelPackage, SkelName);

	int texIdx = 0;
	for (int i = 0; i < mchar.models.Num(); ++i) {
		const Zsc::Model& model = meshs.models[mchar.models[i]];

		for (int j = 0; j < model.parts.Num(); ++j) {
			const Zsc::Part& part = model.parts[j];
			const Zsc::Texture& tex = meshs.textures[part.texIdx];

			if (part.dummyIdx != 0xFFFF || part.boneIdx != 0xFFFF) {
				continue;
			}

			FString TextureName = FString::Printf(TEXT("%s_%d_Texture"), *CharName, texIdx);
			UTexture* UnrealTexture = ImportTexture(PackageName, TextureName, RoseBasePath + tex.filePath);

			FString MaterialName = FString::Printf(TEXT("%s_%d_Material"), *CharName, texIdx);
			UMaterialInterface *UnrealMaterial = ImportMaterial( PackageName, MaterialName, tex, UnrealTexture);

			meshData.materials.Add(UnrealMaterial);

			auto meshZms = new Zms(*(RoseBasePath + meshs.meshes[part.meshIdx]));
			meshData.meshes.Add(ImportMeshData::Item(*meshZms, texIdx));

			texIdx++;
		}
	}

	USkeletalMesh* skelMesh = ImportSkeletalMesh(PackageName, CharName, meshData, skelData);

	for (int i = 0; i < mchar.animations.Num(); ++i) {
		const Chr::Animation& anim = mchar.animations[i];

		if (anim.type >= MaxAnims) {
			UE_LOG(RosePlugin, Warning, TEXT("Skipped unknown animation %d"), anim.type);
			continue;
		}

		Zmo animZmo(*(RoseBasePath + chars.animations[anim.animationIdx]));
		FString AnimName = FString::Printf(TEXT("%s_%s"), *CharName, animNames[anim.type]);
		UAnimSequence* animSeq = ImportSkeletalAnim(PackageName, AnimName, skelData, animZmo);
	}
}

USkeletalMesh* ImportAvatarItem(const FString& ItemTypeName, const Zsc& meshs, ImportSkelData& skelData, int modelIdx, int boneIdx = -1) {
	const Zsc::Model& model = meshs.models[modelIdx];
	ImportMeshData meshData;

	if (model.parts.Num() == 0) {
		DebugBreak();
	}

	for (int j = 0; j < model.parts.Num(); ++j) {
		const Zsc::Part& part = model.parts[j];
		const Zsc::Texture& tex = meshs.textures[part.texIdx];

		if (part.dummyIdx != 0xFFFF || part.boneIdx != 0xFFFF) {
			continue;
		}

		FString ZmsPath = meshs.meshes[part.meshIdx];

		FString TexturePackage, TextureName;
		BuildAssetPath(TexturePackage, TextureName, tex.filePath, "_Texture");
		UTexture* UnrealTexture = ImportTexture(TexturePackage, TextureName, RoseBasePath + tex.filePath);

		FString MaterialPackage, MaterialName;
		BuildAssetPath(MaterialPackage, MaterialName, ZmsPath);
		MaterialName = FString::Printf(TEXT("Model_%d_%d_Material"), modelIdx, j);
		UMaterialInterface *UnrealMaterial = ImportMaterial(MaterialPackage, MaterialName, tex, UnrealTexture);
		meshData.materials.Add(UnrealMaterial);

		auto meshZms = new Zms(*(RoseBasePath + ZmsPath));
		meshData.meshes.Add(ImportMeshData::Item(*meshZms, j));
	}

	FString ModelPackage, ModelName;
	ModelPackage = TEXT("/AVATAR");
	ModelName = FString::Printf(TEXT("%s_%d"), *ItemTypeName, modelIdx);
	USkeletalMesh* skelMesh = ImportSkeletalMesh(ModelPackage, ModelName, meshData, skelData);

	return skelMesh;
}

UK2Node_CallFunction* CreateCallFuncNode(UEdGraph* Graph, UFunction* Function) {
	
	UK2Node_CallFunction* NodeTemplate = NewObject<UK2Node_CallFunction>();
	FVector2D NodeLocation = Graph->GetGoodPlaceForNewNode();
	UK2Node_CallFunction* CallNode = FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate<UK2Node_CallFunction>(Graph, NodeTemplate, NodeLocation);
	CallNode->SetFromFunction(Function);
	CallNode->ReconstructNode();
	return CallNode;
}
UK2Node_CallFunction* CreateCallFuncNode(UEdGraph* Graph, const TCHAR* LibraryName, const TCHAR* FuncName) {
	UFunction* Function = FindObject<UClass>(ANY_PACKAGE, LibraryName)->FindFunctionByName(FuncName);
	return CreateCallFuncNode(Graph, Function);
}
template<typename FuncOwner>
UK2Node_CallFunction* CreateCallFuncNode(UEdGraph* Graph, const TCHAR* FuncName) {
	UFunction* Function = FuncOwner::StaticClass()->FindFunctionByName(FuncName);
	return CreateCallFuncNode(Graph, Function);
}

UK2Node_VariableGet* CreateVarGetNode(UEdGraph* Graph, const FName& VariableName)
{
	UK2Node_VariableGet* NodeTemplate = NewObject<UK2Node_VariableGet>();
	NodeTemplate->VariableReference.SetSelfMember(VariableName);
	FVector2D NodeLocation = Graph->GetGoodPlaceForNewNode();
	UK2Node_VariableGet* GetVarNode = FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate<UK2Node_VariableGet>(Graph, NodeTemplate, NodeLocation);
	return GetVarNode;
}

UBlueprint* ImportWorldZscModelAnim(const FString& MdlTypeName, const Zsc& meshs, int modelIdx) {
	const Zsc::Model& model = meshs.models[modelIdx];

	FString BPPackageName = TEXT("/MAPS");
	FString BPAssetName = FString::Printf(TEXT("%s_%d"), *MdlTypeName, modelIdx);

	UPackage* BPPackage = GetOrMakePackage(BPPackageName, BPAssetName);
	if (BPPackage == NULL) {
		return NULL;
	}

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(), BPPackage, *BPAssetName,
		BPTYPE_Normal, UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		FName("RosePluginWhat"));

	USCS_Node* RootNode = NULL;
	for (int j = 0; j < model.parts.Num(); ++j) {
		const Zsc::Part& part = model.parts[j];
		const Zsc::Texture& tex = meshs.textures[part.texIdx];
		const FString& mesh = meshs.meshes[part.meshIdx];

		FString ModelPackage, ModelName;
		BuildAssetPath(ModelPackage, ModelName, mesh);

		UPackage* Package = GetOrMakePackage(ModelPackage, ModelName);
		if (Package == NULL) {
			return NULL;
		}

		UStaticMesh* StaticMesh = CastChecked<UStaticMesh>(
			StaticConstructObject(UStaticMesh::StaticClass(), Package, *ModelName, RF_Standalone | RF_Public));
		if (StaticMesh == NULL) {
			return NULL;
		}

		// Notify the asset registry
		FAssetRegistryModule::AssetCreated(StaticMesh);

		// Set the dirty flag so this package will get saved later
		StaticMesh->MarkPackageDirty();

		// make sure it has a new lighting guid
		StaticMesh->LightingGuid = FGuid::NewGuid();

		// Set it to use textured lightmaps. Note that Build Lighting will do the error-checking (texcoordindex exists for all LODs, etc).
		StaticMesh->LightMapResolution = 256;
		StaticMesh->LightMapCoordinateIndex = 1;

		new(StaticMesh->SourceModels) FStaticMeshSourceModel();
		FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[0];

		FRawMesh RawMesh;
		SrcModel.RawMeshBulkData->SaveRawMesh(RawMesh);
		{
			FString TexturePackage, TextureName;
			BuildAssetPath(TexturePackage, TextureName, tex.filePath, "_Texture");
			UTexture* UnrealTexture = ImportTexture(TexturePackage, TextureName, RoseBasePath + tex.filePath);

			FString MaterialPackage, MaterialName;
			BuildAssetPath(MaterialPackage, MaterialName, meshs.meshes[part.meshIdx]);
			MaterialName = FString::Printf(TEXT("Model_%d_%d_Material"), modelIdx, j);
			UMaterialInterface *UnrealMaterial = ImportMaterial(MaterialPackage, MaterialName, tex, UnrealTexture);

			StaticMesh->Materials.Add(UnrealMaterial);

			Zms meshZms(*(RoseBasePath + mesh));

			RawMesh.VertexPositions.AddZeroed(meshZms.vertexPositions.Num());
			for (int i = 0; i < meshZms.vertexPositions.Num(); ++i) {
				RawMesh.VertexPositions[i] = meshZms.vertexPositions[i];
			}

			RawMesh.WedgeIndices.AddZeroed(meshZms.indexes.Num());
			//RawMesh.WedgeTangentX.AddZeroed(meshZms.indexes.Num());
			//RawMesh.WedgeTangentY.AddZeroed(meshZms.indexes.Num());
			//RawMesh.WedgeTangentZ.AddZeroed(meshZms.indexes.Num());
			for (int i = 0; i < meshZms.indexes.Num(); ++i) {
				RawMesh.WedgeIndices[i] = meshZms.indexes[i];
				//RawMesh.WedgeTangentZ[indexOffset + i] = meshZms.vertexNormals[meshZms.indexes[i]];
			}

			for (int k = 0; k < 4; ++k) {
				if (meshZms.vertexUvs[k].Num() > 0) {
					RawMesh.WedgeTexCoords[k].AddZeroed(meshZms.indexes.Num());
					for (int i = 0; i < meshZms.indexes.Num(); ++i) {
						RawMesh.WedgeTexCoords[k][i] = meshZms.vertexUvs[k][meshZms.indexes[i]];
					}
				}
			}

			int faceCount = meshZms.indexes.Num() / 3;
			RawMesh.FaceMaterialIndices.AddZeroed(faceCount);
			RawMesh.FaceSmoothingMasks.AddZeroed(faceCount);
			for (int i = 0; i < faceCount; ++i) {
				RawMesh.FaceMaterialIndices[i] = 0;
				RawMesh.FaceSmoothingMasks[i] = 1;
			}
		}
		SrcModel.RawMeshBulkData->SaveRawMesh(RawMesh);

		SrcModel.BuildSettings.bRemoveDegenerates = true;
		SrcModel.BuildSettings.bRecomputeNormals = false;
		SrcModel.BuildSettings.bRecomputeTangents = false;

		StaticMesh->Build(true);

		// Set up the mesh collision
		StaticMesh->CreateBodySetup();

		// Create new GUID
		StaticMesh->BodySetup->InvalidatePhysicsData();

		// Per-poly collision for now
		StaticMesh->BodySetup->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseComplexAsSimple;

		// refresh collision change back to staticmesh components
		RefreshCollisionChange(StaticMesh);

		for (int32 SectionIndex = 0; SectionIndex < StaticMesh->Materials.Num(); SectionIndex++)
		{
			FMeshSectionInfo Info = StaticMesh->SectionInfoMap.Get(0, SectionIndex);
			Info.bEnableCollision = true;
			StaticMesh->SectionInfoMap.Set(0, SectionIndex, Info);
		}

		UStaticMeshComponent* MeshComp = ConstructObject<UStaticMeshComponent>(UStaticMeshComponent::StaticClass());
		MeshComp->StaticMesh = StaticMesh;
		FString MeshCompName = FString::Printf(TEXT("Part_%d"), j);
		USCS_Node* MeshNode = Blueprint->SimpleConstructionScript->CreateNode(MeshComp, *MeshCompName);
		if (RootNode) {
			RootNode->AddChildNode(MeshNode);
		} else {
			Blueprint->SimpleConstructionScript->AddNode(MeshNode);
			RootNode = MeshNode;
		}

		MeshComp->SetRelativeLocationAndRotation(part.position, FRotator(part.rotation));
		MeshComp->SetRelativeScale3D(part.scale);

		if (!part.animPath.IsEmpty())
		{
			FString EGName = FString::Printf(TEXT("Part_%d_EG"), j);
			UEdGraph* EventGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, *EGName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
			FBlueprintEditorUtils::AddUbergraphPage(Blueprint, EventGraph);

			UK2Node_Timeline* NodeTemplate = NewObject<UK2Node_Timeline>(EventGraph);
			FVector2D NodeLocation = EventGraph->GetGoodPlaceForNewNode();
			UK2Node_Timeline* TLNode = FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate<UK2Node_Timeline>(EventGraph, NodeTemplate, NodeLocation);
			UK2Node* TLNodeX = Cast<UK2Node>(TLNode);
			TLNode->TimelineName = *FString::Printf(TEXT("Part_%d_Anim"), j);

			Zmo anim(*(RoseBasePath + part.animPath));

			UTimelineTemplate* TLTmpl = FBlueprintEditorUtils::AddNewTimeline(Blueprint, TLNode->TimelineName);
			TLTmpl->bLoop = true;
			TLTmpl->bAutoPlay = true;
			TLTmpl->TimelineLength = (float)anim.frameCount / (float)anim.framesPerSecond;

			UCurveVector* RCurve = NewObject<UCurveVector>(TLTmpl);
			UCurveVector* PCurve = NewObject<UCurveVector>(TLTmpl);
			UCurveVector* SCurve = NewObject<UCurveVector>(TLTmpl);
			bool UsesRotation = false;
			bool UsesPosition = false;
			bool UsesScale = false;

			for (int i = 0; i < anim.channels.Num(); ++i) {
				Zmo::Channel *channel = anim.channels[i];
				if (channel->index != 0) {
					DebugBreak();
				}

				if (channel->type() == Zmo::ChannelType::Position) {
					UsesPosition = true;
					auto posChannel = (Zmo::PositionChannel*)channel;
					for (int j = 0; j < posChannel->frames.Num(); ++j) {
						const FVector& frame = posChannel->frames[j];
						if (j == 0 || frame.X != posChannel->frames[j - 1].X) {
							PCurve->FloatCurves[0].AddKey((float)j / (float)anim.framesPerSecond, frame.X);
						}
						if (j == 0 || frame.Y != posChannel->frames[j - 1].Y) {
							PCurve->FloatCurves[1].AddKey((float)j / (float)anim.framesPerSecond, frame.Y);
						}
						if (j == 0 || frame.Z != posChannel->frames[j - 1].Z) {
							PCurve->FloatCurves[2].AddKey((float)j / (float)anim.framesPerSecond, frame.Z);
						}
					}
				} else if (channel->type() == Zmo::ChannelType::Rotation) {
					UsesRotation = true;
					auto rotChannel = (Zmo::RotationChannel*)channel;
					FRotator prevFrame;
					for (int j = 0; j < rotChannel->frames.Num(); ++j) {
						FRotator frame = rotChannel->frames[j].Rotator();
						if (j == 0 || frame.Pitch != prevFrame.Pitch) {
							RCurve->FloatCurves[0].AddKey((float)j / (float)anim.framesPerSecond, frame.Pitch, true);
						}
						if (j == 0 || frame.Yaw != prevFrame.Yaw) {
							RCurve->FloatCurves[1].AddKey((float)j / (float)anim.framesPerSecond, frame.Yaw, true);
						}
						if (j == 0 || frame.Roll != prevFrame.Roll) {
							RCurve->FloatCurves[2].AddKey((float)j / (float)anim.framesPerSecond, frame.Roll, true);
						}
						prevFrame = frame;
					}
				} else if (channel->type() == Zmo::ChannelType::Scale) {
					UsesScale = true;
					auto scaleChannel = (Zmo::ScaleChannel*)channel;
					for (int j = 0; j < scaleChannel->frames.Num(); ++j) {
						const FVector& frame = scaleChannel->frames[j];
						if (j == 0 || frame.X != scaleChannel->frames[j - 1].X) {
							SCurve->FloatCurves[0].AddKey((float)j / (float)anim.framesPerSecond, frame.X);
						}
						if (j == 0 || frame.Y != scaleChannel->frames[j - 1].Y) {
							SCurve->FloatCurves[1].AddKey((float)j / (float)anim.framesPerSecond, frame.Y);
						}
						if (j == 0 || frame.Z != scaleChannel->frames[j - 1].Z) {
							SCurve->FloatCurves[2].AddKey((float)j / (float)anim.framesPerSecond, frame.Z);
						}
					}
				} else {
					DebugBreak();
				}
			}

			if (UsesRotation) {
				FTTVectorTrack VTrack;
				VTrack.TrackName = "Rotation";
				VTrack.CurveVector = RCurve;
				TLTmpl->VectorTracks.Add(VTrack);
			}
			if (UsesPosition) {
				FTTVectorTrack VTrack;
				VTrack.TrackName = "Position";
				VTrack.CurveVector = PCurve;
				TLTmpl->VectorTracks.Add(VTrack);
			}
			if (UsesScale) {
				FTTVectorTrack VTrack;
				VTrack.TrackName = "Scale";
				VTrack.CurveVector = SCurve;
				TLTmpl->VectorTracks.Add(VTrack);
			}

			TLNode->ReconstructNode();

			UK2Node_VariableGet* GetNode = CreateVarGetNode(EventGraph, MeshNode->GetVariableName());

			UEdGraphPin* PrevExecPin = TLNode->GetUpdatePin();
			if (UsesRotation) {
				UK2Node_CallFunction* MakeRotNode = CreateCallFuncNode(EventGraph, TEXT("KismetMathLibrary"), TEXT("MakeRot"));
				UK2Node_CallFunction* BreakVecNode = CreateCallFuncNode(EventGraph, TEXT("KismetMathLibrary"), TEXT("BreakVector"));
				UK2Node_CallFunction* SetRotNode = CreateCallFuncNode<USceneComponent>(EventGraph, TEXT("SetRelativeRotation"));

				PrevExecPin->MakeLinkTo(SetRotNode->GetExecPin());
				PrevExecPin = SetRotNode->GetThenPin();

				GetNode->GetValuePin()->MakeLinkTo(SetRotNode->FindPin(TEXT("self")));
				TLNode->FindPin(TEXT("Rotation"))->MakeLinkTo(BreakVecNode->FindPin(TEXT("InVec")));
				BreakVecNode->FindPin(TEXT("X"))->MakeLinkTo(MakeRotNode->FindPin(TEXT("Pitch")));
				BreakVecNode->FindPin(TEXT("Y"))->MakeLinkTo(MakeRotNode->FindPin(TEXT("Yaw")));
				BreakVecNode->FindPin(TEXT("Z"))->MakeLinkTo(MakeRotNode->FindPin(TEXT("Roll")));
				MakeRotNode->GetReturnValuePin()->MakeLinkTo(SetRotNode->FindPin(TEXT("NewRotation")));
			}

			if (UsesPosition) {
				UK2Node_CallFunction* SetPosNode = CreateCallFuncNode<USceneComponent>(EventGraph, TEXT("SetRelativeLocation"));
				PrevExecPin->MakeLinkTo(SetPosNode->GetExecPin());
				PrevExecPin = SetPosNode->GetThenPin();

				GetNode->GetValuePin()->MakeLinkTo(SetPosNode->FindPin(TEXT("self")));
				TLNode->FindPin(TEXT("Position"))->MakeLinkTo(SetPosNode->FindPin(TEXT("NewLocation")));
			}

			if (UsesScale) {
				UK2Node_CallFunction* SetScaleNode = CreateCallFuncNode<USceneComponent>(EventGraph, TEXT("SetRelativeScale"));
				PrevExecPin->MakeLinkTo(SetScaleNode->GetExecPin());
				PrevExecPin = SetScaleNode->GetThenPin();

				GetNode->GetValuePin()->MakeLinkTo(SetScaleNode->FindPin(TEXT("self")));
				TLNode->FindPin(TEXT("Scale"))->MakeLinkTo(SetScaleNode->FindPin(TEXT("NewScale3D")));
			}
		}
	}

	return Blueprint;

	/*
	const Zsc::Model& model = meshs.models[modelIdx];

	FString ModelPackageName, ModelName;
	ModelPackageName = TEXT("/MAPS");
	ModelName = FString::Printf(TEXT("%s_%d"), *MdlTypeName, modelIdx);

	UPackage* ModelPackage = GetOrMakePackage(ModelPackageName, ModelName);
	if (ModelPackage == NULL) {
		return NULL;
	}

	USkeletalMesh* SkeletalMesh = CastChecked<USkeletalMesh>(
		StaticConstructObject(USkeletalMesh::StaticClass(), ModelPackage, *ModelName, RF_Standalone | RF_Public));
	if (SkeletalMesh == NULL) {
		return NULL;
	}

	// Notify the asset registry
	FAssetRegistryModule::AssetCreated(SkeletalMesh);

	// Set the dirty flag so this package will get saved later
	SkeletalMesh->MarkPackageDirty();

	SkeletalMesh->PreEditChange(NULL);

	TArray<int32> PartBones;
	//int32 BoneIndex = 0;
	for (int i = 0; i < model.parts.Num(); ++i) {
		const Zsc::Part& part = model.parts[i];
		const Zsc::Texture& tex = meshs.textures[part.texIdx];

		if (i == 0 || !part.animPath.IsEmpty()) {
			PartBones.Add(i);
		} else {
			int32 BoneParent = PartBones[part.parentIdx - 1];
			PartBones.Add(BoneParent);
		}

		FString TexturePackage, TextureName;
		BuildAssetPath(TexturePackage, TextureName, tex.filePath, "_Texture");
		UTexture* UnrealTexture = ImportTexture(TexturePackage, TextureName, RoseBasePath + tex.filePath);

		FString MaterialPackage, MaterialName;
		BuildAssetPath(MaterialPackage, MaterialName, meshs.meshes[part.meshIdx]);
		MaterialName = FString::Printf(TEXT("Model_%d_%d_Material"), modelIdx, i);
		UMaterialInterface *UnrealMaterial = ImportMaterial(MaterialPackage, MaterialName, tex, UnrealTexture);

		SkeletalMesh->Materials.Add(UnrealMaterial);
	}


	FReferenceSkeleton& RefSkeleton = SkeletalMesh->RefSkeleton;
	for (int i = 0; i < model.parts.Num(); ++i) {
		const Zsc::Part& part = model.parts[i];

		int32 ueParent = (i > 0) ? part.parentIdx - 1 : INDEX_NONE;
		FString BoneName = FString::Printf(TEXT("Part_%d"), i);
		const FMeshBoneInfo BoneInfo(FName(*BoneName, FNAME_Add, true), *BoneName, ueParent);
		const FTransform BoneTransform(part.rotation, part.position, part.scale);

		RefSkeleton.Add(BoneInfo, BoneTransform);
	}
	SkeletalMesh->CalculateInvRefMatrices();

	USkeleton* Skeleton = NULL;
	{
		FString SkeletonName = ModelName + "_Skeleton";

		UPackage* SkelPackage = GetOrMakePackage(ModelPackageName, SkeletonName);
		if (SkelPackage == NULL) {
			return NULL;
		}

		Skeleton = CastChecked<USkeleton>(StaticConstructObject(USkeleton::StaticClass(), SkelPackage, *SkeletonName, RF_Standalone | RF_Public));
		if (Skeleton == NULL) {
			return NULL;
		}

		// Notify the asset registry
		FAssetRegistryModule::AssetCreated(Skeleton);

		// Set the dirty flag so this package will get saved later
		Skeleton->MarkPackageDirty();

		Skeleton->MergeAllBonesToBoneTree(SkeletalMesh);
	}

	{
		FSkeletalMeshResource* ImportedResource = SkeletalMesh->GetImportedResource();
		check(ImportedResource->LODModels.Num() == 0);
		ImportedResource->LODModels.Empty();
		new(ImportedResource->LODModels)FStaticLODModel();

		SkeletalMesh->LODInfo.Empty();
		SkeletalMesh->LODInfo.AddZeroed();
		SkeletalMesh->LODInfo[0].LODHysteresis = 0.02f;
		FSkeletalMeshOptimizationSettings Settings;
		// set default reduction settings values
		SkeletalMesh->LODInfo[0].ReductionSettings = Settings;

		SkeletalMesh->bHasVertexColors = false;

		FStaticLODModel& LODModel = ImportedResource->LODModels[0];
		LODModel.NumTexCoords = 1;

		IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");

		TArray<FVector> LODPoints;
		TArray<FMeshWedge> LODWedges;
		TArray<FMeshFace> LODFaces;
		TArray<FVertInfluence> LODInfluences;
		TArray<int32> LODPointToRawMap;

		TArray<FTransform> partTrans;
		for (int j = 0; j < model.parts.Num(); ++j) {
			const Zsc::Part& part = model.parts[j];

			FTransform trans(part.rotation, part.position, part.scale);
			if (j != 0 && part.parentIdx != 0xFFFF) {
				trans = partTrans[part.parentIdx - 1] * trans;
			}
			partTrans.Add(trans);

			Zms meshZms(*(RoseBasePath + meshs.meshes[part.meshIdx]));

			int vertOffset = LODPoints.Num();
			LODPoints.AddZeroed(meshZms.vertexPositions.Num());
			LODPointToRawMap.AddZeroed(meshZms.vertexPositions.Num());
			for (int i = 0; i < meshZms.vertexPositions.Num(); ++i) {
				LODPoints[vertOffset + i] = trans.TransformPosition(meshZms.vertexPositions[i]);
				LODPointToRawMap[vertOffset + i] = vertOffset + i;
			}

			int infOffset = LODInfluences.Num();
			LODInfluences.AddZeroed(meshZms.vertexPositions.Num());
			uint16 BoneIndex = PartBones[j];
			for (int i = 0; i < meshZms.vertexPositions.Num(); ++i) {
				LODInfluences[infOffset + i].BoneIndex = BoneIndex;
				LODInfluences[infOffset + i].VertIndex = vertOffset + i;
				LODInfluences[infOffset + i].Weight = 1;
			}

			int indexOffset = LODWedges.Num();
			LODWedges.AddZeroed(meshZms.indexes.Num());
			for (int i = 0; i < meshZms.indexes.Num(); ++i) {
				LODWedges[indexOffset + i].iVertex = vertOffset + meshZms.indexes[i];
			}
			for (int k = 0; k < 1; ++k) {
				if (meshZms.vertexUvs[k].Num() > 0) {
					for (int i = 0; i < meshZms.indexes.Num(); ++i) {
						LODWedges[indexOffset + i].UVs[k] = meshZms.vertexUvs[k][meshZms.indexes[i]];
					}
				}
			}

			int faceCount = meshZms.indexes.Num() / 3;
			int faceOffset = LODFaces.Num();
			LODFaces.AddZeroed(faceCount);
			for (int i = 0; i < faceCount; ++i) {
				LODFaces[faceOffset + i].iWedge[0] = indexOffset + (i * 3) + 0;
				LODFaces[faceOffset + i].iWedge[1] = indexOffset + (i * 3) + 1;
				LODFaces[faceOffset + i].iWedge[2] = indexOffset + (i * 3) + 2;
				LODFaces[faceOffset + i].MeshMaterialIndex = j;
				//LODFaces[faceOffset + i].TangentZ[0] = trans.TransformVector(meshZms.vertexNormals[meshZms.indexes[i * 3 + 0]]);
				//LODFaces[faceOffset + i].TangentZ[1] = trans.TransformVector(meshZms.vertexNormals[meshZms.indexes[i * 3 + 1]]);
				//LODFaces[faceOffset + i].TangentZ[2] = trans.TransformVector(meshZms.vertexNormals[meshZms.indexes[i * 3 + 2]]);
			}
		}


		TArray<FText> WarningMessages;
		TArray<FName> WarningNames;
		// Create actual rendering data.
		if (!MeshUtilities.BuildSkeletalMesh(
			ImportedResource->LODModels[0],
			SkeletalMesh->RefSkeleton,
			LODInfluences, LODWedges, LODFaces, LODPoints, LODPointToRawMap,
			false, true, true, &WarningMessages, &WarningNames))
		{
			DebugBreak();
		} else if (WarningMessages.Num() > 0)
		{
			DebugBreak();
		}

		const int32 NumSections = LODModel.Sections.Num();
		for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
		{
			SkeletalMesh->LODInfo[0].TriangleSortSettings.AddZeroed();
		}



		SkeletalMesh->PostEditChange();
	}

	{
		FString PhysName = ModelName + "_PhysicsAsset";
		UPackage* PhysPackage = GetOrMakePackage(ModelPackageName, PhysName);
		if (PhysPackage == NULL) {
			return NULL;
		}

		UPhysicsAsset* PhysicsAsset = CastChecked<UPhysicsAsset>(
			StaticConstructObject(UPhysicsAsset::StaticClass(), PhysPackage, *PhysName, RF_Standalone | RF_Public));
		if (PhysicsAsset) {
			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(PhysicsAsset);

			// Set the dirty flag so this package will get saved later
			PhysicsAsset->MarkPackageDirty();

			// Create the data!
			FPhysAssetCreateParams NewBodyData;
			NewBodyData.Initialize();
			NewBodyData.bBodyForAll = true;
			FText CreationErrorMessage;
			FPhysicsAssetUtils::CreateFromSkeletalMesh(PhysicsAsset, SkeletalMesh, NewBodyData, CreationErrorMessage);
		}
	}

	{
		FString AnimName = ModelName + "_Anim";
		UPackage* AnimPackage = GetOrMakePackage(ModelPackageName, AnimName);
		if (AnimPackage == NULL) {
			return NULL;
		}

		UAnimSequence* AnimSeq = CastChecked<UAnimSequence>(
			StaticConstructObject(UAnimSequence::StaticClass(), AnimPackage, *AnimName, RF_Standalone | RF_Public));
		if (AnimSeq == NULL) {
			return NULL;
		}

		// Notify the asset registry
		FAssetRegistryModule::AssetCreated(AnimSeq);

		// Set the dirty flag so this package will get saved later
		AnimSeq->MarkPackageDirty();

		AnimSeq->PreEditChange(NULL);

		AnimSeq->SetSkeleton(Skeleton);

		for (int j = 0; j < model.parts.Num(); ++j) {
			const Zsc::Part& part = model.parts[j];
			if (!part.animPath.IsEmpty()) {
				Zmo anim(*(RoseBasePath + part.animPath));
				FRawAnimSequenceTrack track;

				AnimSeq->SequenceLength = (float)anim.frameCount / (float)anim.framesPerSecond;
				AnimSeq->NumFrames = anim.frameCount;

				track.PosKeys.Add(part.position);
				track.RotKeys.Add(part.rotation);
				track.ScaleKeys.Add(part.scale);

				for (int i = 0; i < anim.channels.Num(); ++i) {
					Zmo::Channel *channel = anim.channels[i];
					if (channel->index != 0) {
						DebugBreak();
					}

					if (channel->type() == Zmo::ChannelType::Position) {
						auto posChannel = (Zmo::PositionChannel*)channel;
						track.PosKeys.Empty();
						for (int j = 0; j < posChannel->frames.Num(); ++j) {
							track.PosKeys.Add(posChannel->frames[j]);
						}
					} else if (channel->type() == Zmo::ChannelType::Rotation) {
						auto rotChannel = (Zmo::RotationChannel*)channel;
						track.RotKeys.Empty();
						for (int j = 0; j < rotChannel->frames.Num(); ++j) {
							track.RotKeys.Add(rotChannel->frames[j]);
						}
					} else if (channel->type() == Zmo::ChannelType::Scale) {
						auto scaleChannel = (Zmo::ScaleChannel*)channel;
						track.ScaleKeys.Empty();
						for (int j = 0; j < scaleChannel->frames.Num(); ++j) {
							track.ScaleKeys.Add(scaleChannel->frames[j]);
						}
					} else {
						DebugBreak();
					}
				}

				AnimSeq->RawAnimationData.Add(track);
				AnimSeq->AnimationTrackNames.Add(FName(*FString::Printf(TEXT("Part_%d"), j)));
				AnimSeq->TrackToSkeletonMapTable.Add(FTrackToSkeletonMap(j));
			}
		}

		AnimSeq->PostProcessSequence();

		AnimSeq->PostEditChange();
	}

	return SkeletalMesh;
	*/
}



UBlueprint* ImportWorldZscModelStatic(const FString& MdlTypeName, const Zsc& meshs, int modelIdx) {
	const Zsc::Model& model = meshs.models[modelIdx];

	FString BPPackageName = TEXT("/MAPS");
	FString BPAssetName = FString::Printf(TEXT("%s_%d"), *MdlTypeName, modelIdx);

	UPackage* BPPackage = GetOrMakePackage(BPPackageName, BPAssetName);
	if (BPPackage == NULL) {
		return NULL;
	}

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(), BPPackage, *BPAssetName, 
		BPTYPE_Normal, UBlueprint::StaticClass(), 
		UBlueprintGeneratedClass::StaticClass(), 
		FName("RosePluginWhat"));

	for (int j = 0; j < model.parts.Num(); ++j) {
		const Zsc::Part& part = model.parts[j];
		const Zsc::Texture& tex = meshs.textures[part.texIdx];
		const FString& mesh = meshs.meshes[part.meshIdx];

		FString ModelPackage, ModelName;
		BuildAssetPath(ModelPackage, ModelName, mesh);
		
		UPackage* Package = GetOrMakePackage(ModelPackage, ModelName);
		if (Package == NULL) {
			return NULL;
		}

		UStaticMesh* StaticMesh = CastChecked<UStaticMesh>(
			StaticConstructObject(UStaticMesh::StaticClass(), Package, *ModelName, RF_Standalone | RF_Public));
		if (StaticMesh == NULL) {
			return NULL;
		}

		// Notify the asset registry
		FAssetRegistryModule::AssetCreated(StaticMesh);

		// Set the dirty flag so this package will get saved later
		StaticMesh->MarkPackageDirty();

		// make sure it has a new lighting guid
		StaticMesh->LightingGuid = FGuid::NewGuid();

		// Set it to use textured lightmaps. Note that Build Lighting will do the error-checking (texcoordindex exists for all LODs, etc).
		StaticMesh->LightMapResolution = 256;
		StaticMesh->LightMapCoordinateIndex = 1;

		new(StaticMesh->SourceModels) FStaticMeshSourceModel();
		FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[0];

		FRawMesh RawMesh;
		SrcModel.RawMeshBulkData->SaveRawMesh(RawMesh);
		{
			FString TexturePackage, TextureName;
			BuildAssetPath(TexturePackage, TextureName, tex.filePath, "_Texture");
			UTexture* UnrealTexture = ImportTexture(TexturePackage, TextureName, RoseBasePath + tex.filePath);

			FString MaterialPackage, MaterialName;
			BuildAssetPath(MaterialPackage, MaterialName, meshs.meshes[part.meshIdx]);
			MaterialName = FString::Printf(TEXT("Model_%d_%d_Material"), modelIdx, j);
			UMaterialInterface *UnrealMaterial = ImportMaterial(MaterialPackage, MaterialName, tex, UnrealTexture);

			StaticMesh->Materials.Add(UnrealMaterial);

			Zms meshZms(*(RoseBasePath + mesh));

			RawMesh.VertexPositions.AddZeroed(meshZms.vertexPositions.Num());
			for (int i = 0; i < meshZms.vertexPositions.Num(); ++i) {
				RawMesh.VertexPositions[i] = meshZms.vertexPositions[i];
			}

			RawMesh.WedgeIndices.AddZeroed(meshZms.indexes.Num());
			//RawMesh.WedgeTangentX.AddZeroed(meshZms.indexes.Num());
			//RawMesh.WedgeTangentY.AddZeroed(meshZms.indexes.Num());
			//RawMesh.WedgeTangentZ.AddZeroed(meshZms.indexes.Num());
			for (int i = 0; i < meshZms.indexes.Num(); ++i) {
				RawMesh.WedgeIndices[i] = meshZms.indexes[i];
				//RawMesh.WedgeTangentZ[indexOffset + i] = meshZms.vertexNormals[meshZms.indexes[i]];
			}

			for (int k = 0; k < 4; ++k) {
				if (meshZms.vertexUvs[k].Num() > 0) {
					RawMesh.WedgeTexCoords[k].AddZeroed(meshZms.indexes.Num());
					for (int i = 0; i < meshZms.indexes.Num(); ++i) {
						RawMesh.WedgeTexCoords[k][i] = meshZms.vertexUvs[k][meshZms.indexes[i]];
					}
				}
			}

			int faceCount = meshZms.indexes.Num() / 3;
			RawMesh.FaceMaterialIndices.AddZeroed(faceCount);
			RawMesh.FaceSmoothingMasks.AddZeroed(faceCount);
			for (int i = 0; i < faceCount; ++i) {
				RawMesh.FaceMaterialIndices[i] = 0;
				RawMesh.FaceSmoothingMasks[i] = 0;
			}
		}
		SrcModel.RawMeshBulkData->SaveRawMesh(RawMesh);

		SrcModel.BuildSettings.bRemoveDegenerates = true;
		SrcModel.BuildSettings.bRecomputeNormals = false;
		SrcModel.BuildSettings.bRecomputeTangents = false;

		StaticMesh->Build(true);

		// Set up the mesh collision
		StaticMesh->CreateBodySetup();

		// Create new GUID
		StaticMesh->BodySetup->InvalidatePhysicsData();

		// Per-poly collision for now
		StaticMesh->BodySetup->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseComplexAsSimple;

		// refresh collision change back to staticmesh components
		RefreshCollisionChange(StaticMesh);

		for (int32 SectionIndex = 0; SectionIndex < StaticMesh->Materials.Num(); SectionIndex++)
		{
			FMeshSectionInfo Info = StaticMesh->SectionInfoMap.Get(0, SectionIndex);
			Info.bEnableCollision = true;
			StaticMesh->SectionInfoMap.Set(0, SectionIndex, Info);
		}

		UStaticMeshComponent* MeshComp = ConstructObject<UStaticMeshComponent>(UStaticMeshComponent::StaticClass());
		MeshComp->StaticMesh = StaticMesh;
		MeshComp->SetRelativeLocationAndRotation(part.position, FRotator(part.rotation));
		MeshComp->SetRelativeScale3D(part.scale);
		FString MeshCompName = FString::Printf(TEXT("Part_%d"), j);
		USCS_Node* MeshNode = Blueprint->SimpleConstructionScript->CreateNode(MeshComp, *MeshCompName);
		Blueprint->SimpleConstructionScript->AddNode(MeshNode);
	}

	return Blueprint;
}

/* Old version that merges everything
UStaticMesh* ImportWorldZscModelStatic(const FString& MdlTypeName, const Zsc& meshs, int modelIdx) {
	const Zsc::Model& model = meshs.models[modelIdx];

	FString ModelPackage, ModelName;
	ModelPackage = TEXT("/MAPS");
	ModelName = FString::Printf(TEXT("%s_%d"), *MdlTypeName, modelIdx);

	UPackage* Package = GetOrMakePackage(ModelPackage, ModelName);
	if (Package == NULL) {
		return NULL;
	}

	UStaticMesh* StaticMesh = CastChecked<UStaticMesh>(
		StaticConstructObject(UStaticMesh::StaticClass(), Package, *ModelName, RF_Standalone | RF_Public));
	if (StaticMesh == NULL) {
		return NULL;
	}
	
	// Notify the asset registry
	FAssetRegistryModule::AssetCreated(StaticMesh);

	// Set the dirty flag so this package will get saved later
	StaticMesh->MarkPackageDirty();

	// make sure it has a new lighting guid
	StaticMesh->LightingGuid = FGuid::NewGuid();

	// Set it to use textured lightmaps. Note that Build Lighting will do the error-checking (texcoordindex exists for all LODs, etc).
	StaticMesh->LightMapResolution = 256;
	StaticMesh->LightMapCoordinateIndex = 1;

	new(StaticMesh->SourceModels) FStaticMeshSourceModel();
	FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[0];

	FRawMesh RawMesh;
	SrcModel.RawMeshBulkData->SaveRawMesh(RawMesh);

	TArray<FTransform> partTrans;
	for (int j = 0; j < model.parts.Num(); ++j) {
		const Zsc::Part& part = model.parts[j];
		const Zsc::Texture& tex = meshs.textures[part.texIdx];

		FTransform trans(part.rotation, part.position, part.scale);
		if (j != 0 && part.parentIdx != 0xFFFF) {
			trans = partTrans[part.parentIdx-1] * trans;
		}
		partTrans.Add(trans);

		FString TexturePackage, TextureName;
		BuildAssetPath(TexturePackage, TextureName, tex.filePath, "_Texture");
		UTexture* UnrealTexture = ImportTexture(TexturePackage, TextureName, RoseBasePath + tex.filePath);

		FString MaterialPackage, MaterialName;
		BuildAssetPath(MaterialPackage, MaterialName, meshs.meshes[part.meshIdx]);
		MaterialName = FString::Printf(TEXT("Model_%d_%d_Material"), modelIdx, j);
		UMaterialInterface *UnrealMaterial = ImportMaterial(MaterialPackage, MaterialName, tex, UnrealTexture);

		StaticMesh->Materials.Add(UnrealMaterial);
		
		Zms meshZms(*(RoseBasePath + meshs.meshes[part.meshIdx]));

		int vertOffset = RawMesh.VertexPositions.Num();
		RawMesh.VertexPositions.AddZeroed(meshZms.vertexPositions.Num());
		for (int i = 0; i < meshZms.vertexPositions.Num(); ++i) {
			RawMesh.VertexPositions[vertOffset + i] = trans.TransformPosition(meshZms.vertexPositions[i]);
		}

		int indexOffset = RawMesh.WedgeIndices.Num();
		RawMesh.WedgeIndices.AddZeroed(meshZms.indexes.Num());
		//RawMesh.WedgeTangentX.AddZeroed(meshZms.indexes.Num());
		//RawMesh.WedgeTangentY.AddZeroed(meshZms.indexes.Num());
		//RawMesh.WedgeTangentZ.AddZeroed(meshZms.indexes.Num());
		for (int i = 0; i < meshZms.indexes.Num(); ++i) {
			RawMesh.WedgeIndices[indexOffset + i] = vertOffset + meshZms.indexes[i];
			//RawMesh.WedgeTangentZ[indexOffset + i] = meshZms.vertexNormals[meshZms.indexes[i]];
		}

		for (int k = 0; k < 1; ++k) {
			if (meshZms.vertexUvs[k].Num() > 0) {
				RawMesh.WedgeTexCoords[k].AddZeroed(meshZms.indexes.Num());
				for (int i = 0; i < meshZms.indexes.Num(); ++i) {
					RawMesh.WedgeTexCoords[k][indexOffset + i] = meshZms.vertexUvs[k][meshZms.indexes[i]];
				}
			}
		}

		int faceCount = meshZms.indexes.Num() / 3;
		int faceOffset = RawMesh.FaceMaterialIndices.Num();
		RawMesh.FaceMaterialIndices.AddZeroed(faceCount);
		RawMesh.FaceSmoothingMasks.AddZeroed(faceCount);
		for (int i = 0; i < faceCount; ++i) {
			RawMesh.FaceMaterialIndices[faceOffset + i] = j;
			RawMesh.FaceSmoothingMasks[faceOffset + i] = 0;
		}
	}

	IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
	FText OutError;
	float MinChartSpacingPercent = 1.0f;
	float BorderSpacingPercent = 0.0f;
	uint32 MaxCharts = 0;
	float MaxDesiredStretch = 0.5;
	MeshUtilities.GenerateUVs(RawMesh, 1, MinChartSpacingPercent, BorderSpacingPercent, true, NULL, MaxCharts, MaxDesiredStretch, OutError);

	SrcModel.RawMeshBulkData->SaveRawMesh(RawMesh);

	SrcModel.BuildSettings.bRemoveDegenerates = true;
	SrcModel.BuildSettings.bRecomputeNormals = false;
	SrcModel.BuildSettings.bRecomputeTangents = false;

	StaticMesh->Build(true);
	
	// Set up the mesh collision
	StaticMesh->CreateBodySetup();

	// Create new GUID
	StaticMesh->BodySetup->InvalidatePhysicsData();

	// Per-poly collision for now
	StaticMesh->BodySetup->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseComplexAsSimple;

	// refresh collision change back to staticmesh components
	RefreshCollisionChange(StaticMesh);

	for (int32 SectionIndex = 0; SectionIndex < StaticMesh->Materials.Num(); SectionIndex++)
	{
		FMeshSectionInfo Info = StaticMesh->SectionInfoMap.Get(0, SectionIndex);
		Info.bEnableCollision = true;
		StaticMesh->SectionInfoMap.Set(0, SectionIndex, Info);
	}

	return StaticMesh;
}
*/

UBlueprint* ImportWorldZscModel(const FString& MdlTypeName, const Zsc& meshs, int modelIdx) {
	const Zsc::Model& model = meshs.models[modelIdx];

	bool NeedsAnim = false;
	for (int i = 0; i < model.parts.Num(); ++i) {
		if (!model.parts[i].animPath.IsEmpty()) {
			NeedsAnim = true;
			break;
		}
	}

	if (NeedsAnim) {
		return ImportWorldZscModelAnim(MdlTypeName, meshs, modelIdx);
	} else {
		return ImportWorldZscModelStatic(MdlTypeName, meshs, modelIdx);
	}
}

AActor* SpawnWorldModel(const FString& NewName, const FString& PackageName, const FString& AssetName, const FQuat& Rot, const FVector& Pos, const FVector& Scale) {
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Name = *NewName;

	auto Model = GetExistingAsset<UBlueprint>(PackageName, AssetName);
	if (Model != NULL) {
		AActor* ModelAct = GWorld->SpawnActor<AActor>(Model->GeneratedClass, Pos, FRotator(Rot), SpawnInfo);
		ModelAct->SetActorScale3D(Scale);
		return ModelAct;
	}
	/*
	auto SkelMdl = Cast<USkeletalMesh>(Model);
	if (SkelMdl) {
		auto SkelActor = GWorld->SpawnActor<ASkeletalMeshActor>(MyPos, FRotator(Rot), SpawnInfo);
		SkelActor->SetActorScale3D(MyScale);

		auto MeshComp = SkelActor->SkeletalMeshComponent;
		MeshComp->UnregisterComponent();
		MeshComp->SetSkeletalMesh(SkelMdl);
		MeshComp->RegisterComponent();

		auto AnimSeq = GetExistingAsset<UAnimationAsset>(PackageName, AssetName + TEXT("_Anim"));
		MeshComp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		MeshComp->SetAnimation(AnimSeq);

		return SkelActor;
	}
	
	auto StaticMdl = Cast<UStaticMesh>(Model);
	if (StaticMdl) {
		auto StaticActor = GWorld->SpawnActor<AStaticMeshActor>(MyPos, FRotator(Rot), SpawnInfo);
		StaticActor->SetActorScale3D(MyScale);

		auto MeshComp = StaticActor->StaticMeshComponent;
		MeshComp->UnregisterComponent();
		StaticActor->StaticMeshComponent->SetStaticMesh(StaticMdl);
		MeshComp->RegisterComponent();
		return StaticActor;
	}
	*/

	return NULL;
}

void FBrettPlugin::StartButton_Clicked()
{
	GWarn->BeginSlowTask(NSLOCTEXT("RosePlugin", "SlowWorking", "We are working on importing the map!"), true);

	/*
	Zsc meshs(*(RoseBasePath + TEXT("3DDATA/AVATAR/LIST_MBODY.ZSC")));
	Zsc meshs2(*(RoseBasePath + TEXT("3DDATA/AVATAR/LIST_MARMS.ZSC")));
	Zsc meshs3(*(RoseBasePath + TEXT("3DDATA/AVATAR/LIST_MFOOT.ZSC")));

	Zmd meshZmd(*(RoseBasePath + TEXT("3DDATA/AVATAR/MALE.ZMD")));
	FString MaleSkelPackage = TEXT("/AVATAR");
	FString MaleSkelName = TEXT("MALE_Skeleton");
	ImportSkelData skelData(meshZmd, MaleSkelPackage, MaleSkelName);

	ImportAvatarItem(TEXT("MBODY"), meshs, skelData, 1);
	ImportAvatarItem(TEXT("MARMS"), meshs2, skelData, 1);
	ImportAvatarItem(TEXT("MFOOT"), meshs3, skelData, 1);


	Zmo animZmo(*(RoseBasePath + TEXT("3DDATA/MOTION/AVATAR/empty_run_m1.ZMO")));
	FString AnimName = TEXT("Male_Run");
	UAnimSequence* animSeq = ImportSkeletalAnim(TEXT("/AVATAR"), AnimName, skelData, animZmo);
	*/

	//const FString& PackageName, FString& MeshName, const Zsc& meshs, ImportSkelData& skel, int modelIdx

	/*
	Zsc meshsc(*(RoseBasePath + TEXT("3DDATA/JUNON/LIST_CNST_JDT.ZSC")));
	
	// Static
	ImportWorldZscModel("JDTC", meshsc, 12);

	// Animated
	ImportWorldZscModel("JDTC", meshsc, 8);
	
	FQuat rot = FQuat::Identity;
	FVector pos = FVector(0, 0, 0);
	FVector scale = FVector(1, 1, 1);
	SpawnWorldModel("TestObj1", "/MAPS", "JDTC_8", rot, pos, scale);

	FQuat rot2 = FQuat::Identity;
	FVector pos2 = FVector(1000, 0, 0);
	FVector scale2 = FVector(1, 1, 1);
	SpawnWorldModel("TestObj2", "/MAPS", "JDTC_12", rot2, pos2, scale2);
	//*/

	//* LOAD ZSCs
	Zsc meshsc(*(RoseBasePath + TEXT("3DDATA/JUNON/LIST_CNST_JDT.ZSC")));
	for (int32 i = 0; i < meshsc.models.Num(); ++i) {
		if (meshsc.models[i].parts.Num() > 0) {
			ImportWorldZscModel("JDTC", meshsc, i);
		}
	}
	//*/
	//*
	Zsc meshsd(*(RoseBasePath + TEXT("3DDATA/JUNON/LIST_DECO_JDT.ZSC")));
	for (int32 i = 0; i < meshsd.models.Num(); ++i) {
		if (meshsd.models[i].parts.Num() > 0) {
			ImportWorldZscModel("JDTD", meshsd, i);
		}
	}
	//*/


	/*
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Name = TEXT("TestModelThing");
	FVector Location = FVector(0, 0, 0);
	FRotator Rotation = FRotator(0, 0, 0);
	ASkeletalMeshActor* SkelMesh = GWorld->SpawnActor<ASkeletalMeshActor>(Location, Rotation, SpawnInfo);
	SkelMesh->SkeletalMeshComponent->SetSkeletalMesh(SkelMdl);
	*/

	/*
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Name = TEXT("TestModelThing");
	FVector Location = FVector(0, 0, 0);
	FRotator Rotation = FRotator(0, 0, 0);
	AStaticMeshActor* StaticMesh = GWorld->SpawnActor<AStaticMeshActor>(Location, Rotation, SpawnInfo);
	StaticMesh->SetStaticMesh();
	*/

	//*
	const FString CnstPackageName = TEXT("/MAPS");

	const float HIM_HEIGHT_MIN = -25600;
	const float HIM_HEIGHT_MAX = +25600;
	const float UEL_HEIGHT_WMIN = -25600;
	const float UEL_HEIGHT_WMAX = +25600;
	const float UEL_HEIGHT_MIN = 0x0000;
	const float UEL_HEIGHT_MAX = 0x10000;
	const float HIM_HEIGHT_MID = (HIM_HEIGHT_MAX - HIM_HEIGHT_MIN) / 2;
	const float HIM_HEIGHT_MUL = (UEL_HEIGHT_MAX - UEL_HEIGHT_MIN) / (HIM_HEIGHT_MAX - HIM_HEIGHT_MIN);
	const float UEL_ZSCALE = (UEL_HEIGHT_WMAX - UEL_HEIGHT_WMIN) / (HIM_HEIGHT_MAX - HIM_HEIGHT_MIN);
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Name = TEXT("TestThing");
	FVector Location = FVector(0, 0, 0);
	FRotator Rotation = FRotator(0, 0, 0);
	ALandscape* Landscape = GWorld->SpawnActor<ALandscape>(Location, Rotation, SpawnInfo);
	Landscape->SetActorScale3D(FVector(250.0f, 250.0f, 51200.0f / 51200.0f * 100.0f));
	
	int startX = 31;
	int startY = 30;
	int endX = 34;
	int endY = 33;

	uint32 RoseSizeX = 4 * 16 * (endX - startX + 1);
	uint32 RoseSizeY = 4 * 16 * (endY - startY + 1);
	uint32 SizeX = (RoseSizeX / 63 + 1) * 63 + 1;
	uint32 SizeY = (RoseSizeY / 63 + 1) * 63 + 1;
	TArray<uint16> Data;
	Data.AddZeroed(SizeX * SizeY);
	for (int i = 0; i < Data.Num(); ++i) {
		Data[i] = 0x8000;
	}

	float MinHeight = +1000000;
	float MaxHeight = -1000000;
	for (int iy = startY; iy <= endY; ++iy) {
		for (int ix = startX; ix <= endX; ++ix) {
			int outBaseX = (ix - startX) * 64;
			int outBaseY = (iy - startY) * 64;

			FString HimPath = FString::Printf(TEXT("3DDATA/MAPS/JUNON/JDT01/%d_%d.him"), ix, iy);
			Him himData(*(RoseBasePath + HimPath));

			for (int sy = 0; sy < 65; ++sy) {
				for (int sx = 0; sx < 65; ++sx) {
					int outIdx = (outBaseY + sy) * SizeX + (outBaseX + sx);
					float hmValue = himData.heights[sy * 65 + sx];
					float ueValue = FMath::Clamp(hmValue + 25600.0f, 0.0f, 51200.0f) / 51200.0f * 65535.0f;
					
					Data[outIdx] = ueValue;

					if (hmValue < MinHeight) {
						MinHeight = hmValue;
					}
					if (hmValue > MaxHeight) {
						MaxHeight = hmValue;
					}
				}
			}


			FString IfoPath = FString::Printf(TEXT("3DDATA/MAPS/JUNON/JDT01/%d_%d.ifo"), ix, iy);
			Ifo ifoData(*(RoseBasePath + IfoPath));

			for (int32 i = 0; i < ifoData.Buildings.Num(); ++i) {
				const Ifo::FBuildingBlock& obj = ifoData.Buildings[i];
				FString ObjName = FString::Printf(TEXT("Bldg_%d_%d_%d"), ix, iy, i);
				FString AssetName = FString::Printf(TEXT("JDTC_%d"), obj.ObjectID);
				AActor* ObjActor = SpawnWorldModel(ObjName, CnstPackageName, AssetName, obj.Rotation, obj.Position, obj.Scale);
			}

			for (int32 i = 0; i < ifoData.Objects.Num(); ++i) {
				const Ifo::FObjectBlock& obj = ifoData.Objects[i];
				FString ObjName = FString::Printf(TEXT("Deco_%d_%d_%d"), ix, iy, i);
				FString AssetName = FString::Printf(TEXT("JDTD_%d"), obj.ObjectID);
				AActor* ObjActor = SpawnWorldModel(ObjName, CnstPackageName, AssetName, obj.Rotation, obj.Position, obj.Scale);
				if (ObjActor) {
					ObjActor->SetActorScale3D(obj.Scale);
				}
			}
		}
	}

	UE_LOG(RosePlugin, Log, TEXT("Imported map height bounds were: %f, %f"), MinHeight, MaxHeight);

	TArray<FLandscapeImportLayerInfo> LayerInfos;
	FLandscapeImportLayerInfo LayerInfo;
	LayerInfo.LayerData.Empty();
	LayerInfo.LayerName = TEXT("TestLayer");
	LayerInfos.Add(LayerInfo);

	Landscape->LandscapeMaterial = LoadObject<UMaterialInterface>(NULL, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"), NULL, LOAD_None, NULL);
	Landscape->Import(FGuid::NewGuid(), SizeX, SizeY, 63, 1,  63, Data.GetData(), NULL, LayerInfos);
	Landscape->StaticLightingLOD = FMath::DivideAndRoundUp(FMath::CeilLogTwo((SizeX * SizeY) / (2048 * 2048) + 1), (uint32)2);

	Landscape->SetActorLocation(FVector((startX - 32) * 16000 - 8000, (startY - 32) * 16000 - 8000, 0));
	Landscape->StaticLightingResolution = 4.0f;
	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo(true);
	LandscapeInfo->UpdateLayerInfoMap(Landscape);
	//*/

	/*
	FString TextureName = TEXT("BODY02");
	UTexture* UnrealTexture = ImportTexture(
		FString(TEXT("/JELLYBEAN1")), TextureName,
		FString(RoseBasePath + TEXT("3DDATA/NPC/PLANT/JELLYBEAN1/BODY02.DDS")));


	FString MaterialName = TEXT("BODY02_Material");
	UMaterialInterface *UnrealMaterialX = ImportMaterial(
		FString(TEXT("/JELLYBEAN1")), MaterialName,
		UnrealTexture);
	*/

	/*
	std::vector<int> charsToIgnore = { };
	for (int i = 0; i < std::min(400, chars.characters.Num()); ++i) {
		if (std::find(charsToIgnore.begin(), charsToIgnore.end(), i) != charsToIgnore.end()) {
			continue;
		}

		const Chr::Character& tchar = chars.characters[i];
		if (tchar.enabled && tchar.models.Num() > 0) {
			ImportChar(chars, meshs, i);
		}
	}
	//*/

	/*
	Chr chars(*(RoseBasePath + TEXT("3DDATA/NPC/LIST_NPC.CHR")));
	Zsc meshs(*(RoseBasePath + TEXT("3DDATA/NPC/PART_NPC.ZSC")));

	int charsToImport[] = { 396, 0 };
	int* charNext = charsToImport;
	while (*charNext != 0) {
		ImportChar(chars, meshs, *charNext++);
	}
	//*/

	/*
	Zmd bones(*(RoseBasePath + TEXT("3DDATA/NPC/PLANT/JELLYBEAN1/JELLYBEAN2_BONE.ZMD")));
	Zms mesh(*(RoseBasePath + TEXT("3DDATA/NPC/PLANT/JELLYBEAN1/BODY01.ZMS")));
	Zms mesh2(*(RoseBasePath + TEXT("3DDATA/NPC/PLANT/JELLYBEAN1/BODY02.ZMS")));
	Zmo anim(*(RoseBasePath + TEXT("3DDATA/MOTION/NPC/JELLYBEAN1/JELLYBEAN1_WALK.ZMO")));

	ImportSkelData skelData(bones);
	ImportMeshData meshData;
	meshData.materials.Add(UnrealMaterialX);
	meshData.meshes.Add(ImportMeshData::Item(mesh, 0));
	meshData.meshes.Add(ImportMeshData::Item(mesh2, 0));

	FString MeshName = TEXT("JELLYBEAN1");
	USkeletalMesh* skelMesh = ImportSkeletalMesh(
		TEXT("/JELLYBEAN1"), MeshName,
		meshData, skelData);


	FString AnimName = TEXT("JELLYBEAN1_Walk");
	UAnimSequence* animSeq = ImportSkeletalAnim(TEXT("/JELLYBEAN1"), AnimName, skelData, anim);

	*/


	/*
	USkeletalMesh* SkeletalMesh = CastChecked<USkeletalMesh>(StaticConstructObject(USkeletalMesh::StaticClass(), Package, FName("TestMesh"), RF_Standalone | RF_Public));
	
	FAssetRegistryModule::AssetCreated(SkeletalMesh);

	SkeletalMesh->PreEditChange(NULL);

	SkeletalMesh->Materials.Add(FSkeletalMaterial(UnrealMaterialX));

	FReferenceSkeleton& RefSkeleton = SkeletalMesh->RefSkeleton;
	for (int i = 0; i < bones.bones.Num(); ++i) {
		const Zmd::Bone& bone = bones.bones[i];

		int32 ueParent = (i > 0) ? bone.parent : INDEX_NONE;
		const FMeshBoneInfo BoneInfo(FName(bone.name, FNAME_Add, true), bone.name, ueParent);
		const FTransform BoneTransform(bone.rotation, bone.translation);

		RefSkeleton.Add(BoneInfo, BoneTransform);
	}

	FSkeletalMeshResource* ImportedResource = SkeletalMesh->GetImportedResource();
	check(ImportedResource->LODModels.Num() == 0);
	ImportedResource->LODModels.Empty();
	new(ImportedResource->LODModels)FStaticLODModel();

	SkeletalMesh->LODInfo.Empty();
	SkeletalMesh->LODInfo.AddZeroed();
	SkeletalMesh->LODInfo[0].LODHysteresis = 0.02f;
	FSkeletalMeshOptimizationSettings Settings;
	// set default reduction settings values
	SkeletalMesh->LODInfo[0].ReductionSettings = Settings;

	SkeletalMesh->bHasVertexColors = false;

	FStaticLODModel& LODModel = ImportedResource->LODModels[0];

	// Pass the number of texture coordinate sets to the LODModel.  Ensure there is at least one UV coord
	LODModel.NumTexCoords = 1;

	IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
	

	TArray<ImportMeshData::Item> meshList;

	ImportMeshData::Item meshData1(mesh, 0);
	meshData1.vertOffset = 0;
	meshList.Add(meshData1);

	ImportMeshData::Item meshData2(mesh2, 0);
	meshData2.vertOffset = mesh.vertexPositions.Num();
	meshList.Add(meshData2);


	TArray<FVector> LODPoints;
	TArray<FMeshWedge> LODWedges;
	TArray<FMeshFace> LODFaces;
	TArray<FVertInfluence> LODInfluences;
	TArray<int32> LODPointToRawMap;

	int32 totalVertCount = 0;
	int32 totalIndexCount = 0;
	int32 totalFaceCount = 0;
	for (int i = 0; i < meshList.Num(); ++i) {
		meshList[i].vertOffset = totalVertCount;
		meshList[i].indexOffset = totalIndexCount;
		meshList[i].faceOffset = totalFaceCount;
		totalVertCount += meshList[i].data.vertexPositions.Num();
		totalIndexCount += meshList[i].data.indexes.Num();
		totalFaceCount += meshList[i].data.indexes.Num() / 3;
	}

	LODPoints.AddZeroed(totalVertCount);
	LODPointToRawMap.AddZeroed(totalVertCount);
	LODWedges.AddZeroed(totalIndexCount);
	LODFaces.AddZeroed(totalFaceCount);
	LODInfluences.AddZeroed(totalVertCount * 4);

	for (int i = 0; i < meshList.Num(); ++i) {
		Zms& tmesh = meshList[i].data;
		for (int j = 0; j < tmesh.vertexPositions.Num(); ++j) {
			int32 vertIdx = meshList[i].vertOffset + j;
			LODPoints[vertIdx] = tmesh.vertexPositions[j];
			LODPointToRawMap[vertIdx] = vertIdx;
		}

		for (int j = 0; j < tmesh.indexes.Num(); ++j) {
			int32 wedgeIdx = meshList[i].indexOffset + j;
			LODWedges[wedgeIdx].iVertex = meshList[i].vertOffset + tmesh.indexes[j];
			LODWedges[wedgeIdx].UVs[0] = tmesh.vertexUvs[0][tmesh.indexes[j]];
		}

		int32 faceCount = tmesh.indexes.Num() / 3;
		for (int j = 0; j < faceCount; ++j) {
			int32 faceIdx = meshList[i].faceOffset + j;
			LODFaces[faceIdx].iWedge[0] = meshList[i].indexOffset + (j * 3 + 0);
			LODFaces[faceIdx].iWedge[1] = meshList[i].indexOffset + (j * 3 + 1);
			LODFaces[faceIdx].iWedge[2] = meshList[i].indexOffset + (j * 3 + 2);
			LODFaces[faceIdx].MeshMaterialIndex = 0;
		}

		for (int j = 0; j < tmesh.vertexPositions.Num(); ++j) {
			int32 infBaseIdx = (meshList[i].vertOffset + j) * 4;
			for (int k = 0; k < 4; ++k) {
				LODInfluences[infBaseIdx + k].VertIndex = meshList[i].vertOffset + j;
				LODInfluences[infBaseIdx + k].BoneIndex = tmesh.boneWeights[j].boneIdx[k];
				LODInfluences[infBaseIdx + k].Weight = tmesh.boneWeights[j].weight[k];
			}
		}
	}

	TArray<FText> WarningMessages;
	TArray<FName> WarningNames;
	// Create actual rendering data.
	if (!MeshUtilities.BuildSkeletalMesh(ImportedResource->LODModels[0], SkeletalMesh->RefSkeleton, LODInfluences, LODWedges, LODFaces, LODPoints, LODPointToRawMap, false, true, true, &WarningMessages, &WarningNames))
	{
		DebugBreak();
	}
	else if (WarningMessages.Num() > 0)
	{
		DebugBreak();
	}

	const int32 NumSections = LODModel.Sections.Num();
	for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
	{
		SkeletalMesh->LODInfo[0].TriangleSortSettings.AddZeroed();
	}

	SkeletalMesh->CalculateInvRefMatrices();
	SkeletalMesh->PostEditChange();
	SkeletalMesh->MarkPackageDirty();

	FString ObjectName = FString::Printf(TEXT("%s_Skeleton"), *SkeletalMesh->GetName());
	USkeleton* Skeleton = CastChecked<USkeleton>(StaticConstructObject(USkeleton::StaticClass(), Package, *ObjectName, RF_Standalone | RF_Public));
	FAssetRegistryModule::AssetCreated(Skeleton);

	Skeleton->MergeAllBonesToBoneTree(SkeletalMesh);
	Skeleton->UpdateReferencePoseFromMesh(SkeletalMesh);
	FAssetNotifications::SkeletonNeedsToBeSaved(Skeleton);
	
	// ANIMATION STUFF!

	FString AnimName = FString::Printf(TEXT("%s_Anim"), *SkeletalMesh->GetName());
	UAnimSequence* AnimSeq = CastChecked<UAnimSequence>(StaticConstructObject(UAnimSequence::StaticClass(), Package, *AnimName, RF_Standalone | RF_Public));
	FAssetRegistryModule::AssetCreated(AnimSeq);

	AnimSeq->SetSkeleton(Skeleton);

	AnimSeq->SequenceLength = (float)anim.frameCount / (float)anim.framesPerSecond;
	AnimSeq->NumFrames = anim.frameCount;

	TArray<FRawAnimSequenceTrack> tracks;
	for (int i = 0; i < RefSkeleton.GetNum(); ++i) {
		FRawAnimSequenceTrack track;
		// All keys must be ABSOLUTE for Unreal!
		track.PosKeys.Add(bones.bones[i].translation);
		track.RotKeys.Add(bones.bones[i].rotation);
		track.ScaleKeys.Add(FVector(1, 1, 1));
		tracks.Add(track);
	}
	
	for (int i = 0; i < anim.channels.Num(); ++i) {
		Zmo::Channel *channel = anim.channels[i];
		FRawAnimSequenceTrack& track = tracks[channel->index];
		const Zmd::Bone& bone = bones.bones[channel->index];
		if (channel->type() == Zmo::ChannelType::Position) {
			auto posChannel = (Zmo::PositionChannel*)channel;
			track.PosKeys.Empty();
			for (int j = 0; j < posChannel->frames.Num(); ++j) {
				track.PosKeys.Add(posChannel->frames[j]);
			}
		} else if (channel->type() == Zmo::ChannelType::Rotation) {
			auto rotChannel = (Zmo::RotationChannel*)channel;
			track.RotKeys.Empty();
			for (int j = 0; j < rotChannel->frames.Num(); ++j) {
				track.RotKeys.Add(rotChannel->frames[j]);
			}
		} else if (channel->type() == Zmo::ChannelType::Scale) {
			auto scaleChannel = (Zmo::ScaleChannel*)channel;
			track.ScaleKeys.Empty();
			for (int j = 0; j < scaleChannel->frames.Num(); ++j) {
				track.ScaleKeys.Add(scaleChannel->frames[j]);
			}
		} else {
			DebugBreak();
		}
	}

	AnimSeq->RawAnimationData.Empty();
	AnimSeq->AnimationTrackNames.Empty();
	AnimSeq->TrackToSkeletonMapTable.Empty();
	for (int i = 0; i < tracks.Num(); ++i) {
		AnimSeq->RawAnimationData.Add(tracks[i]);
		AnimSeq->AnimationTrackNames.Add(RefSkeleton.GetBoneName(i));
		AnimSeq->TrackToSkeletonMapTable.Add(FTrackToSkeletonMap(i));
	}

	AnimSeq->PostProcessSequence();
	//*/

	/*
	UStaticMesh *StaticMesh = new(Package, FName(*MeshName), RF_Standalone|RF_Public) UStaticMesh(FPostConstructInitializeProperties());
	FAssetRegistryModule::AssetCreated(StaticMesh);

	new(StaticMesh->SourceModels) FStaticMeshSourceModel();
	FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[0];

	FRawMesh RawMesh;
	SrcModel.RawMeshBulkData->SaveRawMesh(RawMesh);

	
	ReadHelper rh;
	FFileHelper::LoadFileToArray(rh.data, TEXT("D:/zz_test_evo/3DDATA/NPC/NPC/CHIEF/BODY01.ZMS"));
 


	auto header = rh.read<char[8]>();
	auto format = rh.read<uint32>();
	rh.skip(sizeof(FVector) * 2);
	
	auto boneCount = rh.read<uint16>();
	for (int i = 0; i < boneCount; ++i) {
		rh.read<uint16>();
	}

	auto vertexCount = rh.read<uint16>();
	RawMesh.VertexPositions.AddZeroed(vertexCount);
	for (int i = 0; i < vertexCount; ++i) {
		RawMesh.VertexPositions[i] = rtuPosition(rh.read<FVector>()) * 100;
	}

	TArray<FVector2D> vertexUvs[4];

	if (format & ZMSF_NORMAL) {
		rh.skip(sizeof(FVector) * vertexCount);
	}
	if (format & ZMSF_COLOR) {
		rh.skip(sizeof(float) * 4 * vertexCount);
	}
	if (format & ZMSF_BLENDINDEX && format & ZMSF_BLENDWEIGHT) {
		rh.skip(24 * vertexCount);
	}
	if (format & ZMSF_TANGENT) {
		rh.skip(sizeof(FVector) * vertexCount);
	}
	if (format & ZMSF_UV1) {
		vertexUvs[0].AddZeroed(vertexCount);
		for (int i = 0; i < vertexCount; ++i) {
			vertexUvs[0][i] = rh.read<FVector2D>();
		}
	}
	if (format & ZMSF_UV2) {
		vertexUvs[1].AddZeroed(vertexCount);
		for (int i = 0; i < vertexCount; ++i) {
			vertexUvs[1][i] = rh.read<FVector2D>();
		}
	}
	if (format & ZMSF_UV3) {
		vertexUvs[2].AddZeroed(vertexCount);
		for (int i = 0; i < vertexCount; ++i) {
			vertexUvs[2][i] = rh.read<FVector2D>();
		}
	}
	if (format & ZMSF_UV4) {
		vertexUvs[3].AddZeroed(vertexCount);
		for (int i = 0; i < vertexCount; ++i) {
			vertexUvs[3][i] = rh.read<FVector2D>();
		}
	}

	auto faceCount = rh.read<uint16>();
	int indexCount = faceCount * 3;
	RawMesh.WedgeIndices.AddZeroed(indexCount);
	for (int i = 0; i < indexCount; ++i) {
		RawMesh.WedgeIndices[i] = rh.read<uint16>();
	}

	RawMesh.FaceMaterialIndices.AddZeroed(faceCount);
	RawMesh.FaceSmoothingMasks.AddZeroed(faceCount);
	for (int i = 0; i < faceCount; ++i) {
		RawMesh.FaceMaterialIndices[i] = 0;
		RawMesh.FaceSmoothingMasks[i] = 1;
	}

	for (int i = 0; i < 4; ++i) {
		if (vertexUvs[i].Num() > 0) {
			RawMesh.WedgeTexCoords[i].AddZeroed(indexCount);

			for (int j = 0; j < indexCount; ++j) {
				RawMesh.WedgeTexCoords[i][j] = vertexUvs[i][RawMesh.WedgeIndices[j]];
			}
		}
	}

	SrcModel.RawMeshBulkData->SaveRawMesh(RawMesh);

	StaticMesh->Build(false);

	StaticMesh->MarkPackageDirty();

	UE_LOG(RosePlugin, Log, TEXT("Read ZMS - %s %08x %d %d %d %d!"), *FString(header), format, boneCount, vertexCount, indexCount, rh.pos);
	*/

	GWarn->EndSlowTask();
}




