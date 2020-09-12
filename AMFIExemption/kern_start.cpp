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

// All patches for binaries

static mach_vm_address_t origDictionarySet {};

extern OSArray *exemptionsList;

static bool matchExemption(OSString *exemption, OSString *entitlement) {
    int prefix_len = exemption->getLength()-1;
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
    for (int i = 0; i < exemptionsList->getCount(); i++) {
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

static void patched_csblob_entitlements_dictionary_set(void *csblob, void *entitlements) {
    applyExemptions((OSDictionary *)entitlements);
    FunctionCast(patched_csblob_entitlements_dictionary_set, origDictionarySet)(csblob, entitlements);
}

// main function
static void pluginStart() {
    LiluAPI::Error error;
    DBGLOG(MODULE_SHORT, "start");
    error = lilu.onPatcherLoad([](void *user, KernelPatcher &patcher){
        DBGLOG(MODULE_SHORT, "patching csblob_entitlements_dictionary_set");
        mach_vm_address_t kern = patcher.solveSymbol(KernelPatcher::KernelID, "_csblob_entitlements_dictionary_set");
        
        if (patcher.getError() == KernelPatcher::Error::NoError) {
            origDictionarySet = patcher.routeFunctionLong(kern, reinterpret_cast<mach_vm_address_t>(patched_csblob_entitlements_dictionary_set), true, true);
            
            if (patcher.getError() != KernelPatcher::Error::NoError) {
                SYSLOG(MODULE_SHORT, "failed to hook _csblob_entitlements_dictionary_set");
            } else {
                DBGLOG(MODULE_SHORT, "hooked _csblob_entitlements_dictionary_set");
            }
        } else {
            SYSLOG(MODULE_SHORT, "failed to find _csblob_entitlements_dictionary_set");
        }
    });
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
    KernelVersion::BigSur,
    pluginStart
};
