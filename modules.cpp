#include "modules.h"
#include <Windows.h>
#include <iostream>
#include "mem.h"
#include "offsets.h"
#include "config.h"
#include "geom.h"
#include "numbers"
#include "chrono"
#include <algorithm>
#include "directx.h"
#include "imgui.h"
#include "math.h"
#include "backends/imgui_impl_dx9.h"
#include "backends/imgui_impl_win32.h"
#include "sdk/csgotrace.h"
#include "sdk/csgoVector.h"
#include "sdk/playerinfo.h"
#include "sdk/createMove.h"

typedef HRESULT(APIENTRY* tEndScene)(LPDIRECT3DDEVICE9 pDevice);
tEndScene oEndScene = nullptr;
LPDIRECT3DDEVICE9 pDevice;
LPD3DXFONT pFont;

typedef void* (__cdecl* tCreateInterface)(const char* name, int* returnCode);

typedef void(__thiscall* tLockCursor)(void* x);
tLockCursor oLockCursor;

typedef bool(__stdcall* CreateMove)(void*, float, UserCmd*);
CreateMove createMove;

static void* g_Client = nullptr;
static void* g_ClientMode = nullptr;

void* d3d9Device[119];
uintptr_t LockCursorAddr;
BYTE CreateMoveBytes[7]{ 0 };
BYTE EndSceneBytes[7]{ 0 };
BYTE LockCursorBytes[7]{ 0 };

float bestYaw = 0, bestPitch = 0;
ent* closestEntity;
HWND window;
int wndWidth, wndHeight;
bool uninjecting = false;

void* GetInterface(const wchar_t* moduleName, const char* interfaceName) {
	tCreateInterface CreateInterface = (tCreateInterface)GetProcAddress(GetModuleHandle(moduleName), "CreateInterface");

	int returnCode = 0;
	void* newInterface = CreateInterface(interfaceName, &returnCode);

	return newInterface;
}

HWND GetProcessWindow() {
	window = NULL;

	window = FindWindowA(NULL, "Counter-Strike: Global Offensive - Direct3D 9");

	RECT size;
	GetClientRect(window, &size);
	wndWidth = size.right - size.left;
	wndHeight = size.bottom - size.top;

	return window;
}

Vector3 GetBonePos(ent* entity, int boneNum) {
	Vector3 bonePos;
	bonePos.x = *(float*)(entity->boneMatrix + 0x30 * boneNum + 0x0C);
	bonePos.y = *(float*)(entity->boneMatrix + 0x30 * boneNum + 0x1C);
	bonePos.z = *(float*)(entity->boneMatrix + 0x30 * boneNum + 0x2C);
	return bonePos;
}

bool GetD3D9Device(void** pTable, size_t size) {
	if (!pTable) return false;

	IDirect3D9* pD3D = Direct3DCreate9(D3D_SDK_VERSION);
	if (!pD3D) return false;

	IDirect3DDevice9* pDummyDevice = nullptr;

	D3DPRESENT_PARAMETERS d3dpp = {};
	d3dpp.Windowed = true;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.hDeviceWindow = GetProcessWindow();

	HRESULT dummyDevCreated = pD3D->CreateDevice(!D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3dpp.hDeviceWindow, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDummyDevice);
	if (dummyDevCreated != S_OK) {
		d3dpp.Windowed = !d3dpp.Windowed;
		HRESULT dummyDevCreated = pD3D->CreateDevice(!D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3dpp.hDeviceWindow, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDummyDevice);

		if (dummyDevCreated != S_OK) {
			pD3D->Release();
			return false;
		}
	}

	memcpy(pTable, *(void***)(pDummyDevice), size);
	pDummyDevice->Release();
	pD3D->Release();
	return true;
}

bool isVisible(ent* entity) {
	IEngineTrace* traceInterface = (IEngineTrace*)GetInterface(L"engine.dll", "EngineTraceClient004");
	void* vtableIndex = (*reinterpret_cast<void***>(traceInterface))[5];
	uintptr_t clientBase = (uintptr_t)GetModuleHandle(L"client.dll");
	ent* dwLocalPlayer = *(ent**)(clientBase + 0x4E051DC);

	Vector3 start = dwLocalPlayer->bodypos + dwLocalPlayer->camPos;
	Vector3 target = entity->bodypos + entity->camPos;

	vec3 startPos = { start.x, start.y, start.z };
	vec3 targetPos = { target.x, target.y, target.z };

	CGameTrace trace;
	Ray_t ray;
	CTraceFilter filter;
	filter.pSkip = (void*)dwLocalPlayer;

	ray.Init(startPos, targetPos);

	traceInterface->TraceRay(ray, MASK_SHOT | CONTENTS_GRATE, &filter, &trace);

	if (trace.fraction >= 0.99f)
		return true;

	return (void*)entity == trace.hit_entity;
}

bool __stdcall hkCreateMove(float frameTime, UserCmd* cmd) {
	uintptr_t clientBase = (uintptr_t)GetModuleHandle(L"client.dll");
	uintptr_t engineBase = (uintptr_t)GetModuleHandle(L"engine.dll");
	ent* dwLocalPlayer = *(ent**)(clientBase + 0x4E051DC);

	if (Config::bSilentAim && closestEntity && (cmd->buttons & IN_ATTACK)) {

		if (Config::bRcs) {
			Vector3 newAngles;
			newAngles.y = bestYaw;
			newAngles.x = bestPitch;

			Vector3 punchAngle = dwLocalPlayer->aimPunch * 2;
			if (dwLocalPlayer->shotsFired > 1) {
				newAngles = newAngles - punchAngle;
				newAngles.normalize();
			}

			cmd->viewPoint.y = newAngles.y;
			cmd->viewPoint.x = newAngles.x;
		}
		else {
			cmd->viewPoint.y = bestYaw;
			cmd->viewPoint.x = bestPitch;
		}
	}

	if (Config::bStrafe) {
		if (!(dwLocalPlayer->flags & (1 << 0)) && GetAsyncKeyState(VK_SPACE) & 0x8000) {

			if (cmd->mouseDeltaX > 0)
				cmd->sideMove = 450.0f;

			if (cmd->mouseDeltaX < 0)
				cmd->sideMove = -450.0f;
		}
	}

	return false;
}


void APIENTRY hkEndScene(LPDIRECT3DDEVICE9 o_pDevice) {
	if (uninjecting) return;
	uintptr_t clientBase = (uintptr_t)GetModuleHandle(L"client.dll");
	uintptr_t engineBase = (uintptr_t)GetModuleHandle(L"engine.dll");
	uintptr_t serverBase = (uintptr_t)GetModuleHandle(L"server.dll");
	ent* dwLocalPlayer = *(ent**)(clientBase + 0x4E051DC);
	uintptr_t entList = (uintptr_t)(clientBase + 0x4E051DC);
	int* currPlayers = (int*)(serverBase + 0xB61618);
	float* viewMatrix = (float*)(clientBase + 0x4DF6024);

	if (!pDevice) {
		pDevice = o_pDevice;

		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();

		GetProcessWindow();
		ImGui_ImplWin32_Init(window);
		ImGui_ImplDX9_Init(pDevice);
	}

	GetProcessWindow();

	ImGui_ImplWin32_NewFrame();
	ImGui_ImplDX9_NewFrame();
	ImGui::NewFrame();

	ImGui::SetNextWindowSize(ImVec2(400, 300));

	if (dwLocalPlayer) {
		float closestBoneDistance = FLT_MAX;

		if (Config::bClosestBone && closestEntity) {
			const int bones[] = { 8,7,6,4 };

			for (int bone : bones) {
				Vector3 bonePos = GetBonePos(closestEntity, bone);
				Vector2 screenPos;
				
				if (DirectX::WorldToScreen(bonePos, screenPos, viewMatrix)) {
					float dx = screenPos.x - wndWidth / 2;
					float dy = screenPos.y - wndHeight / 2;
					float delta = sqrt(dx * dx + dy * dy);

					if (delta < closestBoneDistance) {
						closestBoneDistance = delta;
						Config::boneNum = bone;
					}
				}
			}
		}

		for (int i = 1; i < *currPlayers; i++) {
			ent* entity = *(ent**)(entList + i * 0x10);

			if (!entity) continue;
			if (entity->health == 0) continue;
			if (entity->teamNum == dwLocalPlayer->teamNum) continue;
			if (entity == dwLocalPlayer) continue;

			D3DCOLOR color = (entity->bDormant ? D3DCOLOR_RGBA(150, 150, 150, 150) : D3DCOLOR_RGBA(250, 250, 250, 250));
			
			if (Config::bEsp) {
				Vector2 headScreenPos, originScreenPos;

				Vector3 newHead = entity->bodypos;
				newHead.z = entity->bodypos.z + 75.0f;

				if (DirectX::WorldToScreen(newHead, headScreenPos, viewMatrix) && DirectX::WorldToScreen(entity->origin, originScreenPos, viewMatrix)) {
					DirectX::DrawESPBox(originScreenPos, headScreenPos, entity, dwLocalPlayer, color);
				}
			}

			if (Config::bNames) {
				IVEngineClient* engine = (IVEngineClient*)GetInterface(L"engine.dll", "VEngineClient014");

				PlayerInfo_t playerInfo;
				Vector3 newHead = entity->bodypos;
				newHead.z = entity->bodypos.z + 75.0f;

				Vector2 headScreen, originScreen;
				
				if (engine->GetPlayerInfo(i + 1, &playerInfo)) {
					if (DirectX::WorldToScreen(newHead, headScreen, viewMatrix) && DirectX::WorldToScreen(entity->origin, originScreen, viewMatrix)) {

						float height = originScreen.y - headScreen.y;
						float width = height / 4.0f;

						DirectX::IngameText(originScreen.x - width, originScreen.y, color, playerInfo.szName);
					}
				}
			}

			if (Config::bDistance) {
				Vector3 newHead = entity->bodypos;
				newHead.z = entity->bodypos.z + 75.0f;

				Vector2 headScreen, originScreen;

				if (DirectX::WorldToScreen(newHead, headScreen, viewMatrix) && DirectX::WorldToScreen(entity->origin, originScreen, viewMatrix)) {
					float height = originScreen.y - headScreen.y;
					float width = height / 4.0f;
					
					int distance = entity->bodypos.getDistance(dwLocalPlayer->bodypos);
					int fixedDistance = entity->bodypos.getDistance(dwLocalPlayer->bodypos) * 0.01905f;
					std::string distanceStr = std::to_string(fixedDistance);
					const char* distanceChar = distanceStr.c_str();

					DirectX::IngameText(originScreen.x + width, originScreen.y, color, distanceChar);
				}
			}

			if (Config::bHealthbar && entity->health > 0) {
				Vector3 newHead = entity->bodypos;
				newHead.z = entity->bodypos.z + 75.0f;
				Vector2 headScreen;
				Vector2 originScreen;

				if (DirectX::WorldToScreen(newHead, headScreen, viewMatrix) && DirectX::WorldToScreen(entity->origin, originScreen, viewMatrix)) {
					float height = originScreen.y - headScreen.y;
					float width = height / 4.0f;
					float distance = dwLocalPlayer->bodypos.getDistance(entity->bodypos);
					float newHeight = height * ((float)entity->health / 100.0f);
					float offset = 6.0f / (1.0f + distance * 0.0005f);
					DirectX::DrawOutline(originScreen.x + width + offset, originScreen.y, originScreen.x + width + offset, originScreen.y - newHeight, 1, color);
				}
			}

			if (Config::bEspSnap) {
				Vector2 screenCoords;

				if (DirectX::WorldToScreen(entity->bodypos, screenCoords, viewMatrix)) {
					DirectX::DrawOutline(wndWidth / 2, wndHeight, screenCoords.x, screenCoords.y, 1, D3DCOLOR_RGBA(255, 255, 255, 255));
				}
			}

			if (Config::bSnaplines && Config::bSilentAim && closestEntity) {
				Vector2 screenCoords;

				Vector3 pos = GetBonePos(closestEntity, Config::boneNum);

				if (DirectX::WorldToScreen(pos, screenCoords, viewMatrix)) {
					DirectX::DrawOutline(wndWidth / 2, wndHeight / 2, screenCoords.x, screenCoords.y, 1, D3DCOLOR_RGBA(255, 255, 255, 255));
				}
			}

			if (Config::bSkeleton) {
				Vector3 headPos, neckPos, leftShoulder, rightShoulder, leftElbow, rightElbow, leftHand, rightHand, pelvis, preKneeLeft, preKneeRight, kneeLeft, kneeRight, preFeetLeft, preFeetRight, feetLeft, feetRight;

				headPos = GetBonePos(entity, 8);
				neckPos = GetBonePos(entity, 7);
				leftShoulder = GetBonePos(entity, 11);
				leftElbow = GetBonePos(entity, 12);
				leftHand = GetBonePos(entity, 13);
				pelvis = GetBonePos(entity, 3);

				if (entity->teamNum == 2) { //2T 3CT
					rightShoulder = GetBonePos(entity, 39);
					rightElbow = GetBonePos(entity, 40);
					rightHand = GetBonePos(entity, 41);
					preKneeLeft = GetBonePos(entity, 66);
					kneeLeft = GetBonePos(entity, 67);
					preFeetLeft = GetBonePos(entity, 68);
					feetLeft = GetBonePos(entity, 69);
					preKneeRight = GetBonePos(entity, 73);
					kneeRight = GetBonePos(entity, 74);
					preFeetRight = GetBonePos(entity, 75);
					feetRight = GetBonePos(entity, 76);
				}
				else {
					rightShoulder = GetBonePos(entity, 41);
					rightElbow = GetBonePos(entity, 42);
					rightHand = GetBonePos(entity, 43);
					preKneeLeft = GetBonePos(entity, 70);
					kneeLeft = GetBonePos(entity, 71);
					preFeetLeft = GetBonePos(entity, 72);
					feetLeft = GetBonePos(entity, 73);
					preKneeRight = GetBonePos(entity, 77);
					kneeRight = GetBonePos(entity, 78);
					preFeetRight = GetBonePos(entity, 79);
					feetRight = GetBonePos(entity, 80);
				}

				DirectX::DrawSkeleton(headPos, neckPos, viewMatrix, color);
				DirectX::DrawSkeleton(neckPos, leftShoulder, viewMatrix, color);
				DirectX::DrawSkeleton(neckPos, rightShoulder, viewMatrix, color);
				DirectX::DrawSkeleton(leftShoulder, leftElbow, viewMatrix, color);
				DirectX::DrawSkeleton(leftElbow, leftHand, viewMatrix, color);
				DirectX::DrawSkeleton(rightShoulder, rightElbow, viewMatrix, color);
				DirectX::DrawSkeleton(rightElbow, rightHand, viewMatrix, color);
				DirectX::DrawSkeleton(neckPos, pelvis, viewMatrix, color);
				DirectX::DrawSkeleton(pelvis, preKneeLeft, viewMatrix, color);
				DirectX::DrawSkeleton(pelvis, preKneeRight, viewMatrix, color);
				DirectX::DrawSkeleton(preKneeLeft, kneeLeft, viewMatrix, color);
				DirectX::DrawSkeleton(kneeLeft, preFeetLeft, viewMatrix, color);
				DirectX::DrawSkeleton(preFeetLeft, feetLeft, viewMatrix, color);
				DirectX::DrawSkeleton(preKneeRight, kneeRight, viewMatrix, color);
				DirectX::DrawSkeleton(kneeRight, preFeetRight, viewMatrix, color);
				DirectX::DrawSkeleton(preFeetRight, feetRight, viewMatrix, color);
			}
		}
		
		closestEntity = nullptr;
		float closestFov = FLT_MAX;
		float closestDistance = FLT_MAX;
		Vector3* viewAngles = (Vector3*)(*(uintptr_t*)(engineBase + 0x59F19C) + 0x4D90);
		Vector3 rcsAngles;

		for (int i = 1; i < *currPlayers; i++) {
			ent* entity = *(ent**)(entList + i * 0x10);
			if (!entity) continue;
			if (entity->health == 0) continue;
			if (entity->teamNum == dwLocalPlayer->teamNum) continue;
			if (entity->bDormant) continue;
			if (entity == dwLocalPlayer) continue;
			if (Config::bVisCheck && !isVisible(entity)) continue;

			if (Config::bAimbot || Config::bSilentAim) {
				Vector3 bonePos = GetBonePos(entity, Config::boneNum);
				Vector3 newCam = dwLocalPlayer->bodypos + dwLocalPlayer->camPos;
				Vector3 delta = bonePos - newCam;

				float newYaw = atan2f(delta.y, delta.x) * (180.0f / PI);
				if (newYaw > 180.0f) newYaw -= 360.0f;
				if (newYaw < -180.0f) newYaw += 360.0f;

				float newPitch = -atan2f(delta.z, std::hypot(delta.x, delta.y)) * (180.0f / PI);
				if (newPitch > 89.0f) newPitch = 89.0f;
				if (newPitch < -89.0f) newPitch = -89.0f;

				float yawDiff = newYaw - viewAngles->y;
				if (yawDiff > 180.0f) yawDiff -= 360.0f;
				if (yawDiff < -180.0f) yawDiff += 360.0f;
				float pitchDiff = newPitch - viewAngles->x;
				if (pitchDiff > 89.0f) pitchDiff = 89.0f;
				if (pitchDiff < -89.0f) pitchDiff = -89.0f;

				float distance = entity->bodypos.getDistance(dwLocalPlayer->bodypos);

				Vector2 screenPos;

				if (DirectX::WorldToScreen(bonePos, screenPos, viewMatrix)) {
					float dx = screenPos.x - (wndWidth / 2);
					float dy = screenPos.y - (wndHeight / 2);
					float pixelFov = sqrt(dx * dx + dy * dy);

					if (Config::bFov) {
						if (pixelFov < closestFov && pixelFov <= Config::fovRadius) {
							closestEntity = entity;
							closestFov = pixelFov;
							bestYaw = newYaw;
							bestPitch = newPitch;
						}
					}
					else {
						if (distance < closestDistance) {
							closestEntity = entity;
							closestDistance = distance;
							bestYaw = newYaw;
							bestPitch = newPitch;
						}
					}
				}
			}

			if (Config::bRadar) {
				entity->isSpotted = true;
			}
		}

		static auto lastUpdateTime = std::chrono::steady_clock::now();

		auto currentTime = std::chrono::steady_clock::now();
		if (closestEntity && Config::bAimbot) {
			if (currentTime - lastUpdateTime >= std::chrono::milliseconds(7)) {
				float currentYaw = viewAngles->y;
				float currentPitch = viewAngles->x;

				float yawDiff = bestYaw - currentYaw;
				if (yawDiff > 180.0f) yawDiff -= 360.0f;
				if (yawDiff < -180.0f) yawDiff += 360.0f;
				float pitchDiff = bestPitch - currentPitch;
				if (pitchDiff > 89.0f) pitchDiff = 89.0f;
				if (pitchDiff < -89.0f) pitchDiff = -89.0f;

				float smoothing = std::clamp((1.2f - Config::smoothness), 0.05f, 1.0f);

				currentYaw += yawDiff * smoothing;
				currentPitch += pitchDiff * smoothing;

				if (Config::bRcs) {
					Vector3 newAngles;
					newAngles.y = currentYaw;
					newAngles.x = currentPitch;

					Vector3 punchAngle = dwLocalPlayer->aimPunch * 2;
					if (dwLocalPlayer->shotsFired > 1) {
						float scale = 1.0f - (Config::smoothness * 0.8f);
						newAngles = newAngles - punchAngle * scale;
						newAngles.normalize();
					}

					viewAngles->y = newAngles.y;
					viewAngles->x = newAngles.x;
				}
				else {
					viewAngles->y = currentYaw;
					viewAngles->x = currentPitch;
				}

				lastUpdateTime = currentTime;
			}
		}
	}

	if (Config::bFov) {
		DirectX::DrawCircle(wndWidth / 2, wndHeight / 2, Config::fovRadius, D3DCOLOR_RGBA(255, 255, 255, 255));
	}

	if (Config::showMenu) {
		const char* boneItems[] = { "Head", "Neck", "Torso", "Stomach" };
		int boneIDS[] = { 8,7,6,4 };
		static int selectedBone = 0;

		ImGui::Begin("CSGO", &Config::showMenu, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
		ImGui::BeginTabBar("tabs");

		if (ImGui::BeginTabItem("Combat")) {
			if (ImGui::Checkbox("Aimbot", &Config::bAimbot)) {
				Config::bSilentAim = false;
			}
			ImGui::SliderFloat("Smoothness", &Config::smoothness, 0.0f, 1.0f, "%.3f");
			ImGui::Combo("Bone Selection", &selectedBone, boneItems, IM_ARRAYSIZE(boneItems));
			Config::boneNum = boneIDS[selectedBone];
			ImGui::Checkbox("Select closest bone", &Config::bClosestBone);
			ImGui::Checkbox("FOV", &Config::bFov);
			if (Config::bFov) {
				ImGui::SliderFloat("FOV Radius", &Config::fovRadius, 20.0f, 500.0f, "%.3f");
			}
			if (ImGui::Checkbox("Silent aim", &Config::bSilentAim)) {
				Config::bAimbot = false;
			}
			ImGui::Checkbox("Draw line", &Config::bSnaplines);
			ImGui::Checkbox("Visible Check", &Config::bVisCheck);
			ImGui::Checkbox("Triggerbot", &Config::bTriggerbot);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Visuals")) {
			ImGui::Checkbox("Draw Box", &Config::bEsp);
			ImGui::Checkbox("HealthBar", &Config::bHealthbar);
			ImGui::Checkbox("Draw distance", &Config::bDistance);
			ImGui::Checkbox("Draw Skeleton", &Config::bSkeleton);
			ImGui::Checkbox("Draw Names", &Config::bNames);
			ImGui::Checkbox("Snaplines", &Config::bEspSnap);
			ImGui::Checkbox("Radar on", &Config::bRadar);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Misc")) {
			ImGui::Checkbox("RCS", &Config::bRcs);
			ImGui::Checkbox("No flash", &Config::bAntiFlash);
			ImGui::Checkbox("Bunnyhop", &Config::bBunnyHop);
			ImGui::Checkbox("Auto strafe", &Config::bStrafe);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Settings")) {
			static char fileName[MAX_PATH] = "";
			static std::string selectedConfig = "";
			static char localAppdata[MAX_PATH] = "";
			static bool isConfigSelected = false;

			ExpandEnvironmentStringsA("%localappdata%", localAppdata, MAX_PATH);

			std::string configPath = std::string(localAppdata) + "\\" + fileName + ".ini";
			std::string searchPattern = std::string(localAppdata) + "\\*.ini";

			WIN32_FIND_DATAA findData;
			HANDLE hFind = FindFirstFileA(searchPattern.c_str(), &findData);
			std::vector<std::string> configFiles;

			if (hFind != INVALID_HANDLE_VALUE) {
				do {
					configFiles.push_back(findData.cFileName);
				} while (FindNextFileA(hFind, &findData) != 0);
				FindClose(hFind);
			}

			ImGui::Text("Configs");

			if (ImGui::BeginListBox("##configs", ImVec2(300, 5 * ImGui::GetTextLineHeightWithSpacing()))) {
				for (const std::string& file : configFiles) {
					if (ImGui::Selectable(file.substr(0, file.find(".ini")).c_str(), selectedConfig == file)) {
						selectedConfig = file;
						isConfigSelected = true;
					}
				}
				ImGui::EndListBox();
			}

			if (isConfigSelected) {
				std::string selectedConfigPath = std::string(localAppdata) + "\\" + selectedConfig;

				if (ImGui::Button("Save Config", ImVec2(120, 20))) {
					WritePrivateProfileStringA("Aimbot", "Aimbot", Config::bAimbot ? "1" : "0", selectedConfigPath.c_str());
					WritePrivateProfileStringA("Aimbot", "Smoothing", std::to_string(Config::smoothness).c_str(), selectedConfigPath.c_str());
					WritePrivateProfileStringA("Aimbot", "BoneNum", std::to_string(Config::boneNum).c_str(), selectedConfigPath.c_str());
					WritePrivateProfileStringA("Aimbot", "ClosestBone", std::to_string(Config::bClosestBone).c_str(), selectedConfigPath.c_str());
					WritePrivateProfileStringA("Aimbot", "SilentAim", Config::bSilentAim ? "1" : "0", selectedConfigPath.c_str());
					WritePrivateProfileStringA("Aimbot", "SnapLines", Config::bSnaplines ? "1" : "0", selectedConfigPath.c_str());
					WritePrivateProfileStringA("Aimbot", "FOV", Config::bFov ? "1" : "0", selectedConfigPath.c_str());
					WritePrivateProfileStringA("Aimbot", "FOVRadius", std::to_string(Config::fovRadius).c_str(), selectedConfigPath.c_str());
					WritePrivateProfileStringA("Aimbot", "VisibleCheck", Config::bVisCheck ? "1" : "0", selectedConfigPath.c_str());
					WritePrivateProfileStringA("Aimbot", "TriggerBot", Config::bTriggerbot ? "1" : "0", selectedConfigPath.c_str());
					WritePrivateProfileStringA("ESP", "ESP", Config::bEsp ? "1" : "0", selectedConfigPath.c_str());
					WritePrivateProfileStringA("ESP", "DrawHealthBar", Config::bHealthbar ? "1" : "0", selectedConfigPath.c_str());
					WritePrivateProfileStringA("ESP", "DrawNames", Config::bNames ? "1" : "0", selectedConfigPath.c_str());
					WritePrivateProfileStringA("ESP", "DrawDistance", Config::bDistance ? "1" : "0", selectedConfigPath.c_str());
					WritePrivateProfileStringA("ESP", "RadarHack", Config::bRadar ? "1" : "0", selectedConfigPath.c_str());
					WritePrivateProfileStringA("Misc", "RCS", Config::bRcs ? "1" : "0", selectedConfigPath.c_str());
					WritePrivateProfileStringA("Misc", "NoFlash", Config::bAntiFlash ? "1" : "0", selectedConfigPath.c_str());
					WritePrivateProfileStringA("Misc", "BunnyHop", Config::bBunnyHop ? "1" : "0", selectedConfigPath.c_str());
					WritePrivateProfileStringA("Misc", "Autostrafe", Config::bStrafe ? "1" : "0", selectedConfigPath.c_str());
				}

				if (ImGui::Button("Load Config", ImVec2(120, 20))) {
					char smoothValue[MAX_PATH];
					char fovValue[MAX_PATH];
					GetPrivateProfileStringA("Aimbot", "Smoothing", "0", smoothValue, MAX_PATH, selectedConfigPath.c_str());
					GetPrivateProfileStringA("Aimbot", "FOVRadius", "0", fovValue, MAX_PATH, selectedConfigPath.c_str());

					Config::bAimbot = GetPrivateProfileIntA("Aimbot", "Aimbot", 0, selectedConfigPath.c_str());
					Config::smoothness = std::stof(smoothValue);
					Config::boneNum = GetPrivateProfileIntA("Aimbot", "BoneNum", 7, selectedConfigPath.c_str());
					switch (Config::boneNum)
					{
					case 8: // head
						selectedBone = 0;
						break;
					case 7: //neck
						selectedBone = 1;
						break;
					case 6: // torso
						selectedBone = 2;
						break;
					case 4: // stomach
						selectedBone = 3;
						break;
					}
					Config::bClosestBone = GetPrivateProfileIntA("Aimbot", "ClosestBone", 0, selectedConfigPath.c_str());
					Config::bSilentAim = GetPrivateProfileIntA("Aimbot", "SilentAim", 0, selectedConfigPath.c_str());
					Config::bSnaplines = GetPrivateProfileIntA("Aimbot", "SnapLines", 0, selectedConfigPath.c_str());
					Config::bFov = GetPrivateProfileIntA("Aimbot", "FOV", 0, selectedConfigPath.c_str());
					Config::fovRadius = std::stof(fovValue);
					Config::bVisCheck = GetPrivateProfileIntA("Aimbot", "VisibleCheck", 0, selectedConfigPath.c_str());
					Config::bTriggerbot = GetPrivateProfileIntA("Aimbot", "TriggerBot", 0, selectedConfigPath.c_str());
					Config::bEsp = GetPrivateProfileIntA("ESP", "ESP", 0, selectedConfigPath.c_str());
					Config::bHealthbar = GetPrivateProfileIntA("ESP", "DrawHealthBar", 0, selectedConfigPath.c_str());
					Config::bNames = GetPrivateProfileIntA("ESP", "DrawNames", 0, selectedConfigPath.c_str());
					Config::bDistance = GetPrivateProfileIntA("ESP", "DrawDistance", 0, selectedConfigPath.c_str());
					Config::bRadar = GetPrivateProfileIntA("ESP", "RadarHack", 0, selectedConfigPath.c_str());
					Config::bRcs = GetPrivateProfileIntA("Misc", "RCS", 0, selectedConfigPath.c_str());
					Config::bAntiFlash = GetPrivateProfileIntA("Misc", "NoFlash", 0, selectedConfigPath.c_str());
					Config::bBunnyHop = GetPrivateProfileIntA("Misc", "BunnyHop", 0, selectedConfigPath.c_str());
					Config::bStrafe = GetPrivateProfileIntA("Misc", "Autostrafe", 0, selectedConfigPath.c_str());
				}

				if (ImGui::Button("Delete Config", ImVec2(120, 20))) {
					if (DeleteFileA(selectedConfigPath.c_str())) {
						selectedConfig = "";
						isConfigSelected = false;
					}
				}

				if (ImGui::Button("Rename Config", ImVec2(120, 20))) {
					ImGui::OpenPopup("Rename Config");
					ImGui::SetNextWindowSize(ImVec2(150, 80));
				}

				if (ImGui::BeginPopupModal("Rename Config", 0, ImGuiWindowFlags_NoResize)) {
					static char newName[256];
					std::string newNamePath = std::string(localAppdata) + "\\" + newName + ".ini";
					ImGui::InputText("##cfgRename", newName, 256);

					if (ImGui::Button("Rename")) {
						if (newName[0] != '\0') {
							MoveFileA(selectedConfigPath.c_str(), newNamePath.c_str());
							ImGui::CloseCurrentPopup();
							memset(newName, 0, sizeof(newName));
						}
					}
					ImGui::EndPopup();
				}
			}

			ImGui::InputText("Config name", fileName, MAX_PATH);

			if (ImGui::Button("Create Config", ImVec2(120, 20)) && fileName[0] != '\0') {

				if (GetFileAttributesA(configPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
					HANDLE hFile = CreateFileA(configPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
					if (hFile != INVALID_HANDLE_VALUE) {
						WritePrivateProfileStringA("Aimbot", "Aimbot", Config::bAimbot ? "1" : "0", configPath.c_str());
						WritePrivateProfileStringA("Aimbot", "Smoothing", std::to_string(Config::smoothness).c_str(), configPath.c_str());
						WritePrivateProfileStringA("Aimbot", "BoneNum", std::to_string(Config::boneNum).c_str(), configPath.c_str());
						WritePrivateProfileStringA("Aimbot", "ClosestBone", Config::bClosestBone ? "1" : "0", configPath.c_str());
						WritePrivateProfileStringA("Aimbot", "SilentAim", Config::bSilentAim ? "1" : "0", configPath.c_str());
						WritePrivateProfileStringA("Aimbot", "SnapLines", Config::bSnaplines ? "1" : "0", configPath.c_str());
						WritePrivateProfileStringA("Aimbot", "FOV", Config::bFov ? "1" : "0", configPath.c_str());
						WritePrivateProfileStringA("Aimbot", "FOVRadius", std::to_string(Config::fovRadius).c_str(), configPath.c_str());
						WritePrivateProfileStringA("Aimbot", "VisibleCheck", Config::bVisCheck ? "1" : "0", configPath.c_str());
						WritePrivateProfileStringA("Aimbot", "TriggerBot", Config::bTriggerbot ? "1" : "0", configPath.c_str());
						WritePrivateProfileStringA("ESP", "ESP", Config::bEsp ? "1" : "0", configPath.c_str());
						WritePrivateProfileStringA("ESP", "DrawHealthBar", Config::bHealthbar ? "1" : "0", configPath.c_str());
						WritePrivateProfileStringA("ESP", "DrawNames", Config::bNames ? "1" : "0", configPath.c_str());
						WritePrivateProfileStringA("ESP", "DrawDistance", Config::bDistance ? "1" : "0", configPath.c_str());
						WritePrivateProfileStringA("ESP", "RadarHack", Config::bRadar ? "1" : "0", configPath.c_str());
						WritePrivateProfileStringA("Misc", "RCS", Config::bRcs ? "1" : "0", configPath.c_str());
						WritePrivateProfileStringA("Misc", "NoFlash", Config::bAntiFlash ? "1" : "0", configPath.c_str());
						WritePrivateProfileStringA("Misc", "BunnyHop", Config::bBunnyHop ? "1" : "0", configPath.c_str());
						WritePrivateProfileStringA("Misc", "Autostrafe", Config::bStrafe ? "1" : "0", configPath.c_str());
						CloseHandle(hFile);
					}
				}
			}

			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
		ImGui::End();
	}

	ImGui::EndFrame();
	ImGui::Render();

	ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

	oEndScene(o_pDevice);
}

extern LRESULT IMGUI_IMPL_API ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

using UnlockCursor_t = void(__thiscall*)(void*);
void* SurfaceInterface = nullptr;

WNDPROC	oWndProc;
LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (Config::showMenu) {
		ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
		return true;
	}

	return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

void __fastcall hkLockCursor(void* thisptr) {
	void** vtable = *reinterpret_cast<void***>(SurfaceInterface);
	UnlockCursor_t UnlockCursor = reinterpret_cast<UnlockCursor_t>(vtable[66]);
	uintptr_t engineBase = (uintptr_t)GetModuleHandle(L"engine.dll");
	int* dwClientState = (int*)(uintptr_t)(*(uintptr_t*)(engineBase + 0x59F19C) + 0x108);

	if (Config::showMenu && *dwClientState == 6) {
		UnlockCursor(SurfaceInterface);
	}
	else {
		if (!oLockCursor) {
			return;
		}
		return oLockCursor(thisptr);
	}
}

void initHooks() {
	SurfaceInterface = GetInterface(L"vguimatsurface.dll", "VGUI_Surface031");
	g_Client = GetInterface(L"client.dll", "VClient018");
	g_ClientMode = **reinterpret_cast<void***>((*reinterpret_cast<uintptr_t**>(g_Client))[10] + 5);

	void* vtable = (*reinterpret_cast<void***>(SurfaceInterface))[67];
	LockCursorAddr = (uintptr_t)vtable;
	memcpy(LockCursorBytes, vtable, 7);

	oLockCursor = (tLockCursor)mem::TrampHook((BYTE*)vtable, (BYTE*)hkLockCursor, 7);

	if (GetD3D9Device(d3d9Device, sizeof(d3d9Device))) {
		memcpy(EndSceneBytes, (char*)d3d9Device[42], 7);

		oEndScene = (tEndScene)mem::TrampHook((BYTE*)d3d9Device[42], (BYTE*)hkEndScene, 7);
	}

	memcpy(CreateMoveBytes, (*static_cast<void***>(g_ClientMode))[24], 7);
	mem::TrampHook((BYTE*)(*static_cast<void***>(g_ClientMode))[24], (BYTE*)hkCreateMove, 7);

	oWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc));

	GetProcessWindow();
}

void disableHooks() {
	uninjecting = true;
	Sleep(10);

	mem::Patch((BYTE*)d3d9Device[42], EndSceneBytes, 7);
	mem::Patch((BYTE*)(*static_cast<void***>(g_ClientMode))[24], CreateMoveBytes, 7);
	mem::Patch((BYTE*)LockCursorAddr, LockCursorBytes, 7);

	SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)oWndProc);

	VirtualFree(oLockCursor, 0, MEM_RELEASE);
	VirtualFree(oEndScene, 0, MEM_RELEASE);

	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void executeModules() {
	uintptr_t clientBase = (uintptr_t)GetModuleHandle(L"client.dll");
	uintptr_t engineBase = (uintptr_t)GetModuleHandle(L"engine.dll");
	uintptr_t serverBase = (uintptr_t)GetModuleHandle(L"server.dll");
	ent* dwLocalPlayer = *(ent**)(clientBase + 0x4E051DC);
	uintptr_t entList = (uintptr_t)(clientBase + 0x4E051DC);
	int* currPlayers = (int*)(serverBase + 0xB61618);
	if (!dwLocalPlayer) return;
	if (!entList) return;

	int* dwForceJump = (int*)(clientBase + 0x52C0F50);
	int* forceLMB = (int*)(clientBase + 0x3233024);

	if (Config::bRcs) {
		static Vector3 oPunch{ 0,0,0 };
		Vector3 punchAngle = dwLocalPlayer->aimPunch * 2;

		Vector3* viewAngles = (Vector3*)(*(uintptr_t*)(engineBase + 0x59F19C) + 0x4D90);
		Vector3 rcsAngles;

		if (dwLocalPlayer->shotsFired > 1) {
			rcsAngles = *viewAngles + oPunch - punchAngle;
			rcsAngles.normalize();
			*viewAngles = rcsAngles;
		}

		oPunch = punchAngle;
	}

	if (Config::bTriggerbot) {
		if (dwLocalPlayer->crosshairEnt > 0 && dwLocalPlayer->crosshairEnt < 64) {
			ent* crosshair = *(ent**)(entList + (dwLocalPlayer->crosshairEnt - 1) * 0x10);
			if (crosshair->teamNum != dwLocalPlayer->teamNum) {
				*forceLMB = 6;
			}
		}
		else if (GetAsyncKeyState(VK_LBUTTON)) {
			*forceLMB = 5;
		}
		else {
			*forceLMB = 4;
		}
	}

	if (Config::bAntiFlash) {
		dwLocalPlayer->flashDuration = 0;
	}

	if (Config::bBunnyHop) {
		if (dwLocalPlayer->flags & (1 << 0) && GetAsyncKeyState(VK_SPACE) & 0x8000 && GetForegroundWindow() == window) {
			*dwForceJump = 6;
		}
		else {
			*dwForceJump = 4;
		}
	}
}