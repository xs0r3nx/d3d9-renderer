#include <windows.h>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <string>
#include <dwmapi.h>
#include <d3d9.h>
#include <d3dx9.h>
#pragma comment(lib,"d3d9.lib")
#pragma comment(lib,"d3dx9.lib")
#pragma comment(lib,"dwmapi.lib")
#define CUSTOMFVF (D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1)
const uint32_t SCR_WIDTH = 800;
const uint32_t SCR_HEIGHT = 600;
const wchar_t* CLASS_NAME = L"D3D9";
const wchar_t* SCR_TITLE = L"D3D9 Window";


class Light
{

public:
	enum class Type {
		Directional,
		Point,
		Spot
	};
	Light(Type type, D3DXVECTOR3 positionOrDirection, D3DXCOLOR color, float range = 20.0f) : lightType(type), enabled(true)
	{
		ZeroMemory(&light, sizeof(light));

		switch (type) {
		case Type::Directional:
			light.Type = D3DLIGHT_DIRECTIONAL;
			light.Direction = positionOrDirection;
			break;
		case Type::Point:
			light.Type = D3DLIGHT_POINT;
			light.Position = positionOrDirection;
			light.Range = range;
			light.Attenuation0 = 1.0f;
			break;
		case Type::Spot:
			light.Type = D3DLIGHT_SPOT;
			light.Position = positionOrDirection;
			light.Direction = D3DXVECTOR3(0, 0, 1); // default forward
			light.Range = range;
			light.Attenuation0 = 1.0f;
			light.Phi = D3DXToRadian(45.0f);
			light.Theta = D3DXToRadian(25.0f);
			light.Falloff = 1.0f;
			break;
		}

		light.Diffuse = color;
		light.Ambient = D3DXCOLOR(0.2f, 0.2f, 0.2f, 1.0f);
		light.Specular = D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f);
	}
	void apply(LPDIRECT3DDEVICE9 device, DWORD index) const {
		if (enabled) {
			device->SetLight(index, &light);
			device->LightEnable(index, TRUE);
		}
		else {
			device->LightEnable(index, FALSE);
		}
	}

	void setEnabled(bool state) { enabled = state; }
	void setPosition(const D3DXVECTOR3& pos) { light.Position = pos; }
	void setDirection(const D3DXVECTOR3& dir) { light.Direction = dir; }
	void setColor(const D3DXCOLOR& color) { light.Diffuse = color; }
private:
	D3DLIGHT9 light;
	Type lightType;
	bool enabled;
};

class Mesh
{
private:
	struct CUSTOMVERTEX {
		FLOAT X, Y, Z;
		FLOAT nx, ny, nz;
		DWORD COLOR;
		FLOAT U, V;
	};

	struct Material {
		bool useTexture;
		LPDIRECT3DTEXTURE9 texture;
	};


public:
	Mesh(LPDIRECT3DDEVICE9 device) : d3ddev(device), vBuffer(nullptr), iBuffer(nullptr), vertexCount(0), indexCount(0) {
		position = { 0.0f, 0.0f, 0.0f };
		rotation = { 0.0f, 0.0f, 0.0f };
		scale = { 1.0f, 1.0f, 1.0f };
		D3DXMatrixIdentity(&worldMatrix);
		mat = {};
		mat.useTexture = false;
		mat.texture = nullptr;
		ZeroMemory(&material, sizeof(material));
		material.Diffuse = D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f); // full reflectivity
		material.Ambient = D3DXCOLOR(0.3f, 0.3f, 0.3f, 1.0f); // soft ambient fill
		material.Specular = D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f); // shiny highlights
		material.Emissive = D3DXCOLOR(0.0f, 0.0f, 0.0f, 1.0f); // no glow
		material.Power = 20.0f; // shininess factor
	}
	~Mesh() {if (vBuffer) vBuffer->Release();
	if (iBuffer) iBuffer->Release();
	}
	using Vertex = CUSTOMVERTEX;
	using Index = WORD;
	void setTransform(const D3DXMATRIX& matrix) {
		worldMatrix = matrix;
	}

	void load(const std::vector<Vertex>& vertices, const std::vector<Index>& indices, D3DXVECTOR3 pos, D3DXVECTOR3 inputScale, D3DXVECTOR3 rotationAngle, D3DXVECTOR3 inputRotationMultiplyer, bool doesUseTexture, const wchar_t* textureName) {
		mat.useTexture = doesUseTexture;
		vertexCount = vertices.size();
		indexCount = indices.size();
		d3ddev->CreateVertexBuffer(
			vertexCount * sizeof(Vertex),
			0,
			CUSTOMFVF,
			D3DPOOL_MANAGED,
			&vBuffer,
			NULL
		);
		void* pVoid;
		vBuffer->Lock(0, 0, &pVoid, 0);
		memcpy(pVoid, vertices.data(), vertexCount * sizeof(Vertex));
		vBuffer->Unlock();
		d3ddev->CreateIndexBuffer(
			indexCount * sizeof(Index),
			0,
			D3DFMT_INDEX16,
			D3DPOOL_MANAGED,
			&iBuffer,
			NULL
		);
		void* pIndexData;
		iBuffer->Lock(0, 0, &pIndexData, 0);
		memcpy(pIndexData, indices.data(), indexCount * sizeof(Index));
		iBuffer->Unlock();
		scale = inputScale;
		rotation = rotationAngle;
		position = pos;
		rotationMultiplyer = inputRotationMultiplyer;
		if (FAILED(D3DXCreateTextureFromFile(d3ddev, textureName, &mat.texture))) {
			MessageBox(NULL, L"Failed to load texture", L"Error", MB_OK);
		}
	
	}
	void draw()  {
		updateWorldMatrix();
		d3ddev->SetMaterial(&material);
		d3ddev->SetTransform(D3DTS_WORLD, &worldMatrix); 
		d3ddev->SetStreamSource(0, vBuffer, 0, sizeof(Vertex));
		d3ddev->SetIndices(iBuffer);
		if (mat.useTexture && mat.texture) {
			d3ddev->SetTexture(0, mat.texture);
		}
		else {
			d3ddev->SetTexture(0, nullptr);
		}
		d3ddev->SetFVF(CUSTOMFVF);
		d3ddev->DrawIndexedPrimitive(
			D3DPT_TRIANGLELIST,
			0,              // BaseVertexIndex
			0,              // MinVertexIndex
			vertexCount,    // NumVertices
			0,              // StartIndex
			indexCount / 3  // PrimitiveCount
		);
	}
private:
	LPDIRECT3DDEVICE9 d3ddev;
	LPDIRECT3DVERTEXBUFFER9 vBuffer;
	LPDIRECT3DINDEXBUFFER9 iBuffer;
	UINT vertexCount;
	UINT indexCount;
	D3DXMATRIX worldMatrix;
	D3DXVECTOR3 position;
	D3DXVECTOR3 rotation; // in degrees
	D3DXVECTOR3 scale;
	D3DXMATRIX matScale, matRotX, matRotY, matRotZ, matTrans;
	D3DXVECTOR3 rotationMultiplyer;
	Material mat;
	D3DMATERIAL9 material;
	void updateWorldMatrix() {
		D3DXMatrixScaling(&matScale, scale.x, scale.y, scale.z);
		D3DXMatrixRotationX(&matRotX, D3DXToRadian(rotation.x));
		D3DXMatrixRotationY(&matRotY, D3DXToRadian(rotation.y));
		D3DXMatrixRotationZ(&matRotZ, D3DXToRadian(rotation.z));
		D3DXMatrixTranslation(&matTrans, position.x, position.y, position.z);
		D3DXMATRIX rot = matRotZ * matRotY * matRotX;
		worldMatrix = matTrans * rot * matScale;
		rotation.x += 1.0f * rotationMultiplyer.x;
		rotation.y += 1.0f * rotationMultiplyer.y;
		rotation.z += 1.0f * rotationMultiplyer.z;
		if (rotation.x > 360.0f) rotation.x -= 360.0f;
		if (rotation.y > 360.0f) rotation.y -= 360.0f;
		if (rotation.z > 360.0f) rotation.z -= 360.0f;

	}


};
class Camera
{
public:
	Camera()
	{
		pos = D3DXVECTOR3(0.0f, 0.0f, 10.0f);
		look = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
		up = D3DXVECTOR3(0.0f, 1.0f, 0.0f);
		front = look - pos;
		D3DXVec3Normalize(&front, &front);
		D3DXMatrixLookAtLH(&matView, &pos, &look, &up);
		D3DXVec3Cross(&right, &up, &front);       
		D3DXVec3Normalize(&right, &right);        
		speed = 0.1f;
		yaw = -90.0f;
		pitch = 0.0f;
		mousePos = {};
		center = {};
		rect = {};
		camMouseEnabled = true;
		pastCamMouseEnabled = !camMouseEnabled;
	}
	~Camera()
	{

	}
	void camCtrl(HWND hwnd)
	{

		if (GetAsyncKeyState('W') & 0x8000) pos += speed * front;
		if (GetAsyncKeyState('S') & 0x8000) pos -= speed * front;
		if (GetAsyncKeyState('A') & 0x8000) pos -= right * speed;
		if (GetAsyncKeyState('D') & 0x8000) pos += right * speed;

		if (GetAsyncKeyState(VK_SPACE) & 0x8000) // Move up
			pos += up * speed;

		if (GetAsyncKeyState(VK_SHIFT) & 0x8000) // Move down
			pos -= up * speed;

		if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
			pastCamMouseEnabled = camMouseEnabled;
			camMouseEnabled = !camMouseEnabled;
			Sleep(100);
		}
		if (camMouseEnabled != pastCamMouseEnabled)
		{
			ShowCursor(!camMouseEnabled); // Hide when enabled, show when disabled
			pastCamMouseEnabled = camMouseEnabled;
		}

		if (camMouseEnabled == true)
		{

			GetCursorPos(&mousePos);
			GetClientRect(hwnd, &rect);
			center.x = (rect.right - rect.left) / 2;
			center.y = (rect.bottom - rect.top) / 2;
			ClientToScreen(hwnd, &center);

			float xOffset = static_cast<float>(mousePos.x - center.x);
			float yOffset = static_cast<float>(center.y - mousePos.y); // Y inverted

			float sensitivity = 0.1f;
			yaw -= xOffset * sensitivity;
			pitch += yOffset * sensitivity;

			// Clamp pitch manually
			if (pitch > 89.0f) pitch = 89.0f;
			if (pitch < -89.0f) pitch = -89.0f;

			// Calculate new front vector
			D3DXVECTOR3 direction;
			direction.x = cos(D3DXToRadian(yaw)) * cos(D3DXToRadian(pitch));
			direction.y = sin(D3DXToRadian(pitch));
			direction.z = sin(D3DXToRadian(yaw)) * cos(D3DXToRadian(pitch));

			D3DXVec3Normalize(&front, &direction);
			// Reset cursor to center
			SetCursorPos(center.x, center.y);
		}
		D3DXVec3Cross(&right, &up, &front);
		D3DXVec3Normalize(&right, &right);

	}
	void updateViewMatrix(LPDIRECT3DDEVICE9 d3ddev)
	{
		look = pos + front;
		D3DXMatrixLookAtLH(&matView, &pos, &look, &up);
		d3ddev->SetTransform(D3DTS_VIEW, &matView);
	}
private:
	D3DXMATRIX matView;
	D3DXVECTOR3 pos;
	D3DXVECTOR3 look;
	D3DXVECTOR3 up;
	D3DXVECTOR3 front;
	D3DXVECTOR3 right;
	float speed;
	float yaw;
	float pitch;
	POINT mousePos;
	RECT rect;
	POINT center;
	bool camMouseEnabled;
	bool pastCamMouseEnabled;
};

class Renderer
{
public:
	Renderer()
	{
		wc = {};
		msg = {};
		hwnd = {};
		d3d = {};
		d3ddev = {};
		vBuffer = {};
		cam = new Camera;
	}
	~Renderer()
	{

	}
	void run(HINSTANCE hInstance, int nCmdShow)
	{
#ifndef NDEBUG
		initConsole();
#endif
		initWindow(hInstance, nCmdShow);

		initD3D9();

		mainLoop();

		cleanup();
	}
private:
	WNDCLASSEX wc;
	HWND hwnd;
	MSG msg;
	LPDIRECT3D9 d3d;
	LPDIRECT3DDEVICE9 d3ddev;
	LPDIRECT3DVERTEXBUFFER9 vBuffer;
	Camera* cam; 
	std::vector<std::unique_ptr<Mesh>> meshes;
	static LRESULT CALLBACK wndproc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (uMsg)
		{
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		default:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
		}
	}
#ifndef NDEBUG
	void initConsole()
	{
		AllocConsole();
		SetConsoleTitle(L"D3D9 Debug Console");
		FILE* pFile;
		freopen_s(&pFile, "CONOUT$", "w", stdout);
		FILE* pErr;
		freopen_s(&pErr, "CONOUT$", "w", stderr);   // Redirect std::cerr
		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
		std::cout << "Console initialized successfully!" << std::endl;
	}
#endif
	void initWindow(HINSTANCE hInstance, int nCmdShow)
	{
		wc.cbSize = sizeof(WNDCLASSEX);
		wc.hInstance = hInstance;
		wc.lpszClassName = CLASS_NAME;
		wc.lpfnWndProc = wndproc;
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);
		wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
		wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
		wc.style = CS_HREDRAW | CS_VREDRAW;
		RegisterClassEx(&wc);
		hwnd = CreateWindowEx(
			0,
			wc.lpszClassName,
			SCR_TITLE,
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, SCR_WIDTH, SCR_HEIGHT,
			NULL, NULL, hInstance, NULL
		);
		if (!hwnd)
			throw std::runtime_error("Window creation failed");

		BOOL darkMode = TRUE;
		DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));

		ShowWindow(hwnd, nCmdShow);
		UpdateWindow(hwnd);
	}
	void initD3D9()
	{
		d3d = Direct3DCreate9(D3D_SDK_VERSION);
		D3DPRESENT_PARAMETERS d3dpp = {};
		d3dpp.Windowed = TRUE;
		d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
		d3dpp.hDeviceWindow = hwnd;
		d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
		d3dpp.BackBufferWidth = SCR_WIDTH;
		d3dpp.BackBufferHeight = SCR_HEIGHT;
		d3dpp.EnableAutoDepthStencil = TRUE;
		d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
		d3d->CreateDevice(
			D3DADAPTER_DEFAULT,
			D3DDEVTYPE_HAL,
			hwnd,
			D3DCREATE_SOFTWARE_VERTEXPROCESSING,
			&d3dpp,
			&d3ddev 
		);
		initGraphics();
		d3ddev->SetRenderState(D3DRS_LIGHTING, TRUE);    // turn off the 3D lighting
		d3ddev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		d3ddev->SetRenderState(D3DRS_ZENABLE, TRUE);    // turn on the z-buffer
		d3ddev->SetRenderState(D3DRS_SPECULARENABLE, TRUE);

	}
	void mainLoop()
	{
		while (msg.message != WM_QUIT)
		{
			while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			renderFrame();
			cam->camCtrl(hwnd);
		}
	}
	void cleanup()
	{
		delete cam;
		cleanD3D9();
#ifndef NDEBUG
		FreeConsole();
#endif
	}
	void initGraphics()
	{
		Light sun(Light::Type::Directional, D3DXVECTOR3(0, -1, 1), D3DXCOLOR(1, 1, 0.9f, 1),50.0f);
		sun.apply(d3ddev, 0);

		Light torch(Light::Type::Point, D3DXVECTOR3(2, 1, 0), D3DXCOLOR(1, 0.5f, 0.3f, 1), 10.0f);
		torch.apply(d3ddev, 1);


		auto Triangle = std::make_unique<Mesh>(d3ddev);
			Triangle->load(
			{

				{ -1.0f,  1.0f, 0.0f,  0.0f, 0.0f, -1.0f, D3DCOLOR_XRGB(255, 255, 255), 0.0f, 0.0f }, // Top-left
				{  1.0f,  1.0f, 0.0f,  0.0f, 0.0f, -1.0f, D3DCOLOR_XRGB(255, 255, 255), 1.0f, 0.0f }, // Top-right
				{ -1.0f, -1.0f, 0.0f,  0.0f, 0.0f, -1.0f, D3DCOLOR_XRGB(255, 255, 255), 0.0f, 1.0f }, // Bottom-left
				{  1.0f, -1.0f, 0.0f,  0.0f, 0.0f, -1.0f, D3DCOLOR_XRGB(255, 255, 255), 1.0f, 1.0f }  // Bottom-right



			},
			{
				0, 1, 2,
				2, 1, 3
			},
			D3DXVECTOR3(0.0f, 0.0f, 5.0f),           // position
			D3DXVECTOR3(4.0f, 4.0f, 1.0f),           // scale
			D3DXVECTOR3(0.0f, 0.0f, 0.0f),           // rotation
			D3DXVECTOR3(0.0f, 0.5f, 0.0f),           // rotation speed
			true,                                    // use texture
			L"C:\\Users\\taxa\\images.jpg"                       // texture path
		);
			meshes.push_back(std::move(Triangle));

	}
	void renderFrame()
	{
		d3ddev->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(40, 40, 40),1.0f, 0);
		d3ddev->Clear(0, NULL, D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
		d3ddev->BeginScene();
		d3ddev->SetFVF(CUSTOMFVF);
		D3DXMATRIX matProjection;
		D3DXMatrixPerspectiveFovLH(
			&matProjection,
			D3DXToRadian(45.0f),                  // Field of view
			(FLOAT)SCR_WIDTH / (FLOAT)SCR_HEIGHT, // Aspect ratio
			1.0f,                                 // Near plane
			100.0f                                // Far plane
		);
		d3ddev->SetTransform(D3DTS_PROJECTION, &matProjection);
		cam->updateViewMatrix(d3ddev);

		for (const auto& mesh : meshes) {
			mesh->draw();
		}
		d3ddev->EndScene();
		d3ddev->Present(NULL, NULL, NULL, NULL);
	}
	void cleanD3D9()
	{
		d3ddev->Release();

		d3d->Release();
	}
};
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdline, int nCmdShow)
{
	try
	{
		std::make_unique<Renderer>()->run(hInstance, nCmdShow);
	}
	catch (const std::exception& e)
	{
		std::cerr << "ERROR: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}