#include <thread>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <atomic>
#include <crtdbg.h>


#define SOKOL_IMPL
#define SOKOL_D3D11
#define SOKOL_D3D11_SHADER_COMPILER

#include "Defines.h"

extern "C" {
	#include "Sokol/d3d11entry.h"
}

#include "ResManAPI/ResourceManager.h"
#include "ResManAPI/FormatLoaders/PNGLoader.h"
#include "ResManAPI/FormatLoaders/JPGLoader.h"
#include "ResManAPI/FormatLoaders/OBJLoader.h"
#include "ResManAPI/FormatLoaders/RMMeshLoader.h"
#include "ResManAPI/FormatLoaders/RMTextureLoader.h"
#include "Sokol/Objects/Model.h"


#include "ziplib/zip.h"

#include "../MemoryManager/GlutManager.h"
#include "GL/freeglut.h"
#include "../MemoryManager/MemoryManager.h"

struct RenderData {
	hmm_vec3 camCenter;
	hmm_vec4 camEye;
	hmm_vec3 camUp;
	hmm_mat4 proj;
	vs_params_t vsParams;
	sg_pass_action pass_action;
	sg_draw_state drawState;
};

struct ResourceData {
	Marker marker;
	std::vector<Model*> models;
	std::string tempMeshPath;
	std::string tempTexPath;
};

void initMemMngr() {
	std::vector<StackInstance> stackInstances;
	unsigned int megabyte = 1024 * 1024;
	// Single frame stack
	stackInstances.push_back(StackInstance{ megabyte }); // 1 megabyte
	// Persistent stack
	stackInstances.push_back(StackInstance{ megabyte / 512 }); // 1 megabyte
	// Function stack
	stackInstances.push_back(StackInstance{ megabyte }); // 1 megabyte

	std::vector<PoolInstance> poolInstances;
	unsigned int size = 64; // The size of a 4x4 matrix of floats
	// 4 bytes = 32 bit, the architecture of the program (only supports 32-bit right now)
	unsigned int ARCH_BYTESIZE = 4;
	unsigned int numAssignments = ARCH_BYTESIZE * 50;
	unsigned int maxSizeBytes = size * numAssignments;
	poolInstances.push_back(PoolInstance{ size, numAssignments, 4 });

	MemoryManager &memMngr = MemoryManager::getInstance();
	memMngr.init(stackInstances, poolInstances);
}

RenderData initRenderer() {
	RenderData data;

	/* setup d3d11 app wrapper and sokol_gfx */
	const int msaa_samples = 4;
	const int width = 800;
	const int height = 600;
	d3d11_init(width, height, msaa_samples, L"PA2565 Project"); // Roughly 35MB

	sg_desc sgDesc{ 0 };
	sgDesc.d3d11_device = d3d11_device();
	sgDesc.d3d11_device_context = d3d11_device_context();
	sgDesc.d3d11_render_target_view_cb = d3d11_render_target_view;
	sgDesc.d3d11_depth_stencil_view_cb = d3d11_depth_stencil_view;
	sg_setup(&sgDesc);


	std::ifstream file;
	file.open("Assets/VertShader.hlsl", std::ifstream::in);
	std::string vsString;
	if (file.is_open()) {
		std::stringstream stream;

		stream << file.rdbuf();

		vsString = stream.str();
	}
	else RM_DEBUG_MESSAGE("Couldn't open the vertex shader.", 1);
	file.close();

	file.open("Assets/PixelShader.hlsl", std::ifstream::in);
	std::string psString;
	if (file.is_open()) {
		std::stringstream stream;

		stream << file.rdbuf();

		psString = stream.str();
	}
	else RM_DEBUG_MESSAGE("Couldn't open the pixel shader.", 1);
	file.close();


	sg_shader_desc sgsd{ 0 };
	sgsd.vs.uniform_blocks[0].size = sizeof(vs_params_t);
	sgsd.fs.images[0].type = SG_IMAGETYPE_2D;
	sgsd.vs.source = vsString.c_str();
	sgsd.fs.source = psString.c_str();
	/* create shader */
	sg_shader shd = sg_make_shader(&sgsd);

	/* pipeline object */
	sg_pipeline_desc sgpd{ 0 };
	sgpd.layout.attrs[0].sem_name = "POSITION";
	sgpd.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT3;
	sgpd.layout.attrs[1].sem_name = "NORMAL";
	sgpd.layout.attrs[1].sem_index = 1;
	sgpd.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT3;
	sgpd.layout.attrs[2].sem_name = "TEXCOORD";
	sgpd.layout.attrs[2].sem_index = 1;
	sgpd.layout.attrs[2].format = SG_VERTEXFORMAT_FLOAT2;
	sgpd.shader = shd;
	sgpd.index_type = SG_INDEXTYPE_UINT32;
	sgpd.depth_stencil.depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL;
	sgpd.depth_stencil.depth_write_enabled = true;
	sgpd.rasterizer.cull_mode = SG_CULLMODE_NONE;
	sgpd.rasterizer.sample_count = msaa_samples;
	sg_pipeline pip = sg_make_pipeline(&sgpd);

	/* default pass action (clear to gray) */
	data.pass_action = { 0 };
	//sg_color_attachment_action sgcaa{ SG_ACTION_CLEAR, 0.f };
	//pass_action.colors[0] = sgcaa;

	/* view-projection matrix */
	data.proj = HMM_Perspective(60.0f, static_cast<float>(width) / static_cast<float>(height), 0.01f, 1000.0f);
	data.camEye = HMM_Vec4(0.0f, 1.5f, 6.0f, 0.f);
	data.camCenter = HMM_Vec3(0.0f, 0.0f, 0.0f);
	data.camUp = HMM_Vec3(0.0f, 1.0f, 0.0f);

	float rx = 0.0f, ry = 0.0f;

	auto sunDirVec = HMM_Vec4(0.f, -1.f, 0.f, 0.f);
	data.vsParams.sunDir = sunDirVec;

	data.drawState = { 0 };
	data.drawState.pipeline = pip;

	std::srand(std::time(nullptr));

	return data;
}

ResourceData initResMngr() {
	ResourceData rd;
	rd.marker = 0;
	ResourceManager &rm = ResourceManager::getInstance();
	rm.init(1024 * 500000);

	// Roughly 25 bytes per loader (vector with strings of supported formats per formatloader)
	// Gets deleted in the resource manager
	rm.registerFormatLoader(new PNGLoader);
	rm.registerFormatLoader(new JPGLoader);
	rm.registerFormatLoader(new OBJLoader);
	rm.registerFormatLoader(new RMMeshLoader);
	rm.registerFormatLoader(new RMTextureLoader);

	// Load the async temp mesh and texture
	rd.tempMeshPath = "Assets/meshes/tempOBJRes.obj";
	auto tempMesh = ResourceManager::getInstance().load(rd.tempMeshPath.c_str(), false);
	rd.tempTexPath = "Assets/textures/tempJPGRes.jpg";
	auto tempTex = ResourceManager::getInstance().load(rd.tempTexPath.c_str(), false);

	rd.marker = MemoryManager::getInstance().getStackMarker(PERSISTENT_STACK_INDEX);
	return rd;
}

void loadingTests(ResourceData &rd){


	ResourceManager &rm = ResourceManager::getInstance();
	/*
		Testcases
	*/

	/*
	// Support for all supported formats and loading from zip folders
	*/

	rd.marker = MemoryManager::getInstance().getStackMarker(PERSISTENT_STACK_INDEX);

	/*
	* OBJ
	*/
	rd.models.push_back(RM_NEW_PERSISTENT(Model));
	auto start = std::chrono::high_resolution_clock::now();
	// OBJ Loading test
	rd.models.back()->setMesh(reinterpret_cast<MeshResource*>(rm.load("Assets/meshes/teapot.obj", false)));
	rd.models.back()->getTransform().translate(HMM_Vec3(0.f, -2.f, -3.f));
	auto timeTaken = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
	std::string debugMsg = std::string("Loading of teapot.obj took: " + std::to_string(timeTaken) + "ms.");
	RM_DEBUG_MESSAGE(debugMsg, 0);

	rd.models.push_back(RM_NEW_PERSISTENT(Model));
	start = std::chrono::high_resolution_clock::now();
	// OBJ in Zip loading test
	rd.models.back()->setMesh(reinterpret_cast<MeshResource*>(rm.load("Assets/AssetsPackage.zip/AssetsPackage/meshes/teapot.obj", false)));
	rd.models.back()->getTransform().translate(HMM_Vec3(0.f, -100.f, 0.f));
	timeTaken = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
	debugMsg = std::string("Loading of teapot.obj from zip took: " + std::to_string(timeTaken) + "ms.");
	RM_DEBUG_MESSAGE(debugMsg, 0);


	/*
	* RMMesh
	*/
	rd.models.push_back(RM_NEW_PERSISTENT(Model));
	start = std::chrono::high_resolution_clock::now();
	// RMMesh Loading test
	rd.models.back()->setMesh(reinterpret_cast<MeshResource*>(rm.load("Assets/meshes/teapot.rmmesh", false)));
	rd.models.back()->getTransform().translate(HMM_Vec3(0.f, -100.f, 0.f));
	timeTaken = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
	debugMsg = std::string("Loading of teapot.RMMesh took: " + std::to_string(timeTaken) + "ms.");
	RM_DEBUG_MESSAGE(debugMsg, 0);

	rd.models.push_back(RM_NEW_PERSISTENT(Model));
	start = std::chrono::high_resolution_clock::now();
	// RMMesh in Zip loading test
	rd.models.back()->setMesh(reinterpret_cast<MeshResource*>(rm.load("Assets/AssetsPackage.zip/AssetsPackage/meshes/teapot.rmmesh", false)));
	rd.models.back()->getTransform().translate(HMM_Vec3(0.f, -100.f, 0.f));
	timeTaken = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
	debugMsg = std::string("Loading of teapot.RMMesh from zip took: " + std::to_string(timeTaken) + "ms.");
	RM_DEBUG_MESSAGE(debugMsg, 0);


	/*
	* PNG
	*/
	rd.models.push_back(RM_NEW_PERSISTENT(Model));
	start = std::chrono::high_resolution_clock::now();
	// PNG Loading test
	rd.models.back()->setTexture(reinterpret_cast<TextureResource*>(rm.load("Assets/textures/testImage.png", false)));
	rd.models.back()->getTransform().translate(HMM_Vec3(0.f, -100.f, 0.f));
	timeTaken = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
	debugMsg = std::string("Loading of testImage.png took: " + std::to_string(timeTaken) + "ms.");
	RM_DEBUG_MESSAGE(debugMsg, 0);

	rd.models.push_back(RM_NEW_PERSISTENT(Model));
	start = std::chrono::high_resolution_clock::now();
	// PNG in Zip loading test
	rd.models.back()->setTexture(reinterpret_cast<TextureResource*>(rm.load("Assets/AssetsPackage.zip/AssetsPackage/textures/testImage.png", false)));
	rd.models.back()->getTransform().translate(HMM_Vec3(0.f, -100.f, 0.f));
	timeTaken = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
	debugMsg = std::string("Loading of testImage.png from zip took: " + std::to_string(timeTaken) + "ms.");
	RM_DEBUG_MESSAGE(debugMsg, 0);


	/*
	* JPG
	*/
	rd.models.push_back(RM_NEW_PERSISTENT(Model));
	start = std::chrono::high_resolution_clock::now();
	// JPG Loading test
	rd.models.back()->setTexture(reinterpret_cast<TextureResource*>(rm.load("Assets/textures/testImage1.jpg", false)));
	rd.models.back()->getTransform().translate(HMM_Vec3(0.f, -100.f, 0.f));
	timeTaken = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
	debugMsg = std::string("Loading of testImage1.jpg took: " + std::to_string(timeTaken) + "ms.");
	RM_DEBUG_MESSAGE(debugMsg, 0);

	rd.models.push_back(RM_NEW_PERSISTENT(Model));
	start = std::chrono::high_resolution_clock::now();
	// JPG in Zip loading test
	rd.models.back()->setTexture(reinterpret_cast<TextureResource*>(rm.load("Assets/AssetsPackage.zip/AssetsPackage/textures/testImage1.jpg", false)));
	rd.models.back()->getTransform().translate(HMM_Vec3(0.f, -100.f, 0.f));
	timeTaken = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
	debugMsg = std::string("Loading of testImage1.jpg from zip took: " + std::to_string(timeTaken) + "ms.");
	RM_DEBUG_MESSAGE(debugMsg, 0);


	/*
	* RMTex
	*/
	rd.models.push_back(RM_NEW_PERSISTENT(Model));
	start = std::chrono::high_resolution_clock::now();
	// OBJ Loading test
	rd.models.back()->setTexture(reinterpret_cast<TextureResource*>(rm.load("Assets/textures/testImage1.rmtex", false)));
	timeTaken = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
	debugMsg = std::string("Loading of testImage1.rmtex took: " + std::to_string(timeTaken) + "ms.");
	RM_DEBUG_MESSAGE(debugMsg, 0);

	rd.models.push_back(RM_NEW_PERSISTENT(Model));
	start = std::chrono::high_resolution_clock::now();
	// OBJ in Zip loading test
	rd.models.back()->setTexture(reinterpret_cast<TextureResource*>(rm.load("Assets/AssetsPackage.zip/AssetsPackage/textures/testImage1.rmtex", false)));
	timeTaken = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
	debugMsg = std::string("Loading of testImage1.rmtex from zip took: " + std::to_string(timeTaken) + "ms.");
	RM_DEBUG_MESSAGE(debugMsg, 0);

	/*
	*	Thread-safety
	*/
	// Test thread-safety on normal loading
	std::atomic_int aInt = 0;
	int numPtrs = 2;
	std::vector<Resource*> resPtrs(numPtrs);
	auto threadFunc = [&rm, &aInt, &resPtrs](const char* path) {
		resPtrs[aInt++] = rm.load(path, false);
	};
	std::thread t1(threadFunc, "Assets/meshes/teapot.obj");
	std::thread t2(threadFunc, "Assets/meshes/teapot.rmmesh");
	t1.join();
	t2.join();
	rd.models.push_back(RM_NEW_PERSISTENT(Model));
	rd.models.back()->setMeshNoDeref(resPtrs[0]);
	rd.models.back()->getTransform().translate(HMM_Vec3(0.f, 0.f, -15.f));
	rd.models.push_back(RM_NEW_PERSISTENT(Model));
	rd.models.back()->setMeshNoDeref(resPtrs[1]);
	rd.models.back()->getTransform().translate(HMM_Vec3(0.f, -3.f, -15.f));

	// Test thread-safety on async loading
	auto threadAsyncFunc = [&rm, &aInt, &resPtrs](const char* path, std::function<void(Resource*)> callback) {
		rm.asyncLoad(path, callback);
	};
	rd.models.push_back(RM_NEW_PERSISTENT(Model));
	rd.models.back()->getTransform().translate(HMM_Vec3(2.f, 0.f, -15.f));
	std::thread t3(threadAsyncFunc, "Assets/meshes/cow-normals-test.obj", std::bind(&Model::setMeshCallback, rd.models.back(), std::placeholders::_1));
	rd.models.push_back(RM_NEW_PERSISTENT(Model));
	rd.models.back()->getTransform().translate(HMM_Vec3(-2.f, 0.f, -15.f));
	std::thread t4(threadAsyncFunc, "Assets/meshes/cow-normals.obj", std::bind(&Model::setMeshCallback, rd.models.back(), std::placeholders::_1));
	t3.join();
	t4.join();


	// Append a bunch of async asset loading jobs
	rd.models.push_back(RM_NEW_PERSISTENT(Model));
	rd.models.back()->getTransform().translate(HMM_Vec3(2.f, -3.f, -10.f));
	rm.asyncLoad("Assets/meshes/cow-nonormals.obj", std::bind(&Model::setMeshCallback, rd.models.back(), std::placeholders::_1));
	rm.asyncLoad("Assets/textures/testImage.png", std::bind(&Model::setTexCallback, rd.models.back(), std::placeholders::_1));
	rd.models.push_back(RM_NEW_PERSISTENT(Model));
	rd.models.back()->getTransform().translate(HMM_Vec3(-2.f, -3.f, -10.f));
	rm.asyncLoad("Assets/meshes/cow-nonormals.obj", std::bind(&Model::setMeshCallback, rd.models.back(), std::placeholders::_1));
	rm.asyncLoad("Assets/textures/testImage1.jpg", std::bind(&Model::setTexCallback, rd.models.back(), std::placeholders::_1));
	rd.models.push_back(RM_NEW_PERSISTENT(Model));
	rd.models.back()->getTransform().translate(HMM_Vec3(2.f, -7.f, -10.f));
	rm.asyncLoad("Assets/meshes/cow-normals.obj", std::bind(&Model::setMeshCallback, rd.models.back(), std::placeholders::_1));
	rm.asyncLoad("Assets/textures/testImage.png", std::bind(&Model::setTexCallback, rd.models.back(), std::placeholders::_1));

	/*
		End of testcases
	*/

}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
	// Check for memory leaks
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	// Open a debug console window if running in debug mode
//#ifdef _DEBUG
	AllocConsole(); // Roughly 30 MB
	FILE* a;
	freopen_s(&a, "CONIN$", "r", stdin);
	freopen_s(&a, "CONOUT$", "w", stdout);
	freopen_s(&a, "CONOUT$", "w", stderr);
//#endif
	// Initialize renderer(Sokol)
	RenderData renderData = initRenderer();

	// initialize memory manager
	initMemMngr();

	// Initialize the resource manager and register the format loaders to it and create variables for different tests.
	ResourceData resourceData = initResMngr();


	ResourceManager &rm = ResourceManager::getInstance();

	// Additional tests
	//loadingTests(resourceData);

	bool keepRunning = true;


	// Testing scenario
	auto sokolFunc = [&renderData, &resourceData, &rm, &keepRunning]() {
		auto startTime = std::chrono::high_resolution_clock::now();
		while (d3d11_process_events()) {

			/*Extremely simple camera rotation*/
			hmm_mat4 view = HMM_LookAt(HMM_Vec3(renderData.camEye.X, renderData.camEye.Y, renderData.camEye.Z), renderData.camCenter, renderData.camUp);
			hmm_mat4 view_proj = HMM_MultiplyMat4(renderData.proj, view);
			renderData.vsParams.vp = view_proj;
			//camEye = HMM_MultiplyMat4ByVec4(HMM_Rotate(1.f, camUp), camEye);

			/* draw frame */
			sg_begin_default_pass(&renderData.pass_action, d3d11_width(), d3d11_height());

			//sunDir.rotateAroundX(0.3f);
			//vsParams.sunDir = HMM_MultiplyMat4ByVec4(sunDir.getMatrix(), sunDirVec);
			
			if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startTime) > std::chrono::milliseconds(3000)) {
				for (auto m : resourceData.models)
					m->~Model();
				resourceData.models.clear();
				MemoryManager::getInstance().deallocateStack(PERSISTENT_STACK_INDEX, resourceData.marker);
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
				resourceData.models.push_back(RM_NEW_PERSISTENT(Model));
				resourceData.models.back()->getTransform().translate(HMM_Vec3(10.f, -8.f, -20.f));
				rm.asyncLoad("Assets/meshes/teapot.obj", std::bind(&Model::setMeshCallback, resourceData.models.back(), std::placeholders::_1));
				rm.asyncLoad("Assets/textures/testImage1.jpg", std::bind(&Model::setTexCallback, resourceData.models.back(), std::placeholders::_1));

				resourceData.models.push_back(RM_NEW_PERSISTENT(Model));
				resourceData.models.back()->getTransform().translate(HMM_Vec3(-10.f, -8.f, -20.f));
				rm.asyncLoad("Assets/meshes/cow-normals-test.obj", std::bind(&Model::setMeshCallback, resourceData.models.back(), std::placeholders::_1));
				rm.asyncLoad("Assets/textures/testImage.png", std::bind(&Model::setTexCallback, resourceData.models.back(), std::placeholders::_1));

				resourceData.models.push_back(RM_NEW_PERSISTENT(Model));
				resourceData.models.back()->getTransform().translate(HMM_Vec3(0.f, 0.f, -20.f));
				rm.asyncLoad("Assets/meshes/cow-normals-test.obj", std::bind(&Model::setMeshCallback, resourceData.models.back(), std::placeholders::_1));
				rm.asyncLoad("Assets/textures/testfile.jpg", std::bind(&Model::setTexCallback, resourceData.models.back(), std::placeholders::_1));

				resourceData.models.push_back(RM_NEW_PERSISTENT(Model));
				resourceData.models.back()->getTransform().translate(HMM_Vec3(0.f, -17.f, -20.f));
				rm.asyncLoad("Assets/meshes/teapot.obj", std::bind(&Model::setMeshCallback, resourceData.models.back(), std::placeholders::_1));
				rm.asyncLoad("Assets/textures/testImage.png", std::bind(&Model::setTexCallback, resourceData.models.back(), std::placeholders::_1));

				startTime = std::chrono::high_resolution_clock::now();
			}
			float index = 0.1f;
			for (auto model : resourceData.models) {
				model->getTransform().rotateAroundY(std::rand() % 10);
				model->draw(renderData.drawState, renderData.vsParams);
			}


			sg_end_pass();
			sg_commit();
			d3d11_present();
		}

		for (auto m : resourceData.models) {
			m->~Model();
		}

		rm.cleanup();
		sg_shutdown();
		d3d11_shutdown();
		glutLeaveMainLoop();
	};

	//MemoryTracking thread
	auto allocatedSpaceUpdateFunction = [&keepRunning]() {
		GlutManager& glutMngr = GlutManager::getInstance();
		MemoryManager& memMngr = MemoryManager::getInstance();

		auto allocatedSpace = memMngr.getAllocatedSpace();
		std::vector<std::vector<bool>> stacks(1);
		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		// Update the vectors of the GUI
		while (keepRunning) {
			auto allocatedSpace = memMngr.getAllocatedSpace();
			glutMngr.updateVectors(allocatedSpace.stacks, allocatedSpace.pools);
			memMngr.updateAllocatedSpace();
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		}
	};

	std::thread sokolThread(sokolFunc);
	std::thread memoryTrackingThread(allocatedSpaceUpdateFunction);

	GlutManager& glutMngr = GlutManager::getInstance();
	glutMngr.EnterMainLoop();

	keepRunning = false;
	
	memoryTrackingThread.join();
	sokolThread.join();

	MemoryManager::getInstance().cleanUp();

	return 0;
}
