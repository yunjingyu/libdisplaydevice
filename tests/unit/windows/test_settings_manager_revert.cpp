// system includes
#include <algorithm>
#include <functional>

// local includes
#include "display_device/windows/settings_manager.h"
#include "display_device/windows/settings_utils.h"
#include "fixtures/fixtures.h"
#include "fixtures/mock_audio_context.h"
#include "fixtures/mock_settings_persistence.h"
#include "utils/comparison.h"
#include "utils/helpers.h"
#include "utils/mock_win_display_device.h"

namespace {
  // Convenience keywords for GMock
  using ::testing::_;
  using ::testing::HasSubstr;
  using ::testing::InSequence;
  using ::testing::Return;
  using ::testing::StrictMock;

  // Additional convenience global const(s)
  const display_device::ActiveTopology CURRENT_TOPOLOGY {{"DeviceId4"}};
  const display_device::HdrStateMap CURRENT_MODIFIED_HDR_STATES {
    {"DeviceId1", {display_device::HdrState::Enabled}},
    {"DeviceId3", std::nullopt}
  };
  const display_device::DeviceDisplayModeMap CURRENT_MODIFIED_DISPLAY_MODES {
    {"DeviceId1", {{123, 456}, {120, 1}}},
    {"DeviceId3", {{456, 123}, {60, 1}}}
  };
  const std::string CURRENT_MODIFIED_PRIMARY_DEVICE {"DeviceId3"};
  const display_device::ActiveTopology FULL_EXTENDED_TOPOLOGY {
    {"DeviceId1"},
    {"DeviceId3"},
    {"DeviceId4"}
  };

  // Test fixture(s) for this file
  class SettingsManagerRevertMocked: public BaseTest {
  public:
    display_device::SettingsManager &getImpl() {
      if (!m_impl) {
        m_impl = std::make_unique<display_device::SettingsManager>(m_dd_api, m_audio_context_api, std::make_unique<display_device::PersistentState>(m_settings_persistence_api), display_device::WinWorkarounds {
                                                                                                                                                                                   .m_hdr_blank_delay = std::chrono::milliseconds {123}  // Value is irrelevant for the tests
                                                                                                                                                                                 });
      }

      return *m_impl;
    }

    void expectedDefaultCallsUntilModifiedSettings(InSequence &sequence /* To ensure that sequence is created outside this scope */, const std::optional<display_device::SingleDisplayConfigState> &state = ut_consts::SDCS_FULL) const {
      EXPECT_CALL(*m_settings_persistence_api, load())
        .Times(1)
        .WillOnce(Return(serializeState(state)))
        .RetiresOnSaturation();
      expectedDefaultCallsUntilModifiedSettingsNoPersistence(sequence);
    }

    void expectedDefaultCallsUntilModifiedSettingsNoPersistence(InSequence & /* To ensure that sequence is created outside this scope */) const {
      EXPECT_CALL(*m_dd_api, isApiAccessAvailable())
        .Times(1)
        .WillOnce(Return(true))
        .RetiresOnSaturation();
      EXPECT_CALL(*m_dd_api, getCurrentTopology())
        .Times(1)
        .WillOnce(Return(CURRENT_TOPOLOGY))
        .RetiresOnSaturation();
      EXPECT_CALL(*m_dd_api, isTopologyValid(CURRENT_TOPOLOGY))
        .Times(1)
        .WillOnce(Return(true))
        .RetiresOnSaturation();
    }

    void expectedDefaultMofifiedTopologyCalls(InSequence & /* To ensure that sequence is created outside this scope */) {
      EXPECT_CALL(*m_dd_api, isTopologyValid(ut_consts::SDCS_FULL->m_modified.m_topology))
        .Times(1)
        .WillOnce(Return(true))
        .RetiresOnSaturation();
      EXPECT_CALL(*m_dd_api, isTopologyTheSame(CURRENT_TOPOLOGY, FULL_EXTENDED_TOPOLOGY))
        .Times(1)
        .WillOnce(Return(false))
        .RetiresOnSaturation();
      EXPECT_CALL(*m_dd_api, setTopology(FULL_EXTENDED_TOPOLOGY))
        .Times(1)
        .WillOnce(Return(true))
        .RetiresOnSaturation();
      EXPECT_CALL(*m_dd_api, getCurrentTopology()).WillOnce(Return(FULL_EXTENDED_TOPOLOGY)).RetiresOnSaturation();
      EXPECT_CALL(*m_dd_api, isTopologyTheSame(FULL_EXTENDED_TOPOLOGY, FULL_EXTENDED_TOPOLOGY)).WillOnce(Return(true)).RetiresOnSaturation();
    }

    void expectedDefaultHdrStateGuardInitCall(InSequence & /* To ensure that sequence is created outside this scope */) const {
      EXPECT_CALL(*m_dd_api, getCurrentHdrStates(display_device::win_utils::flattenTopology(ut_consts::SDCS_FULL->m_modified.m_topology)))
        .Times(1)
        .WillOnce(Return(CURRENT_MODIFIED_HDR_STATES))
        .RetiresOnSaturation();
    }

    void expectedDefaultHdrStateGuardCall(InSequence & /* To ensure that sequence is created outside this scope */) {
      EXPECT_CALL(*m_dd_api, setHdrStates(CURRENT_MODIFIED_HDR_STATES))
        .Times(1)
        .WillOnce(Return(true))
        .RetiresOnSaturation();
    }

    void expectedDefaultDisplayModeGuardInitCall(InSequence & /* To ensure that sequence is created outside this scope */) const {
      EXPECT_CALL(*m_dd_api, getCurrentDisplayModes(display_device::win_utils::flattenTopology(ut_consts::SDCS_FULL->m_modified.m_topology)))
        .Times(1)
        .WillOnce(Return(CURRENT_MODIFIED_DISPLAY_MODES))
        .RetiresOnSaturation();
    }

    void expectedDefaultDisplayModeGuardCall(InSequence & /* To ensure that sequence is created outside this scope */) {
      EXPECT_CALL(*m_dd_api, setDisplayModes(CURRENT_MODIFIED_DISPLAY_MODES))
        .Times(1)
        .WillOnce(Return(true))
        .RetiresOnSaturation();
    }

    void expectedDefaultPrimaryDeviceGuardInitCall(InSequence & /* To ensure that sequence is created outside this scope */) const {
      EXPECT_CALL(*m_dd_api, isPrimary("DeviceId1"))
        .Times(1)
        .WillOnce(Return(false))
        .RetiresOnSaturation();
      EXPECT_CALL(*m_dd_api, isPrimary(CURRENT_MODIFIED_PRIMARY_DEVICE))
        .Times(1)
        .WillOnce(Return(true))
        .RetiresOnSaturation();
    }

    void expectedDefaultPrimaryDeviceGuardCall(InSequence & /* To ensure that sequence is created outside this scope */) {
      EXPECT_CALL(*m_dd_api, setAsPrimary(CURRENT_MODIFIED_PRIMARY_DEVICE))
        .Times(1)
        .WillOnce(Return(true))
        .RetiresOnSaturation();
    }

    void expectedDefaultHdrStateCall(InSequence & /* To ensure that sequence is created outside this scope */) {
      EXPECT_CALL(*m_dd_api, setHdrStates(ut_consts::SDCS_FULL->m_modified.m_original_hdr_states))
        .Times(1)
        .WillOnce(Return(true))
        .RetiresOnSaturation();
    }

    void expectedDefaultDisplayModeCall(InSequence & /* To ensure that sequence is created outside this scope */, bool has_changed = true) {
      EXPECT_CALL(*m_dd_api, setDisplayModes(ut_consts::SDCS_FULL->m_modified.m_original_modes))
        .Times(1)
        .WillOnce(Return(true))
        .RetiresOnSaturation();
      EXPECT_CALL(*m_dd_api, getCurrentDisplayModes(display_device::win_utils::flattenTopology(ut_consts::SDCS_FULL->m_modified.m_topology)))
        .Times(1)
        .WillOnce(Return(has_changed ? ut_consts::SDCS_FULL->m_modified.m_original_modes : CURRENT_MODIFIED_DISPLAY_MODES))
        .RetiresOnSaturation();
    }

    void expectedDefaultPrimaryDeviceCall(InSequence & /* To ensure that sequence is created outside this scope */) {
      EXPECT_CALL(*m_dd_api, setAsPrimary(ut_consts::SDCS_FULL->m_modified.m_original_primary_device))
        .Times(1)
        .WillOnce(Return(true))
        .RetiresOnSaturation();
    }

    void expectedDefaultRevertModifiedSettingsCall(InSequence &sequence /* To ensure that sequence is created outside this scope */, const std::optional<display_device::SingleDisplayConfigState> &state = ut_consts::SDCS_FULL) {
      auto expected_persistent_input {state};
      expected_persistent_input->m_modified = {expected_persistent_input->m_modified.m_topology};

      expectedDefaultMofifiedTopologyCalls(sequence);
      expectedDefaultHdrStateGuardInitCall(sequence);
      expectedDefaultHdrStateCall(sequence);
      expectedDefaultDisplayModeGuardInitCall(sequence);
      expectedDefaultDisplayModeCall(sequence);
      expectedDefaultPrimaryDeviceGuardInitCall(sequence);
      expectedDefaultPrimaryDeviceCall(sequence);
      EXPECT_CALL(*m_settings_persistence_api, store(*serializeState(expected_persistent_input)))
        .Times(1)
        .WillOnce(Return(true))
        .RetiresOnSaturation();
    }

    void expectedDefaultInitialTopologyCalls(InSequence & /* To ensure that sequence is created outside this scope */, const std::optional<display_device::SingleDisplayConfigState> &state = ut_consts::SDCS_FULL, const bool modified_topology_applied = true) {
      EXPECT_CALL(*m_dd_api, isTopologyValid(state->m_initial.m_topology))
        .Times(1)
        .WillOnce(Return(true))
        .RetiresOnSaturation();
      const auto before {modified_topology_applied ? FULL_EXTENDED_TOPOLOGY : CURRENT_TOPOLOGY};
      auto staged {state->m_initial.m_topology};
      auto included {display_device::win_utils::flattenTopology(staged)};
      for (const auto &group : before) {
        std::vector<std::string> remaining;
        for (const auto &device : group) {
          if (included.insert(device).second) {
            remaining.push_back(device);
          }
        }
        if (!remaining.empty()) {
          staged.push_back(remaining);
        }
      }
      const bool already_staged {display_device::win_utils::flattenTopology(before) == display_device::win_utils::flattenTopology(staged)};
      EXPECT_CALL(*m_dd_api, isTopologyTheSame(before, staged)).WillOnce(Return(already_staged)).RetiresOnSaturation();
      if (!already_staged) {
        EXPECT_CALL(*m_dd_api, setTopology(staged)).WillOnce(Return(true)).RetiresOnSaturation();
        EXPECT_CALL(*m_dd_api, getCurrentTopology()).WillOnce(Return(staged)).RetiresOnSaturation();
        EXPECT_CALL(*m_dd_api, isTopologyTheSame(staged, staged)).WillOnce(Return(true)).RetiresOnSaturation();
      }
      EXPECT_CALL(*m_dd_api, isTopologyTheSame(staged, state->m_initial.m_topology)).WillOnce(Return(false)).RetiresOnSaturation();
      EXPECT_CALL(*m_dd_api, setTopology(state->m_initial.m_topology))
        .Times(1)
        .WillOnce(Return(true))
        .RetiresOnSaturation();
      EXPECT_CALL(*m_dd_api, getCurrentTopology()).WillOnce(Return(state->m_initial.m_topology)).RetiresOnSaturation();
      EXPECT_CALL(*m_dd_api, isTopologyTheSame(state->m_initial.m_topology, state->m_initial.m_topology)).WillOnce(Return(true)).RetiresOnSaturation();
    }

    void expectedDefaultFinalPersistenceCalls(InSequence & /* To ensure that sequence is created outside this scope */) {
      EXPECT_CALL(*m_settings_persistence_api, clear())
        .Times(1)
        .WillOnce(Return(true))
        .RetiresOnSaturation();
    }

    void expectedDefaultAudioContextCalls(InSequence & /* To ensure that sequence is created outside this scope */, const bool audio_captured) {
      EXPECT_CALL(*m_audio_context_api, isCaptured())
        .Times(1)
        .WillOnce(Return(audio_captured))
        .RetiresOnSaturation();

      if (audio_captured) {
        EXPECT_CALL(*m_audio_context_api, release())
          .Times(1)
          .RetiresOnSaturation();
      }
    }

    void expectedDefaultTopologyGuardCall(InSequence & /* To ensure that sequence is created outside this scope */) {
      EXPECT_CALL(*m_dd_api, getCurrentTopology()).WillOnce(Return(FULL_EXTENDED_TOPOLOGY)).RetiresOnSaturation();
      EXPECT_CALL(*m_dd_api, isTopologyTheSame(FULL_EXTENDED_TOPOLOGY, CURRENT_TOPOLOGY)).WillOnce(Return(false)).RetiresOnSaturation();
      EXPECT_CALL(*m_dd_api, setTopology(CURRENT_TOPOLOGY)).WillOnce(Return(true)).RetiresOnSaturation();
      EXPECT_CALL(*m_dd_api, getCurrentTopology()).WillOnce(Return(CURRENT_TOPOLOGY)).RetiresOnSaturation();
      EXPECT_CALL(*m_dd_api, isTopologyTheSame(CURRENT_TOPOLOGY, CURRENT_TOPOLOGY)).WillOnce(Return(true)).RetiresOnSaturation();
    }

    void expectedHdrWorkaroundCalls(InSequence & /* To ensure that sequence is created outside this scope */) const {
      // Using the "failure" path, to keep it simple
      EXPECT_CALL(*m_dd_api, getCurrentTopology())
        .Times(1)
        .WillOnce(Return(display_device::ActiveTopology {}))
        .RetiresOnSaturation();
      EXPECT_CALL(*m_dd_api, isTopologyValid(display_device::ActiveTopology {}))
        .Times(1)
        .WillOnce(Return(false))
        .RetiresOnSaturation();
    }

    std::shared_ptr<StrictMock<display_device::MockWinDisplayDevice>> m_dd_api {std::make_shared<StrictMock<display_device::MockWinDisplayDevice>>()};
    std::shared_ptr<StrictMock<display_device::MockSettingsPersistence>> m_settings_persistence_api {std::make_shared<StrictMock<display_device::MockSettingsPersistence>>()};
    std::shared_ptr<StrictMock<display_device::MockAudioContext>> m_audio_context_api {std::make_shared<StrictMock<display_device::MockAudioContext>>()};
    std::unique_ptr<display_device::SettingsManager> m_impl;
  };

  // Specialized TEST macro(s) for this test
#define TEST_F_S_MOCKED(...) DD_MAKE_TEST(TEST_F, SettingsManagerRevertMocked, __VA_ARGS__)
}  // namespace

TEST_F_S_MOCKED(NoSettingsAvailable) {
  InSequence sequence;
  EXPECT_CALL(*m_settings_persistence_api, load())
    .Times(1)
    .WillOnce(Return(serializeState(ut_consts::SDCS_EMPTY)));

  EXPECT_EQ(getImpl().revertSettings(), display_device::SettingsManager::RevertResult::NoChangesToRevert);
  EXPECT_EQ(getImpl().revertSettings(), display_device::SettingsManager::RevertResult::NoChangesToRevert);
}

TEST_F_S_MOCKED(ApplyUnavailableLeavesNoChangesToRevert) {
  InSequence sequence;
  EXPECT_CALL(*m_settings_persistence_api, load())
    .Times(1)
    .WillOnce(Return(serializeState(ut_consts::SDCS_EMPTY)));
  EXPECT_CALL(*m_dd_api, isApiAccessAvailable())
    .Times(1)
    .WillOnce(Return(false));

  EXPECT_EQ(getImpl().applySettings({.m_device_id = "DeviceId4"}), display_device::SettingsManager::ApplyResult::ApiTemporarilyUnavailable);
  EXPECT_EQ(getImpl().revertSettings(), display_device::SettingsManager::RevertResult::NoChangesToRevert);
}

TEST_F_S_MOCKED(NoApiAccess) {
  InSequence sequence;
  EXPECT_CALL(*m_settings_persistence_api, load())
    .Times(1)
    .WillOnce(Return(serializeState(ut_consts::SDCS_FULL)));
  EXPECT_CALL(*m_dd_api, isApiAccessAvailable())
    .Times(1)
    .WillOnce(Return(false));

  EXPECT_EQ(getImpl().revertSettings(), display_device::SettingsManager::RevertResult::ApiTemporarilyUnavailable);
}

TEST_F_S_MOCKED(InvalidCurrentTopology) {
  InSequence sequence;
  EXPECT_CALL(*m_settings_persistence_api, load())
    .Times(1)
    .WillOnce(Return(serializeState(ut_consts::SDCS_FULL)));
  EXPECT_CALL(*m_dd_api, isApiAccessAvailable())
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(*m_dd_api, getCurrentTopology())
    .Times(1)
    .WillOnce(Return(CURRENT_TOPOLOGY));
  EXPECT_CALL(*m_dd_api, isTopologyValid(CURRENT_TOPOLOGY))
    .Times(1)
    .WillOnce(Return(false));

  EXPECT_EQ(getImpl().revertSettings(), display_device::SettingsManager::RevertResult::TopologyIsInvalid);
}

TEST_F_S_MOCKED(RevertModifiedSettings, InvalidModifiedTopology) {
  InSequence sequence;
  expectedDefaultCallsUntilModifiedSettings(sequence);

  EXPECT_CALL(*m_dd_api, isTopologyValid(ut_consts::SDCS_FULL->m_modified.m_topology))
    .Times(1)
    .WillOnce(Return(false));
  expectedDefaultTopologyGuardCall(sequence);
  expectedHdrWorkaroundCalls(sequence);

  EXPECT_EQ(getImpl().revertSettings(), display_device::SettingsManager::RevertResult::TopologyIsInvalid);
}

TEST_F_S_MOCKED(RevertModifiedSettings, FailedToSetModifiedTopology) {
  InSequence sequence;
  expectedDefaultCallsUntilModifiedSettings(sequence);

  EXPECT_CALL(*m_dd_api, isTopologyValid(ut_consts::SDCS_FULL->m_modified.m_topology))
    .Times(1)
    .WillOnce(Return(true));
  EXPECT_CALL(*m_dd_api, isTopologyTheSame(CURRENT_TOPOLOGY, FULL_EXTENDED_TOPOLOGY))
    .Times(1)
    .WillOnce(Return(false));
  EXPECT_CALL(*m_dd_api, setTopology(FULL_EXTENDED_TOPOLOGY))
    .Times(1)
    .WillOnce(Return(false));
  EXPECT_CALL(*m_dd_api, getCurrentTopology()).WillOnce(Return(CURRENT_TOPOLOGY)).RetiresOnSaturation();
  EXPECT_CALL(*m_dd_api, isTopologyTheSame(CURRENT_TOPOLOGY, FULL_EXTENDED_TOPOLOGY)).WillOnce(Return(false)).RetiresOnSaturation();
  expectedDefaultTopologyGuardCall(sequence);
  expectedHdrWorkaroundCalls(sequence);

  EXPECT_EQ(getImpl().revertSettings(), display_device::SettingsManager::RevertResult::SwitchingTopologyFailed);
}

TEST_F_S_MOCKED(RevertModifiedSettings, FailedToRevertHdrStates) {
  auto sdcs_stripped {ut_consts::SDCS_FULL};
  sdcs_stripped->m_modified.m_original_modes.clear();
  sdcs_stripped->m_modified.m_original_primary_device.clear();

  InSequence sequence;
  expectedDefaultCallsUntilModifiedSettings(sequence, sdcs_stripped);
  expectedDefaultMofifiedTopologyCalls(sequence);
  expectedDefaultHdrStateGuardInitCall(sequence);
  EXPECT_CALL(*m_dd_api, setHdrStates(ut_consts::SDCS_FULL->m_modified.m_original_hdr_states))
    .Times(1)
    .WillOnce(Return(false))
    .RetiresOnSaturation();

  expectedDefaultTopologyGuardCall(sequence);
  expectedHdrWorkaroundCalls(sequence);

  EXPECT_EQ(getImpl().revertSettings(), display_device::SettingsManager::RevertResult::RevertingHdrStatesFailed);
}

TEST_F_S_MOCKED(RevertModifiedSettings, FailedToRevertDisplayModes) {
  auto sdcs_stripped {ut_consts::SDCS_FULL};
  sdcs_stripped->m_modified.m_original_hdr_states.clear();
  sdcs_stripped->m_modified.m_original_primary_device.clear();

  InSequence sequence;
  expectedDefaultCallsUntilModifiedSettings(sequence, sdcs_stripped);
  expectedDefaultMofifiedTopologyCalls(sequence);
  expectedDefaultDisplayModeGuardInitCall(sequence);
  EXPECT_CALL(*m_dd_api, setDisplayModes(ut_consts::SDCS_FULL->m_modified.m_original_modes))
    .Times(1)
    .WillOnce(Return(false))
    .RetiresOnSaturation();

  expectedDefaultTopologyGuardCall(sequence);
  expectedHdrWorkaroundCalls(sequence);

  EXPECT_EQ(getImpl().revertSettings(), display_device::SettingsManager::RevertResult::RevertingDisplayModesFailed);
}

TEST_F_S_MOCKED(RevertModifiedSettings, RevertedDisplayModes, PersistenceFailed, GuardNotInvoked) {
  auto sdcs_stripped {ut_consts::SDCS_FULL};
  sdcs_stripped->m_modified.m_original_hdr_states.clear();
  sdcs_stripped->m_modified.m_original_primary_device.clear();

  auto expected_persistent_input {ut_consts::SDCS_FULL};
  expected_persistent_input->m_modified = {expected_persistent_input->m_modified.m_topology};

  InSequence sequence;
  expectedDefaultCallsUntilModifiedSettings(sequence, sdcs_stripped);
  expectedDefaultMofifiedTopologyCalls(sequence);
  expectedDefaultDisplayModeGuardInitCall(sequence);
  expectedDefaultDisplayModeCall(sequence, false);
  EXPECT_CALL(*m_settings_persistence_api, store(*serializeState(expected_persistent_input)))
    .Times(1)
    .WillOnce(Return(false))
    .RetiresOnSaturation();

  expectedDefaultTopologyGuardCall(sequence);
  expectedHdrWorkaroundCalls(sequence);

  EXPECT_EQ(getImpl().revertSettings(), display_device::SettingsManager::RevertResult::PersistenceSaveFailed);
}

TEST_F_S_MOCKED(RevertModifiedSettings, FailedToRevertPrimaryDevice) {
  auto sdcs_stripped {ut_consts::SDCS_FULL};
  sdcs_stripped->m_modified.m_original_hdr_states.clear();
  sdcs_stripped->m_modified.m_original_modes.clear();

  InSequence sequence;
  expectedDefaultCallsUntilModifiedSettings(sequence, sdcs_stripped);
  expectedDefaultMofifiedTopologyCalls(sequence);
  expectedDefaultPrimaryDeviceGuardInitCall(sequence);
  EXPECT_CALL(*m_dd_api, setAsPrimary(ut_consts::SDCS_FULL->m_modified.m_original_primary_device))
    .Times(1)
    .WillOnce(Return(false))
    .RetiresOnSaturation();

  expectedDefaultTopologyGuardCall(sequence);
  expectedHdrWorkaroundCalls(sequence);

  EXPECT_EQ(getImpl().revertSettings(), display_device::SettingsManager::RevertResult::RevertingPrimaryDeviceFailed);
}

TEST_F_S_MOCKED(RevertModifiedSettings, FailedToSetPersistence) {
  auto expected_persistent_input {ut_consts::SDCS_FULL};
  expected_persistent_input->m_modified = {expected_persistent_input->m_modified.m_topology};

  InSequence sequence;
  expectedDefaultCallsUntilModifiedSettings(sequence);
  expectedDefaultMofifiedTopologyCalls(sequence);
  expectedDefaultHdrStateGuardInitCall(sequence);
  expectedDefaultHdrStateCall(sequence);
  expectedDefaultDisplayModeGuardInitCall(sequence);
  expectedDefaultDisplayModeCall(sequence);
  expectedDefaultPrimaryDeviceGuardInitCall(sequence);
  expectedDefaultPrimaryDeviceCall(sequence);
  EXPECT_CALL(*m_settings_persistence_api, store(*serializeState(expected_persistent_input)))
    .Times(1)
    .WillOnce(Return(false))
    .RetiresOnSaturation();

  expectedDefaultPrimaryDeviceGuardCall(sequence);
  expectedDefaultDisplayModeGuardCall(sequence);
  expectedDefaultHdrStateGuardCall(sequence);
  expectedDefaultTopologyGuardCall(sequence);
  expectedHdrWorkaroundCalls(sequence);

  EXPECT_EQ(getImpl().revertSettings(), display_device::SettingsManager::RevertResult::PersistenceSaveFailed);
}

TEST_F_S_MOCKED(TopologyGuard, CurrentTopologyUsedAsFallback) {
  InSequence sequence;
  expectedDefaultCallsUntilModifiedSettings(sequence);
  expectedDefaultRevertModifiedSettingsCall(sequence);
  EXPECT_CALL(*m_dd_api, isTopologyValid(ut_consts::SDCS_FULL->m_initial.m_topology))
    .Times(1)
    .WillOnce(Return(false))
    .RetiresOnSaturation();
  EXPECT_CALL(*m_dd_api, getCurrentTopology()).WillOnce(Return(CURRENT_TOPOLOGY)).RetiresOnSaturation();
  EXPECT_CALL(*m_dd_api, isTopologyTheSame(CURRENT_TOPOLOGY, CURRENT_TOPOLOGY)).WillOnce(Return(true)).RetiresOnSaturation();
  expectedHdrWorkaroundCalls(sequence);

  EXPECT_EQ(getImpl().revertSettings(), display_device::SettingsManager::RevertResult::TopologyIsInvalid);
}

TEST_F_S_MOCKED(TopologyGuard, SystemSettingsUntouched) {
  InSequence sequence;
  expectedDefaultCallsUntilModifiedSettings(sequence);
  EXPECT_CALL(*m_dd_api, isTopologyValid(ut_consts::SDCS_FULL->m_modified.m_topology))
    .Times(1)
    .WillOnce(Return(false))
    .RetiresOnSaturation();
  EXPECT_CALL(*m_dd_api, getCurrentTopology()).WillOnce(Return(CURRENT_TOPOLOGY)).RetiresOnSaturation();
  EXPECT_CALL(*m_dd_api, isTopologyTheSame(CURRENT_TOPOLOGY, CURRENT_TOPOLOGY)).WillOnce(Return(true)).RetiresOnSaturation();

  EXPECT_EQ(getImpl().revertSettings(), display_device::SettingsManager::RevertResult::TopologyIsInvalid);
}

TEST_F_S_MOCKED(TopologyGuard, FailedToSetTopology) {
  InSequence sequence;
  expectedDefaultCallsUntilModifiedSettings(sequence);
  EXPECT_CALL(*m_dd_api, isTopologyValid(ut_consts::SDCS_FULL->m_modified.m_topology))
    .Times(1)
    .WillOnce(Return(false))
    .RetiresOnSaturation();
  EXPECT_CALL(*m_dd_api, getCurrentTopology()).WillOnce(Return(FULL_EXTENDED_TOPOLOGY)).RetiresOnSaturation();
  EXPECT_CALL(*m_dd_api, isTopologyTheSame(FULL_EXTENDED_TOPOLOGY, CURRENT_TOPOLOGY)).WillOnce(Return(false)).RetiresOnSaturation();
  EXPECT_CALL(*m_dd_api, setTopology(CURRENT_TOPOLOGY)).WillOnce(Return(false)).RetiresOnSaturation();
  EXPECT_CALL(*m_dd_api, getCurrentTopology()).WillOnce(Return(FULL_EXTENDED_TOPOLOGY)).RetiresOnSaturation();
  EXPECT_CALL(*m_dd_api, isTopologyTheSame(FULL_EXTENDED_TOPOLOGY, CURRENT_TOPOLOGY)).WillOnce(Return(false)).RetiresOnSaturation();
  EXPECT_CALL(*m_dd_api, isTopologyTheSame(CURRENT_TOPOLOGY, CURRENT_TOPOLOGY)).WillOnce(Return(true)).RetiresOnSaturation();
  expectedHdrWorkaroundCalls(sequence);

  EXPECT_EQ(getImpl().revertSettings(), display_device::SettingsManager::RevertResult::TopologyIsInvalid);
}

TEST_F_S_MOCKED(InvalidInitialTopology) {
  InSequence sequence;
  expectedDefaultCallsUntilModifiedSettings(sequence);
  expectedDefaultRevertModifiedSettingsCall(sequence);
  EXPECT_CALL(*m_dd_api, isTopologyValid(ut_consts::SDCS_FULL->m_initial.m_topology))
    .Times(1)
    .WillOnce(Return(false))
    .RetiresOnSaturation();

  expectedDefaultTopologyGuardCall(sequence);
  expectedHdrWorkaroundCalls(sequence);

  EXPECT_EQ(getImpl().revertSettings(), display_device::SettingsManager::RevertResult::TopologyIsInvalid);
}

TEST_F_S_MOCKED(FailedToSetInitialTopology) {
  InSequence sequence;
  expectedDefaultCallsUntilModifiedSettings(sequence);
  expectedDefaultRevertModifiedSettingsCall(sequence);
  EXPECT_CALL(*m_dd_api, isTopologyValid(ut_consts::SDCS_FULL->m_initial.m_topology))
    .Times(1)
    .WillOnce(Return(true))
    .RetiresOnSaturation();
  EXPECT_CALL(*m_dd_api, isTopologyTheSame(FULL_EXTENDED_TOPOLOGY, FULL_EXTENDED_TOPOLOGY)).WillOnce(Return(true)).RetiresOnSaturation();
  EXPECT_CALL(*m_dd_api, isTopologyTheSame(FULL_EXTENDED_TOPOLOGY, ut_consts::SDCS_FULL->m_initial.m_topology)).WillOnce(Return(false)).RetiresOnSaturation();
  EXPECT_CALL(*m_dd_api, setTopology(ut_consts::SDCS_FULL->m_initial.m_topology)).WillOnce(Return(false)).RetiresOnSaturation();
  EXPECT_CALL(*m_dd_api, getCurrentTopology()).WillOnce(Return(FULL_EXTENDED_TOPOLOGY)).RetiresOnSaturation();
  EXPECT_CALL(*m_dd_api, isTopologyTheSame(FULL_EXTENDED_TOPOLOGY, ut_consts::SDCS_FULL->m_initial.m_topology)).WillOnce(Return(false)).RetiresOnSaturation();
  EXPECT_CALL(*m_dd_api, getCurrentTopology()).WillOnce(Return(FULL_EXTENDED_TOPOLOGY)).RetiresOnSaturation();
  EXPECT_CALL(*m_dd_api, isTopologyTheSame(FULL_EXTENDED_TOPOLOGY, FULL_EXTENDED_TOPOLOGY)).WillOnce(Return(true)).RetiresOnSaturation();
  expectedHdrWorkaroundCalls(sequence);

  EXPECT_EQ(getImpl().revertSettings(), display_device::SettingsManager::RevertResult::SwitchingTopologyFailed);
}

TEST_F_S_MOCKED(FailedToClearPersistence) {
  InSequence sequence;
  expectedDefaultCallsUntilModifiedSettings(sequence);
  expectedDefaultRevertModifiedSettingsCall(sequence);
  expectedDefaultInitialTopologyCalls(sequence);
  EXPECT_CALL(*m_settings_persistence_api, clear())
    .Times(1)
    .WillOnce(Return(false))
    .RetiresOnSaturation();

  EXPECT_CALL(*m_dd_api, getCurrentTopology()).WillOnce(Return(ut_consts::SDCS_FULL->m_initial.m_topology)).RetiresOnSaturation();
  EXPECT_CALL(*m_dd_api, isTopologyTheSame(ut_consts::SDCS_FULL->m_initial.m_topology, ut_consts::SDCS_FULL->m_initial.m_topology)).WillOnce(Return(true)).RetiresOnSaturation();
  expectedHdrWorkaroundCalls(sequence);

  EXPECT_EQ(getImpl().revertSettings(), display_device::SettingsManager::RevertResult::PersistenceSaveFailed);
}

TEST_F_S_MOCKED(SuccesfullyReverted, WithAudioCapture) {
  InSequence sequence;
  expectedDefaultCallsUntilModifiedSettings(sequence);
  expectedDefaultRevertModifiedSettingsCall(sequence);
  expectedDefaultInitialTopologyCalls(sequence);
  expectedDefaultFinalPersistenceCalls(sequence);
  expectedDefaultAudioContextCalls(sequence, true);
  expectedHdrWorkaroundCalls(sequence);

  EXPECT_EQ(getImpl().revertSettings(), display_device::SettingsManager::RevertResult::Ok);
  EXPECT_EQ(getImpl().revertSettings(), display_device::SettingsManager::RevertResult::NoChangesToRevert);
}

TEST_F_S_MOCKED(SuccesfullyReverted, NoAudioCapture) {
  InSequence sequence;
  expectedDefaultCallsUntilModifiedSettings(sequence);
  expectedDefaultRevertModifiedSettingsCall(sequence);
  expectedDefaultInitialTopologyCalls(sequence);
  expectedDefaultFinalPersistenceCalls(sequence);
  expectedDefaultAudioContextCalls(sequence, false);
  expectedHdrWorkaroundCalls(sequence);

  EXPECT_EQ(getImpl().revertSettings(), display_device::SettingsManager::RevertResult::Ok);
  EXPECT_EQ(getImpl().revertSettings(), display_device::SettingsManager::RevertResult::NoChangesToRevert);
}

TEST_F_S_MOCKED(SuccesfullyReverted, TopologySetToBackToInitialSinceItWasChangedToModified) {
  auto initial_state {ut_consts::SDCS_FULL};
  initial_state->m_initial.m_topology = CURRENT_TOPOLOGY;

  InSequence sequence;
  expectedDefaultCallsUntilModifiedSettings(sequence, initial_state);
  expectedDefaultRevertModifiedSettingsCall(sequence, initial_state);
  expectedDefaultInitialTopologyCalls(sequence, initial_state);
  expectedDefaultFinalPersistenceCalls(sequence);
  expectedDefaultAudioContextCalls(sequence, false);
  expectedHdrWorkaroundCalls(sequence);

  EXPECT_EQ(getImpl().revertSettings(), display_device::SettingsManager::RevertResult::Ok);
}

TEST_F_S_MOCKED(RevertModifiedSettings, CachedSettingsAreUpdated) {
  InSequence sequence;
  expectedDefaultCallsUntilModifiedSettings(sequence);
  expectedDefaultRevertModifiedSettingsCall(sequence);
  EXPECT_CALL(*m_dd_api, isTopologyValid(ut_consts::SDCS_FULL->m_initial.m_topology))
    .Times(1)
    .WillOnce(Return(false))
    .RetiresOnSaturation();

  expectedDefaultTopologyGuardCall(sequence);
  expectedHdrWorkaroundCalls(sequence);
  EXPECT_EQ(getImpl().revertSettings(), display_device::SettingsManager::RevertResult::TopologyIsInvalid);

  expectedDefaultCallsUntilModifiedSettingsNoPersistence(sequence);
  // No need for expectedDefaultRevertModifiedSettingsCall anymore.
  expectedDefaultInitialTopologyCalls(sequence, ut_consts::SDCS_FULL, false);
  expectedDefaultFinalPersistenceCalls(sequence);
  expectedDefaultAudioContextCalls(sequence, false);
  expectedHdrWorkaroundCalls(sequence);

  EXPECT_EQ(getImpl().revertSettings(), display_device::SettingsManager::RevertResult::Ok);
}

TEST_F_S_MOCKED(CurrentSettingsMatchPersistentOnes) {
  display_device::SingleDisplayConfigState state {
    {CURRENT_TOPOLOGY,
     {"DeviceId4"}},
    {CURRENT_TOPOLOGY,
     CURRENT_MODIFIED_DISPLAY_MODES,
     CURRENT_MODIFIED_HDR_STATES,
     "DeviceId4"}
  };

  InSequence sequence;
  expectedDefaultCallsUntilModifiedSettings(sequence, state);
  EXPECT_CALL(*m_dd_api, isTopologyValid(state.m_modified.m_topology)).Times(1).WillOnce(Return(true)).RetiresOnSaturation();
  EXPECT_CALL(*m_dd_api, isTopologyTheSame(CURRENT_TOPOLOGY, state.m_modified.m_topology)).Times(1).WillOnce(Return(true)).RetiresOnSaturation();
  EXPECT_CALL(*m_dd_api, getCurrentHdrStates(display_device::win_utils::flattenTopology(state.m_modified.m_topology))).Times(1).WillOnce(Return(CURRENT_MODIFIED_HDR_STATES)).RetiresOnSaturation();
  EXPECT_CALL(*m_dd_api, getCurrentDisplayModes(display_device::win_utils::flattenTopology(state.m_modified.m_topology))).Times(1).WillOnce(Return(CURRENT_MODIFIED_DISPLAY_MODES)).RetiresOnSaturation();
  EXPECT_CALL(*m_dd_api, isPrimary(state.m_modified.m_original_primary_device)).Times(1).WillOnce(Return(true)).RetiresOnSaturation();

  auto cleared_modifications {state};
  cleared_modifications.m_modified = {cleared_modifications.m_modified.m_topology};
  EXPECT_CALL(*m_settings_persistence_api, store(*serializeState(cleared_modifications))).Times(1).WillOnce(Return(true)).RetiresOnSaturation();
  EXPECT_CALL(*m_dd_api, isTopologyValid(state.m_initial.m_topology)).Times(1).WillOnce(Return(true)).RetiresOnSaturation();
  EXPECT_CALL(*m_dd_api, isTopologyTheSame(CURRENT_TOPOLOGY, state.m_initial.m_topology)).Times(1).WillOnce(Return(true)).RetiresOnSaturation();
  EXPECT_CALL(*m_dd_api, isTopologyTheSame(CURRENT_TOPOLOGY, state.m_initial.m_topology)).WillOnce(Return(true)).RetiresOnSaturation();
  EXPECT_CALL(*m_dd_api, getCurrentTopology()).WillOnce(Return(CURRENT_TOPOLOGY)).RetiresOnSaturation();
  EXPECT_CALL(*m_dd_api, isTopologyTheSame(CURRENT_TOPOLOGY, state.m_initial.m_topology)).WillOnce(Return(true)).RetiresOnSaturation();
  expectedDefaultFinalPersistenceCalls(sequence);
  expectedDefaultAudioContextCalls(sequence, false);

  EXPECT_EQ(getImpl().revertSettings(), display_device::SettingsManager::RevertResult::Ok);
}

namespace {
  /** @brief Stateful display API fixture that exposes partial and falsely successful transitions. */
  class StagedRestore: public BaseTest {
  public:
    using Topology = display_device::ActiveTopology;  ///< Active output groups.
    using Result = display_device::SettingsManager::RevertResult;  ///< Restore outcome.

    /** @brief Construct a manager backed by a persistent recovery record and observable outputs. */
    void init() {
      ON_CALL(*api, isApiAccessAvailable()).WillByDefault(Return(true));
      ON_CALL(*api, isTopologyValid(_)).WillByDefault([](const Topology &topology) {
        return !topology.empty();
      });
      ON_CALL(*api, isTopologyTheSame(_, _)).WillByDefault([](Topology left, Topology right) {
        for (auto *topology : {&left, &right}) {
          for (auto &group : *topology) {
            std::ranges::sort(group);
          }
          std::ranges::sort(*topology);
        }
        return left == right;
      });
      ON_CALL(*api, getCurrentTopology()).WillByDefault([this]() {
        return active;
      });
      ON_CALL(*api, setTopology(_)).WillByDefault([this](const Topology &target) {
        writes.push_back(target);
        if (transition) {
          return transition(target);
        }
        active = target;
        return true;
      });
      EXPECT_CALL(*api, enumAvailableDevices()).Times(0);
      ON_CALL(*persistence, load()).WillByDefault([this]() {
        return serializeState(state);
      });
      ON_CALL(*persistence, clear()).WillByDefault([this]() {
        ++clear_attempts;
        if (fail_clear) {
          return false;
        }
        state = std::nullopt;
        return true;
      });
      manager = std::make_unique<display_device::SettingsManager>(api, audio, std::make_unique<display_device::PersistentState>(persistence), display_device::WinWorkarounds {});
    }

    const Topology original {{"left", "right"}};  ///< Original physical outputs are cloned.
    const Topology stream {{"stream"}};  ///< Streaming output before recovery.
    const Topology staged {{"left", "right"}, {"stream"}};  ///< Physical clone and streaming output together.
    Topology active {stream};  ///< State reported by readback.
    std::optional<display_device::SingleDisplayConfigState> state {display_device::SingleDisplayConfigState {{original, {"left", "right"}}, {stream}}};  ///< Persistent original and streaming layouts.
    std::vector<Topology> writes;  ///< Requested topology mutations.
    std::function<bool(const Topology &)> transition;  ///< Optional fault injection at the actual display API boundary.
    unsigned int clear_attempts {};  ///< Number of persistence removal attempts.
    bool fail_clear {};  ///< Simulate storage failure after a verified restore.
    std::shared_ptr<::testing::NiceMock<display_device::MockWinDisplayDevice>> api {std::make_shared<::testing::NiceMock<display_device::MockWinDisplayDevice>>()};  ///< Stateful API mock.
    std::shared_ptr<::testing::NiceMock<display_device::MockSettingsPersistence>> persistence {std::make_shared<::testing::NiceMock<display_device::MockSettingsPersistence>>()};  ///< Storage mock.
    std::shared_ptr<::testing::NiceMock<display_device::MockAudioContext>> audio {std::make_shared<::testing::NiceMock<display_device::MockAudioContext>>()};  ///< Audio mock.
    std::unique_ptr<display_device::SettingsManager> manager;  ///< Manager under test.
  };
}  // namespace

TEST_F(StagedRestore, VerifiesPhysicalCloneBeforeRemovingStream) {
  init();
  EXPECT_EQ(manager->revertSettings(), Result::Ok);
  EXPECT_EQ(writes, (std::vector<Topology> {staged, original}));
  EXPECT_EQ(active, original);
  EXPECT_FALSE(state);
  EXPECT_EQ(clear_attempts, 1u);
}

TEST_F(StagedRestore, StageFailureKeepsCaptureAndRecoveryRecord) {
  transition = [](const Topology &) {
    return false;
  };
  init();
  EXPECT_EQ(manager->revertSettings(), Result::SwitchingTopologyFailed);
  EXPECT_EQ(writes, (std::vector<Topology> {staged}));
  EXPECT_EQ(active, stream);
  EXPECT_TRUE(state);
  EXPECT_EQ(clear_attempts, 0u);
}

TEST_F(StagedRestore, StageReadbackMismatchRestoresCaptureBeforeRetry) {
  transition = [this](const Topology &target) {
    active = writes.size() == 1 ? Topology {{"left"}} : target;
    return true;
  };
  init();
  EXPECT_EQ(manager->revertSettings(), Result::SwitchingTopologyFailed);
  EXPECT_EQ(writes, (std::vector<Topology> {staged, stream}));
  EXPECT_EQ(active, stream);
  EXPECT_TRUE(state);
  EXPECT_EQ(clear_attempts, 0u);
  transition = {};
  EXPECT_EQ(manager->revertSettings(), Result::Ok);
  EXPECT_EQ(active, original);
  EXPECT_FALSE(state);
}

TEST_F(StagedRestore, FinalFailureRollsBackToVerifiedStage) {
  transition = [this](const Topology &target) {
    if (writes.size() == 2) {
      active.clear();
      return false;
    }
    active = target;
    return true;
  };
  init();
  EXPECT_EQ(manager->revertSettings(), Result::SwitchingTopologyFailed);
  EXPECT_EQ(writes, (std::vector<Topology> {staged, original, staged}));
  EXPECT_EQ(active, staged);
  EXPECT_TRUE(state);
  EXPECT_EQ(clear_attempts, 0u);
}

TEST_F(StagedRestore, FinalReadbackMismatchDoesNotClearRecovery) {
  transition = [this](const Topology &target) {
    active = writes.size() == 2 ? stream : target;
    return true;
  };
  init();
  EXPECT_EQ(manager->revertSettings(), Result::SwitchingTopologyFailed);
  EXPECT_EQ(writes, (std::vector<Topology> {staged, original, staged}));
  EXPECT_EQ(active, staged);
  EXPECT_TRUE(state);
  EXPECT_EQ(clear_attempts, 0u);
}

TEST_F(StagedRestore, StageRollbackMismatchFallsBackToOriginalCapture) {
  transition = [this](const Topology &target) {
    active = writes.size() == 2 || writes.size() == 3 ? Topology {} : target;
    return true;
  };
  init();
  EXPECT_EQ(manager->revertSettings(), Result::SwitchingTopologyFailed);
  EXPECT_EQ(writes, (std::vector<Topology> {staged, original, staged, stream}));
  EXPECT_EQ(active, stream);
  EXPECT_TRUE(state);
  EXPECT_EQ(clear_attempts, 0u);
}

TEST_F(StagedRestore, PersistenceFailureKeepsVerifiedPhysicalOutputsAndCanRetry) {
  fail_clear = true;
  init();
  EXPECT_EQ(manager->revertSettings(), Result::PersistenceSaveFailed);
  EXPECT_EQ(writes, (std::vector<Topology> {staged, original}));
  EXPECT_EQ(active, original);
  EXPECT_TRUE(state);
  fail_clear = false;
  EXPECT_EQ(manager->revertSettings(), Result::Ok);
  EXPECT_EQ(writes.size(), 2u);
  EXPECT_FALSE(state);
  EXPECT_EQ(clear_attempts, 2u);
}

TEST_F(StagedRestore, AlreadyRestoredTopologyDoesNotCycleDisplays) {
  active = original;
  init();
  EXPECT_EQ(manager->revertSettings(), Result::Ok);
  EXPECT_TRUE(writes.empty());
  EXPECT_EQ(active, original);
  EXPECT_FALSE(state);
}

TEST_F(StagedRestore, PreservesAdditionalActiveCaptureAndTargetCloneGroups) {
  active = {{"stream", "capture"}};
  init();
  EXPECT_EQ(manager->revertSettings(), Result::Ok);
  EXPECT_EQ(writes.front(), (Topology {{"left", "right"}, {"stream", "capture"}}));
  EXPECT_EQ(active, original);
}

TEST_F(StagedRestore, EmptyActiveTopologyRecoversUsingPersistedOutputs) {
  active.clear();
  init();
  EXPECT_EQ(manager->revertSettings(), Result::Ok);
  EXPECT_EQ(writes, (std::vector<Topology> {staged, original}));
  EXPECT_EQ(active, original);
  EXPECT_FALSE(state);
}

TEST_F(StagedRestore, EmptyTopologyStageFailureRecoversCaptureAndRemainsPending) {
  active.clear();
  transition = [this](const Topology &target) {
    if (writes.size() == 1) {
      return false;
    }
    active = target;
    return true;
  };
  init();
  EXPECT_EQ(manager->revertSettings(), Result::SwitchingTopologyFailed);
  EXPECT_EQ(writes, (std::vector<Topology> {staged, stream}));
  EXPECT_EQ(active, stream);
  EXPECT_TRUE(state);
  EXPECT_EQ(clear_attempts, 0u);
}

TEST_F(StagedRestore, EmptyTopologyPartialRecoveryKeepsOutputsThatBecameActive) {
  active.clear();
  transition = [this](const Topology &) {
    active = {{"left"}};
    return true;
  };
  init();
  EXPECT_EQ(manager->revertSettings(), Result::SwitchingTopologyFailed);
  EXPECT_EQ(writes, (std::vector<Topology> {staged}));
  EXPECT_EQ(active, (Topology {{"left"}}));
  EXPECT_TRUE(state);
  EXPECT_EQ(clear_attempts, 0u);
}

TEST_F(StagedRestore, RevertingModifiedSettingsKeepsCurrentPhysicalOutputs) {
  active = original;
  state->m_modified.m_original_hdr_states = {{"stream", display_device::HdrState::Disabled}};
  init();
  ON_CALL(*api, getCurrentHdrStates(_)).WillByDefault(Return(display_device::HdrStateMap {{"stream", display_device::HdrState::Enabled}}));
  ON_CALL(*api, setHdrStates(_)).WillByDefault(Return(true));
  ON_CALL(*persistence, store(_)).WillByDefault(Return(true));
  EXPECT_EQ(manager->revertSettings(), Result::Ok);
  EXPECT_EQ(writes, (std::vector<Topology> {{{"stream"}, {"left", "right"}}, original}));
  EXPECT_EQ(active, original);
  EXPECT_FALSE(state);
}

TEST_F(StagedRestore, ModifiedSettingsStageRequiresPhysicalOutputReadback) {
  active = original;
  state->m_modified.m_original_hdr_states = {{"stream", display_device::HdrState::Disabled}};
  transition = [this](const Topology &target) {
    active = writes.size() == 1 ? stream : target;
    return true;
  };
  init();
  EXPECT_CALL(*api, setHdrStates(_)).Times(0);
  EXPECT_EQ(manager->revertSettings(), Result::SwitchingTopologyFailed);
  EXPECT_EQ(writes, (std::vector<Topology> {{{"stream"}, {"left", "right"}}, original}));
  EXPECT_EQ(active, original);
  EXPECT_TRUE(state);
  EXPECT_EQ(clear_attempts, 0u);
}

TEST_F(StagedRestore, PersistenceFailureRestoresPrimaryOutsideModifiedTopology) {
  active = {{"stream"}, {"left"}, {"preserved-primary"}};
  state->m_initial = {active, {"preserved-primary"}};
  state->m_modified.m_topology = {{"stream"}, {"left"}};
  state->m_modified.m_original_primary_device = "left";
  std::string primary {"preserved-primary"};
  std::vector<std::string> primary_writes;
  init();
  ON_CALL(*api, isPrimary(_)).WillByDefault([&primary](const std::string &device) {
    return device == primary;
  });
  ON_CALL(*api, setAsPrimary(_)).WillByDefault([&primary, &primary_writes](const std::string &device) {
    primary_writes.push_back(device);
    primary = device;
    return true;
  });
  ON_CALL(*persistence, store(_)).WillByDefault(Return(false));
  EXPECT_CALL(*api, setAsPrimary("")).Times(0);
  EXPECT_CALL(*api, setAsPrimary("left")).Times(1);
  EXPECT_CALL(*api, setAsPrimary("preserved-primary")).Times(1);
  EXPECT_EQ(manager->revertSettings(), Result::PersistenceSaveFailed);
  EXPECT_EQ(primary_writes, (std::vector<std::string> {"left", "preserved-primary"}));
  EXPECT_EQ(primary, "preserved-primary");
  EXPECT_EQ(active, state->m_initial.m_topology);
  EXPECT_TRUE(writes.empty());
  EXPECT_EQ(clear_attempts, 0u);
}
