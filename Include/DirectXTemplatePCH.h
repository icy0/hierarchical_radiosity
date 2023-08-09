#pragma once

// System includes
#include <windows.h>

// DirectX includes
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXColors.h>

// STL includes
#include <iostream>
#include <string>
#include <algorithm>
#include <fstream>
#include <list>
#include <chrono>
#include <cmath>

// Link library dependencies
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "winmm.lib")

using namespace DirectX;

// Safely release a COM object.
template<typename T>
inline void SafeRelease(T& ptr)
{
    if (ptr != NULL)
    {
        ptr->Release();
        ptr = NULL;
    }
}
struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT3 color;
};

struct Face
{
    int vertex_indices[4];
    int normal_index;
};

struct OBJ_Model
{
    Vertex* vertices;
    int vertex_count;
    Face* faces;
    int face_count;
};

struct Patch
{
    XMFLOAT3 vertex_pos[4];
    XMFLOAT3 centroid;
    XMFLOAT3 normal;
    XMFLOAT3 radiosity;
    XMFLOAT3 irradiance;
    XMFLOAT3 reflectance;
    float area;

    // rendering relevant members
    ID3D11InputLayout* input_layout;
    ID3D11Buffer* vertex_buffer;
    ID3D11Buffer* index_buffer;

    // hierarchical radiosity relevant members
    int influencing_partner_count;
    std::list<Patch*> influencing_partners;
    std::list<double> influencing_partner_formfactors;

    bool has_parent;
    bool has_children;
    Patch* parent;
    Patch** children;

    XMVECTOR gathered_brightness;
    XMVECTOR brightness;
};