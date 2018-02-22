#pragma once

static const unsigned int WM_USER_MSG_LINE_DATA = 3;

extern void gPostMsg(int userMsg, int param1, int param2);
extern void gPostMsg(int userMsg, int param1, void* param2);
/*
struct PaintContext;

extern void gSetPoint(_In_ const PaintContext* ctx, int x, int y);
extern void gMoveTo(_In_ const PaintContext* ctx, int x, int y);
extern void gLineTo(_In_ const PaintContext* ctx, int x, int y);
extern void gCircle(_In_ const PaintContext* ctx, int x, int y, int sz);
extern void gInvalidateDisplay();
*/