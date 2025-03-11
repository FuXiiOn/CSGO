#include "directx.h"

extern LPDIRECT3DDEVICE9 pDevice;
extern LPD3DXFONT pFont;

static int wndWidth, wndHeight;
static HWND window;
ID3DXLine* LineL;

void DirectX::initD3D() {
	D3DXCreateLine(pDevice, &LineL);

	D3DXCreateFont(pDevice, 15, 5, FW_NORMAL, 1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Arial", &pFont);
}

void DirectX::Cleanup() {
	if (LineL) {
		LineL->Release();
	}
}

static HWND GetProcessWindow() {
	window = FindWindowA(NULL, "Counter-Strike: Global Offensive - Direct3D 9");

	RECT size;
	GetClientRect(window, &size);
	wndWidth = size.right - size.left;
	wndHeight = size.bottom - size.top;

	return window;
}

bool DirectX::WorldToScreen(Vector3 pos, Vector2& screenCoords, float matrix[16]) {
	GetProcessWindow();
	Vector4 clipCoords;
	clipCoords.x = pos.x * matrix[0] + pos.y * matrix[1] + pos.z * matrix[2] + matrix[3];
	clipCoords.y = pos.x * matrix[4] + pos.y * matrix[5] + pos.z * matrix[6] + matrix[7];
	clipCoords.z = pos.x * matrix[8] + pos.y * matrix[9] + pos.z * matrix[10] + matrix[11];
	clipCoords.w = pos.x * matrix[12] + pos.y * matrix[13] + pos.z * matrix[14] + matrix[15];

	if (clipCoords.w < 0.1f) return false;

	Vector3 NDC;
	NDC.x = clipCoords.x / clipCoords.w;
	NDC.y = clipCoords.y / clipCoords.w;
	NDC.z = clipCoords.z / clipCoords.w;

	screenCoords.x = (wndWidth / 2 * NDC.x) + (NDC.x + wndWidth / 2);
	screenCoords.y = -(wndHeight / 2 * NDC.y) + (NDC.y + wndHeight / 2);
	return true;
}

void DirectX::DrawFilledRect(int x, int y, int w, int h, D3DCOLOR color) {
	D3DRECT rect = { x,y,x + w,y + h };
	pDevice->Clear(1, &rect, D3DCLEAR_TARGET, color, 0, 0);
}

void DirectX::IngameText(int x, int y, D3DCOLOR color, const char* text) {
	RECT rect;
	rect.left = x;
	rect.top = y;
	rect.right = x + 200;

	if (pFont) {
		pFont->DrawTextA(NULL, text, -1, &rect, DT_NOCLIP, color);
	}
}

void DirectX::DrawOutline(int x1, int y1, int x2, int y2, int width, D3DCOLOR color) {
	if (!LineL) return;

	D3DXVECTOR2 Line[2];
	Line[0] = D3DXVECTOR2(x1, y1);
	Line[1] = D3DXVECTOR2(x2, y2);
	LineL->SetWidth(width);
	LineL->Draw(Line, 2, color);
}

void DirectX::DrawESPBox(Vector3 origin, Vector3 headPos, ent* entity, ent* localPlayer, D3DCOLOR color) {
	GetProcessWindow();
	float height = origin.y - headPos.y;
	float width = height / 4.0f;

	float distance = localPlayer->bodypos.getDistance(entity->bodypos);

	DirectX::DrawOutline(origin.x - width, origin.y, origin.x + width, origin.y, 1, color);
	DirectX::DrawOutline(origin.x - width, origin.y, origin.x - width, origin.y - height, 1, color);
	DirectX::DrawOutline(origin.x + width, origin.y, origin.x + width, origin.y - height, 1, color);
	DirectX::DrawOutline(origin.x - width, origin.y - height, origin.x + width, origin.y - height, 1, color);
}

void DirectX::DrawCircle(float centerX, float centerY, float radius, D3DCOLOR color) {
	if (!LineL) return;

	D3DXVECTOR2* points = new D3DXVECTOR2[101];

	for (int i = 0; i < 100; ++i) {
		float angle = (float)i / 100 * PI * 2.0f;
		points[i] = D3DXVECTOR2(centerX + radius * cos(angle), centerY + radius * sin(angle));
	}

	points[100] = points[0];

	LineL->SetWidth(1);
	LineL->Draw(points, 101, color);

	delete[] points;
}

void DirectX::DrawSkeleton(Vector3 bone1, Vector3 bone2, float viewMatrix[16], D3DCOLOR color) {
	Vector2 bone1Screen, bone2Screen;

	GetProcessWindow();

	if (WorldToScreen(bone1, bone1Screen, viewMatrix) && WorldToScreen(bone2, bone2Screen, viewMatrix)) {
		DrawOutline(bone1Screen.x, bone1Screen.y, bone2Screen.x, bone2Screen.y, 1, color);
	}
}
