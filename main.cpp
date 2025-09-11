//includes
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <string>
#include <synchapi.h>
#include <dwmapi.h>
#include "d3dx12.h"
//linking libs
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment (lib,"dwmapi.lib")
//defining debug mode
#ifndef NDEBUG
#define D3DCOMPILE_DEBUG 1
#endif
//constants
const uint32_t SCR_WIDTH = 640;
const uint32_t SCR_HEIGHT = 480;
const wchar_t* SCR_TITLE = L"DIRECT3D12 Window";
const wchar_t* CLASS_NAME = L"D3D12";
// a helper function for HRs
namespace DX {
	inline void ThrowIfFailed(HRESULT hr)
	{
		if (FAILED(hr))
		{
			throw std::exception();
		}
	}
}
//RendererClass
class Renderer
{
public:
	Renderer()
	{
		darkMode = TRUE;
		wc = {};
		msg = {};
		hwnd = {};
	}
	~Renderer()
	{

	}
	void run(HINSTANCE hInstance, int nCmdShow)
	{
#ifndef NDEBUG
			initConsole();
#endif
		initWindow(hInstance,nCmdShow);
		initD3D12();
		mainLoop();
		cleanup();
	}
private:
	//private Members. self [explanatory
	static const UINT textureWidth = 256;
	static const UINT textureHeight = 256;
	static const UINT texturePixelSize = 4;
	static const UINT FrameCount = 2;
	BOOL darkMode;
	WNDCLASSEX wc;
	MSG msg;
	HWND hwnd;
	D3D12_VIEWPORT viewPort;
	D3D12_RECT scissorRect;
	Microsoft::WRL::ComPtr<IDXGISwapChain3> mainSwapChain;
	Microsoft::WRL::ComPtr<ID3D12Device> device;
	Microsoft::WRL::ComPtr<ID3D12Resource> renderTargets[FrameCount];
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
	UINT rtvDescriptorSize;
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	UINT frameIndex;
	HANDLE fenceEvent;
	Microsoft::WRL::ComPtr<ID3D12Fence> fence;
	UINT64 fenceValue;
	//makes our aspect ratio
	float aspectRatio = static_cast<float>(SCR_WIDTH) / static_cast<float>(SCR_HEIGHT);
	Microsoft::WRL::ComPtr<ID3D12Resource> texture;
	//struct of vertex data
	struct Vertex
	{
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT4 color;
	};
	//our sexy WINPROC
	static LRESULT CALLBACK WinProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		auto app = reinterpret_cast<Renderer*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
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
	//making a console
	void initConsole()
	{
		AllocConsole();
		SetConsoleTitle(L"DIRECT 3D DEBUG CONSOLE");
		FILE* pFile;
		freopen_s(&pFile, "CONOUT$", "w", stdout);
		FILE* pErr;
		freopen_s(&pErr, "CONOUT$", "w", stderr);   // Redirect std::cerr
		std::ios::sync_with_stdio(); // Sync C++ streams with C streams
		std::wcout.clear(); // Clear any error flags
		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
		std::cout << "Console initialized successfully!" << std::endl;
	}
#endif
	void initWindow(HINSTANCE hInstance,int nCmdShow)
	{
		//making a window
		wc.cbSize = sizeof(WNDCLASSEX);
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = WinProc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hInstance = hInstance;
		wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wc.lpszMenuName = NULL;
		wc.lpszClassName = CLASS_NAME;
		wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
		//registering the window class and making it 
		RegisterClassEx(&wc);
		hwnd = CreateWindowEx(
			0,
			CLASS_NAME,
			SCR_TITLE,
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT,
			SCR_WIDTH, SCR_HEIGHT,
			NULL, NULL,
			hInstance,
			NULL);
		//this is for reinterpreting stuff later
		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
		//who doesn't like dark mode?
		DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
		ShowWindow(hwnd, nCmdShow);
		UpdateWindow(hwnd);
	}
	void initD3D12()
	{
		//initializing pipeline and shaders for d3d12
		loadPipeline();
		loadAssets();
	}
	void mainLoop()
	{
		//ah. i first learned this for dx9. time flies doesn't it?
		while (msg.message != WM_QUIT)
		{
			while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			//Renderer Updates
			onUpdate();
			onRender();
		}
	}
	void cleanup()
	{
		//destroying dx12
		onDestroy();
		//destroying console if it exists
#ifndef NDEBUG
			FreeConsole();
#endif
	}
	void loadPipeline()
	{
#ifndef NDEBUG
		{
			//making the debug controller and enabling it
			Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();
			}
		}
#endif
		//making a factory for checking the hardware adapter and getting  it
		Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
		DX::ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
		Microsoft::WRL::ComPtr<IDXGIAdapter1> hardwareAdapter;
		for (UINT adapterIndex = 0; ; ++adapterIndex)
		{
			Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
			if (DXGI_ERROR_NOT_FOUND == factory->EnumAdapters1(adapterIndex, &adapter))
			{
				break;
			}
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);
			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				continue;
			}
			// Check if adapter supports D3D12
			if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
			{
				hardwareAdapter = adapter;
				break;
			}
			std::wcout << L"Using adapter: " << desc.Description << std::endl;

		}
		//giving the adapter to device
		DX::ThrowIfFailed(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)));
		//making command queue description
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		//you know already
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		DX::ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)));
		//swap chain description
		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
		swapChainDesc.BufferCount = FrameCount;
		swapChainDesc.BufferDesc.Width = SCR_WIDTH;
		swapChainDesc.BufferDesc.Height = SCR_HEIGHT;
		swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.OutputWindow = hwnd;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.Windowed = TRUE;
		//creating the swap chain
		Microsoft::WRL::ComPtr <IDXGISwapChain> swapChain;
		DX::ThrowIfFailed(factory->CreateSwapChain(commandQueue.Get(), &swapChainDesc, &swapChain));
		DX::ThrowIfFailed(swapChain.As(&mainSwapChain));
		DX::ThrowIfFailed(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
		frameIndex = mainSwapChain->GetCurrentBackBufferIndex();
		{
			D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
			rtvHeapDesc.NumDescriptors = FrameCount;
			rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			DX::ThrowIfFailed(device->CreateDescriptorHeap(&rtvHeapDesc,IID_PPV_ARGS(&rtvHeap)));
			rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		}
		{
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());
			for (UINT n = 0; n < FrameCount; n++)
			{
				DX::ThrowIfFailed(swapChain->GetBuffer(n, IID_PPV_ARGS(&renderTargets[n])));
				device->CreateRenderTargetView(renderTargets[n].Get(), nullptr, rtvHandle);
				rtvHandle.Offset(1, rtvDescriptorSize);

			}
		}
		{
			viewPort = { 0.0f, 0.0f, static_cast<float>(SCR_WIDTH), static_cast<float>(SCR_HEIGHT), 0.0f, 1.0f };
			scissorRect = { 0, 0, static_cast<LONG>(SCR_WIDTH), static_cast<LONG>(SCR_HEIGHT) };

		}
		DX::ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
	}
	void loadAssets()
	{
		{
			CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
			rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
			Microsoft::WRL::ComPtr<ID3DBlob> signature;
			Microsoft::WRL::ComPtr<ID3DBlob> error;
			DX::ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
			DX::ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));

		}
		{
			Microsoft::WRL::ComPtr<ID3DBlob> vertexShader;
			Microsoft::WRL::ComPtr<ID3DBlob> pixelShader;
#ifndef NDEBUG
			// Enable better shader debugging with the graphics debugging tools.
			UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
			UINT compileFlags = 0;
#endif
			DX::ThrowIfFailed(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
			DX::ThrowIfFailed(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));
			D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
			{
				{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
				{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}
			};
			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
			psoDesc.InputLayout = { inputElementDescs,_countof(inputElementDescs) };
			psoDesc.pRootSignature = rootSignature.Get();
			psoDesc.VS = { reinterpret_cast<UINT8*>(vertexShader->GetBufferPointer()),vertexShader->GetBufferSize() };
			psoDesc.PS = { reinterpret_cast<UINT8*>(pixelShader->GetBufferPointer()),pixelShader->GetBufferSize() };
			psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			psoDesc.DepthStencilState.DepthEnable = FALSE;
			psoDesc.DepthStencilState.StencilEnable = FALSE;
			psoDesc.SampleMask = UINT_MAX;
			psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			psoDesc.NumRenderTargets = 1;
			psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			psoDesc.SampleDesc.Count = 1;
			psoDesc.SampleDesc.Quality = 0;
			DX::ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState)));

		}
		DX::ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), pipelineState.Get(), IID_PPV_ARGS(&commandList)));
		DX::ThrowIfFailed(commandList->Close());
		{
			Vertex triangleVertices[] =
			{
				{{0.0f,0.25f * aspectRatio,0.0f},{1.0f,0.0f,0.0f,1.0f}},
				{{0.25f,-0.25f * aspectRatio,0.0f},{0.0f,1.0f,0.0f,1.0f}},
				{{-0.25f,-0.25f * aspectRatio,0.0f},{0.0f,0.0f,1.0f,1.0f}}

			};
			const UINT vertexBufferSize = sizeof(triangleVertices);
			CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
			auto desc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
			DX::ThrowIfFailed(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexBuffer)));
			UINT8* pVertexDataBegin;
			CD3DX12_RANGE readRange(0, 0);
			DX::ThrowIfFailed(vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
			memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
			vertexBuffer->Unmap(0, nullptr);
			vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
			vertexBufferView.StrideInBytes = sizeof(Vertex);
			vertexBufferView.SizeInBytes = vertexBufferSize;

		}
		{
			DX::ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
			fenceValue = 1;
			fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (fenceEvent == nullptr)
			{
				DX::ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));

			}
			waitForPreviousFrame();
		}
	}
	void onUpdate()
	{

	}
	void onRender()
	{
		populateCommandList();
		ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
		commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
		DX::ThrowIfFailed(mainSwapChain->Present(1, 0));
		waitForPreviousFrame();
	}
	void populateCommandList()
	{
		DX::ThrowIfFailed(commandAllocator->Reset());
		DX::ThrowIfFailed(commandList->Reset(commandAllocator.Get(), pipelineState.Get()));
		commandList->SetGraphicsRootSignature(rootSignature.Get());
		commandList->RSSetViewports(1, &viewPort);
		commandList->RSSetScissorRects(1,&scissorRect);
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		commandList->ResourceBarrier(1, &barrier);
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart(), frameIndex, rtvDescriptorSize);
		commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
		const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
		commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList->DrawInstanced(3, 1, 0, 0);
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		commandList->ResourceBarrier(1, &barrier);

		DX::ThrowIfFailed(commandList->Close());

	}
	void waitForPreviousFrame()
	{
		const UINT64 sFence = fenceValue;
		DX::ThrowIfFailed(commandQueue->Signal(fence.Get(), sFence));
		fenceValue++;

		// Wait until the previous frame is finished.
		if (fence->GetCompletedValue() < sFence)
		{
			DX::ThrowIfFailed(fence->SetEventOnCompletion(sFence, fenceEvent));
			WaitForSingleObject(fenceEvent, INFINITE);
		}

		frameIndex = mainSwapChain->GetCurrentBackBufferIndex();
	}
	void onDestroy()
	{
		waitForPreviousFrame();
		CloseHandle(fenceEvent);
	}
};
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdline, int nCmdShow)
{
	try
	{
		std::make_unique<Renderer>()->run(hInstance,nCmdShow);
	}
	catch (const std::exception& e)
	{
		std::cerr << "ERROR: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}