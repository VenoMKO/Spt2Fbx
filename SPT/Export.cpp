#include "Common.h"
#include "Export.h"

#include <algorithm>
#include <iostream>
#include <vector>

struct TreeStorage
{
  TreeStorage()
  {
    SdkManager = NULL;
    Scene = NULL;
    Mesh = NULL;
    Lod = 0;
  }
  FbxManager  *SdkManager;
	FbxScene    *Scene;

  FbxNode     *MeshNode;
  FbxMesh     *Mesh;

  int Lod;
};

bool GenerateTree(CSpeedTreeRT *tree, TreeStorage &o)
{
  // SIndexed geometry contains vertices for all LODs.
  // We need to keep vertices of the current LOD only.
  int totalVerticesCount = 0;
  std::vector<int> uniqueBranchIndices;
  std::vector<int> uniqueFrondIndices;
  {
    CSpeedTreeRT::SGeometry geometry;
    tree->GetGeometry(geometry);
    

    if (geometry.m_sBranches.m_nNumLods > o.Lod && geometry.m_sBranches.m_pNumStrips)
    {
      CSpeedTreeRT::SGeometry::SIndexed const *s = &geometry.m_sBranches;
      for(int strip = 0; strip < s->m_pNumStrips[o.Lod]; ++strip)
      {
        const int length = s->m_pStripLengths[o.Lod][strip];
        const int *indices = s->m_pStrips[o.Lod][strip];

        for(int i = 0; i < length - 2; ++i)
        {
          for (int j = 0; j < 3; ++j)
          {
            int idx = indices[i + j];
            if (std::find(uniqueBranchIndices.begin(), uniqueBranchIndices.end(), idx) == uniqueBranchIndices.end())
            {
              uniqueBranchIndices.push_back(idx);
            }
          }
        }
      }
      totalVerticesCount += (int)uniqueBranchIndices.size();
    }

    if (geometry.m_sFronds.m_nNumLods > o.Lod && geometry.m_sFronds.m_pNumStrips)
    {
      CSpeedTreeRT::SGeometry::SIndexed const *s = &geometry.m_sFronds;
      for(int strip = 0; strip < s->m_pNumStrips[o.Lod]; ++strip)
      {
        const int length = s->m_pStripLengths[o.Lod][strip];
        const int *indices = s->m_pStrips[o.Lod][strip];

        for(int i = 0; i < length - 2; ++i)
        {
          for (int j = 0; j < 3; ++j)
          {
            int idx = indices[i + j];
            if (std::find(uniqueFrondIndices.begin(), uniqueFrondIndices.end(), idx) == uniqueFrondIndices.end())
            {
              uniqueFrondIndices.push_back(idx);
            }
          }
        }
      }
      totalVerticesCount += (int)uniqueFrondIndices.size();
    }

    if (tree->GetNumLeafLodLevels() > o.Lod)
    {
      CSpeedTreeRT::SGeometry::SLeaf *s = &geometry.m_pLeaves[o.Lod];
      int leafCount = s->m_nNumLeaves;
      for (int leaf = 0; leaf < leafCount; ++leaf)
      {
        const CSpeedTreeRT::SGeometry::SLeaf::SCard *card = &s->m_pCards[s->m_pLeafCardIndices[leaf]];
        if (card->m_pMesh)
        {
          totalVerticesCount += card->m_pMesh->m_nNumVertices;
        }
        else
        {
          totalVerticesCount += 4;
        }
      }
    }
  }

  // Begin to build the mesh

  CSpeedTreeRT::SGeometry geometry;
  tree->GetGeometry(geometry);

  o.Mesh->InitControlPoints(totalVerticesCount);
  FbxVector4 *controlPoints = o.Mesh->GetControlPoints();
  int offset = 0;
  int group = 0;

  FbxLayer *layer = o.Mesh->GetLayer(0);
  if (!layer)
  {
    o.Mesh->CreateLayer();
    layer = o.Mesh->GetLayer(0);
    if (!layer)
    {
      std::cout << " Failed to create FbxLayer!" << std::endl;
      return false;
    }
  }
  FbxLayerElementMaterial* matLayer = FbxLayerElementMaterial::Create(o.Mesh, "");
  matLayer->SetMappingMode(FbxLayerElement::eByPolygon);
  matLayer->SetReferenceMode(FbxLayerElement::eIndexToDirect);
  layer->SetMaterials(matLayer);

  int branchMatIdx = -1;
  int frondMatIdx = -1;
  int leafMatIdx = -1;
  const char *uData = tree->GetUserData();
  if (uData && uData[0] == '$') // Real Editor's material mapping for UE4 import
  {
    int pos = 1;
    while (pos)
    {
      const char mType = uData[pos];
      if (!mType || (mType != 'b' && mType != 'f' && mType != 'l'))
      {
        break;
      }
      pos++;
      int length = uData[pos];
      pos++;
      char *nameRaw = (char *)malloc(length + 1);
      memcpy(nameRaw, &uData[pos], length);
      nameRaw[length] = '\0';
      std::string name(nameRaw);
      pos += length;
      FbxSurfaceMaterial *m = FbxSurfaceLambert::Create(o.Scene, name.c_str());
      switch (mType)
      {
        case 'b':
          branchMatIdx = o.MeshNode->GetMaterialCount();
          break;
        case 'f':
          frondMatIdx = o.MeshNode->GetMaterialCount();
          break;
        case 'l':
          leafMatIdx = o.MeshNode->GetMaterialCount();
          break;
        default:
          break;
      }
      o.MeshNode->AddMaterial(m);
      free(nameRaw);
    }
  }

  FbxLayerElementUV *uvDiffuseLayer = FbxLayerElementUV::Create(o.Mesh, "DiffuseUV");
  FbxLayerElementNormal *layerElementNormal = FbxLayerElementNormal::Create(o.Mesh, "");
  FbxLayerElementBinormal *layerElementBinormal = FbxLayerElementBinormal::Create(o.Mesh, "");
  FbxLayerElementTangent *layerElementTangent = FbxLayerElementTangent::Create(o.Mesh, "");
  uvDiffuseLayer->SetMappingMode(FbxLayerElement::eByControlPoint);
  uvDiffuseLayer->SetReferenceMode(FbxLayerElement::eDirect);
  layerElementNormal->SetMappingMode(FbxLayerElement::eByControlPoint);
  layerElementNormal->SetReferenceMode(FbxLayerElement::eDirect);
  layerElementBinormal->SetMappingMode(FbxLayerElement::eByControlPoint);
  layerElementBinormal->SetReferenceMode(FbxLayerElement::eDirect);
  layerElementTangent->SetMappingMode(FbxLayerElement::eByControlPoint);
  layerElementTangent->SetReferenceMode(FbxLayerElement::eDirect);


  // Building branches


  if (geometry.m_sBranches.m_nNumLods > o.Lod && geometry.m_sBranches.m_pNumStrips)
  {
    CSpeedTreeRT::SGeometry::SIndexed const *s = &geometry.m_sBranches;
    for (int i = 0; i < uniqueBranchIndices.size(); ++i)
    {
      int idx = uniqueBranchIndices[i];
      const float *pos = &s->m_pCoords[idx * 3];
      const float *normal = &s->m_pNormals[idx * 3];
      const float *binorm = &s->m_pBinormals[idx * 3];
      const float *tangent = &s->m_pTangents[idx * 3];
      const float *uvs = &s->m_pTexCoords[CSpeedTreeRT::TL_DIFFUSE][idx * 2];
      controlPoints[offset + i] = FbxVector4(pos[0], pos[1], pos[2]);
      layerElementNormal->GetDirectArray().Add(FbxVector4(normal[0], normal[1], normal[2]));
      layerElementBinormal->GetDirectArray().Add(FbxVector4(binorm[0], binorm[1], binorm[2]));
      layerElementTangent->GetDirectArray().Add(FbxVector4(tangent[0], tangent[1], tangent[2]));
      uvDiffuseLayer->GetDirectArray().Add(FbxVector2(uvs[0], uvs[1]));
    }

    if (branchMatIdx == -1)
    {
      FbxSurfaceMaterial *m = FbxSurfaceLambert::Create(o.Scene, "BranchMAT");
      branchMatIdx = o.MeshNode->GetMaterialCount();
      o.MeshNode->AddMaterial(m);
    }

    for(int strip = 0; strip < s->m_pNumStrips[o.Lod]; ++strip)
    {
      const int length = s->m_pStripLengths[o.Lod][strip];
      const int *indices = s->m_pStrips[o.Lod][strip];

      for(int i = 0; i < length - 2; ++i)
      {
        int polygon[3] = {indices[i], indices[i+1], indices[i+2]};
        if(i % 2)
        {
          std::swap(polygon[2], polygon[1]);
        }

        o.Mesh->BeginPolygon(branchMatIdx, -1, group);
        for (int j = 0; j < 3; ++j)
        {
          o.Mesh->AddPolygon(offset + std::distance(uniqueBranchIndices.begin(), std::find(uniqueBranchIndices.begin(), uniqueBranchIndices.end(), polygon[j])));
        }
        o.Mesh->EndPolygon();
      }
    }
    offset += uniqueBranchIndices.size();
    group++;
  }

  // Building fronds

  if (geometry.m_sFronds.m_nNumLods > o.Lod && geometry.m_sFronds.m_pNumStrips)
  {
    CSpeedTreeRT::SGeometry::SIndexed const *s = &geometry.m_sFronds;
    for (int i = 0; i < uniqueFrondIndices.size(); ++i)
    {
      int idx = uniqueFrondIndices[i];
      const float *pos = &s->m_pCoords[idx * 3];
      const float *normal = &s->m_pNormals[idx * 3];
      const float *binorm = &s->m_pBinormals[idx * 3];
      const float *tangent = &s->m_pTangents[idx * 3];
      const float *uvs = &s->m_pTexCoords[CSpeedTreeRT::TL_DIFFUSE][idx * 2];
      controlPoints[offset + i] = FbxVector4(pos[0], pos[1], pos[2]);
      layerElementNormal->GetDirectArray().Add(FbxVector4(normal[0], normal[1], normal[2]));
      layerElementBinormal->GetDirectArray().Add(FbxVector4(binorm[0], binorm[1], binorm[2]));
      layerElementTangent->GetDirectArray().Add(FbxVector4(tangent[0], tangent[1], tangent[2]));
      uvDiffuseLayer->GetDirectArray().Add(FbxVector2(uvs[0], uvs[1]));
    }

    if (frondMatIdx == -1)
    {
      FbxSurfaceMaterial *m = FbxSurfaceLambert::Create(o.Scene, "FrondMAT");
      frondMatIdx = o.MeshNode->GetMaterialCount();
      o.MeshNode->AddMaterial(m);
    }

    for(int strip = 0; strip < s->m_pNumStrips[o.Lod]; ++strip)
    {
      const int length = s->m_pStripLengths[o.Lod][strip];
      const int *indices = s->m_pStrips[o.Lod][strip];

      for(int i = 0; i < length - 2; ++i)
      {
        int polygon[3] = {indices[i], indices[i+1], indices[i+2]};
        if(i % 2)
        {
          std::swap(polygon[2], polygon[1]);
        }

        o.Mesh->BeginPolygon(frondMatIdx, -1, group);
        for (int j = 0; j < 3; ++j)
        {
          o.Mesh->AddPolygon(offset + std::distance(uniqueFrondIndices.begin(), std::find(uniqueFrondIndices.begin(), uniqueFrondIndices.end(), polygon[j])));
        }
        o.Mesh->EndPolygon();
      }
    }
    offset += uniqueFrondIndices.size();
    group++;
  }

  // Building leaf cards

  bool usesCardLeafs = false;
  if (tree->GetNumLeafLodLevels() > o.Lod)
  {
    bool secondaryIndexForMesh = false;
    CSpeedTreeRT::SGeometry::SLeaf *s = &geometry.m_pLeaves[o.Lod];
    int leafCount = s->m_nNumLeaves;

    if (leafCount && leafMatIdx == -1)
    {
      FbxSurfaceMaterial *m = FbxSurfaceLambert::Create(o.Scene, "LeafMAT");
      leafMatIdx = o.MeshNode->GetMaterialCount();
      o.MeshNode->AddMaterial(m);
    }
    int processedLeafs = 0;
    for (int leaf = 0; leaf < leafCount; ++leaf)
    {
      const CSpeedTreeRT::SGeometry::SLeaf::SCard *card = &s->m_pCards[s->m_pLeafCardIndices[leaf]];
      const float *center = &s->m_pCenterCoords[leaf * 3];
      if (!card->m_pMesh)
      {
        usesCardLeafs = true;
        int cornerIndicies[] = {0,0,0,0};
        for (int corner = 0; corner < 4; ++corner)
        {
          const float *pos = &card->m_pCoords[corner * 4];
          const float *normal = &s->m_pNormals[12 * leaf + (corner * 3)];
          const float *binorm = &s->m_pBinormals[12 * leaf + (corner * 3)];
          const float *tangent = &s->m_pTangents[12 * leaf + (corner * 3)];
          const float *uvs = &card->m_pTexCoords[corner * 2];
          controlPoints[offset + (processedLeafs * 4 + corner)] = FbxVector4(pos[0] + center[0], pos[1] + center[1], pos[2] + center[2]);
          layerElementNormal->GetDirectArray().Add(FbxVector4(normal[0], normal[1], normal[2]));
          layerElementBinormal->GetDirectArray().Add(FbxVector4(binorm[0], binorm[1], binorm[2]));
          layerElementTangent->GetDirectArray().Add(FbxVector4(tangent[0], tangent[1], tangent[2]));
          uvDiffuseLayer->GetDirectArray().Add(FbxVector2(uvs[0], uvs[1]));
          cornerIndicies[corner] = offset + (processedLeafs * 4 + corner);
        }

        o.Mesh->BeginPolygon(leafMatIdx, -1, group);
        o.Mesh->AddPolygon(cornerIndicies[0]);
        o.Mesh->AddPolygon(cornerIndicies[1]);
        o.Mesh->AddPolygon(cornerIndicies[2]);
        o.Mesh->EndPolygon();
        o.Mesh->BeginPolygon(leafMatIdx, -1, group);
        o.Mesh->AddPolygon(cornerIndicies[0]);
        o.Mesh->AddPolygon(cornerIndicies[2]);
        o.Mesh->AddPolygon(cornerIndicies[3]);
        o.Mesh->EndPolygon();
        processedLeafs++;
      }
    }
    group++;
    offset += processedLeafs * 4;
  }

  // Building leaf meshes
  if (tree->GetNumLeafLodLevels() > o.Lod)
  {
    int leafMatIdx2 = -1;
    CSpeedTreeRT::SGeometry::SLeaf *s = &geometry.m_pLeaves[o.Lod];
    int leafCount = s->m_nNumLeaves;

    if (leafCount && leafMatIdx == -1)
    {
      FbxSurfaceMaterial *m = FbxSurfaceLambert::Create(o.Scene, "LeafMAT");
      leafMatIdx = o.MeshNode->GetMaterialCount();
      o.MeshNode->AddMaterial(m);
    }

    for (int leaf = 0; leaf < leafCount; ++leaf)
    {
      const CSpeedTreeRT::SGeometry::SLeaf::SCard *card = &s->m_pCards[s->m_pLeafCardIndices[leaf]];
      const float *center = &s->m_pCenterCoords[leaf * 3];
      if (card->m_pMesh)
      {
        if (usesCardLeafs && leafMatIdx2 == -1)
        {
          const char *leafMatName = o.MeshNode->GetMaterial(leafMatIdx)->GetName();
          std::string newMatName("LeafMeshMAT");
          if (leafMatName && leafMatName != "LeafMAT")
          {
            newMatName = std::string(leafMatName) + "_lMesh";
          }
          FbxSurfaceMaterial *m = FbxSurfaceLambert::Create(o.Scene, newMatName.c_str());
          leafMatIdx2 = o.MeshNode->GetMaterialCount();
          o.MeshNode->AddMaterial(m);
        }
        else if (leafMatIdx2 == -1)
        {
          leafMatIdx2 = leafMatIdx;
        }
        const CSpeedTreeRT::SGeometry::SLeaf::SMesh *mesh = card->m_pMesh;
        for (int vert = 0; vert < mesh->m_nNumVertices; ++vert)
        {
          const float *pos = &mesh->m_pCoords[vert * 3];
          const float *normal = &mesh->m_pNormals[vert * 3];
          const float *binorm = &mesh->m_pBinormals[vert * 3];
          const float *tangent = &mesh->m_pTangents[vert * 3];
          const float *uvs = &mesh->m_pTexCoords[vert * 2];
          controlPoints[offset + vert] = FbxVector4(pos[0] + center[0], pos[1] + center[1], pos[2] + center[2]);
          layerElementNormal->GetDirectArray().Add(FbxVector4(normal[0], normal[1], normal[2]));
          layerElementBinormal->GetDirectArray().Add(FbxVector4(binorm[0], binorm[1], binorm[2]));
          layerElementTangent->GetDirectArray().Add(FbxVector4(tangent[0], tangent[1], tangent[2]));
          uvDiffuseLayer->GetDirectArray().Add(FbxVector2(uvs[0], uvs[1]));
        }
        for (int i = 0; i < mesh->m_nNumIndices - 2; i+=3)
        {
          o.Mesh->BeginPolygon(leafMatIdx2, -1, group);
          for (int j = 0; j < 3; ++j)
          {
            o.Mesh->AddPolygon(offset + mesh->m_pIndices[i + j]);
          }
          o.Mesh->EndPolygon();
        }
        offset += mesh->m_nNumVertices;
      }
    }
    group++;
  }

  layer->SetNormals(layerElementNormal);
  layer->SetBinormals(layerElementBinormal);
  layer->SetTangents(layerElementTangent);
  layer->SetUVs(uvDiffuseLayer, FbxLayerElement::eTextureDiffuse);
  return true;
}

void ExportTree(CSpeedTreeRT *tree, std::wstring const&path)
{
  std::wstring name;
	if (path.find_last_of('\\') == std::wstring::npos)
	{
		name = path.substr(0, path.find_last_of('.'));
	}
	else
	{
		size_t pos = path.find_last_of('\\') + 1;
		name = path.substr(pos, path.find_last_of('.') - pos);
	}

  std::wstring destination(path.substr(0, path.find_last_of('.')) + L".fbx");
  std::wcout << "Exporting " << name << "... ";

  TreeStorage o;
  InitializeSdkObjects(o.SdkManager, o.Scene);
  FbxDocumentInfo* sceneInfo = FbxDocumentInfo::Create(o.SdkManager, "SceneInfo");
  sceneInfo->mTitle = w2a(name).c_str();
  o.Scene->SetSceneInfo(sceneInfo);
  o.Mesh = FbxMesh::Create(o.Scene, "geometry");
  o.MeshNode = FbxNode::Create(o.Scene, w2a(name).c_str());
  o.MeshNode->SetNodeAttribute(o.Mesh);
  o.Scene->GetRootNode()->AddChild(o.MeshNode);

  if (!GenerateTree(tree, o))
  {
    std::cout << "Failed to export: " << w2a(name) << std::endl;
    DestroySdkObjects(o.SdkManager);
    return;
  }

  if (!SaveScene(o.SdkManager, o.Scene, w2a(destination).c_str(), 0))
  {
    std::cout << "Failed to save: " << w2a(destination) << std::endl;
  }
  else
  {
    std::wcout << "Done." << std::endl;
  }
  DestroySdkObjects(o.SdkManager);
}