#pragma once

#include <imgui.h>

namespace gui
{
	namespace colors
	{

		// Base color values (RGBA, 0-255)
		namespace rgb
		{
			constexpr int OK_GREEN[4] = { 56, 142, 60, 255 }; // Muted green
			constexpr int WARNING_YELLOW[4] = { 245, 163, 10, 255 }; // Warm golden yellow
			constexpr int ERROR_RED[4] = { 211, 47, 47, 255 }; // Strong (but not neon) red
		} // namespace rgb

		// IM_COL32 compatible colors (directly usable with ImGui)
		struct ImColors
		{
			static ImU32 OK()
			{
				return IM_COL32(rgb::OK_GREEN[0], rgb::OK_GREEN[1], rgb::OK_GREEN[2], rgb::OK_GREEN[3]);
			}

			static ImU32 WARNING()
			{
				return IM_COL32(rgb::WARNING_YELLOW[0], rgb::WARNING_YELLOW[1], rgb::WARNING_YELLOW[2],
								rgb::WARNING_YELLOW[3]);
			}

			static ImU32 ERROR()
			{
				return IM_COL32(rgb::ERROR_RED[0], rgb::ERROR_RED[1], rgb::ERROR_RED[2], rgb::ERROR_RED[3]);
			}

			static ImVec4 OK_Float()
			{
				return ImVec4(rgb::OK_GREEN[0] / 255.0f, rgb::OK_GREEN[1] / 255.0f, rgb::OK_GREEN[2] / 255.0f,
							  rgb::OK_GREEN[3] / 255.0f);
			}

			static ImVec4 WARNING_Float()
			{
				return ImVec4(rgb::WARNING_YELLOW[0] / 255.0f, rgb::WARNING_YELLOW[1] / 255.0f,
							  rgb::WARNING_YELLOW[2] / 255.0f, rgb::WARNING_YELLOW[3] / 255.0f);
			}

			static ImVec4 ERROR_Float()
			{
				return ImVec4(rgb::ERROR_RED[0] / 255.0f, rgb::ERROR_RED[1] / 255.0f, rgb::ERROR_RED[2] / 255.0f,
							  rgb::ERROR_RED[3] / 255.0f);
			}
		};

	} // namespace colors
} // namespace gui
