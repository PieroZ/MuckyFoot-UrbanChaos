#pragma once

#include <vector>
#include <string>

#include <array>
#include <map>


// Vertex position (v)
using Vertex = std::array<float, 3>;

// UV coordinate (vt)
using UV = std::array<float, 2>;

// Material
struct Material
{
    std::string mName;        // Material name (from "newmtl")
    std::string mTextureFile; // Texture file path (from "map_Kd")
	bool mIsTextureLoaded; // Flag to check if texture is loaded
	int mPage;         // Texture page number for backward compatibility
};

// Face with 3 vertices
struct TriangleFace
{
    Vertex mPoints[3]; // Vertex positions
	int mPointsIndex[3]; // Index of points in the vertex buffer
    UV mUV[3];         // UV coordinates
    int mMaterialIndex; // Material index
};

// Face with 4 vertices
struct QuadFace
{
    Vertex mPoints[4]; // Vertex positions
	int mPointsIndex[4]; // Index of points in the vertex buffer
    UV mUV[4];         // UV coordinates
    int mMaterialIndex; // Material index
};

struct Mesh
{
    std::vector<TriangleFace> triangleFaces;
    std::vector<QuadFace> quadFaces;
};

class ResourceManager
{
private:
	ResourceManager() = default; // Private constructor for singleton pattern
	~ResourceManager() = default; // Private destructor
	ResourceManager(const ResourceManager&) = delete; // Prevent copying
	ResourceManager& operator=(const ResourceManager&) = delete; // Prevent assignment
public:
    static ResourceManager& Get();

    std::vector<Material> materials;
    std::map<int, Mesh> primMeshes; // <--- mapowanie prim -> mesh

    Mesh& GetMeshForPrim(int prim) {
        return primMeshes[prim];
    }

    /*std::vector<ObjModel> models;

    void LoadMtl(const std::string& path);
    void LoadObj(const std::string& path);*/
};