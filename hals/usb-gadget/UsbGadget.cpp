/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "android.hardware.usb.gadget.aidl-service"

#include "UsbGadget.h"
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/inotify.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <aidl/android/frameworks/stats/IStats.h>

namespace aidl {
namespace android {
namespace hardware {
namespace usb {
namespace gadget {

UsbGadget::UsbGadget() {
    for (int i = 0; i < 10; i++) {
        if (access(OS_DESC_PATH, R_OK) == 0)
            break;

        ALOGE("configfs setup not done yet (attempt: %i/10)", i + 1);
        sleep(1);
    }

    mUsbController = GetProperty(kUsbController, "");

    if (mUsbController.empty())
        ALOGE("Failed to read controller name");

    mMonitorFfs = std::make_unique<MonitorFfs>(mUsbController.c_str());
}

void currentFunctionsAppliedCallback(bool functionsApplied, void *payload) {
    UsbGadget *gadget = (UsbGadget *)payload;
    gadget->mCurrentUsbFunctionsApplied = functionsApplied;
}

ScopedAStatus UsbGadget::getCurrentUsbFunctions(const shared_ptr<IUsbGadgetCallback>& callback,
                                                int64_t in_transactionId) {
    if (callback == nullptr) {
        return ScopedAStatus::fromExceptionCode(EX_NULL_POINTER);
    }
    ScopedAStatus ret = callback->getCurrentUsbFunctionsCb(
        mCurrentUsbFunctions,
        mCurrentUsbFunctionsApplied ? Status::FUNCTIONS_APPLIED : Status::FUNCTIONS_NOT_APPLIED,
	in_transactionId);
    if (!ret.isOk())
        ALOGE("Call to getCurrentUsbFunctionsCb failed %s", ret.getDescription().c_str());

    return ScopedAStatus::ok();
}

ScopedAStatus UsbGadget::getUsbSpeed(const shared_ptr<IUsbGadgetCallback> &callback,
	int64_t in_transactionId) {
    std::string current_speed;
    if (ReadFileToString("/sys/class/udc/" + mUsbController + "/current_speed", &current_speed)) {
        current_speed = Trim(current_speed);
        ALOGI("current USB speed is %s", current_speed.c_str());
        if (current_speed == "low-speed")
            mUsbSpeed = UsbSpeed::LOWSPEED;
        else if (current_speed == "full-speed")
            mUsbSpeed = UsbSpeed::FULLSPEED;
        else if (current_speed == "high-speed")
            mUsbSpeed = UsbSpeed::HIGHSPEED;
        else if (current_speed == "super-speed")
            mUsbSpeed = UsbSpeed::SUPERSPEED;
        else if (current_speed == "super-speed-plus")
            mUsbSpeed = UsbSpeed::SUPERSPEED_10Gb;
        else if (current_speed == "UNKNOWN")
            mUsbSpeed = UsbSpeed::UNKNOWN;
        else
            mUsbSpeed = UsbSpeed::UNKNOWN;
    } else {
        ALOGE("Fail to read current speed");
        mUsbSpeed = UsbSpeed::UNKNOWN;
    }

    if (callback) {
        ScopedAStatus ret = callback->getUsbSpeedCb(mUsbSpeed, in_transactionId);

        if (!ret.isOk())
            ALOGE("Call to getUsbSpeedCb failed %s", ret.getDescription().c_str());
    }

    return ScopedAStatus::ok();
}



Status UsbGadget::tearDownGadget() {
    if ((int)resetGadget() != 0) return Status::ERROR;

    if (mMonitorFfs->isMonitorRunning()) {
        mMonitorFfs->reset();
    } else {
        ALOGI("mMonitor not running");
    }
    return Status::SUCCESS;
}

ScopedAStatus UsbGadget::reset(const shared_ptr<IUsbGadgetCallback> &callback,
        int64_t in_transactionId) {
    if (!WriteStringToFile("none", PULLUP_PATH)) {
        ALOGI("Gadget cannot be pulled down");
        if (callback)
            callback->resetCb(Status::ERROR, in_transactionId);
        return ScopedAStatus::ok();
    }

    usleep(kDisconnectWaitUs);

    if (!WriteStringToFile(mUsbController, PULLUP_PATH)) {
        ALOGI("Gadget cannot be pulled up");
        if (callback)
            callback->resetCb(Status::ERROR, in_transactionId);
        return ScopedAStatus::ok();
    }

    if (callback)
        callback->resetCb(Status::SUCCESS, in_transactionId);
    return ScopedAStatus::ok();
}

Status setVidPid(const char *vid, const char *pid) {
    auto result = ::android::hardware::usb::gadget::setVidPid(vid, pid);

    return (Status)result;
}

static Status validateAndSetVidPid(uint64_t functions) {
    Status ret = Status::SUCCESS;

    switch (functions) {
        case static_cast<uint64_t>(GadgetFunction::MTP):
            ret = setVidPid("0x18d1", "0x4ee1");
            break;
        case GadgetFunction::ADB | GadgetFunction::MTP:
            ret = setVidPid("0x18d1", "0x4ee2");
            break;
        case static_cast<uint64_t>(GadgetFunction::RNDIS):
            ret = setVidPid("0x18d1", "0x4ee3");
            break;
        case GadgetFunction::ADB | GadgetFunction::RNDIS:
            ret = setVidPid("0x18d1", "0x4ee4");
            break;
        case static_cast<uint64_t>(GadgetFunction::PTP):
            ret = setVidPid("0x18d1", "0x4ee5");
            break;
        case GadgetFunction::ADB | GadgetFunction::PTP:
            ret = setVidPid("0x18d1", "0x4ee6");
            break;
        case static_cast<uint64_t>(GadgetFunction::ADB):
            ret = setVidPid("0x18d1", "0x4ee7");
            break;
        case static_cast<uint64_t>(GadgetFunction::MIDI):
            ret = setVidPid("0x18d1", "0x4ee8");
            break;
        case GadgetFunction::ADB | GadgetFunction::MIDI:
            ret = setVidPid("0x18d1", "0x4ee9");
            break;
        case static_cast<uint64_t>(GadgetFunction::NCM):
            ret = setVidPid("0x18d1", "0x4eeb");
            break;
        case GadgetFunction::ADB | GadgetFunction::NCM:
            ret = setVidPid("0x18d1", "0x4eec");
            break;
        case static_cast<uint64_t>(GadgetFunction::ACCESSORY):
            ret = setVidPid("0x18d1", "0x2d00");
            break;
        case GadgetFunction::ADB | GadgetFunction::ACCESSORY:
            ret = setVidPid("0x18d1", "0x2d01");
            break;
        case static_cast<uint64_t>(GadgetFunction::AUDIO_SOURCE):
            ret = setVidPid("0x18d1", "0x2d02");
            break;
        case GadgetFunction::ADB | GadgetFunction::AUDIO_SOURCE:
            ret = setVidPid("0x18d1", "0x2d03");
            break;
        case GadgetFunction::ACCESSORY | GadgetFunction::AUDIO_SOURCE:
            ret = setVidPid("0x18d1", "0x2d04");
            break;
        case GadgetFunction::ADB | GadgetFunction::ACCESSORY |
                GadgetFunction::AUDIO_SOURCE:
            ret = setVidPid("0x18d1", "0x2d05");
            break;
        default:
            ALOGE("Combination not supported");
            ret = Status::CONFIGURATION_NOT_SUPPORTED;
    }
    return ret;
}

Status UsbGadget::setupFunctions(long functions,
	const shared_ptr<IUsbGadgetCallback> &callback, uint64_t timeout,
	int64_t in_transactionId) {
    bool ffsEnabled = false;
    int i = 0;
    if (timeout == 0) {
        ALOGI("timeout not setup");
    }

    if ((int)addGenericAndroidFunctions(mMonitorFfs.get(), functions, &ffsEnabled, &i) != 0)
        return Status::ERROR;

    if ((functions & GadgetFunction::ADB) != 0) {
        ffsEnabled = true;
        if ((int)addAdb(mMonitorFfs.get(), &i) != 0) return Status::ERROR;
    }

    // Pull up the gadget right away when there are no ffs functions.
    if (!ffsEnabled) {
        if (!WriteStringToFile(mUsbController, PULLUP_PATH)) return Status::ERROR;
        mCurrentUsbFunctionsApplied = true;
        if (callback)
            callback->setCurrentUsbFunctionsCb(functions, Status::SUCCESS, in_transactionId);
        return Status::SUCCESS;
    }

    mMonitorFfs->registerFunctionsAppliedCallback(&currentFunctionsAppliedCallback, this);
    // Monitors the ffs paths to pull up the gadget when descriptors are written.
    // Also takes of the pulling up the gadget again if the userspace process
    // dies and restarts.
    mMonitorFfs->startMonitor();

    if (callback) {
        bool pullup = mMonitorFfs->waitForPullUp(timeout);
        auto ret = callback->setCurrentUsbFunctionsCb(
                functions, pullup ? Status::SUCCESS : Status::ERROR, in_transactionId);
        if (!ret.isOk()) ALOGE("setCurrentUsbFunctionsCb error");
    }

    return Status::SUCCESS;
}

ScopedAStatus UsbGadget::setCurrentUsbFunctions(int64_t functions,
                                               const shared_ptr<IUsbGadgetCallback> &callback,
					       int64_t timeoutMs,
					       int64_t in_transactionId) {
    std::unique_lock<std::mutex> lk(mLockSetCurrentFunction);

    mCurrentUsbFunctions = functions;
    mCurrentUsbFunctionsApplied = false;

    // Unlink the gadget and stop the monitor if running.
    Status status = tearDownGadget();
    if (status != Status::SUCCESS) {
        goto error;
    }

    ALOGI("Returned from tearDown gadget");

    // Leave the gadget pulled down to give time for the host to sense disconnect.
    usleep(kDisconnectWaitUs);

    if (functions == GadgetFunction::NONE) {
        if (callback == NULL)
            return ScopedAStatus::fromServiceSpecificErrorWithMessage(
                -1, "callback == NULL");
        ScopedAStatus ret = callback->setCurrentUsbFunctionsCb(functions, Status::SUCCESS, in_transactionId);
        if (!ret.isOk())
            ALOGE("Error while calling setCurrentUsbFunctionsCb %s", ret.getDescription().c_str());
        return ScopedAStatus::fromServiceSpecificErrorWithMessage(
                -1, "Error while calling setCurrentUsbFunctionsCb");
    }

    status = validateAndSetVidPid(functions);

    if (status != Status::SUCCESS) {
        goto error;
    }

    status = setupFunctions(functions, callback, timeoutMs, in_transactionId);
    if (status != Status::SUCCESS) {
        goto error;
    }

    ALOGI("Usb Gadget setcurrent functions called successfully");
    return ScopedAStatus::fromServiceSpecificErrorWithMessage(
                -1, "Usb Gadget setcurrent functions called successfully");


error:
    ALOGI("Usb Gadget setcurrent functions failed");
    if (callback == NULL)
        return ScopedAStatus::fromServiceSpecificErrorWithMessage(
                -1, "Usb Gadget setcurrent functions failed");
    ScopedAStatus ret = callback->setCurrentUsbFunctionsCb(functions, status, in_transactionId);
    if (!ret.isOk())
        ALOGE("Error while calling setCurrentUsbFunctionsCb %s", ret.getDescription().c_str());
    return ScopedAStatus::fromServiceSpecificErrorWithMessage(
                -1, "Error while calling setCurrentUsbFunctionsCb");
}
}  // namespace gadget
}  // namespace usb
}  // namespace hardware
}  // namespace android
}  // aidl
