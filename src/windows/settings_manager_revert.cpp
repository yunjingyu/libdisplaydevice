/**
 * @file src/windows/settings_manager_revert.cpp
 * @brief Definitions for the methods for reverting settings in SettingsManager.
 */
// class header include
#include "display_device/windows/settings_manager.h"

// system includes
#include <boost/scope/scope_exit.hpp>

// local includes
#include "display_device/logging.h"
#include "display_device/windows/json.h"
#include "display_device/windows/settings_utils.h"

namespace display_device {
  namespace {
    /**
     * @brief Function that does nothing.
     */
    void noopFn() {
      // Intentionally empty guard callback.
    }

    /**
     * @brief Add currently active outputs without changing the target's clone groups.
     * @param target Topology whose groups must be restored.
     * @param current Outputs that must stay active until restoration is verified.
     * @return Target groups followed by current-only groups, without duplicate devices.
     */
    ActiveTopology preserveCurrentOutputs(ActiveTopology target, const ActiveTopology &current) {
      auto included {win_utils::flattenTopology(target)};
      for (const auto &group : current) {
        std::vector<std::string> remaining;
        for (const auto &device : group) {
          if (included.insert(device).second) {
            remaining.push_back(device);
          }
        }
        if (!remaining.empty()) {
          target.push_back(std::move(remaining));
        }
      }
      return target;
    }

    /**
     * @brief Keep outputs activated outside the saved streaming transaction during restoration.
     * @param initial Saved pre-stream layout.
     * @param modified Outputs managed by the saved streaming layout.
     * @param current Active outputs observed before restoration starts.
     * @return Saved layout plus active outputs absent from the streaming layout.
     */
    ActiveTopology preserveExternallyActivatedOutputs(ActiveTopology initial, const ActiveTopology &modified, const ActiveTopology &current) {
      const auto managed {win_utils::flattenTopology(modified)};
      ActiveTopology external;
      for (const auto &group : current) {
        std::vector<std::string> devices;
        for (const auto &device : group) {
          if (!managed.contains(device)) {
            devices.push_back(device);
          }
        }
        if (!devices.empty()) {
          external.push_back(std::move(devices));
        }
      }
      return preserveCurrentOutputs(std::move(initial), external);
    }

    /**
     * @brief Apply a topology and verify the active paths before removing any safety output.
     * @param api Display API used to apply and read back the topology.
     * @param topology Requested topology.
     * @return True only when both the operation and its readback succeed.
     */
    bool setVerifiedTopology(WinDisplayDeviceInterface &api, const ActiveTopology &topology) {
      const bool applied {api.setTopology(topology)};
      const auto active {api.getCurrentTopology()};
      const bool matches {api.isTopologyTheSame(active, topology)};
      if (!applied || !matches) {
        DD_LOG(error) << "Display topology transition failed verification (API accepted: " << toJson(applied) << "). Requested:\n"
                      << toJson(topology) << "\nObserved:\n"
                      << toJson(active);
      }
      return applied && matches;
    }
  }  // namespace

  SettingsManager::RevertResult SettingsManager::revertSettings() {
    const auto &cached_state {m_persistence_state->getState()};
    if (!cached_state) {
      return RevertResult::NoChangesToRevert;
    }

    const auto api_access {m_dd_api->isApiAccessAvailable()};
    DD_LOG(info) << "Trying to revert applied display device settings. API is available: " << toJson(api_access);

    if (!api_access) {
      return RevertResult::ApiTemporarilyUnavailable;
    }

    auto current_topology {m_dd_api->getCurrentTopology()};
    bool system_settings_touched {false};
    if (!m_dd_api->isTopologyValid(current_topology)) {
      DD_LOG(error) << "Retrieved current topology is invalid:\n"
                    << toJson(current_topology);
      if (!current_topology.empty() || !m_dd_api->isTopologyValid(cached_state->m_initial.m_topology)) {
        return RevertResult::TopologyIsInvalid;
      }
      // A partially failed Windows transition may leave no active paths. The
      // normal guards cannot use that empty snapshot as a rollback target.
      // Reconstruct from the durable recovery record and verify before proceeding.
      const bool modified_valid {m_dd_api->isTopologyValid(cached_state->m_modified.m_topology)};
      const auto recovery_topology {modified_valid ? preserveCurrentOutputs(cached_state->m_initial.m_topology, cached_state->m_modified.m_topology) : cached_state->m_initial.m_topology};
      if (!setVerifiedTopology(*m_dd_api, recovery_topology)) {
        // Keep any paths that did become active. Only try the captured streaming
        // topology if Windows still reports no output at all.
        if (modified_valid && m_dd_api->getCurrentTopology().empty() && !setVerifiedTopology(*m_dd_api, cached_state->m_modified.m_topology)) {
          DD_LOG(error) << "No active display could be recovered; the saved recovery record remains pending.";
        }
        return RevertResult::SwitchingTopologyFailed;
      }
      current_topology = recovery_topology;
      system_settings_touched = true;
    }

    boost::scope::scope_exit hdr_blank_always_executed_guard {[this, &system_settings_touched]() {
      if (system_settings_touched) {
        win_utils::blankHdrStates(*m_dd_api, m_workarounds.m_hdr_blank_delay);
      }
    }};
    auto safe_topology {current_topology};
    boost::scope::scope_exit topology_prep_guard {[this, &current_topology, &safe_topology, &system_settings_touched]() {
      const auto active {m_dd_api->getCurrentTopology()};
      if (m_dd_api->isTopologyTheSame(active, safe_topology)) {
        return;
      }
      system_settings_touched = true;
      if (setVerifiedTopology(*m_dd_api, safe_topology)) {
        return;
      }
      // Do not invent an all-extended configuration here: it may drop the only
      // usable output or split a clone group. Keep the recovery record for retry.
      if (!m_dd_api->isTopologyTheSame(safe_topology, current_topology) && setVerifiedTopology(*m_dd_api, current_topology)) {
        return;
      }
      DD_LOG(error) << "Failed to restore the last verified display topology; display recovery remains pending.";
    }};

    // We can revert the modified setting independently before playing around with initial topology.
    bool switched_to_modified_topology {false};
    if (const auto result = revertModifiedSettings(current_topology, system_settings_touched, &switched_to_modified_topology, true); result != RevertResult::Ok) {
      // Error already logged
      return result;
    }

    if (!m_dd_api->isTopologyValid(cached_state->m_initial.m_topology)) {
      DD_LOG(error) << "Trying to revert to an invalid initial topology:\n"
                    << toJson(cached_state->m_initial.m_topology);
      return RevertResult::TopologyIsInvalid;
    }

    const auto before_restore {switched_to_modified_topology ? preserveCurrentOutputs(cached_state->m_modified.m_topology, current_topology) : current_topology};
    safe_topology = before_restore;
    // A stale record may contain only the virtual output. Do not disable physical
    // displays the user has recovered since that transaction was recorded.
    const auto initial_topology {preserveExternallyActivatedOutputs(cached_state->m_initial.m_topology, cached_state->m_modified.m_topology, current_topology)};
    if (initial_topology != cached_state->m_initial.m_topology) {
      DD_LOG(info) << "Preserving outputs activated outside the saved display transaction during restoration.";
    }
    const auto staged_topology {preserveCurrentOutputs(initial_topology, before_restore)};
    if (!m_dd_api->isTopologyTheSame(before_restore, staged_topology)) {
      system_settings_touched = true;
      if (!setVerifiedTopology(*m_dd_api, staged_topology)) {
        return RevertResult::SwitchingTopologyFailed;
      }
      safe_topology = staged_topology;
    }
    // Only remove the streaming output after the replacement outputs have been
    // observed active together with it. A sleeping display cannot consume the
    // recovery record merely because SetDisplayConfig returned success.
    if (!m_dd_api->isTopologyTheSame(staged_topology, initial_topology)) {
      system_settings_touched = true;
      if (!setVerifiedTopology(*m_dd_api, initial_topology)) {
        return RevertResult::SwitchingTopologyFailed;
      }
    } else if (!m_dd_api->isTopologyTheSame(m_dd_api->getCurrentTopology(), initial_topology)) {
      return RevertResult::SwitchingTopologyFailed;
    }
    safe_topology = initial_topology;

    if (!m_persistence_state->persistState(std::nullopt)) {
      DD_LOG(error) << "Failed to save reverted settings! Keeping the verified display topology and recovery record.";
      return RevertResult::PersistenceSaveFailed;
    }

    if (m_audio_context_api->isCaptured()) {
      m_audio_context_api->release();
    }

    // Disable guards
    topology_prep_guard.set_active(false);
    return RevertResult::Ok;
  }

  SettingsManager::RevertResult SettingsManager::revertModifiedHdrStates(const SingleDisplayConfigState::Modified &modified_state, DdGuardFn &guard_fn, bool &system_settings_touched) {
    using enum RevertResult;

    if (modified_state.m_original_hdr_states.empty()) {
      return Ok;
    }

    const auto current_states {m_dd_api->getCurrentHdrStates(win_utils::flattenTopology(modified_state.m_topology))};
    if (current_states == modified_state.m_original_hdr_states) {
      return Ok;
    }

    system_settings_touched = true;

    DD_LOG(info) << "Trying to change back the HDR states to:\n"
                 << toJson(modified_state.m_original_hdr_states);
    if (!m_dd_api->setHdrStates(modified_state.m_original_hdr_states)) {
      // Error already logged
      return RevertingHdrStatesFailed;
    }

    guard_fn = win_utils::hdrStateGuardFn(*m_dd_api, current_states);
    return Ok;
  }

  SettingsManager::RevertResult SettingsManager::revertModifiedDisplayModes(const SingleDisplayConfigState::Modified &modified_state, DdGuardFn &guard_fn, bool &system_settings_touched) {
    using enum RevertResult;

    if (modified_state.m_original_modes.empty()) {
      return Ok;
    }

    const auto current_modes {m_dd_api->getCurrentDisplayModes(win_utils::flattenTopology(modified_state.m_topology))};
    if (current_modes == modified_state.m_original_modes) {
      return Ok;
    }

    DD_LOG(info) << "Trying to change back the display modes to:\n"
                 << toJson(modified_state.m_original_modes);
    if (!m_dd_api->setDisplayModes(modified_state.m_original_modes)) {
      system_settings_touched = true;
      // Error already logged
      return RevertingDisplayModesFailed;
    }

    // It is possible that the display modes will not actually change even though the "current != new" condition is true.
    // This is because of some additional internal checks that determine whether the change is actually needed.
    // Therefore, we should check the current display modes after the fact!
    if (current_modes != m_dd_api->getCurrentDisplayModes(win_utils::flattenTopology(modified_state.m_topology))) {
      system_settings_touched = true;
      guard_fn = win_utils::modeGuardFn(*m_dd_api, current_modes);
    }

    return Ok;
  }

  SettingsManager::RevertResult SettingsManager::revertModifiedPrimaryDevice(const SingleDisplayConfigState::Modified &modified_state, const ActiveTopology &active_topology, DdGuardFn &guard_fn, bool &system_settings_touched) {
    using enum RevertResult;

    if (modified_state.m_original_primary_device.empty()) {
      return Ok;
    }

    const auto current_primary_device {win_utils::getPrimaryDevice(*m_dd_api, active_topology)};
    if (current_primary_device == modified_state.m_original_primary_device) {
      return Ok;
    }

    system_settings_touched = true;

    DD_LOG(info) << "Trying to change back the original primary device to: " << toJson(modified_state.m_original_primary_device);
    if (!m_dd_api->setAsPrimary(modified_state.m_original_primary_device)) {
      // Error already logged
      return RevertingPrimaryDeviceFailed;
    }

    guard_fn = win_utils::primaryGuardFn(*m_dd_api, current_primary_device);
    return Ok;
  }

  SettingsManager::RevertResult SettingsManager::revertModifiedSettings(const ActiveTopology &current_topology, bool &system_settings_touched, bool *switched_topology, const bool preserve_current_outputs) {
    const auto &cached_state {m_persistence_state->getState()};
    if (!cached_state || !cached_state->m_modified.hasModifications()) {
      return RevertResult::Ok;
    }

    if (!m_dd_api->isTopologyValid(cached_state->m_modified.m_topology)) {
      DD_LOG(error) << "Trying to revert modified settings using invalid topology:\n"
                    << toJson(cached_state->m_modified.m_topology);
      return RevertResult::TopologyIsInvalid;
    }

    const auto target_topology {preserve_current_outputs ? preserveCurrentOutputs(cached_state->m_modified.m_topology, current_topology) : cached_state->m_modified.m_topology};
    const bool is_topology_the_same {m_dd_api->isTopologyTheSame(current_topology, target_topology)};
    system_settings_touched = system_settings_touched || !is_topology_the_same;
    if (!is_topology_the_same && !(preserve_current_outputs ? setVerifiedTopology(*m_dd_api, target_topology) : m_dd_api->setTopology(target_topology))) {
      DD_LOG(error) << "Failed to change topology to:\n"
                    << toJson(target_topology);
      return RevertResult::SwitchingTopologyFailed;
    }
    if (switched_topology) {
      *switched_topology = !is_topology_the_same;
    }

    DdGuardFn hdr_guard_fn {noopFn};
    boost::scope::scope_exit<DdGuardFn &> hdr_guard {hdr_guard_fn};
    if (const auto result {revertModifiedHdrStates(cached_state->m_modified, hdr_guard_fn, system_settings_touched)}; result != RevertResult::Ok) {
      // Error already logged
      return result;
    }

    DdGuardFn mode_guard_fn {noopFn};
    boost::scope::scope_exit<DdGuardFn &> mode_guard {mode_guard_fn};
    if (const auto result {revertModifiedDisplayModes(cached_state->m_modified, mode_guard_fn, system_settings_touched)}; result != RevertResult::Ok) {
      // Error already logged
      return result;
    }

    DdGuardFn primary_guard_fn {noopFn};
    boost::scope::scope_exit<DdGuardFn &> primary_guard {primary_guard_fn};
    if (const auto result {revertModifiedPrimaryDevice(cached_state->m_modified, target_topology, primary_guard_fn, system_settings_touched)}; result != RevertResult::Ok) {
      // Error already logged
      return result;
    }

    auto cleared_data {*cached_state};
    cleared_data.m_modified = {cleared_data.m_modified.m_topology};
    if (!m_persistence_state->persistState(cleared_data)) {
      DD_LOG(error) << "Failed to save reverted settings! Undoing changes to modified topology...";
      return RevertResult::PersistenceSaveFailed;
    }

    // Disable guards
    hdr_guard.set_active(false);
    mode_guard.set_active(false);
    primary_guard.set_active(false);
    return RevertResult::Ok;
  }
}  // namespace display_device
