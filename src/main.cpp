#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>

#include <Imgui/imgui.h>
#include <Imgui/imgui_impl_dx11.h>
#include <Imgui/imgui_impl_win32.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(linker, "/subsystem:windows /entry:WinMainCRTStartup")

using namespace DirectX;

float g_rotX = 0.0f;
float g_rotY = 0.0f;
float g_cameraDist = -5.0f;
float g_cubeScale = 1.0f;
float g_ClearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };

static bool isImGuiOpen = true;
static bool isOparateOpen = false;

const char* shaderCode = R"(
cbuffer ConstantBuffer : register(b0) { matrix WorldViewProj; }
struct VS_OUTPUT { float4 Pos : SV_POSITION; float4 Color : COLOR; };
VS_OUTPUT VS(float4 Pos : POSITION, float4 Color : COLOR) {
    VS_OUTPUT output;
    output.Pos = mul(Pos, WorldViewProj);
    output.Color = Color;
    return output;
}
float4 PS(VS_OUTPUT input) : SV_Target { return input.Color; }
)";

struct Vertex { XMFLOAT3 Pos; XMFLOAT4 Color; };
struct ConstantBuffer { XMMATRIX mWorldViewProj; };

IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;
ID3D11InputLayout* g_pVertexLayout = nullptr;
ID3D11Buffer* g_pVertexBuffer = nullptr;
ID3D11Buffer* g_pIndexBuffer = nullptr;
ID3D11Buffer* g_pConstantBuffer = nullptr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam)) {
        return true;
    }

    switch (message) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_MOUSEWHEEL:
        g_cameraDist += (float)GET_WHEEL_DELTA_WPARAM(wParam) / 120.0f * 0.5f;
        return 0;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

void Movement(HWND hWnd) {
    if (ImGui::GetIO().WantCaptureMouse) return;

    if (GetAsyncKeyState(VK_UP) & 0x8000) g_rotX -= 0.02f;
    if (GetAsyncKeyState(VK_DOWN) & 0x8000) g_rotX += 0.02f;
    if (GetAsyncKeyState(VK_LEFT) & 0x8000) g_rotY -= 0.02f;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) g_rotY += 0.02f;

    if (GetAsyncKeyState('W') & 0x8000) g_cameraDist += 0.05f;
    if (GetAsyncKeyState('S') & 0x8000) g_cameraDist -= 0.05f;

    static POINT lastMousePos;
    POINT currentMousePos;
    GetCursorPos(&currentMousePos);
    ScreenToClient(hWnd, &currentMousePos);

    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
        g_rotY += (currentMousePos.x - lastMousePos.x) * 0.005f;
        g_rotX += (currentMousePos.y - lastMousePos.y) * 0.005f;
    }
    lastMousePos = currentMousePos;

    if (g_cameraDist > -1.0f) g_cameraDist = -1.0f;
    if (g_cameraDist < -20.0f) g_cameraDist = -20.0f;
}

void ImguiUI() {
    if (GetAsyncKeyState('P') & 0x8000) isImGuiOpen = true;
    if (GetAsyncKeyState('L') & 0x8000) isImGuiOpen = false;
    if (isImGuiOpen) {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Debug mode");

        if (ImGui::Button("How to Operate")) {
            isOparateOpen = true;
        }
        if (isOparateOpen) {
            ImGui::Begin("How to Operate Tab");

            if (ImGui::Button("Delate Tab")) isOparateOpen = false;

            ImGui::Separator();

            ImGui::Text("Left key to Left");
            ImGui::Text("Right key to Right");
            ImGui::Text("Up key to Up");
            ImGui::Text("Down key to Down");

            ImGui::Text("You can also move it with the mouse");

            ImGui::Separator();

            ImGui::Text("L key to close Imgui");
            ImGui::Text("P key to open Imgui");

            ImGui::End();
        }

        ImGui::Separator();

        ImGui::Text("Cube scale");
        ImGui::SliderFloat("Scale", &g_cubeScale, 0.1f, 5.0f);

        ImGui::Separator();

        ImGui::Text("Background");
        ImGui::ColorEdit4("Background color", g_ClearColor);

        ImGui::End();
    }
}

void Render() {
    g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, g_ClearColor);

    XMMATRIX mScale = XMMatrixScaling(g_cubeScale, g_cubeScale, g_cubeScale);
    XMMATRIX mRot = XMMatrixRotationRollPitchYaw(g_rotX, g_rotY, 0);
    XMMATRIX mWorld = mScale * mRot;
    XMMATRIX mView = XMMatrixLookAtLH(XMVectorSet(0, 3, g_cameraDist, 0), XMVectorZero(), XMVectorSet(0, 1, 0, 0));
    XMMATRIX mProj = XMMatrixPerspectiveFovLH(XM_PIDIV4, 900.0f / 700.0f, 0.01f, 100.0f);

    ConstantBuffer cb;
    cb.mWorldViewProj = XMMatrixTranspose(mWorld * mView * mProj);
    g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, nullptr, &cb, 0, 0);

    g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
    g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);
    g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);
    g_pImmediateContext->DrawIndexed(36, 0, 0);

    if (isImGuiOpen) {
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    g_pSwapChain->Present(1, 0);
}

void Update(HWND hWnd) {

    ImguiUI();
    Movement(hWnd);
    Render();

}

HRESULT InitDevice(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 1280; sd.BufferDesc.Height = 720;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd; sd.SampleDesc.Count = 1; sd.Windowed = TRUE;

    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();
    g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

    D3D11_VIEWPORT vp = { 0, 0, 1280, 720, 0, 1 };
    g_pImmediateContext->RSSetViewports(1, &vp);

    ID3DBlob* pVSBlob = nullptr, * pErrorBlob = nullptr;
    D3DCompile(shaderCode, strlen(shaderCode), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &pVSBlob, &pErrorBlob);
    g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &g_pVertexShader);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    g_pd3dDevice->CreateInputLayout(layout, 2, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &g_pVertexLayout);
    g_pImmediateContext->IASetInputLayout(g_pVertexLayout);
    pVSBlob->Release();

    ID3DBlob* pPSBlob = nullptr;
    D3DCompile(shaderCode, strlen(shaderCode), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &pPSBlob, nullptr);
    g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pPixelShader);
    pPSBlob->Release();

    Vertex vertices[] = {
        { XMFLOAT3(-1, 1,-1), XMFLOAT4(1,0,0,1) }, { XMFLOAT3(1, 1,-1), XMFLOAT4(0,1,0,1) },
        { XMFLOAT3(1, 1, 1), XMFLOAT4(0,0,1,1) }, { XMFLOAT3(-1, 1, 1), XMFLOAT4(1,1,0,1) },
        { XMFLOAT3(-1,-1,-1), XMFLOAT4(1,0,1,1) }, { XMFLOAT3(1,-1,-1), XMFLOAT4(0,1,1,1) },
        { XMFLOAT3(1,-1, 1), XMFLOAT4(1,1,1,1) }, { XMFLOAT3(-1,-1, 1), XMFLOAT4(0,0,0,1) },
    };
    D3D11_BUFFER_DESC bd = { sizeof(vertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER };
    D3D11_SUBRESOURCE_DATA sdV = { vertices };
    g_pd3dDevice->CreateBuffer(&bd, &sdV, &g_pVertexBuffer);
    UINT stride = sizeof(Vertex), offset = 0;
    g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

    WORD indices[] = { 3,1,0, 2,1,3, 0,5,4, 1,5,0, 1,2,5, 2,6,5, 2,3,6, 3,7,6, 3,0,4, 7,3,4, 4,5,6, 4,6,7 };
    bd = { sizeof(indices), D3D11_USAGE_DEFAULT, D3D11_BIND_INDEX_BUFFER };
    sdV = { indices };
    g_pd3dDevice->CreateBuffer(&bd, &sdV, &g_pIndexBuffer);
    g_pImmediateContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    bd = { sizeof(ConstantBuffer), D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER };
    g_pd3dDevice->CreateBuffer(&bd, nullptr, &g_pConstantBuffer);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pImmediateContext);
    ImGui::StyleColorsDark();

    return S_OK;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0, 0, hInst, nullptr, nullptr, nullptr, nullptr, L"MyGameClass", nullptr };
    RegisterClassEx(&wc);

    HWND hWnd = CreateWindow(
        wc.lpszClassName,
        L"DirectX11 - SimpleCUBE",
        WS_OVERLAPPEDWINDOW,
        100, 100,
        1280, 720,
        nullptr, nullptr, hInst, nullptr
    );

    if (FAILED(InitDevice(hWnd))) return 0;
    ShowWindow(hWnd, nCmdShow);

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            Update(hWnd);
        }
    }
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (g_pConstantBuffer) g_pConstantBuffer->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pVertexShader) g_pVertexShader->Release();
    if (g_pPixelShader) g_pPixelShader->Release();
    if (g_pVertexLayout) g_pVertexLayout->Release();
    if (g_pVertexBuffer) g_pVertexBuffer->Release();
    if (g_pIndexBuffer) g_pIndexBuffer->Release();

    return (int)msg.wParam;
}