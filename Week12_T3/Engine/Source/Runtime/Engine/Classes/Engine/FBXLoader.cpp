#include "FBXLoader.h"
#include "Animation/AnimData/AnimDataModel.h"
#include "Components/Material/Material.h"
#include "Engine/FLoaderOBJ.h"
#include "Math/Rotator.h"

bool FFBXLoader::InitFBXManager()
{
    FbxManager = FbxManager::Create();

    FbxIOSettings* IOSetting = FbxIOSettings::Create(FbxManager, IOSROOT);
    FbxManager->SetIOSettings(IOSetting);

    return true;
}

FSkeletalMeshRenderData FFBXLoader::ParseFBX(const FString& FilePath)
{
    static bool bInitialized = false;
    if (bInitialized == false)
    {
        InitFBXManager();
        bInitialized = true;
    }
    FbxScene* Scene = FbxScene::Create(FbxManager, "myScene");
    FbxImporter* Importer = FbxImporter::Create(FbxManager, "myImporter");
    
    bool bResult = Importer->Initialize(GetData(FilePath), -1, FbxManager->GetIOSettings());
    if (!bResult)
        return {};
    
    Importer->Import(Scene);
    Importer->Destroy();

    FbxAxisSystem UnrealAxisSystem(
        FbxAxisSystem::eZAxis,
        FbxAxisSystem::eParityEven, // TODO Check
        FbxAxisSystem::eLeftHanded
    );
    if (Scene->GetGlobalSettings().GetAxisSystem() != UnrealAxisSystem)
        UnrealAxisSystem.DeepConvertScene(Scene);
    
    FbxSystemUnit SceneSystemUnit = Scene->GetGlobalSettings().GetSystemUnit();
    if( SceneSystemUnit.GetScaleFactor() != 1.0 )
    {
        FbxSystemUnit::cm.ConvertScene(Scene);
    }

    ParsedAnimData.Empty();
    FSkeletalMeshRenderData NewMeshData;
    FRefSkeletal RefSkeletal;
    
    NewMeshData.Name = FilePath;
    RefSkeletal.Name = FilePath;
    
    ExtractFBXMeshData(Scene, NewMeshData, RefSkeletal);
    ExtractFBXAnimData(Scene, FilePath);

    for (const auto Vertex: NewMeshData.Vertices)
    {
        FSkeletalVertex RawVertex = FSkeletalVertex();
        RawVertex = Vertex;
        RefSkeletal.RawVertices.Add(RawVertex);
    }

    for (const auto Bone : NewMeshData.Bones)
    {
        FBone RawBone;
        RawBone = Bone;
        RefSkeletal.RawBones.Add(RawBone);
    }
    
    SkeletalMeshData.Add(FilePath, NewMeshData);
    RefSkeletalData.Add(FilePath, RefSkeletal);
    Scene->Destroy();

    // caching
    std::filesystem::path fullpath(FilePath);
    FString binSaveFilePath = "Contents/FBX/" + fullpath.filename().string() + ".bin";
    FArchive ar;
    ar << FilePath;
    ar << NewMeshData << RefSkeletal;
    ar << ParsedAnimData.Num();
    for (const auto& parsed: ParsedAnimData)
    {
        ar << parsed.Key;
        ar << *parsed.Value;
    }
    FWindowsBinHelper::SaveToBin(binSaveFilePath, ar);
    
    return NewMeshData;
}

bool FFBXLoader::ParseSkeletalMeshAndSkeletonFromFBX(const FString& FilePath, FSkeletalMeshRenderData& OutMeshData, FRefSkeletal& OutRefSkeletal)
{
    // 에셋 식별자
    const std::filesystem::path fullpath(FilePath);
    // 파일명 뒤에 "_SkeletalMesh" 접미사를 붙입니다.
    FString BaseName = FString(fullpath.stem().string());
    
    static bool bInitialized = false;
    if (bInitialized == false)
    {
        InitFBXManager();
        bInitialized = true;
    }
    FbxScene* Scene = FbxScene::Create(FbxManager, "myScene");
    FbxImporter* Importer = FbxImporter::Create(FbxManager, "myImporter");
    
    bool bResult = Importer->Initialize(GetData(FilePath), -1, FbxManager->GetIOSettings());
    if (!bResult)
        return false;
    
    Importer->Import(Scene);
    Importer->Destroy();

    FbxAxisSystem UnrealAxisSystem(
        FbxAxisSystem::eZAxis,
        FbxAxisSystem::eParityEven, // TODO Check
        FbxAxisSystem::eLeftHanded
    );
    if (Scene->GetGlobalSettings().GetAxisSystem() != UnrealAxisSystem)
        UnrealAxisSystem.DeepConvertScene(Scene);
    
    FbxSystemUnit SceneSystemUnit = Scene->GetGlobalSettings().GetSystemUnit();
    if( SceneSystemUnit.GetScaleFactor() != 1.0 )
    {
        FbxSystemUnit::cm.ConvertScene(Scene);
    }

    ExtractFBXMeshData(Scene, OutMeshData, OutRefSkeletal);

    for (const auto Vertex: OutMeshData.Vertices)
    {
        FSkeletalVertex RawVertex = FSkeletalVertex();
        RawVertex = Vertex;
        OutRefSkeletal.RawVertices.Add(RawVertex);
    }

    for (const auto Bone : OutMeshData.Bones)
    {
        FBone RawBone;
        RawBone = Bone;
        OutRefSkeletal.RawBones.Add(RawBone);
    }

    const FName SkeletalMeshAssetName = FName(BaseName + TEXT("_SkeletalMesh"));
    const FName SkeletonAssetName = FName(BaseName + TEXT("_Skeleton"));

    OutMeshData.Name = SkeletalMeshAssetName.ToString();
    OutRefSkeletal.Name = SkeletonAssetName.ToString();
    
    SkeletalMeshData.Add(SkeletalMeshAssetName, OutMeshData);
    RefSkeletalData.Add(SkeletonAssetName, OutRefSkeletal);
    
    Scene->Destroy();

    return true;
}

bool FFBXLoader::ParseSkeletalMeshFromFBX(const FString& FilePath, FSkeletalMeshRenderData& OutMeshData)
{
    // 에셋 식별자
    const std::filesystem::path fullpath(FilePath);
    // 파일명 뒤에 "_SkeletalMesh" 접미사를 붙입니다.
    FString BaseName = FString(fullpath.stem().string());
    const FName AssetName = FName(BaseName + TEXT("_USkeletalMesh"));
    
    if (SkeletalMeshData.Contains(AssetName))
    {
        OutMeshData = SkeletalMeshData[AssetName];
        return true;
    }

    FRefSkeletal NewRefSkeletal;
    return ParseSkeletalMeshAndSkeletonFromFBX(FilePath, OutMeshData, NewRefSkeletal);
}

bool FFBXLoader::ParseSkeletonFromFBX(const FString& FilePath, FRefSkeletal& OutRefSkeletal)
{
    // 에셋 식별자
    const std::filesystem::path fullpath(FilePath);
    // 파일명 뒤에 "_SkeletalMesh" 접미사를 붙입니다.
    FString BaseName = FString(fullpath.stem().string());
    const FName AssetName = FName(BaseName + TEXT("_Skeleton"));
    if (RefSkeletalData.Contains(AssetName))
    {
        OutRefSkeletal = RefSkeletalData[AssetName];
        return true;
    }

    FSkeletalMeshRenderData NewSkeletalMesh;
    return ParseSkeletalMeshAndSkeletonFromFBX(FilePath, NewSkeletalMesh, OutRefSkeletal);
}

bool FFBXLoader::ParseAnimationFromFBX(const FString& FilePath, UAnimDataModel*& OutAnimDataModel)
{
    // 에셋 식별자
    const std::filesystem::path fullpath(FilePath);
    // 파일명 뒤에 "_SkeletalMesh" 접미사를 붙입니다.
    FString BaseName = FString(fullpath.stem().string());
    const FName AssetName = FName(* (BaseName + TEXT("_UAnimSequence")));
    if (AnimDataMap.Contains(AssetName))
    {
        OutAnimDataModel = AnimDataMap[AssetName];
        return true;
    }
    
    static bool bInitialized = false;
    if (bInitialized == false)
    {
        InitFBXManager();
        bInitialized = true;
    }
    FbxScene* Scene = FbxScene::Create(FbxManager, "myScene");
    FbxImporter* Importer = FbxImporter::Create(FbxManager, "myImporter");
    
    bool bResult = Importer->Initialize(GetData(FilePath), -1, FbxManager->GetIOSettings());
    if (!bResult)
        return false;
    
    Importer->Import(Scene);
    Importer->Destroy();

    FbxAxisSystem UnrealAxisSystem(
        FbxAxisSystem::eZAxis,
        FbxAxisSystem::eParityEven, // TODO Check
        FbxAxisSystem::eLeftHanded
    );
    if (Scene->GetGlobalSettings().GetAxisSystem() != UnrealAxisSystem)
        UnrealAxisSystem.DeepConvertScene(Scene);
    
    FbxSystemUnit SceneSystemUnit = Scene->GetGlobalSettings().GetSystemUnit();
    if( SceneSystemUnit.GetScaleFactor() != 1.0 )
    {
        FbxSystemUnit::cm.ConvertScene(Scene);
    }

    ExtractFBXAnimData(Scene, FilePath, OutAnimDataModel);

    return true;
}

bool FFBXLoader::ParseMaterialFromFBX(const FString& FilePath, TArray<FObjMaterialInfo>& OutMaterialInfos)
{
    static bool bInitialized = false;
    if (bInitialized == false)
    {
        InitFBXManager();
        bInitialized = true;
    }
    FbxScene* Scene = FbxScene::Create(FbxManager, "myScene");
    FbxImporter* Importer = FbxImporter::Create(FbxManager, "myImporter");
    
    bool bResult = Importer->Initialize(GetData(FilePath), -1, FbxManager->GetIOSettings());
    if (!bResult)
        return {};
    
    Importer->Import(Scene);
    Importer->Destroy();

    FbxAxisSystem UnrealAxisSystem(
        FbxAxisSystem::eZAxis,
        FbxAxisSystem::eParityEven, // TODO Check
        FbxAxisSystem::eLeftHanded
    );
    if (Scene->GetGlobalSettings().GetAxisSystem() != UnrealAxisSystem)
        UnrealAxisSystem.DeepConvertScene(Scene);
    
    FbxSystemUnit SceneSystemUnit = Scene->GetGlobalSettings().GetSystemUnit();
    if( SceneSystemUnit.GetScaleFactor() != 1.0 )
    {
        FbxSystemUnit::cm.ConvertScene(Scene);
    }

    FbxNode* RootNode = Scene->GetRootNode();
    if (RootNode == nullptr)
        return false;

    // 3) 재귀 추출 (중복 방지용 Set)
    TSet<FName> Processed;
    ExtractMaterialFromNode(Scene->GetRootNode(), OutMaterialInfos, Processed);

    // 4) 메시에서 머티리얼을 하나도 못 뽑았다면 기본 머티리얼 하나 추가
    if (OutMaterialInfos.Num() == 0)
    {
        OutMaterialInfos.Add(FObjMaterialInfo());
    }
}

FSkeletalMeshRenderData FFBXLoader::ParseBin(const FString FilePath)
{
    FSkeletalMeshRenderData NewMeshData;
    FRefSkeletal RefSkeletal;
    
    std::filesystem::path fullpath(FilePath);
    FArchive ar;
    FString originalFilePath;
    FWindowsBinHelper::LoadFromBin(FilePath, ar);
    ar >> originalFilePath;
    ar >> NewMeshData >> RefSkeletal;
    
    int ParsedAnimCount;
    FName AnimKey;
    UAnimDataModel* AnimData = FObjectFactory::ConstructObject<UAnimDataModel>(nullptr);
    ar >> ParsedAnimCount;
    for (int i = 0; i < ParsedAnimCount; ++i)
    {
        ar >> AnimKey;
        ar >> *AnimData;
        AnimDataMap.Add(AnimKey, AnimData);
    }
    
    SkeletalMeshData.Add(originalFilePath, NewMeshData);
    RefSkeletalData.Add(originalFilePath, RefSkeletal);
    
    return NewMeshData;
}

void FFBXLoader::ExtractFBXMeshData(const FbxScene* Scene, FSkeletalMeshRenderData& OutMeshData, FRefSkeletal& OutRefSkeletal)
{
    FbxNode* RootNode = Scene->GetRootNode();
    if (RootNode == nullptr)
        return;

    ExtractBoneFromNode(RootNode, OutMeshData, OutRefSkeletal);
    ExtractMeshFromNode(RootNode, OutMeshData, OutRefSkeletal);
}

void FFBXLoader::ExtractBoneFromNode(FbxNode* Node, FSkeletalMeshRenderData& OutMeshData, FRefSkeletal& OutRefSkeletal)
{
    // Clear existing bone tree data
    OutRefSkeletal.BoneTree.Empty();
    OutRefSkeletal.RootBoneIndices.Empty();
    OutRefSkeletal.BoneNameToIndexMap.Empty();

    // Before - collect all bone nodes 
    TArray<FbxNode*> BoneNodes;
    std::function<void(FbxNode*)> FindBones = [&BoneNodes, &FindBones](FbxNode* node)
    {
        if (!node)
            return;

        FbxNodeAttribute* attr = node->GetNodeAttribute();
        if (attr && attr->GetAttributeType() == FbxNodeAttribute::eSkeleton)
            BoneNodes.Add(node);

        for (int i = 0; i < node->GetChildCount(); ++i)
            FindBones(node->GetChild(i));
    };
    FindBones(Node);
    
    // First pass - collect all bone nodes from clusters and add to flat bone array
    for (int i = 0; i < BoneNodes.Num(); ++i)
    {
        FbxNode* BoneNode = BoneNodes[i];
        
        if (!BoneNode)
            continue;
        
        FString BoneName = BoneNode->GetName();
        /*FString LeftPart, RightPart;
        if (BoneName.Split(TEXT(":"), &LeftPart, &RightPart))
        {
            BoneName = RightPart;
        }*/

        // Check if this bone already exists
        int* ExistingBoneIndex = OutRefSkeletal.BoneNameToIndexMap.Find(BoneName);
        if (ExistingBoneIndex)
            continue;
            
        // Create new bone and add to array
        FBone NewBone;
        NewBone.BoneName = BoneName;
        NewBone.ParentIndex = -1; // Will be set in second pass
        
        // Get transform matrices
        FbxAMatrix GlobalTransform = BoneNode->EvaluateGlobalTransform();
        FbxAMatrix LocalTransform = BoneNode->EvaluateLocalTransform();
        
        // Store transforms
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                NewBone.GlobalTransform.M[i][j] = static_cast<float>(GlobalTransform.Get(i, j));
                NewBone.LocalTransform.M[i][j] = static_cast<float>(LocalTransform.Get(i, j));
            }
        }
        
        // Add bone to array and create mapping
        int BoneIndex = OutMeshData.Bones.Add(NewBone);
        OutRefSkeletal.BoneNameToIndexMap.Add(BoneName, BoneIndex);
        
        // Create corresponding bone tree node
        FBoneNode NewNode;
        NewNode.BoneName = BoneName;
        NewNode.BoneIndex = BoneIndex;
        OutRefSkeletal.BoneTree.Add(NewNode);
    }
    
    // Second pass - establish parent-child relationships
    for (int i = 0; i < BoneNodes.Num(); ++i)
    {
        FbxNode* BoneNode = BoneNodes[i];
        
        if (!BoneNode)
            continue;
            
        FString BoneName = BoneNode->GetName();
        FbxNode* ParentNode = BoneNode->GetParent();
        
        if (!OutRefSkeletal.BoneNameToIndexMap.Contains(BoneName))
            continue;
            
        int BoneIndex = OutRefSkeletal.BoneNameToIndexMap[BoneName];
        
        if (ParentNode)
        {
            FString ParentName = ParentNode->GetName();
            
            // If parent is also a bone, establish the relationship
            if (OutRefSkeletal.BoneNameToIndexMap.Contains(ParentName))
            {
                int ParentIndex = OutRefSkeletal.BoneNameToIndexMap[ParentName];
                
                // Update parent index in the bone
                OutMeshData.Bones[BoneIndex].ParentIndex = ParentIndex;
                
                // Add this bone as a child of the parent in the tree structure
                OutRefSkeletal.BoneTree[ParentIndex].ChildIndices.Add(BoneIndex);
            }
        }
    }

    // Find and Save Root bone nodes
    for (int i = 0; i < OutMeshData.Bones.Num(); ++i)
    {
        if (OutMeshData.Bones[i].ParentIndex == -1)
        {
            OutRefSkeletal.RootBoneIndices.Add(i);
        }
    }
}

/* Extract할 때 FBX의 Mapping Mode와 Reference Mode에 따라 모두 다르게 파싱을 진행해야 함!! */
void FFBXLoader::ExtractMeshFromNode(FbxNode* Node, FSkeletalMeshRenderData& OutMeshData, FRefSkeletal& OutRefSkeletal)
{
    FbxMesh* Mesh = Node->GetMesh();
    if (Mesh)
    {
        if (!IsTriangulated(Mesh))
        {
            FbxGeometryConverter Converter = FbxGeometryConverter(FbxManager);
            Converter.Triangulate(Mesh, true);
            Mesh = Node->GetMesh();
        }
        
        int BaseVertexIndex = OutMeshData.Vertices.Num();
        int BaseIndexOffset = OutMeshData.Indices.Num();
        
        for (int d = 0; d < Mesh->GetDeformerCount(FbxDeformer::eSkin); ++d)
        {
            auto* Skin = static_cast<FbxSkin*>(Mesh->GetDeformer(d, FbxDeformer::eSkin));
            if (Skin)
            {
                // BaseVertexIndex는 Vertex 추출 직후 오프셋을 위해 넘겨 줍니다.
                ProcessSkinning(Skin, OutMeshData, OutRefSkeletal, BaseVertexIndex);
                // 보통 메시당 하나의 Skin만 쓰므로 break 해도 무방합니다.
                break;
            }
        }

        // 버텍스 데이터 추출
        ExtractVertices(Mesh, OutMeshData, OutRefSkeletal);
        
        // 인덱스 데이터 추출.
        // 250510) ExtractVertices에서 같이 추출하도록 수정.
        // ExtractIndices(Mesh, MeshData, BaseVertexIndex);
        
        // 머테리얼 데이터 추출
        ExtractMaterials(Node, Mesh, OutMeshData, OutRefSkeletal, BaseIndexOffset);
        
        // 바운딩 박스 업데이트
        UpdateBoundingBox(OutMeshData);
    }

    // 자식 노드들에 대해 재귀적으로 수행
    int childCount = Node->GetChildCount();
    for (int i = 0; i < childCount; i++)
    {
        ExtractMeshFromNode(Node->GetChild(i), OutMeshData, OutRefSkeletal);
    }
}

void FFBXLoader::ExtractMaterialFromNode(FbxNode* Node, TArray<FObjMaterialInfo>& OutMaterialInfos, TSet<FName>& ProcessedMaterials)
{
    FbxMesh* Mesh = Node->GetMesh();
    if (Mesh)
    {
        if (!IsTriangulated(Mesh))
        {
            FbxGeometryConverter Converter = FbxGeometryConverter(FbxManager);
            Converter.Triangulate(Mesh, true);
            Mesh = Node->GetMesh();
        }

        auto* MatElem = Mesh->GetElementMaterial();
        int  MatCount = Node->GetMaterialCount();
        for (int32 i = 0; i < MatCount; ++i)
        {
            FbxSurfaceMaterial* SrcMtl = Node->GetMaterial(i);
            if (!SrcMtl) continue;

            // 중복 검사: FbxSurfaceMaterial::GetName()을 FName으로 변환
            FName MtlName = FName(FString(SrcMtl->GetName()));
            if (ProcessedMaterials.Contains(MtlName))
                continue;
            ProcessedMaterials.Add(MtlName);

            // 변환
            FObjMaterialInfo Info = ConvertFbxToObjMaterialInfo(SrcMtl);
            OutMaterialInfos.Add(MoveTemp(Info));
        }
    }

    // 자식 노드들에 대해 재귀적으로 수행
    int childCount = Node->GetChildCount();
    for (int i = 0; i < childCount; i++)
    {
        ExtractMaterialFromNode(Node->GetChild(i), OutMaterialInfos, ProcessedMaterials);
    }
}

void FFBXLoader::ExtractVertices(FbxMesh* Mesh, FSkeletalMeshRenderData& OutMeshData, FRefSkeletal& OutRefSkeletal)
{
    IndexMap.Empty();
    SkinWeightMap.Empty();
    ExtractSkinningData(Mesh, OutRefSkeletal);


    int polyCnt = Mesh->GetPolygonCount();
    for (int p = 0; p < polyCnt; ++p)
    {
        int polySize = Mesh->GetPolygonSize(p);
        for (int v = 0; v < polySize; ++v)
        {
            // int cpIdx = Mesh->GetPolygonVertex(p, v);
            FSkeletalVertex vertex = GetVertexFromControlPoint(Mesh, p, v);
            ExtractNormal(Mesh, vertex, p, v);
            ExtractUV(Mesh, vertex, p, v);
            ExtractTangent(Mesh, vertex, p, v);
            StoreWeights(Mesh, vertex, p, v);
            StoreVertex(vertex, OutMeshData);
        }
    }
}

FSkeletalVertex FFBXLoader::GetVertexFromControlPoint(FbxMesh* Mesh, int PolygonIndex, int VertexIndex)
{
    auto* ControlPoints = Mesh->GetControlPoints();
    FSkeletalVertex Vertex = FSkeletalVertex();

    // 위치
    // auto& CP = ControlPoints[ControlPointIndex];
    int controlPointIndex = Mesh->GetPolygonVertex(PolygonIndex, VertexIndex);
    Vertex.Position.X = static_cast<float>(ControlPoints[controlPointIndex][0]);
    Vertex.Position.Y = static_cast<float>(ControlPoints[controlPointIndex][1]);
    Vertex.Position.Z = static_cast<float>(ControlPoints[controlPointIndex][2]);
    Vertex.Position.W = 1.0f;
    
    // 기본값
    Vertex.Normal   = FVector(0.0f, 0.0f, 1.0f);
    Vertex.TexCoord = FVector2D(0.0f, 0.0f);
    Vertex.Tangent  = FVector4(1.0f, 0.0f, 0.0f, 1.0f);

    return Vertex;
}

void FFBXLoader::ExtractNormal(FbxMesh* Mesh, FSkeletalVertex& Vertex, int PolygonIndex, int VertexIndex)
{
    FbxLayerElementNormal* NormalElem = Mesh->GetElementNormal();
    if (!NormalElem) return;

    // 매핑·레퍼런스 모드
    auto mapMode = NormalElem->GetMappingMode();
    auto refMode = NormalElem->GetReferenceMode();
    int index = -1;

    switch (mapMode)
    {
    case FbxLayerElement::eNone:
        break;
    case FbxLayerElement::eByControlPoint:
        index = Mesh->GetPolygonVertex(PolygonIndex, VertexIndex);
        break;
    case FbxLayerElement::eByPolygonVertex:
        index = PolygonIndex * 3 + VertexIndex;
        break;
    case FbxLayerElement::eByPolygon:
        index = PolygonIndex;
        break;
    case FbxLayerElement::eByEdge:
        break;
    case FbxLayerElement::eAllSame:
        break;
    }

    if (refMode != FbxLayerElement::eDirect)
        index = NormalElem->GetIndexArray().GetAt(index);
    
    auto Normal = NormalElem->GetDirectArray().GetAt(index);
    Vertex.Normal.X = Normal[0];
    Vertex.Normal.Y = Normal[1];
    Vertex.Normal.Z = Normal[2];

}

void FFBXLoader::ExtractUV(FbxMesh* Mesh, FSkeletalVertex& Vertex, int PolygonIndex, int VertexIndex)
{
    FbxLayerElementUV* UVElem = Mesh->GetElementUV(0);
    if (!UVElem) return;

    // 매핑·레퍼런스 모드
    auto mapMode = UVElem->GetMappingMode();
    auto refMode = UVElem->GetReferenceMode();
    int index = -1;

    switch (mapMode)
    {
    case FbxLayerElement::eNone:
        break;
    case FbxLayerElement::eByControlPoint:
        index = Mesh->GetPolygonVertex(PolygonIndex, VertexIndex);
        break;
    case FbxLayerElement::eByPolygonVertex:
        index = PolygonIndex * 3 + VertexIndex;
        break;
    case FbxLayerElement::eByPolygon:
        index = PolygonIndex;
        break;
    case FbxLayerElement::eByEdge:
        break;
    case FbxLayerElement::eAllSame:
        break;
    }

    if (refMode != FbxLayerElement::eDirect)
        index = UVElem->GetIndexArray().GetAt(index);

    auto UV = UVElem->GetDirectArray().GetAt(index);
    Vertex.TexCoord.X = UV[0];
    Vertex.TexCoord.Y = 1.0f - UV[1];  // DirectX 좌표계 보정

}

void FFBXLoader::ExtractTangent(FbxMesh* Mesh, FSkeletalVertex& Vertex, int PolygonIndex, int VertexIndex)
{
    auto* TanElem = Mesh->GetElementTangent(0);
    if (!TanElem || TanElem->GetDirectArray().GetCount() == 0)
    {
        Mesh->GenerateTangentsData(0, /*overwrite=*/ true);
        TanElem = Mesh->GetElementTangent(0);
        if (!TanElem) return;
    }

    auto mapMode = TanElem->GetMappingMode();
    auto refMode = TanElem->GetReferenceMode();
    int index = -1;

    switch (mapMode)
    {
    case FbxLayerElement::eNone:
        break;
    case FbxLayerElement::eByControlPoint:
        index = Mesh->GetPolygonVertex(PolygonIndex, VertexIndex);
        break;
    case FbxLayerElement::eByPolygonVertex:
        index = PolygonIndex * 3 + VertexIndex;
        break;
    case FbxLayerElement::eByPolygon:
        index = PolygonIndex;
        break;
    case FbxLayerElement::eByEdge:
        break;
    case FbxLayerElement::eAllSame:
        break;
    }

    if (refMode != FbxLayerElement::eDirect)
        index = TanElem->GetIndexArray().GetAt(index);
    
    auto Tan = TanElem->GetDirectArray().GetAt(index);
    Vertex.Tangent.X = Tan[0];
    Vertex.Tangent.Y = Tan[1];
    Vertex.Tangent.Z = Tan[2];
    Vertex.Tangent.W = Tan[3];
}

void FFBXLoader::ExtractSkinningData(FbxMesh* Mesh, FRefSkeletal& OutRefSkeletal)
{
    if (!Mesh) return;
    
    int skinCount = Mesh->GetDeformerCount(FbxDeformer::eSkin);
    for (int s = 0; s < skinCount; ++s)
    {
        FbxSkin* skin = static_cast<FbxSkin*>(Mesh->GetDeformer(s, FbxDeformer::eSkin));
        int clustureCount = skin->GetClusterCount();
        for (int c = 0; c < clustureCount; ++c)
        {
            FbxCluster* cluster = skin->GetCluster(c);
            FbxNode* linkedBone = cluster->GetLink();
            if (!linkedBone)
                continue;

            FString boneName = linkedBone->GetName();
            int boneIndex = OutRefSkeletal.BoneNameToIndexMap[boneName];

            int* indices = cluster->GetControlPointIndices();
            double* weights = cluster->GetControlPointWeights();
            int count = cluster->GetControlPointIndicesCount();
            for (int i = 0; i < count; ++i)
            {
                int ctrlIdx = indices[i];
                float weight = static_cast<float>(weights[i]);
                if (!SkinWeightMap.Contains(ctrlIdx))
                    SkinWeightMap.Add(ctrlIdx, TArray<FBoneWeightInfo>());
                SkinWeightMap[ctrlIdx].Add({boneIndex, weight});
            }
            
        }
    } 
}

void FFBXLoader::StoreWeights(FbxMesh* Mesh, FSkeletalVertex& Vertex, int PolygonIndex, int VertexIndex)
{
    int ControlPointIndex = Mesh->GetPolygonVertex(PolygonIndex, VertexIndex);
    for (int i = 0; i < std::min(SkinWeightMap[ControlPointIndex].Num(), 4); ++i)
    {
        const FBoneWeightInfo& info = SkinWeightMap[ControlPointIndex][i];
        if (i == 0)
        {
            Vertex.BoneIndices0 = info.BoneIndex;
            Vertex.BoneWeights0 = info.BoneWeight;
        }
        else if (i == 1)
        {
            Vertex.BoneIndices1 = info.BoneIndex;
            Vertex.BoneWeights1 = info.BoneWeight;
        }
        else if (i == 2)
        {
            Vertex.BoneIndices2 = info.BoneIndex;
            Vertex.BoneWeights2 = info.BoneWeight;
        }
        else if (i == 3)
        {
            Vertex.BoneIndices3 = info.BoneIndex;
            Vertex.BoneWeights3 = info.BoneWeight;
        }
    }
}

void FFBXLoader::StoreVertex(FSkeletalVertex& vertex, FSkeletalMeshRenderData& OutMeshData)
{
    std::stringstream ss;
    ss << vertex.Position.X << "," << vertex.Position.Y << "," << vertex.Position.Z << ",";
    ss << vertex.Normal.X << "," << vertex.Normal.Y << "," << vertex.Normal.Z << ",";
    ss << vertex.TexCoord.X << "," << vertex.TexCoord.Y;
    FString key = ss.str();
    uint32 index;
    if (!IndexMap.Contains(key))
    {
        index = OutMeshData.Vertices.Num();
        OutMeshData.Vertices.Add(vertex);
        IndexMap[key] = index;
    } else
    {
        index = IndexMap[key];
    }
    OutMeshData.Indices.Add(index);
}

void FFBXLoader::ProcessSkinning(FbxSkin* Skin, FSkeletalMeshRenderData& OutMeshData, FRefSkeletal& OutRefSkeletal, int BaseVertexIndex)
{
    int ClusterCount = Skin->GetClusterCount();

    for (int ClusterIndex = 0; ClusterIndex < ClusterCount; ClusterIndex++)
    {
        FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);
        FbxNode* BoneNode = Cluster->GetLink();
        
        if (!BoneNode)
            continue;
            
        FString BoneName = BoneNode->GetName();
        int* ExistingBoneIndex = OutRefSkeletal.BoneNameToIndexMap.Find(BoneName);
        if (!ExistingBoneIndex)
            continue;
        FBone& bone = OutMeshData.Bones[*ExistingBoneIndex];
            
        // Get binding pose transformation
        // NewBone.InverseBindPoseMatrix = FMatrix::Inverse(NewBone.GlobalTransform);
        FbxAMatrix MeshTransform; // Mesh의 바인드 시점의 Global Transform
        Cluster->GetTransformMatrix(MeshTransform); // 메시의 변환 행렬 (기준점)
        FbxAMatrix LinkTransform; // Bone의 바인드 시점의 Global Transform
        Cluster->GetTransformLinkMatrix(LinkTransform); // 본의 바인드 포즈 행렬
        
        FbxAMatrix InverseBindMatrix = LinkTransform.Inverse() * MeshTransform;

        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                bone.InverseBindPoseMatrix.M[i][j] = static_cast<float>(InverseBindMatrix.Get(i, j));
            }
        }
        
        bone.SkinningMatrix = bone.InverseBindPoseMatrix * bone.GlobalTransform;
    }
    
    // Process vertex weights
    for (int ClusterIndex = 0; ClusterIndex < ClusterCount; ClusterIndex++)
    {
        FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);
        FbxNode* BoneNode = Cluster->GetLink();
        
        if (!BoneNode || !OutRefSkeletal.BoneNameToIndexMap.Contains(BoneNode->GetName()))
            continue;
        
        int BoneIndex = OutRefSkeletal.BoneNameToIndexMap[BoneNode->GetName()];
        
        // Get control point indices and weights
        int VertexCount = Cluster->GetControlPointIndicesCount();
        int* ControlPointIndices = Cluster->GetControlPointIndices();
        double* ControlPointWeights = Cluster->GetControlPointWeights();
        
        // Apply weights to vertices
        for (int i = 0; i < VertexCount; i++)
        {
            int VertexIndex = BaseVertexIndex + ControlPointIndices[i];
            float Weight = static_cast<float>(ControlPointWeights[i]);
            
            // Make sure vertex index is valid
            if (VertexIndex >= 0 && VertexIndex < OutMeshData.Vertices.Num())
            {
                if (OutMeshData.Vertices[VertexIndex].BoneWeights0 == 0.0f)
                {
                    OutMeshData.Vertices[VertexIndex].BoneIndices0 = BoneIndex;
                    OutMeshData.Vertices[VertexIndex].BoneWeights0 = Weight;
                }
                else if (OutMeshData.Vertices[VertexIndex].BoneWeights1 == 0.0f)
                {
                    OutMeshData.Vertices[VertexIndex].BoneIndices1 = BoneIndex;
                    OutMeshData.Vertices[VertexIndex].BoneWeights1 = Weight;
                }
                else if (OutMeshData.Vertices[VertexIndex].BoneWeights2 == 0.0f)
                {
                    OutMeshData.Vertices[VertexIndex].BoneIndices2 = BoneIndex;
                    OutMeshData.Vertices[VertexIndex].BoneWeights2 = Weight;
                }
                else if (OutMeshData.Vertices[VertexIndex].BoneWeights3 == 0.0f)
                {
                    OutMeshData.Vertices[VertexIndex].BoneIndices3 = BoneIndex;
                    OutMeshData.Vertices[VertexIndex].BoneWeights3 = Weight;
                }
            }
        }
    }
}

void FFBXLoader::ExtractIndices(FbxMesh* Mesh, FSkeletalMeshRenderData& OutMeshData, int BaseVertexIndex)
{
    // 1) 정점 생성 때 쓴 매핑 모드 재확인
    auto* NormalElem = Mesh->GetElementNormal();
    auto  mapMode    = NormalElem
                      ? NormalElem->GetMappingMode()
                      : FbxGeometryElement::eByControlPoint;

    int polyVertCounter = 0;   // 폴리곤-버텍스 모드용 누적 카운터
    int polyCount       = Mesh->GetPolygonCount();

    for (int p = 0; p < polyCount; ++p)
    {
        int polySize = Mesh->GetPolygonSize(p);
        int pvStart  = polyVertCounter;  // 이 폴리곤의 시작 polyVert 인덱스

        // 삼각형 팬 트라이앵글
        for (int i = 2; i < polySize; ++i)
        {
            if (mapMode == FbxGeometryElement::eByControlPoint)
            {
                // ControlPoint 기준 인덱스
                int c0 = Mesh->GetPolygonVertex(p, 0);
                int c1 = Mesh->GetPolygonVertex(p, i - 1);
                int c2 = Mesh->GetPolygonVertex(p, i);
                OutMeshData.Indices.Add(BaseVertexIndex + c0);
                OutMeshData.Indices.Add(BaseVertexIndex + c1);
                OutMeshData.Indices.Add(BaseVertexIndex + c2);
            }
            else
            {
                // 폴리곤-버텍스 기준 인덱스
                OutMeshData.Indices.Add(BaseVertexIndex + pvStart + 0);
                OutMeshData.Indices.Add(BaseVertexIndex + pvStart + (i - 1));
                OutMeshData.Indices.Add(BaseVertexIndex + pvStart + i);
            }
        }

        polyVertCounter += polySize;
    }
}

void FFBXLoader::ExtractMaterials(FbxNode* Node, FbxMesh* Mesh, FSkeletalMeshRenderData& OutMeshData, FRefSkeletal& OutRefSkeletal, int BaseIndexOffset)
{
    auto* MatElem = Mesh->GetElementMaterial();
    int  matCount = Node->GetMaterialCount();

    // 매핑·레퍼런스 모드
    auto mapMode = MatElem
                 ? MatElem->GetMappingMode()
                 : FbxGeometryElement::eAllSame;
    auto refMode = MatElem
                 ? MatElem->GetReferenceMode()
                 : FbxGeometryElement::eDirect;

    int polyVertCounter = 0;
    int polyCount       = Mesh->GetPolygonCount();

    // 총 삼각형 수 미리 계산 (eAllSame, eByPolygon 모두 공통)
    int totalTris = 0;
    for (int p = 0; p < polyCount; ++p)
        totalTris += Mesh->GetPolygonSize(p) - 2;

    int currentOffset = BaseIndexOffset;

    for (int matIdx = 0; matIdx < matCount; ++matIdx)
    {
        // 이 재질에 속하는 삼각형 개수 세기
        int triCount = 0;
        polyVertCounter = 0;

        for (int p = 0; p < polyCount; ++p)
        {
            int polySize = Mesh->GetPolygonSize(p);

            // 폴리곤 하나당 매핑된 재질 인덱스를 구하는 방법
            int thisMat = 0;
            switch (mapMode)
            {
            case FbxGeometryElement::eAllSame:
                thisMat = 0; 
                break;
            case FbxGeometryElement::eByPolygon:
                thisMat = MatElem->GetIndexArray().GetAt(p);
                break;
            case FbxGeometryElement::eByPolygonVertex:
                {
                    // 1) 항상 IndexArray에서 머티리얼 레이어 인덱스 꺼내기
                    int layerMatIdx = MatElem->GetIndexArray().GetAt(polyVertCounter);

                    // 2) 그 인덱스로 노드의 실제 머티리얼 얻기
                    //    (thisMat은 단순히 머티리얼 번호로 사용)
                    thisMat = layerMatIdx;
                    break;
                }
            default:
                thisMat = 0;
            }

            if (thisMat == matIdx)
                triCount += (polySize - 2);

            polyVertCounter += (mapMode == FbxGeometryElement::eByPolygonVertex
                                ? polySize
                                : 1);
        }

        // Subset 만들기
        // Material 생성 & 등록
        FbxSurfaceMaterial* srcMtl = Node->GetMaterial(matIdx);
        FString             mtlName = srcMtl ? FString(srcMtl->GetName()) : TEXT("Mat") + FString::FromInt(matIdx);
        auto                newMtl = FManagerOBJ::CreateMaterial(ConvertFbxToObjMaterialInfo(srcMtl));
        int                 finalIdx = OutRefSkeletal.Materials.Add(newMtl);

        FMaterialSubset subset;
        subset.MaterialName  = mtlName;
        subset.MaterialIndex = finalIdx;
        subset.IndexStart    = currentOffset;
        subset.IndexCount    = triCount * 3;
        OutRefSkeletal.MaterialSubsets.Add(subset);

        currentOffset += triCount * 3;
    }

    // 재질이 하나도 없으면 디폴트
    if (matCount == 0)
    {
        UMaterial* DefaultMaterial = FManagerOBJ::GetDefaultMaterial();
        int MaterialIndex = OutRefSkeletal.Materials.Add(DefaultMaterial);
        
        FMaterialSubset Subset;
        Subset.MaterialName = DefaultMaterial->GetName();
        Subset.MaterialIndex = MaterialIndex;
        Subset.IndexStart = BaseIndexOffset;
        Subset.IndexCount = OutMeshData.Indices.Num() - BaseIndexOffset;
        
        OutRefSkeletal.MaterialSubsets.Add(Subset);
    }
}

void FFBXLoader::UpdateBoundingBox(FSkeletalMeshRenderData& MeshData)
{
    if (MeshData.Vertices.Num() == 0)
        return;
        
    // 초기값 설정
    FVector Min = MeshData.Vertices[0].Position.xyz();
    FVector Max = MeshData.Vertices[0].Position.xyz();
    
    // 모든 정점을 순회하며 최소/최대값 업데이트
    for (int i = 1; i < MeshData.Vertices.Num(); i++)
    {
        const FVector& Pos = MeshData.Vertices[i].Position.xyz();
        
        // 최소값 갱신
        Min.X = FMath::Min(Min.X, Pos.X);
        Min.Y = FMath::Min(Min.Y, Pos.Y);
        Min.Z = FMath::Min(Min.Z, Pos.Z);
        
        // 최대값 갱신
        Max.X = FMath::Max(Max.X, Pos.X);
        Max.Y = FMath::Max(Max.Y, Pos.Y);
        Max.Z = FMath::Max(Max.Z, Pos.Z);
    }
    
    // 바운딩 박스 설정
    MeshData.BoundingBox.Min = Min;
    MeshData.BoundingBox.Max = Max;
}

void FFBXLoader::ExtractFBXAnimData(const FbxScene* scene, const FString& FilePath)
{
    int AnimStackCount = scene->GetSrcObjectCount<FbxAnimStack>();

    // Before - collect all bone nodes 
    TArray<FbxNode*> BoneNodes;
    std::function<void(FbxNode*)> FindBones = [&BoneNodes, &FindBones](FbxNode* node)
    {
        if (!node)
            return;

        FbxNodeAttribute* attr = node->GetNodeAttribute();
        if (attr && attr->GetAttributeType() == FbxNodeAttribute::eSkeleton)
            BoneNodes.Add(node);

        for (int i = 0; i < node->GetChildCount(); ++i)
            FindBones(node->GetChild(i));
    };
    FindBones(scene->GetRootNode());

    // parse AnimClips (AnimLayers)
    for (int i = 0; i < AnimStackCount; ++i)
    {
        FbxAnimStack* AnimStack = scene->GetSrcObject<FbxAnimStack>(i);
        if (!AnimStack)
            continue;

        ExtractAnimClip(AnimStack, BoneNodes, FilePath);
    }
}

void FFBXLoader::ExtractFBXAnimData(const FbxScene* scene, const FString& FilePath, UAnimDataModel*& OutAnimDataModel)
{
    int AnimStackCount = scene->GetSrcObjectCount<FbxAnimStack>();

    // Before - collect all bone nodes 
    TArray<FbxNode*> BoneNodes;
    std::function<void(FbxNode*)> FindBones = [&BoneNodes, &FindBones](FbxNode* node)
    {
        if (!node)
            return;

        FbxNodeAttribute* attr = node->GetNodeAttribute();
        if (attr && attr->GetAttributeType() == FbxNodeAttribute::eSkeleton)
            BoneNodes.Add(node);

        for (int i = 0; i < node->GetChildCount(); ++i)
            FindBones(node->GetChild(i));
    };
    FindBones(scene->GetRootNode());

    // parse AnimClips (AnimLayers)
    for (int i = 0; i < AnimStackCount; ++i)
    {
        FbxAnimStack* AnimStack = scene->GetSrcObject<FbxAnimStack>(i);
        if (!AnimStack)
            continue;

        ExtractAnimClip(AnimStack, BoneNodes, FilePath, OutAnimDataModel);
    }
}

void FFBXLoader::ExtractAnimClip(FbxAnimStack* AnimStack, const TArray<FbxNode*>& BoneNodes, const FString& FilePath)
{
    UAnimDataModel* AnimData = FObjectFactory::ConstructObject<UAnimDataModel>(nullptr);
    AnimData->Name = AnimStack->GetName();
    
    FbxTime::EMode timeMode = AnimStack->GetScene()->GetGlobalSettings().GetTimeMode();
    switch (timeMode)
    {
    case FbxTime::eFrames120:
        AnimData->FrameRate = FFrameRate(120, 1); break;
    case FbxTime::eFrames100:
        AnimData->FrameRate = FFrameRate(100, 1); break;
    case FbxTime::eFrames60:
        AnimData->FrameRate = FFrameRate(60, 1); break;
    case FbxTime::eFrames50:
        AnimData->FrameRate = FFrameRate(50, 1); break;
    case FbxTime::eFrames48:
        AnimData->FrameRate = FFrameRate(48, 1); break;
    case FbxTime::eFrames30:
        AnimData->FrameRate = FFrameRate(30, 1); break;
    case FbxTime::eFrames24:
        AnimData->FrameRate = FFrameRate(24, 1); break;
    }

    AnimData->PlayLength = static_cast<float>(AnimStack->GetLocalTimeSpan().GetDuration().GetSecondDouble());
    AnimData->NumberOfFrames = FMath::Floor( AnimData->PlayLength / AnimData->FrameRate.AsInterval() + 0.5f );
    AnimData->NumberOfKeys = AnimData->NumberOfFrames + 1;
    
    int layerCount = AnimStack->GetMemberCount<FbxAnimLayer>();
    for (int i = 0; i < BoneNodes.Num(); ++i)
    {
        FbxNode* BoneNode = BoneNodes[i];
        if (!BoneNode)
            continue;
        FBoneAnimationTrack AnimTrack;
        AnimTrack.Name = BoneNode->GetName();
        
        FRawAnimSequenceTrack AnimTrackData;
        for (int j = 0; j < layerCount; ++j)
        {
            FbxAnimLayer* AnimLayer = AnimStack->GetMember<FbxAnimLayer>(j);
            if (!AnimLayer)
                continue;

            ExtractAnimTrack(BoneNode, AnimTrackData, AnimData);
            // ExtractAnimCurve(AnimLayer, BoneNode, AnimTrackData);
        }
        AnimTrack.InternalTrackData = AnimTrackData;

        AnimData->BoneAnimationTracks.Add(AnimTrack);
    }

    FName key(FilePath + "/" + AnimData->Name);
    AnimDataMap.Add(key, AnimData);
    ParsedAnimData.Add(key, AnimData);
}

void FFBXLoader::ExtractAnimClip(FbxAnimStack* AnimStack, const TArray<FbxNode*>& BoneNodes, const FString& FilePath,
    UAnimDataModel*& OutAnimDataModel)
{
    FbxTime::EMode timeMode = AnimStack->GetScene()->GetGlobalSettings().GetTimeMode();
    switch (timeMode)
    {
    case FbxTime::eFrames120:
        OutAnimDataModel->FrameRate = FFrameRate(120, 1); break;
    case FbxTime::eFrames100:
        OutAnimDataModel->FrameRate = FFrameRate(100, 1); break;
    case FbxTime::eFrames60:
        OutAnimDataModel->FrameRate = FFrameRate(60, 1); break;
    case FbxTime::eFrames50:
        OutAnimDataModel->FrameRate = FFrameRate(50, 1); break;
    case FbxTime::eFrames48:
        OutAnimDataModel->FrameRate = FFrameRate(48, 1); break;
    case FbxTime::eFrames30:
        OutAnimDataModel->FrameRate = FFrameRate(30, 1); break;
    case FbxTime::eFrames24:
        OutAnimDataModel->FrameRate = FFrameRate(24, 1); break;
    }

    OutAnimDataModel->PlayLength = static_cast<float>(AnimStack->GetLocalTimeSpan().GetDuration().GetSecondDouble());
    OutAnimDataModel->NumberOfFrames = FMath::Floor( OutAnimDataModel->PlayLength / OutAnimDataModel->FrameRate.AsInterval() + 0.5f );
    OutAnimDataModel->NumberOfKeys = OutAnimDataModel->NumberOfFrames + 1;
    
    int layerCount = AnimStack->GetMemberCount<FbxAnimLayer>();
    for (int i = 0; i < BoneNodes.Num(); ++i)
    {
        FbxNode* BoneNode = BoneNodes[i];
        if (!BoneNode)
            continue;
        FBoneAnimationTrack AnimTrack;
        AnimTrack.Name = BoneNode->GetName();
        
        FRawAnimSequenceTrack AnimTrackData;
        for (int j = 0; j < layerCount; ++j)
        {
            FbxAnimLayer* AnimLayer = AnimStack->GetMember<FbxAnimLayer>(j);
            if (!AnimLayer)
                continue;

            ExtractAnimTrack(BoneNode, AnimTrackData, OutAnimDataModel);
            // ExtractAnimCurve(AnimLayer, BoneNode, AnimTrackData);
        }
        AnimTrack.InternalTrackData = AnimTrackData;

        OutAnimDataModel->BoneAnimationTracks.Add(AnimTrack);
    }
    // 에셋 식별자
    const std::filesystem::path fullpath(FilePath);
    // 파일명 뒤에 "_SkeletalMesh" 접미사를 붙입니다.
    FString BaseName = FString(fullpath.stem().string());
    const FName AssetName = FName(* (BaseName + TEXT("_Animation")));

    AnimDataMap.Add(AssetName, OutAnimDataModel);
    ParsedAnimData.Add(AssetName, OutAnimDataModel);
}

void FFBXLoader::ExtractAnimTrack(FbxNode* BoneNode, FRawAnimSequenceTrack& AnimTrack, const UAnimDataModel* AnimData)
{
    for (int i = 0; i < AnimData->NumberOfKeys; ++i)
    {
        FbxTime t;
        t.SetTime(0, 0, 0, i);
        FbxAMatrix tf = BoneNode->EvaluateLocalTransform(t);
        FbxVector4 translation = tf.GetT();
        FbxQuaternion rotation = tf.GetQ();
        FbxVector4 scaling = tf.GetS();
        
        AnimTrack.PosKeys.Add(FVector(
            translation[0],
            translation[1],
            translation[2]
        ));

        // quaternion 순서 맞추기 (xyzw -> wxyz)
        AnimTrack.RotKeys.Add(FQuat(
            rotation[3],
            rotation[0],
            rotation[1],
            rotation[2]
        ));

        AnimTrack.ScaleKeys.Add(FVector(
            scaling[0],
            scaling[1],
            scaling[2]
        ));
    }
}

void FFBXLoader::ExtractAnimTrack(FbxNode* BoneNode, FRawAnimSequenceTrack& AnimTrack, UAnimDataModel*& OutAnimDataModel)
{
    for (int i = 0; i < OutAnimDataModel->NumberOfKeys; ++i)
    {
        FbxTime t;
        t.SetTime(0, 0, 0, i);
        FbxAMatrix tf = BoneNode->EvaluateLocalTransform(t);
        FbxVector4 translation = tf.GetT();
        FbxQuaternion rotation = tf.GetQ();
        FbxVector4 scaling = tf.GetS();
        
        AnimTrack.PosKeys.Add(FVector(
            translation[0],
            translation[1],
            translation[2]
        ));

        // quaternion 순서 맞추기 (xyzw -> wxyz)
        AnimTrack.RotKeys.Add(FQuat(
            rotation[3],
            rotation[0],
            rotation[1],
            rotation[2]
        ));

        AnimTrack.ScaleKeys.Add(FVector(
            scaling[0],
            scaling[1],
            scaling[2]
        ));
    }
}

void FFBXLoader::ExtractAnimCurve(FbxAnimLayer* AnimLayer, FbxNode* BoneNode, FRawAnimSequenceTrack& AnimTrack)
{
    FbxAnimCurve* tx = BoneNode->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X);
    FbxAnimCurve* ty = BoneNode->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y);
    FbxAnimCurve* tz = BoneNode->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z);
    FbxAnimCurve* rx = BoneNode->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X);
    FbxAnimCurve* ry = BoneNode->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y);
    FbxAnimCurve* rz = BoneNode->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z);
    FbxAnimCurve* sx = BoneNode->LclScaling.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X);
    FbxAnimCurve* sy = BoneNode->LclScaling.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y);
    FbxAnimCurve* sz = BoneNode->LclScaling.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z);

    TArray<FbxTime> keyTimes;
    auto HasKeyTimes = [&keyTimes](FbxTime t) -> bool
    {
        for (const FbxTime& k : keyTimes)
        {
            if (k.GetSecondDouble() == t.GetSecondDouble())
                return true;
        }
        return false;
    };
    
    if (tx && ty && tz)
    {
        int keyCount = tx->KeyGetCount();
        for (int i = 0; i < keyCount; ++i)
        {
            FbxTime t = tx->KeyGetTime(i);
            if (!HasKeyTimes(t))
                keyTimes.Add(t);
        }
    }
    if (rx && ry && rz)
    {
        int keyCount = rx->KeyGetCount();
        for (int i = 0; i < keyCount; ++i)
        {
            FbxTime t = rx->KeyGetTime(i);
            if (!HasKeyTimes(t))
                keyTimes.Add(t);
        }
    }
    if (sx && sy && sz)
    {
        int keyCount = sx->KeyGetCount();
        for (int i = 0; i < keyCount; ++i)
        {
            FbxTime t = sx->KeyGetTime(i);
            if (!HasKeyTimes(t))
                keyTimes.Add(t);
        }
    }
    std::sort(keyTimes.begin(), keyTimes.end(), [](const FbxTime& a, const FbxTime& b)->bool { return a.GetSecondDouble() < b.GetSecondDouble(); });

    for (const FbxTime& time : keyTimes)
    {
        float t = time.GetSecondDouble();
        AnimTrack.KeyTimes.Add(t);
        if (tx && ty && tz)
        {
            AnimTrack.PosKeys.Add( FVector(
                tx->Evaluate(time),    
                ty->Evaluate(time),    
                tz->Evaluate(time)    
            ));
        }
        if (rx && ry && rz)
        {
            AnimTrack.RotKeys.Add( FRotator(
                rx->Evaluate(time),
                ry->Evaluate(time),
                rz->Evaluate(time)
            ).ToQuaternion());
        }
        if (sx && sy && sz)
        {
            AnimTrack.ScaleKeys.Add( FVector(
                sx->Evaluate(time),    
                sy->Evaluate(time),    
                sz->Evaluate(time)    
            ));
        }
    }

    // 원래는 key마다 보간 모드가 개별로 있는데
    // 현재 자료구조상 하드코딩으로 설정
    AnimTrack.InterpMode = EAnimInterpolationType::Cubic;
}

FSkeletalMeshRenderData FFBXLoader::GetSkeletalRenderData(const FString& FilePath)
{
    // TODO: 폴더에서 가져올 수 있으면 가져오기
    if (SkeletalMeshData.Contains(FilePath))
    {
        return SkeletalMeshData[FilePath];
    }
    
    return {};
}

UAnimDataModel* FFBXLoader::GetAnimData(const FString& FilePath)
{
    if (AnimDataMap.Contains(FilePath))
    {
        return AnimDataMap[FilePath];
    }
    
    return nullptr;
}

// FBX 머티리얼 → FObjMaterialInfo 변환 헬퍼
FObjMaterialInfo FFBXLoader::ConvertFbxToObjMaterialInfo(FbxSurfaceMaterial* FbxMat, const FString& BasePath)
{
    FObjMaterialInfo OutInfo;

    // Material Name
    OutInfo.MTLName = FString(FbxMat->GetName());
    
    // Lambert 전용 프로퍼티
    if (auto* Lam = FbxCast<FbxSurfaceLambert>(FbxMat))
    {
        // Ambient
        {
            auto c = Lam->Ambient.Get();
            float f = (float)Lam->AmbientFactor.Get();
            OutInfo.Ambient = FVector(c[0]*f, c[1]*f, c[2]*f);
        }
        // Diffuse
        {
            auto c = Lam->Diffuse.Get();
            float f = (float)Lam->DiffuseFactor.Get();
            OutInfo.Diffuse = FVector(c[0]*f, c[1]*f, c[2]*f);
        }
        // Emissive
        {
            auto c = Lam->Emissive.Get();
            float f = (float)Lam->EmissiveFactor.Get();
            OutInfo.Emissive = FVector(c[0]*f, c[1]*f, c[2]*f);
        }
        // BumpScale
        OutInfo.NormalScale = (float)Lam->BumpFactor.Get();
    }

    // Phong 전용 프로퍼티
    if (auto* Pho = FbxCast<FbxSurfacePhong>(FbxMat))
    {
        // Specular
        {
            auto c = Pho->Specular.Get();
            OutInfo.Specular = FVector((float)c[0], (float)c[1], (float)c[2]);
        }
        // Shininess
        OutInfo.SpecularScalar = (float)Pho->Shininess.Get();
    }

    // 공통 프로퍼티
    {
        // TransparencyFactor
        if (auto prop = FbxMat->FindProperty(FbxSurfaceMaterial::sTransparencyFactor); prop.IsValid())
        {
            double tf = prop.Get<FbxDouble>();
            OutInfo.TransparencyScalar = (float)tf;
            OutInfo.bTransparent = OutInfo.TransparencyScalar < 1.f - KINDA_SMALL_NUMBER;
        }

        // Index of Refraction
        constexpr char const* sIndexOfRefraction = "IndexOfRefraction";
        if (auto prop = FbxMat->FindProperty(sIndexOfRefraction); prop.IsValid())
        {
            OutInfo.DensityScalar = (float)prop.Get<FbxDouble>();
        }

        // Illumination Model은 FBX에 따로 없으므로 기본 0
        OutInfo.IlluminanceModel = 0;
    }

    // 텍스처 채널 (Diffuse, Ambient, Specular, Bump/Normal, Alpha)
    auto ReadFirstTexture = [&](const char* PropName, FString& OutName, FString& OutPath)
    {
        auto prop = FbxMat->FindProperty(PropName);
        if (!prop.IsValid()) return;
        int nbTex = prop.GetSrcObjectCount<FbxFileTexture>();
        if (nbTex <= 0) return;
        if (auto* Tex = prop.GetSrcObject<FbxFileTexture>(0))
        {
            FString fname = FString(Tex->GetFileName());
            OutName = fname;
            OutPath = (BasePath + fname).ToWideString();
            OutInfo.bHasTexture = true;
        }
    };

    // map_Kd
    ReadFirstTexture(FbxSurfaceMaterial::sDiffuse,OutInfo.DiffuseTextureName,OutInfo.DiffuseTexturePath);
    // map_Ka
    ReadFirstTexture(FbxSurfaceMaterial::sAmbient,OutInfo.AmbientTextureName, OutInfo.AmbientTexturePath);
    // map_Ks
    ReadFirstTexture(FbxSurfaceMaterial::sSpecular,OutInfo.SpecularTextureName,OutInfo.SpecularTexturePath);
    // map_Bump 또는 map_Ns
    ReadFirstTexture(FbxSurfaceMaterial::sBump,OutInfo.BumpTextureName,OutInfo.BumpTexturePath);
    ReadFirstTexture(FbxSurfaceMaterial::sNormalMap,OutInfo.NormalTextureName,OutInfo.NormalTexturePath);
    // map_d (Alpha)
    ReadFirstTexture(FbxSurfaceMaterial::sTransparentColor,OutInfo.AlphaTextureName,OutInfo.AlphaTexturePath);

    return OutInfo;
}

// USkeletalMesh* FFBXLoader::CreateSkeletalMesh(const FString& FilePath)
// {
//     // USkeletalMesh가 있으면 return
//     USkeletalMesh* SkeletalMesh = GetSkeletalMesh(FilePath);
//     if (SkeletalMesh != nullptr)
//     {
//         return SkeletalMesh;
//     }
//
//     // Contents/FBX/ 폴더 내에 대응되는 bin 파일 있는지 확인하고.
//     // 있으면 bin 파싱, 없으면 fbx 직접 파싱.
//     std::filesystem::path path(FilePath);
//     std::filesystem::path binPath("Contents/FBX/" + path.filename().string() + ".bin");
//     FSkeletalMeshRenderData MeshData;
//     if (std::filesystem::exists(binPath))
//     {
//         MeshData = ParseBin(binPath.string());
//     }
//     else
//     {
//         MeshData = ParseFBX(FilePath);
//     }
//
//     SkeletalMesh = FObjectFactory::ConstructObject<USkeletalMesh>(nullptr);
//     SkeletalMesh->SetData(FilePath);
//
//     SkeletalMeshMap.Add(FilePath, SkeletalMesh);
//     return SkeletalMesh;
// }
//
// USkeletalMesh* FFBXLoader::GetSkeletalMesh(const FString& FilePath)
// {
//     if (SkeletalMeshMap.Contains(FilePath))
//         return SkeletalMeshMap[FilePath];
//
//     return nullptr;
// }

FSkeletalMeshRenderData FFBXLoader::GetCopiedSkeletalRenderData(const FString& FilePath)
{
    if (SkeletalMeshData.Contains(FilePath))
    {
        return SkeletalMeshData[FilePath];
    }
    // 없으면 기본 생성자
    return {};
}

FRefSkeletal FFBXLoader::GetRefSkeletal(FString FilePath)
{
    // TODO: 폴더에서 가져올 수 있으면 가져오기
    if (RefSkeletalData.Contains(FilePath))
    {
        return RefSkeletalData[FilePath];
    }
    
    return FRefSkeletal();
}


bool FFBXLoader::IsTriangulated(FbxMesh* Mesh)
{
    for (int i = 0; i < Mesh->GetPolygonCount(); ++i)
    {
        if (Mesh->GetPolygonSize(i) != 3)
            return false;
    }
    return true;
}