#pragma once
#include "config.h"
#include <algorithm>
#include "geom.h"
#include <d3d9.h>
#include <d3dx9.h>
#include "offsets.h"
#include "math.h"
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

namespace DirectX {

	void initD3D();
	void Cleanup();
	bool WorldToScreen(Vector3 pos, Vector2& screenCoords, float matrix[16]);
	void IngameText(int x, int y, D3DCOLOR color, const char* text);
	void DrawFilledRect(int x, int y, int w, int h, D3DCOLOR color);
	void DrawOutline(int x1, int y1, int x2, int y2, int width, D3DCOLOR color);
	void DrawESPBox(Vector3 origin, Vector3 headPos, ent* entity, ent* localPlayer, D3DCOLOR color);
	void DrawCircle(float centerX, float centerY, float radius, D3DCOLOR color);
	void DrawSkeleton(Vector3 bone1, Vector3 bone2, float viewMatrix[16], D3DCOLOR color);
}