#include "ObjLoader.h"
#include "memory.h"
//#include <algorithm>
//#include <cstdio>  // For fopen, fgets, fclose, etc.
//#include <sstream>
//#include <iostream>
//#include <unordered_map>

#include <fstream>
#include <sstream>
#include <string>
#include <iostream>



// Function to parse .mtl file
void parseMtlFile(const std::string& filePath, std::vector<Material>& materials)
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        std::cerr << "Failed to open file: " << filePath << std::endl;
        return;
    }

    Material currentMaterial;
    std::string line;
    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        std::string type;
        iss >> type;

        if (type == "newmtl")
        { // New material definition
            if (!currentMaterial.mName.empty())
            {
                if (currentMaterial.mTextureFile.empty())
                {
                    ASSERT(0);
                }
                else
                {
                    materials.push_back(currentMaterial); // Save the previous material
                }
            }
            currentMaterial = Material(); // Reset for the new material
            iss >> currentMaterial.mName; // Store material name
        }
        else if (type == "map_Kd")
        { // Texture file
// Read the rest of the line after "map_Kd"
            std::string textureFilePath;
            std::getline(iss, textureFilePath);

            // Remove leading/trailing whitespace (e.g., the space after "map_Kd")
            textureFilePath.erase(0, textureFilePath.find_first_not_of(' '));
            textureFilePath.erase(textureFilePath.find_last_not_of(' ') + 1);

            // Assign the full path to the material
            currentMaterial.mTextureFile = textureFilePath;

			// Extract texture name from the file path
			std::string textureName = textureFilePath.substr(textureFilePath.find_last_of("/\\") + 1);

			// Extract characters from 3 to 5 of the texture name
			std::string textureNamePart = textureName.substr(3, 3);

            // Convert to number
			int texturePage = std::stoi(textureNamePart);

			// Assign the texture page number to the material
			currentMaterial.mPage = texturePage + 64 * 22;

            TRACE("Material page: %d\n", currentMaterial.mPage);
        }
        // Handle other material properties (e.g., "Ka", "Kd", "Ks")
    }

    // Push the last material
    if (!currentMaterial.mName.empty())
    {

		if (currentMaterial.mTextureFile.empty())
		{
			ASSERT(0);
		}
        else
        {
            materials.push_back(currentMaterial);
        }
    }
}

void parseObjFile(
    const std::string& filePath,
    const std::vector<Material>& materials,
    int primNumber,
    PrimObject* prim_objects)
{
    auto& res = ResourceManager::Get();
    Mesh& mesh = res.GetMeshForPrim(primNumber);

    std::ifstream file(filePath);
    if (!file.is_open())
    {
        std::cerr << "Failed to open file: " << filePath << std::endl;
        return;
    }

    std::vector<Vertex> vertices;
    std::vector<UV> uvs;

    int currentMaterialIndex = -1;
    std::string line;
    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        std::string type;
        iss >> type;

        if (type == "v")
        {
            Vertex vertex;
            iss >> vertex[0] >> vertex[1] >> vertex[2];
            vertices.push_back(vertex);
        }
        else if (type == "vt")
        {
            UV uv;
            iss >> uv[0] >> uv[1];
            uv[1] = 1.0f - uv[1];
            uvs.push_back(uv);
        }
        else if (type == "usemtl")
        {
            std::string materialName;
            iss >> materialName;
            bool materialFound = false;
            for (size_t i = 0; i < materials.size(); ++i)
            {
                if (materials[i].mName == materialName)
                {
                    currentMaterialIndex = static_cast<int>(i);
                    materialFound = true;
                    break;
                }
            }
            if (!materialFound)
            {
                std::cerr << "Error: Material '" << materialName << "' not found in .mtl file." << std::endl;
            }
        }
        else if (type == "f")
        {
            std::vector<unsigned short> vertexIndices;
            std::vector<unsigned short> uvIndices;
            std::string token;
            while (iss >> token)
            {
                std::istringstream tokenStream(token);
                unsigned short vertexIndex, uvIndex;
                char slash;
                tokenStream >> vertexIndex >> slash >> uvIndex;
                vertexIndices.push_back(vertexIndex - 1);
                uvIndices.push_back(uvIndex - 1);
            }

            if (vertexIndices.size() == 3)
            {
                TriangleFace face;
                for (int i = 0; i < 3; ++i)
                {
                    face.mPoints[i] = vertices[vertexIndices[i]];
                    face.mPointsIndex[i] = vertexIndices[i];
                    face.mUV[i] = uvs[uvIndices[i]];
                }
                face.mMaterialIndex = currentMaterialIndex;
                mesh.triangleFaces.push_back(face);
            }
            else if (vertexIndices.size() == 4)
            {
                QuadFace face;
                for (int i = 0; i < 4; ++i)
                {
                    face.mPoints[i] = vertices[vertexIndices[i]];
                    face.mPointsIndex[i] = vertexIndices[i];
                    face.mUV[i] = uvs[uvIndices[i]];
                }
                face.mMaterialIndex = currentMaterialIndex;
                mesh.quadFaces.push_back(face);
            }
        }
    }



    signed long num_points = vertices.size();
    signed long num_faces3 = mesh.triangleFaces.size();
    signed long num_faces4 = mesh.quadFaces.size();


    // Fill the prim_points struct
    for (int i = 0; i < vertices.size(); i++)
    {
        prim_points[next_prim_point + i].X = vertices[i].at(0);
        prim_points[next_prim_point + i].Y = vertices[i].at(1);
        prim_points[next_prim_point + i].Z = vertices[i].at(2);
    }

    PrimFace3* f3;
    PrimFace4* f4;

    auto po = &prim_objects[primNumber];

	po->StartPoint = next_prim_point;
	po->StartFace3 = next_prim_face3;
	po->StartFace4 = next_prim_face4;
	po->EndPoint = num_points + next_prim_point;
	po->EndFace3 = num_faces3 + next_prim_face3;
	po->EndFace4 = num_faces4 + next_prim_face4;
    /*po->pziFormat = true;


    TRACE(" po->pziFormat = %d\n", po->pziFormat);*/


    for (int i = 0; i < num_faces3; i++)
    {
        f3 = &prim_faces3[next_prim_face3 + i];

        for (int j = 0; j < 3; j++)
        {
            f3->Points[j] += next_prim_point - po->StartPoint;
        }
    }

    next_prim_point += num_points;
    next_prim_face3 += num_faces3;
    next_prim_face4 += num_faces4;
}


//std::unordered_map<std::string, std::string> LoadMTL(const std::string& mtl_filename)
//{
//    std::unordered_map<std::string, std::string> material_map;
//    FILE* file = fopen(mtl_filename.c_str(), "r");
//    if (!file)
//    {
//        std::cerr << "Failed to open MTL file: " << mtl_filename << "\n";
//        return material_map;
//    }
//
//    char line[512];  // Increased buffer size to handle long paths
//    std::string material_name, texture_file;
//
//    while (fgets(line, sizeof(line), file))
//    {
//        std::istringstream iss(line);
//        std::string keyword;
//        iss >> keyword;
//
//        if (keyword == "newmtl")
//        {
//            iss >> material_name;
//        }
//        else if (keyword == "map_Kd")
//        {
//            std::getline(iss >> std::ws, texture_file);  // Read full path, trimming leading spaces
//            material_map[material_name] = texture_file;
//        }
//    }
//
//    fclose(file);
//    return material_map;
//}

//bool LoadOBJPrim(const std::string& obj_filename, signed long prim)
//{
//
//    FILE* file = fopen(obj_filename, "r");
//    if (!file)
//    {
//        std::cerr << "Failed to open OBJ file: " << obj_filename << "\n";
//        return false;
//    }
//
//    std::vector<Vector3> vertices;
//    std::vector<Vector2> uvs;
//    std::vector<PZIObjFace3> faces3;
//    std::vector<PZIObjFace4> faces4;
//    std::unordered_map<std::string, std::string> materials;
//    std::string current_material;
//    char line[256];
//
//    while (fgets(line, sizeof(line), file))
//    {
//        std::istringstream iss(line);
//        std::string keyword;
//        iss >> keyword;
//
//        if (keyword == "mtllib")
//        {
//            std::string mtl_filename;
//            iss >> mtl_filename;
//            materials = LoadMTL(mtl_filename);
//        }
//        else if (keyword == "usemtl")
//        {
//            iss >> current_material;
//        }
//        else if (keyword == "v")
//        {
//            Vector3 v;
//            iss >> v.X >> v.Y >> v.Z;
//            vertices.push_back(v);
//        }
//        else if (keyword == "vt")
//        {
//            Vector2 uv;
//            iss >> uv.U >> uv.V;
//            uv.V = 1.0f - uv.V; // Flip V coordinate for OBJ format
//            uvs.push_back(uv);
//        }
//        else if (keyword == "f")
//        {
//            std::vector<unsigned short> v_indices, uv_indices;
//            std::string face_data;
//            while (iss >> face_data)
//            {
//                std::replace(face_data.begin(), face_data.end(), '/', ' ');
//                std::istringstream face_ss(face_data);
//                unsigned short v_idx, uv_idx;
//                face_ss >> v_idx >> uv_idx;
//                v_indices.push_back(v_idx - 1);
//                uv_indices.push_back(uv_idx - 1);
//            }
//            if (v_indices.size() == 3)
//            {
//                PZIObjFace3 face;
//                for (int i = 0; i < 3; i++)
//                {
//                    face.Points[i] = v_indices[i];
//                    face.UV[i][0] = uvs[uv_indices[i]].U;
//                    face.UV[i][1] = uvs[uv_indices[i]].V;
//                }
//                face.TextureFile = materials[current_material].data();
//                faces3.push_back(face);
//            }
//            else if (v_indices.size() == 4)
//            {
//                PZIObjFace4 face;
//                for (int i = 0; i < 4; i++)
//                {
//                    face.Points[i] = v_indices[i];
//                    face.UV[i][0] = uvs[uv_indices[i]].U;
//                    face.UV[i][1] = uvs[uv_indices[i]].V;
//                }
//                face.TextureFile = materials[current_material].data();
//                faces4.push_back(face);
//            }
//        }
//    }
//
//    fclose(file);
//
//    //// Convert faces into Urban Chaos format
//    //PrimObject* po = &prim_objects[prim];
//    //po->StartPoint = next_prim_point;
//    //po->StartFace3 = next_prim_face3;
//    //po->StartFace4 = next_prim_face4;
//
//    //for (auto& v : vertices)
//    //{
//    //    prim_points[next_prim_point++] = { static_cast<SWORD>(v.X), static_cast<SWORD>(v.Y), static_cast<SWORD>(v.Z) };
//    //}
//
//    //for (auto& f : faces3)
//    //{
//    //    PrimFace3 pf;
//    //    memcpy(pf.Points, f.Points, sizeof(f.Points));
//    //    for (int i = 0; i < 3; i++)
//    //    {
//    //        pf.UV[i][0] = static_cast<UBYTE>(f.UV[i][0] * 255);
//    //        pf.UV[i][1] = static_cast<UBYTE>(f.UV[i][1] * 255);
//    //    }
//    //    pf.TexturePage = 0;  // TexturePage system replaced by TextureFile
//    //    prim_faces3[next_prim_face3++] = pf;
//    //}
//
//    //for (auto& f : faces4)
//    //{
//    //    PrimFace4 pf;
//    //    memcpy(pf.Points, f.Points, sizeof(f.Points));
//    //    for (int i = 0; i < 4; i++)
//    //    {
//    //        pf.UV[i][0] = static_cast<UBYTE>(f.UV[i][0] * 255);
//    //        pf.UV[i][1] = static_cast<UBYTE>(f.UV[i][1] * 255);
//    //    }
//    //    pf.TexturePage = 0;
//    //    prim_faces4[next_prim_face4++] = pf;
//    //}
//
//    //po->EndPoint = next_prim_point;
//    //po->EndFace3 = next_prim_face3;
//    //po->EndFace4 = next_prim_face4;
//
//    return true;
//}