#pragma once
#include <string>
#include <vector>
#include <array>
#include "ResourceManager.h"

#include "game.h"

// Vertex position (v)
using Vertex = std::array<float, 3>;

// UV coordinate (vt)
using UV = std::array<float, 2>;

//// Material
//struct Material
//{
//    std::string mName;        // Material name (from "newmtl")
//    std::string mTextureFile; // Texture file path (from "map_Kd")
//};
//
//// Face with 3 vertices
//struct TriangleFace
//{
//    Vertex mPoints[3]; // Vertex positions
//    UV mUV[3];         // UV coordinates
//    int mMaterialIndex; // Material index
//};
//
//// Face with 4 vertices
//struct QuadFace
//{
//    Vertex mPoints[4]; // Vertex positions
//    UV mUV[4];         // UV coordinates
//    int mMaterialIndex; // Material index
//};

//struct Vector3
//{
//    float X, Y, Z;
//
//    Vector3() : X(0), Y(0), Z(0) {}
//    Vector3(float x, float y, float z) : X(x), Y(y), Z(z) {}
//};
//
//struct Vector2
//{
//    float U, V;
//
//    Vector2() : U(0), V(0) {}
//    Vector2(float u, float v) : U(u), V(v) {}
//};

// Function to parse .obj file
//void parseObjFile(const std::string& filePath, std::vector<TriangleFace>& triangleFaces, std::vector<QuadFace>& quadFaces, const std::vector<Material>& materials, int primNumber, PrimObject* prim_objects);

void parseObjFile(
    const std::string& filePath,
    const std::vector<Material>& materials,
    int primNumber,
    PrimObject* prim_objects);

// Function to parse .mtl file
void parseMtlFile(const std::string& filePath, std::vector<Material>& materials);

//bool LoadOBJPrim(const std::string& obj_filename, signed long prim);
//std::unordered_map<std::string, std::string> LoadMTL(const std::string& mtl_filename);