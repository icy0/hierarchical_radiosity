#include <DirectXTemplatePCH.h>

using namespace DirectX;

const LONG g_WindowWidth = 1920;
const LONG g_WindowHeight = 1080;
LPCWSTR g_WindowClassName = L"DirectXWindowClass";
LPCWSTR g_WindowName = L"Hierarchical Radiosity";
HWND g_WindowHandle = 0;

const BOOL g_EnableVSync = TRUE;

// Direct3D device and swap chain.
ID3D11Device* g_d3dDevice = nullptr;
ID3D11DeviceContext* g_d3dDeviceContext = nullptr;
IDXGISwapChain* g_d3dSwapChain = nullptr;

// Render target view for the back buffer of the swap chain.
ID3D11RenderTargetView* g_d3dRenderTargetView = nullptr;
// Depth/stencil view for use as a depth buffer.
ID3D11DepthStencilView* g_d3dDepthStencilView = nullptr;
// A texture to associate to the depth stencil view.
ID3D11Texture2D* g_d3dDepthStencilBuffer = nullptr;

// Define the functionality of the depth/stencil stages.
ID3D11DepthStencilState* g_d3dDepthStencilState = nullptr;
// Define the functionality of the rasterizer stage.
ID3D11RasterizerState* g_d3dRasterizerState = nullptr;
D3D11_VIEWPORT g_Viewport = { 0 };

// Vertex buffer data
ID3D11InputLayout* g_d3dInputLayout = nullptr;
ID3D11Buffer* g_d3dVertexBuffer = nullptr;
ID3D11Buffer* g_d3dIndexBuffer = nullptr;

// Shader data
ID3D11VertexShader* g_d3dVertexShader = nullptr;
ID3D11PixelShader* g_d3dPixelShader = nullptr;

// Demo parameters
XMMATRIX g_WorldMatrix;
XMMATRIX g_ViewMatrix;
XMMATRIX g_ProjectionMatrix;

// the room as an .obj-model
OBJ_Model g_room_model;

Patch* g_patches;
int g_patch_count;

double* g_formfactors;

bool g_without_hierarch_radiosity = true;

// Shader resources
enum ConstantBuffer
{
    CB_Application,
    CB_Frame,
    CB_Object,
    NumConstantBuffers
};

ID3D11Buffer* g_d3dConstantBuffers[NumConstantBuffers];

// Forward declarations.
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

template<class ShaderClass>
ShaderClass* LoadShader(const std::wstring& fileName, const std::string& entryPoint, const std::string& profile);

Patch InitPatch(ID3D11Device* device, Vertex v1, Vertex v2, Vertex v3, Vertex v4, XMFLOAT3 normal, XMFLOAT3 reflectance);

bool LoadContent();
void UnloadContent();

void Update(float deltaTime);
void Render();
void Cleanup();

void LoadModel(std::string path)
{
    std::ifstream modelFileIFStream;
    modelFileIFStream.open(path, std::ios::in);
    std::string line;
    std::list<XMFLOAT3> positions;
    std::list<XMFLOAT3> normals;
    std::list<Face> faces;

    if (modelFileIFStream.is_open())
    {
        std::string case_substring;
        while (std::getline(modelFileIFStream, line))
        {
            case_substring = line.substr(0, 2);

            // read vertices
            if (!case_substring.compare("v "))
            {
                g_room_model.vertex_count++;

                XMFLOAT3 position;
                line = line.substr(line.find_first_of(' ') + 1); // cut away the "v "
                position.x = std::atof(line.substr(0, line.find_first_of(' ') + 1).c_str());

                line = line.substr(line.find_first_of(' ') + 1); // cut away the x-coord
                position.y = std::atof(line.substr(0, line.find_first_of(' ') + 1).c_str());

                line = line.substr(line.find_first_of(' ') + 1); // cut away the y-coord
                position.z = std::atof(line.c_str());

                positions.push_back(position);
            }
            // read vertex normals
            else if (!case_substring.compare("vn"))
            {
                XMFLOAT3 normal;
                line = line.substr(line.find_first_of(' ') + 1); // cut away the "v "
                normal.x = std::atof(line.substr(0, line.find_first_of(' ') + 1).c_str());

                line = line.substr(line.find_first_of(' ') + 1); // cut away the x-coord
                normal.y = std::atof(line.substr(0, line.find_first_of(' ') + 1).c_str());

                line = line.substr(line.find_first_of(' ') + 1); // cut away the y-coord
                normal.z = std::atof(line.c_str());

                normals.push_back(normal);
            }
            // read faces
            else if (!case_substring.compare("f "))
            {
                g_room_model.face_count++;
                
                Face face = {};

                line = line.substr(2);
                std::string number_string;
                number_string = line.substr(0, line.find_first_of(' '));
                face.vertex_indices[0] = std::atoi(number_string.substr(0, line.find_first_of('/')).c_str()) - 1;
                line = line.substr(line.find_first_of(' ') + 1);
                number_string = line.substr(0, line.find_first_of(' '));
                face.vertex_indices[1] = std::atoi(number_string.substr(0, line.find_first_of('/')).c_str()) - 1;
                line = line.substr(line.find_first_of(' ') + 1);
                number_string = line.substr(0, line.find_first_of(' '));
                face.vertex_indices[2] = std::atoi(number_string.substr(0, line.find_first_of('/')).c_str()) - 1;
                line = line.substr(line.find_first_of(' ') + 1);
                number_string = line.substr(0, line.find_first_of(' '));
                face.vertex_indices[3] = std::atoi(number_string.substr(0, line.find_first_of('/')).c_str()) - 1;
                line = line.substr(line.find_first_of('/') + 1);
                face.normal_index = std::atoi(line.substr(line.find_first_of('/')+1).c_str()) - 1;

                faces.push_back(face);
            }
        }
        modelFileIFStream.close();
    }

    int vi = 0;
    std::list<Vertex> vertices = {};
    for (auto p_it = positions.begin(); p_it != positions.end(); p_it++)
    {
        int ni = -1;
        for (auto f_it = faces.begin(); f_it != faces.end(); f_it++)
        {
            if (f_it->vertex_indices[0] == vi || f_it->vertex_indices[1] == vi || f_it->vertex_indices[2] == vi || f_it->vertex_indices[3] == vi)
            {
                ni = f_it->normal_index;
                break;
            }
        }
        assert(ni != -1);
        auto n_it = normals.begin();
        std::advance(n_it, ni);
        vertices.push_back({ *p_it, *n_it, XMFLOAT3(0.0f, 0.0f, 0.0f) });
        vi++;
    }

    g_room_model.vertices = new Vertex[g_room_model.vertex_count];
    g_room_model.faces = new Face[g_room_model.face_count];

    std::copy(vertices.begin(), vertices.end(), g_room_model.vertices);
    std::copy(faces.begin(), faces.end(), g_room_model.faces);
}

/**
 * Initialize the application window.
 */
int InitApplication(HINSTANCE hInstance, int cmdShow)
{
    WNDCLASSEX wndClass = { 0 };
    wndClass.cbSize = sizeof(WNDCLASSEX);
    wndClass.style = CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc = &WndProc;
    wndClass.hInstance = hInstance;
    wndClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wndClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wndClass.lpszMenuName = nullptr;
    wndClass.lpszClassName = g_WindowClassName;

    if (!RegisterClassEx(&wndClass))
    {
        return -1;
    }

    RECT windowRect = { 0, 0, g_WindowWidth, g_WindowHeight };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    g_WindowHandle = CreateWindow(g_WindowClassName, g_WindowName,
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr, nullptr, hInstance, nullptr);

    if (!g_WindowHandle)
    {
        return -1;
    }

    ShowWindow(g_WindowHandle, cmdShow);
    UpdateWindow(g_WindowHandle);

    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT paintStruct;
    HDC hDC;

    switch (message)
    {
    case WM_PAINT:
    {
        hDC = BeginPaint(hwnd, &paintStruct);
        EndPaint(hwnd, &paintStruct);
    }
    break;
    case WM_DESTROY:
    {
        PostQuitMessage(0);
    }
    break;
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}

/**
 * The main application loop.
 */
int Run()
{
    MSG msg = { 0 };

    static DWORD previousTime = timeGetTime();
    static const float targetFramerate = 30.0f;
    static const float maxTimeStep = 1.0f / targetFramerate;

    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            DWORD currentTime = timeGetTime();
            float deltaTime = (currentTime - previousTime) / 1000.0f;
            previousTime = currentTime;

            // Cap the delta time to the max time step (useful if your 
            // debugging and you don't want the deltaTime value to explode.)
            deltaTime = std::min<float>(deltaTime, maxTimeStep);

            Update( deltaTime );
            Render();
        }
    }

    return static_cast<int>(msg.wParam);
}

/**
 * Initialize the DirectX device and swap chain.
 */
int InitDirectX(HINSTANCE hInstance, BOOL vSync)
{
    // A window handle must have been created already.
    assert(g_WindowHandle != 0);

    RECT clientRect;
    GetClientRect(g_WindowHandle, &clientRect);

    // Compute the exact client dimensions. This will be used
    // to initialize the render targets for our swap chain.
    unsigned int clientWidth = clientRect.right - clientRect.left;
    unsigned int clientHeight = clientRect.bottom - clientRect.top;

    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    ZeroMemory(&swapChainDesc, sizeof(DXGI_SWAP_CHAIN_DESC));

    swapChainDesc.BufferCount = 1;
    swapChainDesc.BufferDesc.Width = clientWidth;
    swapChainDesc.BufferDesc.Height = clientHeight;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.RefreshRate = {75, 0};// QueryRefreshRate(clientWidth, clientHeight, vSync);
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = g_WindowHandle;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    swapChainDesc.Windowed = TRUE;

    UINT createDeviceFlags = 0;
#if _DEBUG
    createDeviceFlags = D3D11_CREATE_DEVICE_DEBUG;
#endif

    // These are the feature levels that we will accept.
    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1
    };

    // This will be the feature level that 
    // is used to create our device and swap chain.
    D3D_FEATURE_LEVEL featureLevel;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE,
        nullptr, createDeviceFlags, featureLevels, _countof(featureLevels),
        D3D11_SDK_VERSION, &swapChainDesc, &g_d3dSwapChain, &g_d3dDevice, &featureLevel,
        &g_d3dDeviceContext);

    if (hr == E_INVALIDARG)
    {
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE,
            nullptr, createDeviceFlags, &featureLevels[1], _countof(featureLevels) - 1,
            D3D11_SDK_VERSION, &swapChainDesc, &g_d3dSwapChain, &g_d3dDevice, &featureLevel,
            &g_d3dDeviceContext);
    }

    if (FAILED(hr))
    {
        return -1;
    }

    // Next initialize the back buffer of the swap chain and associate it to a 
    // render target view.
    ID3D11Texture2D* backBuffer;
    hr = g_d3dSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backBuffer);
    if (FAILED(hr))
    {
        return -1;
    }

    hr = g_d3dDevice->CreateRenderTargetView(backBuffer, nullptr, &g_d3dRenderTargetView);
    if (FAILED(hr))
    {
        return -1;
    }

    SafeRelease(backBuffer);

    // Create the depth buffer for use with the depth/stencil view.
    D3D11_TEXTURE2D_DESC depthStencilBufferDesc;
    ZeroMemory(&depthStencilBufferDesc, sizeof(D3D11_TEXTURE2D_DESC));

    depthStencilBufferDesc.ArraySize = 1;
    depthStencilBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    depthStencilBufferDesc.CPUAccessFlags = 0; // No CPU access required.
    depthStencilBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthStencilBufferDesc.Width = clientWidth;
    depthStencilBufferDesc.Height = clientHeight;
    depthStencilBufferDesc.MipLevels = 1;
    depthStencilBufferDesc.SampleDesc.Count = 1;
    depthStencilBufferDesc.SampleDesc.Quality = 0;
    depthStencilBufferDesc.Usage = D3D11_USAGE_DEFAULT;

    hr = g_d3dDevice->CreateTexture2D(&depthStencilBufferDesc, nullptr, &g_d3dDepthStencilBuffer);
    if (FAILED(hr))
    {
        return -1;
    }

    hr = g_d3dDevice->CreateDepthStencilView(g_d3dDepthStencilBuffer, nullptr, &g_d3dDepthStencilView);
    if (FAILED(hr))
    {
        return -1;
    }

    // Setup depth/stencil state.
    D3D11_DEPTH_STENCIL_DESC depthStencilStateDesc;
    ZeroMemory(&depthStencilStateDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));

    depthStencilStateDesc.DepthEnable = TRUE;
    depthStencilStateDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthStencilStateDesc.DepthFunc = D3D11_COMPARISON_LESS;
    depthStencilStateDesc.StencilEnable = FALSE;

    hr = g_d3dDevice->CreateDepthStencilState(&depthStencilStateDesc, &g_d3dDepthStencilState);

    // Setup rasterizer state.
    D3D11_RASTERIZER_DESC rasterizerDesc;
    ZeroMemory(&rasterizerDesc, sizeof(D3D11_RASTERIZER_DESC));

    rasterizerDesc.AntialiasedLineEnable = FALSE;
    rasterizerDesc.CullMode = D3D11_CULL_BACK;
    rasterizerDesc.DepthBias = 0;
    rasterizerDesc.DepthBiasClamp = 0.0f;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.FrontCounterClockwise = FALSE;
    rasterizerDesc.MultisampleEnable = FALSE;
    rasterizerDesc.ScissorEnable = FALSE;
    rasterizerDesc.SlopeScaledDepthBias = 0.0f;

    // Create the rasterizer state object.
    hr = g_d3dDevice->CreateRasterizerState(&rasterizerDesc, &g_d3dRasterizerState);
    if (FAILED(hr))
    {
        return -1;
    }

    // Initialize the viewport to occupy the entire client area.
    g_Viewport.Width = static_cast<float>(clientWidth);
    g_Viewport.Height = static_cast<float>(clientHeight);
    g_Viewport.TopLeftX = 0.0f;
    g_Viewport.TopLeftY = 0.0f;
    g_Viewport.MinDepth = 0.0f;
    g_Viewport.MaxDepth = 1.0f;

    return 0;
}

// Entrypoint, main-method.
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE prevInstance, LPWSTR cmdLine, int cmdShow)
{
    UNREFERENCED_PARAMETER(prevInstance);
    UNREFERENCED_PARAMETER(cmdLine);

    // Check for DirectX Math library support.
    if (!XMVerifyCPUSupport())
    {
        MessageBox(nullptr, TEXT("Failed to verify DirectX Math library support."), TEXT("Error"), MB_OK);
        return -1;
    }

    if (InitApplication(hInstance, cmdShow) != 0)
    {
        MessageBox(nullptr, TEXT("Failed to create application window."), TEXT("Error"), MB_OK);
        return -1;
    }

    if (InitDirectX(hInstance, g_EnableVSync) != 0)
    {
        MessageBox(nullptr, TEXT("Failed to create DirectX device and swap chain."), TEXT("Error"), MB_OK);
        return -1;
    }

    if (!LoadContent())
    {
        MessageBox(nullptr, TEXT("Failed to load content."), TEXT("Error"), MB_OK);
        return -1;
    }

    int returnCode = Run();

    UnloadContent();
    Cleanup();

    return returnCode;
}

// Get the latest profile for the specified shader type.
template<class ShaderClass>
std::string GetLatestProfile();

template<>
std::string GetLatestProfile<ID3D11VertexShader>()
{
    assert(g_d3dDevice);

    // Query the current feature level:
    D3D_FEATURE_LEVEL featureLevel = g_d3dDevice->GetFeatureLevel();

    switch (featureLevel)
    {
    case D3D_FEATURE_LEVEL_11_1:
    case D3D_FEATURE_LEVEL_11_0:
    {
        return "vs_5_0";
    }
    break;
    case D3D_FEATURE_LEVEL_10_1:
    {
        return "vs_4_1";
    }
    break;
    case D3D_FEATURE_LEVEL_10_0:
    {
        return "vs_4_0";
    }
    break;
    case D3D_FEATURE_LEVEL_9_3:
    {
        return "vs_4_0_level_9_3";
    }
    break;
    case D3D_FEATURE_LEVEL_9_2:
    case D3D_FEATURE_LEVEL_9_1:
    {
        return "vs_4_0_level_9_1";
    }
    break;
    } // switch( featureLevel )

    return "";
}

template<>
std::string GetLatestProfile<ID3D11PixelShader>()
{
    assert(g_d3dDevice);

    // Query the current feature level:
    D3D_FEATURE_LEVEL featureLevel = g_d3dDevice->GetFeatureLevel();
    switch (featureLevel)
    {
    case D3D_FEATURE_LEVEL_11_1:
    case D3D_FEATURE_LEVEL_11_0:
    {
        return "ps_5_0";
    }
    break;
    case D3D_FEATURE_LEVEL_10_1:
    {
        return "ps_4_1";
    }
    break;
    case D3D_FEATURE_LEVEL_10_0:
    {
        return "ps_4_0";
    }
    break;
    case D3D_FEATURE_LEVEL_9_3:
    {
        return "ps_4_0_level_9_3";
    }
    break;
    case D3D_FEATURE_LEVEL_9_2:
    case D3D_FEATURE_LEVEL_9_1:
    {
        return "ps_4_0_level_9_1";
    }
    break;
    }
    return "";
}

template<class ShaderClass>
ShaderClass* CreateShader(ID3DBlob* pShaderBlob, ID3D11ClassLinkage* pClassLinkage);

template<>
ID3D11VertexShader* CreateShader<ID3D11VertexShader>(ID3DBlob* pShaderBlob, ID3D11ClassLinkage* pClassLinkage)
{
    assert(g_d3dDevice);
    assert(pShaderBlob);

    ID3D11VertexShader* pVertexShader = nullptr;
    g_d3dDevice->CreateVertexShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), pClassLinkage, &pVertexShader);

    return pVertexShader;
}

template<>
ID3D11PixelShader* CreateShader<ID3D11PixelShader>(ID3DBlob* pShaderBlob, ID3D11ClassLinkage* pClassLinkage)
{
    assert(g_d3dDevice);
    assert(pShaderBlob);

    ID3D11PixelShader* pPixelShader = nullptr;
    g_d3dDevice->CreatePixelShader(pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), pClassLinkage, &pPixelShader);

    return pPixelShader;
}

template<class ShaderClass>
ShaderClass* LoadShader(const std::wstring& fileName, const std::string& entryPoint, const std::string& _profile)
{
    ID3DBlob* pShaderBlob = nullptr;
    ID3DBlob* pErrorBlob = nullptr;
    ShaderClass* pShader = nullptr;

    std::string profile = _profile;
    if (profile == "latest")
    {
        profile = GetLatestProfile<ShaderClass>();
    }

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if _DEBUG
    flags |= D3DCOMPILE_DEBUG;
#endif

    HRESULT hr = D3DCompileFromFile(fileName.c_str(), nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint.c_str(), profile.c_str(),
        flags, 0, &pShaderBlob, &pErrorBlob);

    if (FAILED(hr))
    {
        if (pErrorBlob)
        {
            std::string errorMessage = (char*)pErrorBlob->GetBufferPointer();
            OutputDebugStringA(errorMessage.c_str());

            SafeRelease(pShaderBlob);
            SafeRelease(pErrorBlob);
        }

        return nullptr;
    }

    pShader = CreateShader<ShaderClass>(pShaderBlob, nullptr);

    SafeRelease(pShaderBlob);
    SafeRelease(pErrorBlob);

    return pShader;
}

// Creates a Patch and returns it.
Patch InitPatch(XMFLOAT3 pos[4], XMFLOAT3 irradiance)
{
    Patch p = {};
    p.vertex_pos[0] = pos[0];
    p.vertex_pos[1] = pos[1];
    p.vertex_pos[2] = pos[2];
    p.vertex_pos[3] = pos[3];

    XMVECTOR acc = {0.0f, 0.0f, 0.0f};
    for (int i = 0; i < 4; i++)
    {
        XMVECTOR vp = XMLoadFloat3(&p.vertex_pos[i]);
        acc = XMVectorAdd(acc, vp);
    }
    acc /= 4;
    XMStoreFloat3(&p.centroid, acc);

    p.radiosity = { 0.0f, 0.0f, 0.0f };
    p.irradiance = irradiance;

    XMVECTOR edge1 = XMLoadFloat3(&p.vertex_pos[0]) - XMLoadFloat3(&p.vertex_pos[1]);
    XMVECTOR edge2 = XMLoadFloat3(&p.vertex_pos[1]) - XMLoadFloat3(&p.vertex_pos[2]);
    XMVECTOR edge3 = XMLoadFloat3(&p.vertex_pos[2]) - XMLoadFloat3(&p.vertex_pos[3]);
    XMVECTOR edge4 = XMLoadFloat3(&p.vertex_pos[3]) - XMLoadFloat3(&p.vertex_pos[0]);

    XMVECTOR crossproduct1 = XMVector3Cross(edge1, edge2);
    XMVECTOR crossproduct2 = XMVector3Cross(edge3, edge4);

    XMStoreFloat3(&p.normal, XMVector3Normalize(crossproduct1));

    if (p.normal.x == 1.0f)
        p.reflectance = { 1.0f, 1.0f, 1.0f }; // left
    else if (p.normal.x == -1.0f)
        p.reflectance = { 1.0f, 1.0f, 1.0f }; // right
    else if (p.normal.y == 1.0f)
        p.reflectance = { 0.3f, 0.3f, 0.3f }; // bottom
    else if (p.normal.y == -1.0f)
        p.reflectance = { 0.3f, 0.3f, 0.3f }; // top
    else if (p.normal.z == 1.0f)
        p.reflectance = { 1.0f, 1.0f, 1.0f }; // back
    else if (p.normal.z == -1.0f)
        p.reflectance = { 1.0f, 1.0f, 1.0f }; // front

    p.area = 0.5f * std::abs(XMVector3Length(crossproduct1).m128_f32[0]) + std::abs(XMVector3Length(crossproduct2).m128_f32[0]);

    p.influencing_partner_count = 0;
    p.influencing_partners = {};
    p.influencing_partner_formfactors = {};
    p.has_children = false;
    p.has_parent = false;
    p.parent = nullptr;
    p.children = new Patch*[4];

    p.gathered_brightness = { 0.0f, 0.0f, 0.0f };
    p.brightness = { 0.0f, 0.0f, 0.0f };

    return p;
}

// Builds the Vertex and Index Buffers for all patches
void BuildPatchBuffers(ID3D11Device* device)
{
    for (int patch = 0; patch < g_room_model.face_count; patch++)
    {
        XMFLOAT3 color;
        if (g_without_hierarch_radiosity)
        {
            float x = g_patches[patch].irradiance.x + g_patches[patch].reflectance.x * g_patches[patch].radiosity.x;
            float y = g_patches[patch].irradiance.y + g_patches[patch].reflectance.y * g_patches[patch].radiosity.y;
            float z = g_patches[patch].irradiance.z + g_patches[patch].reflectance.z * g_patches[patch].radiosity.z;
            color = { x, y, z };
        }
        else
        {
            float x = g_patches[patch].irradiance.x + g_patches[patch].reflectance.x * g_patches[patch].brightness.m128_f32[0];
            float y = g_patches[patch].irradiance.y + g_patches[patch].reflectance.y * g_patches[patch].brightness.m128_f32[1];
            float z = g_patches[patch].irradiance.z + g_patches[patch].reflectance.z * g_patches[patch].brightness.m128_f32[2];
            color = { x, y, z };
        }

        Vertex* vertices = new Vertex[4];
        vertices[0] = { g_patches[patch].vertex_pos[0], g_patches[patch].normal, color };
        vertices[1] = { g_patches[patch].vertex_pos[1], g_patches[patch].normal, color };
        vertices[2] = { g_patches[patch].vertex_pos[2], g_patches[patch].normal, color };
        vertices[3] = { g_patches[patch].vertex_pos[3], g_patches[patch].normal, color };

        D3D11_BUFFER_DESC vertexBufferDesc;
        ZeroMemory(&vertexBufferDesc, sizeof(D3D11_BUFFER_DESC));

        vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vertexBufferDesc.ByteWidth = sizeof(Vertex) * 6;
        vertexBufferDesc.CPUAccessFlags = 0;
        vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;

        D3D11_SUBRESOURCE_DATA resourceData;
        ZeroMemory(&resourceData, sizeof(D3D11_SUBRESOURCE_DATA));

        resourceData.pSysMem = vertices;

        HRESULT hr = device->CreateBuffer(&vertexBufferDesc, &resourceData, &g_patches[patch].vertex_buffer);

        WORD* indices = new WORD[6]{ 0, 1, 2, 2, 3, 0 };

        D3D11_BUFFER_DESC indexBufferDesc;
        ZeroMemory(&indexBufferDesc, sizeof(D3D11_BUFFER_DESC));

        indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        indexBufferDesc.ByteWidth = sizeof(WORD) * 6;
        indexBufferDesc.CPUAccessFlags = 0;
        indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;

        ZeroMemory(&resourceData, sizeof(D3D11_SUBRESOURCE_DATA));
        resourceData.pSysMem = indices;

        hr = device->CreateBuffer(&indexBufferDesc, &resourceData, &g_patches[patch].index_buffer);
    }    
}

// Estimates all formfactors quickly and packs them into a global array g_formfactors.
void EstimateFormFactors()
{
    int patch_count = g_room_model.face_count;
    g_formfactors = new double[patch_count * patch_count];

    int count = 0;
    for (int i = 0; i < patch_count; i++)
    {
        for (int j = 0; j < patch_count; j++)
        {
            double ff = 0.0;
            if (i != j)
            {
                // calculate formfactor from path i to path j:

                XMVECTOR ni = XMLoadFloat3(&g_patches[i].normal);
                XMVECTOR nj = XMLoadFloat3(&g_patches[j].normal);
                XMVECTOR ci = XMLoadFloat3(&g_patches[i].centroid);
                XMVECTOR cj = XMLoadFloat3(&g_patches[j].centroid);
                double dAi = g_patches[i].area;
                double dAj = g_patches[j].area;

                XMVECTOR vecDist = cj - ci;
                double dRadius = XMVector3Length(vecDist).m128_f32[0];
                XMVECTOR vecDir = vecDist;
                vecDir = XMVector3Normalize(vecDir);

                double cosPhiI = XMVector3Dot(vecDir, ni).m128_f32[0];
                double cosPhiJ = XMVector3Dot(-vecDir, nj).m128_f32[0];

                if (cosPhiI < 0.0)
                {
                    cosPhiI = 0.0;
                }
                if (cosPhiJ < 0.0)
                {
                    cosPhiJ = 0.0;
                }

                ff = cosPhiI * cosPhiJ * dAj / dRadius / dRadius / XM_PI;
            }

            g_formfactors[count++] = ff;
        }
    }

    count = 0;
    double* sums = new double[patch_count];
    for (int i = 0; i < patch_count; ++i)
    {
        sums[i] = 0.0;
        for (int j = 0; j < patch_count; ++j)
        {
            sums[i] += g_formfactors[count++];
        }
    }

    count = 0;
    for (int i = 0; i < patch_count; ++i)
    {
        for (int j = 0; j < patch_count; ++j)
        {
            g_formfactors[count++] /= sums[i];
        }
    }

    delete[] sums;
}

// returns the actual color, computed by adding the irradiance to the product of reflectance and brightness(radiosity)
void GetRadiosity(int patch_index, XMFLOAT3& color)
{
    float x = g_patches[patch_index].irradiance.x + g_patches[patch_index].reflectance.x * g_patches[patch_index].radiosity.x;
    float y = g_patches[patch_index].irradiance.y + g_patches[patch_index].reflectance.y * g_patches[patch_index].radiosity.y;
    float z = g_patches[patch_index].irradiance.z + g_patches[patch_index].reflectance.z * g_patches[patch_index].radiosity.z;
    color = { x, y, z };
}

// this is the normal radiosity iteration
void IterateRadiosity()
{
    static int run_count = 0;
    static XMVECTOR* radiosity = new XMVECTOR[g_room_model.face_count];
    while (run_count < 20)
    {
        int i, j, count= 0;

        for (i = 0; i < g_room_model.face_count; ++i)
        {
            radiosity[i] = XMVECTOR{ 0.0, 0.0, 0.0, 0.0 };

            for (j = 0; j < g_room_model.face_count; ++j)
            {
                double dFormFactor = g_formfactors[count++];
                XMFLOAT3 color;
                GetRadiosity(j, color);
                radiosity[i] = XMVectorAdd(radiosity[i], XMVectorScale(XMLoadFloat3(&color), dFormFactor));
            }

        }

        for (i = 0; i < g_room_model.face_count; ++i)
        {
            XMStoreFloat3(&g_patches[i].radiosity, radiosity[i]);
        }

        run_count++;
    }
    delete[] radiosity;
}

// this returns a formfactor estimation between to patches
double EstimateFormFactor(Patch &p, Patch &q)
{
    const XMVECTOR& ni = XMLoadFloat3(&p.normal);
    const XMVECTOR& nj = XMLoadFloat3(&q.normal);
    const XMVECTOR& ci = XMLoadFloat3(&p.centroid);
    const XMVECTOR& cj = XMLoadFloat3(&q.centroid);
    double dAi = p.area;
    double dAj = q.area;

    XMVECTOR vecDist = cj - ci;
    double dRadius = XMVector3Length(vecDist).m128_f32[0];
    XMVECTOR vecDir = vecDist;
    vecDir = XMVector3Normalize(vecDir);

    double cosPhiI = XMVector3Dot(vecDir, ni).m128_f32[0];
    double cosPhiJ = XMVector3Dot(-vecDir, nj).m128_f32[0];

    if (cosPhiI < 0.0)
    {
        cosPhiI = 0.0;
    }
    if (cosPhiJ < 0.0)
    {
        cosPhiJ = 0.0;
    }

    return cosPhiI * cosPhiJ * dAj / dRadius / dRadius / XM_PI;
}

// this function links two patches for hierarchical gathering
void Link(Patch& p, Patch& q, double ff_ptoq, double ff_qtop)
{
    p.influencing_partners.push_back(&q);
    p.influencing_partner_count++;
    p.influencing_partner_formfactors.push_back(ff_qtop);
}

// this function checks if a patch is still divisible concerning its 
// area threshold.
bool SubdivPossible(Patch &p)
{
    return p.area > 0.3f;
}

// this function subdivides a patch.
void Subdivide(Patch& p)
{
    if (p.has_children)
        return;

    Patch* nw = new Patch();
    Patch* ne = new Patch();
    Patch* se = new Patch();
    Patch* sw = new Patch();

    XMVECTOR v0 = XMLoadFloat3(&p.vertex_pos[0]);
    XMVECTOR v1 = XMLoadFloat3(&p.vertex_pos[1]);
    XMVECTOR v2 = XMLoadFloat3(&p.vertex_pos[2]);
    XMVECTOR v3 = XMLoadFloat3(&p.vertex_pos[3]);

    XMVECTOR v0v1 = XMVectorSubtract(v1, v0);
    XMVECTOR v1v2 = XMVectorSubtract(v2, v1);
    XMVECTOR v2v3 = XMVectorSubtract(v3, v2);
    XMVECTOR v3v0 = XMVectorSubtract(v0, v3);

    XMVECTOR v4 = XMVectorAdd(v0, XMVectorScale(v0v1, 0.5f));
    XMVECTOR v5 = XMVectorAdd(v1, XMVectorScale(v1v2, 0.5f));
    XMVECTOR v6 = XMVectorAdd(v2, XMVectorScale(v2v3, 0.5f));
    XMVECTOR v7 = XMVectorAdd(v3, XMVectorScale(v3v0, 0.5f));

    XMVECTOR v8 = XMVectorAdd(v4, XMVectorScale(XMVectorSubtract(v6, v4), 0.5f));// middlepoint

    XMFLOAT3 v0f, v1f, v2f, v3f, v4f, v5f, v6f, v7f, v8f;

    XMStoreFloat3(&v0f, v0);
    XMStoreFloat3(&v1f, v1);
    XMStoreFloat3(&v2f, v2);
    XMStoreFloat3(&v3f, v3);
    XMStoreFloat3(&v4f, v4);
    XMStoreFloat3(&v5f, v5);
    XMStoreFloat3(&v6f, v6);
    XMStoreFloat3(&v7f, v7);
    XMStoreFloat3(&v8f, v8);

    XMFLOAT3 vertices1[4] = { v1f, v4f, v8f, v7f };
    *nw = InitPatch(vertices1, p.reflectance);
    XMFLOAT3 vertices2[4] = { v4f, v1f, v5f, v8f };
    *ne = InitPatch(vertices2, p.reflectance);
    XMFLOAT3 vertices3[4] = { v8f, v5f, v2f, v6f };
    *se = InitPatch(vertices3, p.reflectance);
    XMFLOAT3 vertices4[4] = { v7f, v8f, v6f, v3f };
    *sw = InitPatch(vertices4, p.reflectance);

    nw->has_parent = true;
    nw->parent = &p;

    p.has_children = true;
    p.children[0] = nw;
    p.children[1] = ne;
    p.children[2] = se;
    p.children[3] = sw;
}

// this is the known refine-algorithm from the 1984-paper for rapid hierarchical
// radiosity.
int Refine(Patch &p, Patch &q, double F_eps)
{
    double ff_ptoq = EstimateFormFactor(p, q);
    double ff_qtop = EstimateFormFactor(q, p);

    static int subdivisions = 0;

    if (ff_ptoq < F_eps && ff_qtop < F_eps)
    {
        Link(p, q, ff_ptoq, ff_qtop);
    }
    else if (ff_ptoq >= ff_qtop && SubdivPossible(q))
    {
        Subdivide(q);
        Refine(p, *q.children[0], F_eps);
        Refine(p, *q.children[1], F_eps);
        Refine(p, *q.children[2], F_eps);
        Refine(p, *q.children[3], F_eps);
        subdivisions++;
    }
    else if (ff_ptoq >= ff_qtop && !SubdivPossible(q))
    {
        Link(p, q, ff_ptoq, ff_qtop);
    }
    else if(ff_ptoq < ff_qtop && SubdivPossible(p))
    {
        Subdivide(p);
        Refine(q, *p.children[0], F_eps);
        Refine(q, *p.children[1], F_eps);
        Refine(q, *p.children[2], F_eps);
        Refine(q, *p.children[3], F_eps);
        subdivisions++;
    }
    else if (ff_ptoq < ff_qtop && !SubdivPossible(p))
    {
        Link(p, q, ff_ptoq, ff_qtop);
    }
    return subdivisions;
}

// this is a helper function for vectors.
XMVECTOR CompwiseMult(XMVECTOR& v1, XMVECTOR& v2)
{
    float x = v1.m128_f32[0] * v2.m128_f32[0];
    float y = v1.m128_f32[1] * v2.m128_f32[1];
    float z = v1.m128_f32[2] * v2.m128_f32[2];
    return { x, y, z };
}

// this returns the actual color brightness in the hierarchical radiosity method.
void GetBrightness(Patch& p, XMFLOAT3& color)
{
    float x = p.irradiance.x + p.reflectance.x * p.brightness.m128_f32[0];
    float y = p.irradiance.y + p.reflectance.y * p.brightness.m128_f32[1];
    float z = p.irradiance.z + p.reflectance.z * p.brightness.m128_f32[2];
    color = { x, y, z };
}

// this returns the actual color brightness gathered in the latest iteration 
// in the hierarchical radiosity method.
void GetGatheredBrightness(Patch& p, XMFLOAT3& color)
{
    float x = p.irradiance.x + p.reflectance.x * p.gathered_brightness.m128_f32[0];
    float y = p.irradiance.y + p.reflectance.y * p.gathered_brightness.m128_f32[1];
    float z = p.irradiance.z + p.reflectance.z * p.gathered_brightness.m128_f32[2];
    color = { x, y, z };
}

// this is the gather-algorithm to compute the radiosities from all linked patches of a patch
// in one iteration. it is part of the hierarchical radiosity method.
void Gather(Patch& p)
{
    p.gathered_brightness = { 0.0, 0.0, 0.0 };
    for (int i = 0; i < p.influencing_partner_count; i++)
    {
        auto ff_it = p.influencing_partner_formfactors.begin();
        std::advance(ff_it, i);
        double ff = *ff_it;

        auto partner_it = p.influencing_partners.begin();
        std::advance(partner_it, i);

        XMVECTOR color = XMLoadFloat3(&p.reflectance);
        XMFLOAT3 partner_brightness;
        GetBrightness(**partner_it, partner_brightness);
        XMVECTOR partner_brightness_v = XMLoadFloat3(&partner_brightness);
        
        p.gathered_brightness = XMVectorAdd(p.gathered_brightness, XMVectorScale(CompwiseMult(partner_brightness_v, color), ff));
                
        if (p.has_children)
        {
            for (int child = 0; child < 4; child++)
            {
                Gather(*p.children[child]);
            }
        }
    }
}

// this function pushes the brightness values down to its subpatches.
void PushBrightness(Patch& p)
{
    if (p.has_children)
    {
        XMFLOAT3 patch_brightness;
        GetGatheredBrightness(p, patch_brightness);
        XMVECTOR patch_brightness_v = XMLoadFloat3(&patch_brightness);
        for (int child = 0; child < 4; child++)
        {
            Patch& c = *p.children[child];
            c.gathered_brightness = XMVectorAdd(c.gathered_brightness, p.gathered_brightness);
            PushBrightness(c);
        }
    }
}

// this function pulls the brightness values of its subpatches and averages them out.
XMVECTOR PullBrightness(Patch& p)
{
    if (p.has_children)
    {
        XMVECTOR accumulate_brightness = {0.0f, 0.0f, 0.0f};
        for (int child = 0; child < 4; child++)
        {
            Patch& c = *p.children[child];
            accumulate_brightness = XMVectorAdd(accumulate_brightness, PullBrightness(c));
        }
        return XMVectorScale(accumulate_brightness, 0.25f);
    }
    else
    {
        return p.gathered_brightness;
    }
}

// this is the function which iterates the hierarchical radiosity method.
// it has a intentionally small amount of iterations so that it doesn't take too long.
void IterateHierarchicalRadiosity()
{
    for (int iterations = 0; iterations < 2; iterations++)
    {
        char buffer[256];
        for (int i = 0; i < g_patch_count; i++)
        {
            Gather(g_patches[i]);

            sprintf_s(buffer, "%d out of %d gathering-progression.\n", i + 1, g_patch_count);
            OutputDebugStringA(buffer);
        }
        for (int i = 0; i < g_patch_count; i++)
        {
            PushBrightness(g_patches[i]);

            sprintf_s(buffer, "%d out of %d push-progression.\n", i + 1, g_patch_count);
            OutputDebugStringA(buffer);
        }
        for (int i = 0; i < g_patch_count; i++)
        {
            g_patches[i].brightness = PullBrightness(g_patches[i]);

            sprintf_s(buffer, "%d out of %d pull-progression.\n", i + 1, g_patch_count);
            OutputDebugStringA(buffer);
        }
    }
}

// this function loads all necessary content for the rendering.
bool LoadContent()
{
    assert(g_d3dDevice);

    LoadModel(R"(..\Models\radiosity_room.obj)");

    // create patches
    g_patches = new Patch[g_room_model.face_count];
    XMFLOAT3 irradiance;
    XMFLOAT3 v_pos[4];
    for (int face_index = 0; face_index < g_room_model.face_count; face_index++)
    {
        Face& face = g_room_model.faces[face_index];

        for (int i = 0; i < 4; i++)
        {
            v_pos[i] = g_room_model.vertices[face.vertex_indices[i]].position;
        }

        if (face_index == 1001)
            irradiance = { 200.0f, 170.0f, 150.0f }; // warm light
        else
            irradiance = { 0.0f, 0.0f, 0.0f };

        g_patches[face_index] = InitPatch(v_pos, irradiance);
        g_patch_count++;
    }

    if (g_without_hierarch_radiosity)
    {
        EstimateFormFactors();
        IterateRadiosity();
    }
    else
    {
        char buffer[256];
        for (int i = 0; i < g_patch_count; i++)
        {
            for (int j = 0; j < g_patch_count; j++)
            {
                if (i != j)
                {
                    Refine(g_patches[i], g_patches[j], 0.1f);
                }
            }
        }
        IterateHierarchicalRadiosity();
    }

    BuildPatchBuffers(g_d3dDevice);

    // Create the constant buffers for the variables defined in the vertex shader.
    D3D11_BUFFER_DESC constantBufferDesc;
    ZeroMemory(&constantBufferDesc, sizeof(D3D11_BUFFER_DESC));

    constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    constantBufferDesc.ByteWidth = sizeof(XMMATRIX);
    constantBufferDesc.CPUAccessFlags = 0;
    constantBufferDesc.Usage = D3D11_USAGE_DEFAULT;

    HRESULT hr = g_d3dDevice->CreateBuffer(&constantBufferDesc, nullptr, &g_d3dConstantBuffers[CB_Application]);
    if (FAILED(hr))
    {
        return false;
    }
    hr = g_d3dDevice->CreateBuffer(&constantBufferDesc, nullptr, &g_d3dConstantBuffers[CB_Frame]);
    if (FAILED(hr))
    {
        return false;
    }
    hr = g_d3dDevice->CreateBuffer(&constantBufferDesc, nullptr, &g_d3dConstantBuffers[CB_Object]);
    if (FAILED(hr))
    {
        return false;
    }

    // Load the shaders
    g_d3dVertexShader = LoadShader<ID3D11VertexShader>(L"../Shaders/SimpleVertexShader.hlsl", "SimpleVertexShader", "latest");
    g_d3dPixelShader = LoadShader<ID3D11PixelShader>(L"../Shaders/SimplePixelShader.hlsl", "SimplePixelShader", "latest");

    // Load the compiled vertex shader.
    ID3DBlob* vertexShaderBlob;
    LPCWSTR compiledVertexShaderObject = L"Precompiled Shaders/SimpleVertexShader.cso";

    hr = D3DReadFileToBlob(compiledVertexShaderObject, &vertexShaderBlob);
    if (FAILED(hr))
    {
        return false;
    }

    hr = g_d3dDevice->CreateVertexShader(vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize(), nullptr, &g_d3dVertexShader);
    if (FAILED(hr))
    {
        return false;
    }

    // Create the input layout for the vertex shader.
    D3D11_INPUT_ELEMENT_DESC vertexLayoutDesc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex,position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex,normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex,color), D3D11_INPUT_PER_VERTEX_DATA, 0 }        
    };

    hr = g_d3dDevice->CreateInputLayout(vertexLayoutDesc, _countof(vertexLayoutDesc), vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize(), &g_d3dInputLayout);
    if (FAILED(hr))
    {
        return false;
    }

    SafeRelease(vertexShaderBlob);

    // Load the compiled pixel shader.
    ID3DBlob* pixelShaderBlob;
    LPCWSTR compiledPixelShaderObject = L"Precompiled Shaders/SimplePixelShader.cso";

    hr = D3DReadFileToBlob(compiledPixelShaderObject, &pixelShaderBlob);
    if (FAILED(hr))
    {
        return false;
    }

    hr = g_d3dDevice->CreatePixelShader(pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize(), nullptr, &g_d3dPixelShader);
    if (FAILED(hr))
    {
        return false;
    }

    SafeRelease(pixelShaderBlob);

    // Setup the projection matrix.
    RECT clientRect;
    GetClientRect(g_WindowHandle, &clientRect);

    // Compute the exact client dimensions.
    // This is required for a correct projection matrix.
    float clientWidth = static_cast<float>(clientRect.right - clientRect.left);
    float clientHeight = static_cast<float>(clientRect.bottom - clientRect.top);

    g_ProjectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(45.0f), clientWidth / clientHeight, 0.1f, 100.0f);

    g_d3dDeviceContext->UpdateSubresource(g_d3dConstantBuffers[CB_Application], 0, nullptr, &g_ProjectionMatrix, 0, 0);

    return true;
}

void Update(float deltaTime)
{
    XMVECTOR eyePosition = XMVectorSet(0.1, 3, -12, 1);
    XMVECTOR focusPoint = XMVectorSet(0, 3, 0, 1);
    XMVECTOR upDirection = XMVectorSet(0, 1, 0, 0);
    g_ViewMatrix = XMMatrixLookAtLH(eyePosition, focusPoint, upDirection);
    g_d3dDeviceContext->UpdateSubresource(g_d3dConstantBuffers[CB_Frame], 0, nullptr, &g_ViewMatrix, 0, 0);


    static float angle = 0.0f;
    angle += 10.0f * deltaTime;
    XMVECTOR rotationAxis = XMVectorSet(0, 1, 0, 0);

    g_WorldMatrix = XMMatrixRotationAxis(rotationAxis, XMConvertToRadians(angle));
    g_d3dDeviceContext->UpdateSubresource(g_d3dConstantBuffers[CB_Object], 0, nullptr, &g_WorldMatrix, 0, 0);
}

// Clear the color and depth buffers.
void Clear(const FLOAT clearColor[4], FLOAT clearDepth, UINT8 clearStencil)
{
    g_d3dDeviceContext->ClearRenderTargetView(g_d3dRenderTargetView, clearColor);
    g_d3dDeviceContext->ClearDepthStencilView(g_d3dDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, clearDepth, clearStencil);
}

void Present(bool vSync)
{
    if (vSync)
    {
        g_d3dSwapChain->Present(1, 0);
    }
    else
    {
        g_d3dSwapChain->Present(0, 0);
    }
}

void Render()
{
    assert(g_d3dDevice);
    assert(g_d3dDeviceContext);

    Clear(Colors::Black, 1.0f, 0);

    const UINT vertexStride = sizeof(Vertex);
    const UINT offset = 0;

    g_d3dDeviceContext->IASetInputLayout(g_d3dInputLayout);
    g_d3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    g_d3dDeviceContext->VSSetShader(g_d3dVertexShader, nullptr, 0);
    g_d3dDeviceContext->VSSetConstantBuffers(0, 3, g_d3dConstantBuffers);

    g_d3dDeviceContext->RSSetState(g_d3dRasterizerState);
    g_d3dDeviceContext->RSSetViewports(1, &g_Viewport);

    g_d3dDeviceContext->PSSetShader(g_d3dPixelShader, nullptr, 0);

    g_d3dDeviceContext->OMSetRenderTargets(1, &g_d3dRenderTargetView, g_d3dDepthStencilView);
    g_d3dDeviceContext->OMSetDepthStencilState(g_d3dDepthStencilState, 1);

    for (int patch = 0; patch < g_room_model.face_count; patch++)
    {
        g_d3dDeviceContext->IASetVertexBuffers(0, 1, &(g_patches[patch].vertex_buffer), &vertexStride, &offset);
        g_d3dDeviceContext->IASetIndexBuffer(g_patches[patch].index_buffer, DXGI_FORMAT_R16_UINT, 0);
        g_d3dDeviceContext->DrawIndexed(6, 0, 0);
    }

    Present(g_EnableVSync);
}

void UnloadContent()
{
    SafeRelease(g_d3dConstantBuffers[CB_Application]);
    SafeRelease(g_d3dConstantBuffers[CB_Frame]);
    SafeRelease(g_d3dConstantBuffers[CB_Object]);
    SafeRelease(g_d3dIndexBuffer);
    SafeRelease(g_d3dVertexBuffer);
    SafeRelease(g_d3dInputLayout);
    SafeRelease(g_d3dVertexShader);
    SafeRelease(g_d3dPixelShader);
}

void Cleanup()
{
    SafeRelease(g_d3dDepthStencilView);
    SafeRelease(g_d3dRenderTargetView);
    SafeRelease(g_d3dDepthStencilBuffer);
    SafeRelease(g_d3dDepthStencilState);
    SafeRelease(g_d3dRasterizerState);
    SafeRelease(g_d3dSwapChain);
    SafeRelease(g_d3dDeviceContext);
    SafeRelease(g_d3dDevice);
}