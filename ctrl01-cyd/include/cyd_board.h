#pragma once
namespace CYD {
constexpr int Rotation = 1;
static_assert(Rotation >= 0 && Rotation <= 3, "Rotation must be 0..3");
constexpr bool Landscape = (Rotation & 1) != 0;
constexpr int Width = Landscape ? 320 : 240;
constexpr int Height = Landscape ? 240 : 320;
constexpr int Boot = 0;
constexpr int LedRed = 4, LedGreen = 16, LedBlue = 17;
constexpr int TouchClk = 25, TouchMiso = 39, TouchMosi = 32;
constexpr int TouchCs = 33, TouchIrq = 36;
// Raw touch endpoints after applying Rotation; adjust after hardware testing.
constexpr int TouchMinX = 200, TouchMaxX = 3800;
constexpr int TouchMinY = 200, TouchMaxY = 3800;
constexpr int ControlHeight = 42, ControlWidth = Width / 5;
constexpr int ControlsY = Height - ControlHeight - 4;
constexpr int ProgressY = ControlsY - 16;
}
