#include <thread>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <atomic>
#include <crtdbg.h>
#include <experimental/filesystem>


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

#include "../ImGui/imgui.h"

#include "ziplib/zip.h"

#include "../MemoryManager/MemoryManager.h"

void imgui_draw_cb(ImDrawData*);

struct RenderData {
	hmm_vec3 camCenter;
	hmm_vec4 camEye;
	hmm_vec3 camUp;
	hmm_mat4 proj;
	Transform sunDir;
	hmm_vec4 sunDirVec;
	vs_params_t vsParams;
	sg_pass_action pass_action;
	sg_draw_state drawState;
};

struct IMGUIRenderData {
	sg_draw_state drawState;
	sg_pass_action passAction;
};
IMGUIRenderData g_imguiRenderData;

struct ResourceData {
	Marker marker;
	std::vector<Model*> models;
	std::string tempMeshPath;
	std::string tempTexPath;
};

struct vs_params_imgui {
	ImVec2 disp_size;
};

void initMemMngr() {
	std::vector<StackInstance> stackInstances;
	unsigned int megabyte = 1024 * 1024;
	// Single frame stack
	stackInstances.push_back(StackInstance{ megabyte }); // 1 megabyte
	// Persistent stack
	stackInstances.push_back(StackInstance{ megabyte / 512 }); // 1 megabyte
	// Function stack
	stackInstances.push_back(StackInstance{ megabyte * 128 }); // 1 megabyte

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

const int sokol_width = 1280;
const int sokol_height = 720;

RenderData initRenderer() {
	RenderData data;

	/* setup d3d11 app wrapper and sokol_gfx */
	const int msaa_samples = 4;
	d3d11_init(sokol_width, sokol_height, msaa_samples, L"PA2565 Project"); // Roughly 35MB

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
	data.proj = HMM_Perspective(60.0f, static_cast<float>(sokol_width) / static_cast<float>(sokol_height), 0.01f, 1000.0f);
	data.camEye = HMM_Vec4(0.0f, 1.5f, 6.0f, 0.f);
	data.camCenter = HMM_Vec3(0.0f, 0.0f, 0.0f);
	data.camUp = HMM_Vec3(0.0f, 1.0f, 0.0f);

	float rx = 0.0f, ry = 0.0f;

	data.sunDirVec = HMM_Vec4(0.f, 1.f, 0.f, 1.f);
	data.vsParams.sunDir = data.sunDirVec;

	data.drawState = { 0 };
	data.drawState.pipeline = pip;

	std::srand(std::time(nullptr));

	data.sunDir = Transform();

	return data;
}

ResourceData initResMngr() {
	ResourceData rd;
	rd.marker = 0;
	ResourceManager &rm = ResourceManager::getInstance();
	rm.init(10);

	// Roughly 25 bytes per loader (vector with strings of supported formats per formatloader)
	// Gets deleted in the resource manager
	rm.registerFormatLoader(new PNGLoader);
	rm.registerFormatLoader(new JPGLoader);
	rm.registerFormatLoader(new OBJLoader);
	rm.registerFormatLoader(new RMMeshLoader);
	rm.registerFormatLoader(new RMTextureLoader);

	// Load the async temp mesh and texture
	rd.tempMeshPath = "Assets/meshes/tempOBJRes.obj";
	auto tempMesh = ResourceManager::getInstance().load(rd.tempMeshPath.c_str());
	rd.tempTexPath = "Assets/textures/tempJPGRes.jpg";
	auto tempTex = ResourceManager::getInstance().load(rd.tempTexPath.c_str());

	rd.marker = MemoryManager::getInstance().getStackMarker(PERSISTENT_STACK_INDEX);
	return rd;
}

void initIMGUIRenderData() {
	// input forwarding
	d3d11_mouse_pos([](float x, float y) { ImGui::GetIO().MousePos = ImVec2(x, y); });
	d3d11_mouse_btn_down([](int btn) { ImGui::GetIO().MouseDown[btn] = true; });
	d3d11_mouse_btn_up([](int btn) { ImGui::GetIO().MouseDown[btn] = false; });
	d3d11_mouse_wheel([](float v) { ImGui::GetIO().MouseWheel = v; });
	d3d11_char([](wchar_t c) { ImGui::GetIO().AddInputCharacter(c); });
	d3d11_key_down([](int key) { if (key < 512) ImGui::GetIO().KeysDown[key] = true; });
	d3d11_key_up([](int key) { if (key < 512) ImGui::GetIO().KeysDown[key] = false; });

	// setup Dear Imgui 
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr;
	io.RenderDrawListsFn = imgui_draw_cb;
	io.Fonts->AddFontDefault();
	io.KeyMap[ImGuiKey_Tab] = VK_TAB;
	io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
	io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
	io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
	io.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
	io.KeyMap[ImGuiKey_Home] = VK_HOME;
	io.KeyMap[ImGuiKey_End] = VK_END;
	io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
	io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
	io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
	io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
	io.KeyMap[ImGuiKey_A] = 'A';
	io.KeyMap[ImGuiKey_C] = 'C';
	io.KeyMap[ImGuiKey_V] = 'V';
	io.KeyMap[ImGuiKey_X] = 'X';
	io.KeyMap[ImGuiKey_Y] = 'Y';
	io.KeyMap[ImGuiKey_Z] = 'Z';

	const int MaxVertices = (1 << 16);
	const int MaxIndices = MaxVertices * 3;

	// dynamic vertex- and index-buffers for imgui-generated geometry
	sg_buffer_desc vbuf_desc = { };
	vbuf_desc.usage = SG_USAGE_STREAM;
	vbuf_desc.size = MaxVertices * sizeof(ImDrawVert);
	g_imguiRenderData.drawState.vertex_buffers[0] = sg_make_buffer(&vbuf_desc);

	sg_buffer_desc ibuf_desc = { };
	ibuf_desc.type = SG_BUFFERTYPE_INDEXBUFFER;
	ibuf_desc.usage = SG_USAGE_STREAM;
	ibuf_desc.size = MaxIndices * sizeof(ImDrawIdx);
	g_imguiRenderData.drawState.index_buffer = sg_make_buffer(&ibuf_desc);

	// font texture for imgui's default font
	unsigned char* font_pixels;
	int font_width, font_height;
	io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);
	sg_image_desc img_desc = { };
	img_desc.width = font_width;
	img_desc.height = font_height;
	img_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
	img_desc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
	img_desc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
	img_desc.content.subimage[0][0].ptr = font_pixels;
	img_desc.content.subimage[0][0].size = font_width * font_height * 4;
	g_imguiRenderData.drawState.fs_images[0] = sg_make_image(&img_desc);

	// shader object for imgui rendering
	sg_shader_desc shd_desc = { };
	auto& ub = shd_desc.vs.uniform_blocks[0];
	ub.size = sizeof(vs_params_imgui);
	shd_desc.vs.source =
		"cbuffer params {\n"
		"  float2 disp_size;\n"
		"};\n"
		"struct vs_in {\n"
		"  float2 pos: POSITION;\n"
		"  float2 uv: TEXCOORD0;\n"
		"  float4 color: COLOR0;\n"
		"};\n"
		"struct vs_out {\n"
		"  float2 uv: TEXCOORD0;\n"
		"  float4 color: COLOR0;\n"
		"  float4 pos: SV_Position;\n"
		"};\n"
		"vs_out main(vs_in inp) {\n"
		"  vs_out outp;\n"
		"  outp.pos = float4(((inp.pos/disp_size)-0.5)*float2(2.0,-2.0), 0.5, 1.0);\n"
		"  outp.uv = inp.uv;\n"
		"  outp.color = inp.color;\n"
		"  return outp;\n"
		"}\n";
	shd_desc.fs.images[0].type = SG_IMAGETYPE_2D;
	shd_desc.fs.source =
		"Texture2D<float4> tex: register(t0);\n"
		"sampler smp: register(s0);\n"
		"float4 main(float2 uv: TEXCOORD0, float4 color: COLOR0): SV_Target0 {\n"
		"  return tex.Sample(smp, uv) * color;\n"
		"}\n";
	sg_shader shd = sg_make_shader(&shd_desc);

	// pipeline object for imgui rendering
	sg_pipeline_desc pip_desc = { };
	pip_desc.layout.buffers[0].stride = sizeof(ImDrawVert);
	auto& attrs = pip_desc.layout.attrs;
	attrs[0].sem_name = "POSITION"; attrs[0].offset = offsetof(ImDrawVert, pos); attrs[0].format = SG_VERTEXFORMAT_FLOAT2;
	attrs[1].sem_name = "TEXCOORD"; attrs[1].offset = offsetof(ImDrawVert, uv); attrs[1].format = SG_VERTEXFORMAT_FLOAT2;
	attrs[2].sem_name = "COLOR"; attrs[2].offset = offsetof(ImDrawVert, col); attrs[2].format = SG_VERTEXFORMAT_UBYTE4N;
	pip_desc.shader = shd;
	pip_desc.index_type = SG_INDEXTYPE_UINT16;
	pip_desc.blend.enabled = true;
	pip_desc.blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
	pip_desc.blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
	pip_desc.blend.color_write_mask = SG_COLORMASK_RGB;
	g_imguiRenderData.drawState.pipeline = sg_make_pipeline(&pip_desc);


	// initial clear color
	g_imguiRenderData.passAction.colors[0].action = SG_ACTION_CLEAR;
	g_imguiRenderData.passAction.colors[0].val[0] = 0.0f;
	g_imguiRenderData.passAction.colors[0].val[1] = 0.5f;
	g_imguiRenderData.passAction.colors[0].val[2] = 0.7f;
	g_imguiRenderData.passAction.colors[0].val[3] = 1.0f;
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
	initIMGUIRenderData();

	// initialize memory manager
	initMemMngr();

	// Initialize the resource manager and register the format loaders to it and create variables for different tests.
	ResourceData resourceData = initResMngr();

	ResourceManager &rm = ResourceManager::getInstance();

	bool keepRunning = true;

	/*
		SOKOL RENDERING
	*/
	/* IMGUI */
	bool show_test_window = true;
	bool show_another_window = false;
	/* END OF IMGUI */

	auto startTime = std::chrono::high_resolution_clock::now();
	std::vector<ResourceManager::AsyncJobIndex> activeJobs;
	while (d3d11_process_events()) {


		/*
		*	IMGUI START
		*/

		const int cur_width = d3d11_width();
		const int cur_height = d3d11_height();

		// this is standard ImGui demo code
		ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize = ImVec2(float(cur_width), float(cur_height));
		//io.DeltaTime = (float)stm_sec(stm_laptime(&last_time));
		ImGui::NewFrame();

		// 1. Show a simple window
		// Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
		static float f = 0.0f;
		ImGui::Text("Hello, world!");
		ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
		ImGui::ColorEdit3("clear color", &renderData.pass_action.colors[0].val[0]);
		if (ImGui::Button("Test Window")) show_test_window ^= 1;
		if (ImGui::Button("Another Window")) show_another_window ^= 1;
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

		auto resources = rm.getResources();
		std::string resString = "";
		for (auto res : resources) {
			//std::experimental::filesystem::path path(res.second->getPath());
			resString.append("GUID: ");
			resString.append(std::to_string(res.second->getGUID()));
			resString.append("  Path: ");
			resString.append(res.second->getPath());
			resString.append("\n");
		}

		// 2. Show another simple window, this time using an explicit Begin/End pair
		ImGui::SetNextWindowSize(ImVec2(200, 100), ImGuiSetCond_FirstUseEver);
		ImGui::Begin("Another Window", &show_another_window);
		ImGui::Text(resString.c_str());
		ImGui::End();

		/*
		*	IMGUI END
		*/



		/*Extremely simple camera rotation*/
		hmm_mat4 view = HMM_LookAt(HMM_Vec3(renderData.camEye.X, renderData.camEye.Y, renderData.camEye.Z), renderData.camCenter, renderData.camUp);
		hmm_mat4 view_proj = HMM_MultiplyMat4(renderData.proj, view);
		renderData.vsParams.vp = view_proj;

		/* draw frame */
		sg_begin_default_pass(&renderData.pass_action, d3d11_width(), d3d11_height());

		if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startTime) > std::chrono::milliseconds(10000)) {

			RM_DEBUG_MESSAGE("----------CLEARING----------", 0);

			rm.clearResourceManager();
			activeJobs.clear();

			for (auto m : resourceData.models)
				m->~Model();
			resourceData.models.clear();

			RM_DEBUG_MESSAGE("----------DONE CLEARING----------", 0);
			RM_DEBUG_MESSAGE("----------INIT----------", 0);

			MemoryManager::getInstance().deallocateStack(PERSISTENT_STACK_INDEX, resourceData.marker);
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			resourceData.models.push_back(RM_NEW_PERSISTENT(Model));
			resourceData.models.back()->getTransform().translate(HMM_Vec3(10.f, -8.f, -20.f));
			resourceData.models.back()->getTransform().rotateAroundX(static_cast<float>((std::rand() % 180)));
			resourceData.models.back()->getTransform().rotateAroundY(static_cast<float>((std::rand() % 180)));
			resourceData.models.back()->getTransform().rotateAroundZ(static_cast<float>((std::rand() % 180)));
			activeJobs.push_back(rm.asyncLoad("Assets/meshes/teapot.obj", std::bind(&Model::setMeshCallback, resourceData.models.back(), std::placeholders::_1)));
			activeJobs.push_back(rm.asyncLoad("Assets/textures/testImage1.jpg", std::bind(&Model::setTexCallback, resourceData.models.back(), std::placeholders::_1)));

			resourceData.models.push_back(RM_NEW_PERSISTENT(Model));
			resourceData.models.back()->getTransform().translate(HMM_Vec3(-10.f, -8.f, -20.f));
			resourceData.models.back()->getTransform().rotateAroundX(static_cast<float>((std::rand() % 180)));
			resourceData.models.back()->getTransform().rotateAroundY(static_cast<float>((std::rand() % 180)));
			resourceData.models.back()->getTransform().rotateAroundZ(static_cast<float>((std::rand() % 180)));
			activeJobs.push_back(rm.asyncLoad("Assets/meshes/teapot.obj", std::bind(&Model::setMeshCallback, resourceData.models.back(), std::placeholders::_1)));
			activeJobs.push_back(rm.asyncLoad("Assets/textures/testImage.png", std::bind(&Model::setTexCallback, resourceData.models.back(), std::placeholders::_1)));

			resourceData.models.push_back(RM_NEW_PERSISTENT(Model));
			resourceData.models.back()->getTransform().translate(HMM_Vec3(0.f, 0.f, -20.f));
			resourceData.models.back()->getTransform().rotateAroundX(static_cast<float>((std::rand() % 180)));
			resourceData.models.back()->getTransform().rotateAroundY(static_cast<float>((std::rand() % 180)));
			resourceData.models.back()->getTransform().rotateAroundZ(static_cast<float>((std::rand() % 180)));
			activeJobs.push_back(rm.asyncLoad("Assets/meshes/cow-normals-test.obj", std::bind(&Model::setMeshCallback, resourceData.models.back(), std::placeholders::_1)));
			activeJobs.push_back(rm.asyncLoad("Assets/textures/testfile.jpg", std::bind(&Model::setTexCallback, resourceData.models.back(), std::placeholders::_1)));

			resourceData.models.push_back(RM_NEW_PERSISTENT(Model));
			resourceData.models.back()->getTransform().translate(HMM_Vec3(0.f, -20.f, -40.f));
			resourceData.models.back()->getTransform().setScale(HMM_Vec3(4.f, 4.f, 4.f));
			resourceData.models.back()->getTransform().rotateAroundY(65.f);
			activeJobs.push_back(rm.asyncLoad("Assets/meshes/shelbyEdited.obj", std::bind(&Model::setMeshCallback, resourceData.models.back(), std::placeholders::_1)));

			RM_DEBUG_MESSAGE("----------DONE INIT----------", 0);

			startTime = std::chrono::high_resolution_clock::now();
		}
		float index = 0.1f;
		for (auto model : resourceData.models) {
			model->getTransform().rotateAroundY(2.f);
			model->draw(renderData.drawState, renderData.vsParams);
		}


		ImGui::Render();
		sg_end_pass();

		sg_commit();
		d3d11_present();
	}

	for (auto job : activeJobs)
		rm.removeAsyncJob(job);

	for (auto m : resourceData.models) {
		m->~Model();
	}

	rm.cleanup();
	ImGui::DestroyContext();
	sg_shutdown();
	d3d11_shutdown();

	/*
		END OF SOKOL RENDERING
	*/

	keepRunning = false;

	MemoryManager::getInstance().cleanUp();

	return 0;
}


// imgui draw callback
void imgui_draw_cb(ImDrawData* draw_data) {
	assert(draw_data);
	if (draw_data->CmdListsCount == 0) {
		return;
	}

	// render the command list
	vs_params_imgui vs_params;
	vs_params.disp_size.x = ImGui::GetIO().DisplaySize.x;
	vs_params.disp_size.y = ImGui::GetIO().DisplaySize.y;
	for (int cl_index = 0; cl_index < draw_data->CmdListsCount; cl_index++) {
		const ImDrawList* cl = draw_data->CmdLists[cl_index];

		// append vertices and indices to buffers, record start offsets in draw state
		const int vtx_size = cl->VtxBuffer.size() * sizeof(ImDrawVert);
		const int idx_size = cl->IdxBuffer.size() * sizeof(ImDrawIdx);
		const int vb_offset = sg_append_buffer(g_imguiRenderData.drawState.vertex_buffers[0], &cl->VtxBuffer.front(), vtx_size);
		const int ib_offset = sg_append_buffer(g_imguiRenderData.drawState.index_buffer, &cl->IdxBuffer.front(), idx_size);
		/* don't render anything if the buffer is in overflow state (this is also
			checked internally in sokol_gfx, draw calls that attempt from
			overflowed buffers will be silently dropped)
		*/
		if (sg_query_buffer_overflow(g_imguiRenderData.drawState.vertex_buffers[0]) ||
			sg_query_buffer_overflow(g_imguiRenderData.drawState.index_buffer))
		{
			continue;
		}

		g_imguiRenderData.drawState.vertex_buffer_offsets[0] = vb_offset;
		g_imguiRenderData.drawState.index_buffer_offset = ib_offset;
		sg_apply_draw_state(&g_imguiRenderData.drawState);
		sg_apply_uniform_block(SG_SHADERSTAGE_VS, 0, &vs_params, sizeof(vs_params));

		int base_element = 0;
		for (const ImDrawCmd& pcmd : cl->CmdBuffer) {
			if (pcmd.UserCallback) {
				pcmd.UserCallback(cl, &pcmd);
			}
			else {
				const int scissor_x = (int)(pcmd.ClipRect.x);
				const int scissor_y = (int)(pcmd.ClipRect.y);
				const int scissor_w = (int)(pcmd.ClipRect.z - pcmd.ClipRect.x);
				const int scissor_h = (int)(pcmd.ClipRect.w - pcmd.ClipRect.y);
				sg_apply_scissor_rect(scissor_x, scissor_y, scissor_w, scissor_h, true);
				sg_draw(base_element, pcmd.ElemCount, 1);
			}
			base_element += pcmd.ElemCount;
		}
	}
}