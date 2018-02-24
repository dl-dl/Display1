#pragma once

static const int SCREEN_DX = 240;
static const int SCREEN_DY = 400;

struct PointDataMsg
{
	unsigned int addr;
	unsigned char data[SCREEN_DY * 3]; // RGB
};
