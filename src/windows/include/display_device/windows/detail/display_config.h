/**
 * @file src/windows/include/display_device/windows/detail/display_config.h
 * @brief Declarations for private DisplayConfig compatibility helpers.
 */
#pragma once

// local includes
#include "display_device/windows/types.h"

// system includes
#include <algorithm>
#include <cstddef>
#include <cstring>

namespace display_device::detail {
  /**
   * @brief Query display paths with non-null output buffers even when the size estimate is zero.
   * @tparam QueryFn Native QueryDisplayConfig function or a compatible callable.
   * @param flags Native query flags.
   * @param path_count Number of path records estimated by GetDisplayConfigBufferSizes.
   * @param mode_count Number of mode records estimated by GetDisplayConfigBufferSizes.
   * @param paths Buffer receiving the actual returned path records.
   * @param modes Buffer receiving the actual returned mode records.
   * @param query Function that performs the native query.
   * @return Native query result; a successful empty query leaves both output arrays empty.
   * @see https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-querydisplayconfig
   */
  template<class QueryFn>
  LONG queryDisplayConfigWithBuffers(UINT32 flags, UINT32 path_count, UINT32 mode_count, std::vector<DISPLAYCONFIG_PATH_INFO> &paths, std::vector<DISPLAYCONFIG_MODE_INFO> &modes, QueryFn query) {
    // Both output pointers are mandatory, including when every display is inactive.
    path_count = std::max<UINT32>(path_count, 1);
    mode_count = std::max<UINT32>(mode_count, 1);
    paths.resize(path_count);
    modes.resize(mode_count);
    const LONG result {query(flags, &path_count, paths.data(), &mode_count, modes.data(), nullptr)};
    if (result == ERROR_SUCCESS) {
      paths.resize(path_count);
      modes.resize(mode_count);
    }
    return result;
  }

  /**
   * @brief Gets desktop image metadata from a DisplayConfig mode record.
   * @param info Mode record whose union payload contains desktop image metadata.
   * @returns The desktop image metadata stored in the mode record.
   */
  [[nodiscard]] inline DISPLAYCONFIG_DESKTOP_IMAGE_INFO getDesktopImageInfo(const DISPLAYCONFIG_MODE_INFO &info) {
    constexpr std::size_t union_offset {offsetof(DISPLAYCONFIG_MODE_INFO, targetMode)};
    static_assert(sizeof(DISPLAYCONFIG_MODE_INFO) >= union_offset + sizeof(DISPLAYCONFIG_DESKTOP_IMAGE_INFO));

    DISPLAYCONFIG_DESKTOP_IMAGE_INFO desktop_image_info {};
    std::memcpy(&desktop_image_info, reinterpret_cast<const std::byte *>(&info) + union_offset, sizeof(desktop_image_info));

    return desktop_image_info;
  }

  /**
   * @brief Sets desktop image metadata on a DisplayConfig mode record.
   * @param info Mode record whose union payload should be updated.
   * @param desktop_image_info Desktop image metadata to store in the mode record.
   */
  inline void setDesktopImageInfo(DISPLAYCONFIG_MODE_INFO &info, const DISPLAYCONFIG_DESKTOP_IMAGE_INFO &desktop_image_info) {
    constexpr std::size_t union_offset {offsetof(DISPLAYCONFIG_MODE_INFO, targetMode)};
    static_assert(sizeof(DISPLAYCONFIG_MODE_INFO) >= union_offset + sizeof(DISPLAYCONFIG_DESKTOP_IMAGE_INFO));

    std::memcpy(reinterpret_cast<std::byte *>(&info) + union_offset, &desktop_image_info, sizeof(desktop_image_info));
  }
}  // namespace display_device::detail
