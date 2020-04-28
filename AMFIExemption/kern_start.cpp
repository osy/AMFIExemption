//
// Copyright © 2020 osy86. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSCollectionIterator.h>
#include <Headers/plugin_start.hpp>
#include <Headers/kern_api.hpp>

#define MODULE_SHORT "amfiex"

// Paths

static const char *pathAMFIExtension[] { "/System/Library/Extensions/AppleMobileFileIntegrity.kext/Contents/MacOS/AppleMobileFileIntegrity" };

// All patches for binaries

static KernelPatcher::KextInfo kextAMFIExtension[] = {
    { "com.apple.driver.AppleMobileFileIntegrity", pathAMFIExtension, arrsize(pathAMFIExtension), {true}, {}, KernelPatcher::KextInfo::Unloaded },
};

static mach_vm_address_t orgDeriveCSFlags {};
static mach_vm_address_t orgProcessEntitlements {};

extern OSArray *exemptionsList;

static bool matchExemption(OSString *exemption, OSString *entitlement) {
    size_t prefix_len = exemption->getLength()-1;
    if (exemption->getChar(prefix_len) == '*') { // prefix matching
        if (entitlement->getLength() >= prefix_len) {
            if (strncmp(exemption->getCStringNoCopy(), entitlement->getCStringNoCopy(), prefix_len) == 0) {
                DBGLOG(MODULE_SHORT, "entitlement '%s' matches prefix '%s'", entitlement->getCStringNoCopy(), exemption->getCStringNoCopy());
                return true;
            }
        }
    } else {
        if (exemption->isEqualTo(entitlement)) {
            DBGLOG(MODULE_SHORT, "entitlement '%s' matches exemption '%s'", entitlement->getCStringNoCopy(), exemption->getCStringNoCopy());
            return true;
        }
    }
    return false;
}

static bool matchExemptions(OSString *entitlement) {
    if (!exemptionsList) {
        DBGLOG(MODULE_SHORT, "exemptions list not loaded, no exemptions found");
        return false;
    }
    for (size_t i = 0; i < exemptionsList->getCount(); i++) {
        OSString *exemption = OSDynamicCast(OSString, exemptionsList->getObject(i));
        if (!exemption) {
            DBGLOG(MODULE_SHORT, "invalid exemption not a string at index %u", i);
            continue;
        }
        if (matchExemption(exemption, entitlement)) {
            return true;
        }
    }
    return false;
}

static void applyExemptions(OSDictionary *entitlements) {
    if (entitlements == NULL) {
        SYSLOG(MODULE_SHORT, "NULL entitlement list");
    } else {
        DBGLOG(MODULE_SHORT, "called deriveCSFlagsForEntitlements with %u entitlements", entitlements->getCount());
        OSDictionary *copy = OSDictionary::withDictionary(entitlements);
        
        if (copy == NULL) {
            SYSLOG(MODULE_SHORT, "NULL entitlement copy");
        } else {
            OSCollectionIterator *it = OSCollectionIterator::withCollection(copy);
            
            // clear original entitlement and add back anything not exempt
            entitlements->flushCollection();
            OSObject *object = NULL;
            while ((object = it->getNextObject()) != NULL) {
                OSString *entitlement = OSDynamicCast(OSString, object);
                if (!matchExemptions(entitlement)) {
                    entitlements->setObject(entitlement, copy->getObject(entitlement));
                }
            }
            copy->release(); // release copy
        }
    }
}

// OSX 10.14
static uint32_t patchedDeriveCSFlags(OSDictionary *entitlements, bool *restricted, bool *restrictedBypass) {
    applyExemptions(entitlements);
    return FunctionCast(patchedDeriveCSFlags, orgDeriveCSFlags)(entitlements, restricted, restrictedBypass);
}

// OSX 10.15
static bool patchedProcessEntitlements(OSDictionary *entitlements, uint32_t *flags, bool *restricted, bool *restrictedBypass, void *path, char **err, size_t *err_len) {
    applyExemptions(entitlements);
    return FunctionCast(patchedProcessEntitlements, orgProcessEntitlements)(entitlements, flags, restricted, restrictedBypass, path, err, err_len);
}

static KernelPatcher::RouteRequest requests[] {
    KernelPatcher::RouteRequest("__Z28deriveCSFlagsForEntitlementsP12OSDictionaryPbS1_", patchedDeriveCSFlags, orgDeriveCSFlags),
    KernelPatcher::RouteRequest("__Z19processEntitlementsP12OSDictionaryPjPbS2_P8LazyPathPPcPm", patchedProcessEntitlements, orgProcessEntitlements),
};

static void processKext(void *user, KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
    DBGLOG(MODULE_SHORT, "processing AMFIExemption patches");
    for (size_t i = 0; i < arrsize(kextAMFIExtension); i++) {
        if (kextAMFIExtension[i].loadIndex == index) {
            if (patcher.routeMultiple(index, requests, address, size, true, true)) {
                DBGLOG(MODULE_SHORT, "patched deriveCSFlagsForEntitlements");
            } else {
                SYSLOG(MODULE_SHORT, "failed to find deriveCSFlagsForEntitlements in loaded kext: %d", patcher.getError());
            }
        }
    }
}

// main function
static void pluginStart() {
    LiluAPI::Error error;
    DBGLOG(MODULE_SHORT, "start");
    error = lilu.onKextLoad(kextAMFIExtension, arrsize(kextAMFIExtension), processKext, nullptr);
    if (error != LiluAPI::Error::NoError) {
        SYSLOG(MODULE_SHORT, "failed to register onKextLoad method: %d", error);
    }
}

// Boot args.
static const char *bootargOff[] {
    "-amfiexemptoff"
};
static const char *bootargDebug[] {
    "-amfiexemptdbg"
};
static const char *bootargBeta[] {
    "-amfiexemptbeta"
};

// Plugin configuration.
PluginConfiguration ADDPR(config) {
    xStringify(PRODUCT_NAME),
    parseModuleVersion(xStringify(MODULE_VERSION)),
    LiluAPI::AllowNormal,
    bootargOff,
    arrsize(bootargOff),
    bootargDebug,
    arrsize(bootargDebug),
    bootargBeta,
    arrsize(bootargBeta),
    KernelVersion::Mojave,
    KernelVersion::Catalina,
    pluginStart
};
