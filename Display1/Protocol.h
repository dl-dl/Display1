#pragma once

static const int SCREEN_DX = 400;
static const int SCREEN_DY = 240;

struct PointDataMsg
{
	unsigned int addr;
	unsigned char data[SCREEN_DX*3];
};
