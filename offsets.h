#pragma once
#include <Windows.h>
#include <iostream>
#include "geom.h"

class ent
{
public:
	char pad_0000[148]; //0x0000
	Vector3 velocity; //0x0094
	Vector3 bodypos; //0x00A0
	char pad_00AC[64]; //0x00AC
	int32_t bDormant; //0x00EC
	char pad_00F0[4]; //0x00F0
	int32_t teamNum; //0x00F4
	char pad_00F8[8]; //0x00F8
	int32_t health; //0x0100
	int32_t flags; //0x0104
	Vector3 camPos; //0x0108
	char pad_0114[36]; //0x0114
	Vector3 origin; //0x0138
	char pad_0144[2040]; //0x0144
	bool N00000290; //0x093C
	bool isSpotted; //0x093D
	char pad_093E[7530]; //0x093E
	uintptr_t boneMatrix; //0x26A8
	char pad_26AC[2448]; //0x26AC
	Vector3 aimPunch; //0x303C
	char pad_3048[54168]; //0x3048
	int32_t shotsFired; //0x103E0
	char pad_103E4[140]; //0x103E4
	float flashDuration; //0x10470
	char pad_10474[5060]; //0x10474
	int32_t crosshairEnt; //0x11838
	char pad_1183C[7584]; //0x1183C
}; //Size: 0x135DC
static_assert(sizeof(ent) == 0x135DC);