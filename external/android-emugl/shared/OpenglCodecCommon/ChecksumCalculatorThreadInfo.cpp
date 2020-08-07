/*
* Copyright (C) 2016 The Android Open Source Project
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "ChecksumCalculatorThreadInfo.h"

#include "emugl/common/crash_reporter.h"
#include "emugl/common/lazy_instance.h"
#include "emugl/common/thread_store.h"

#include <stdio.h>
#include <atomic>
#include <string>

namespace {

class ChecksumCalculatorThreadStore : public ::emugl::ThreadStore {
public:
    ChecksumCalculatorThreadStore() : ::emugl::ThreadStore(NULL) {}
};

#ifdef TRACE_CHECKSUMHELPER
std::atomic<size_t> sNumInstances(0);
#endif  // TRACE_CHECKSUMHELPER

}

static ::emugl::LazyInstance<ChecksumCalculatorThreadStore> s_tls =
        LAZY_INSTANCE_INIT;

static ChecksumCalculatorThreadInfo* getChecksumCalculatorThreadInfo() {
    return static_cast<ChecksumCalculatorThreadInfo*>(s_tls->get());
}

ChecksumCalculatorThreadInfo::ChecksumCalculatorThreadInfo() {
    LOG_CHECKSUMHELPER(
        "%s: Checksum thread created (%u instances)\n", __FUNCTION__,
        (size_t)sNumInstances);
    s_tls->set(this);
}

ChecksumCalculatorThreadInfo::~ChecksumCalculatorThreadInfo() {
    LOG_CHECKSUMHELPER(
        "%s: GLprotocol destroyed (%u instances)\n", __FUNCTION__,
        (size_t)sNumInstances);
    s_tls->set(NULL);
}

uint32_t ChecksumCalculatorThreadInfo::getVersion() {
    uint32_t ret = 0;
    if(getChecksumCalculatorThreadInfo() != nullptr) {
        ret = getChecksumCalculatorThreadInfo()->m_protocol.getVersion();
    }
    return ret;
}

bool ChecksumCalculatorThreadInfo::setVersion(uint32_t version) {
    bool ret = false;
    if(getChecksumCalculatorThreadInfo() != nullptr) {
       ret = getChecksumCalculatorThreadInfo()->m_protocol.setVersion(version);
    }
    return ret; 
}

size_t ChecksumCalculatorThreadInfo::checksumByteSize() {
    size_t ret = 0;
    if(getChecksumCalculatorThreadInfo() != nullptr) {
       ret = getChecksumCalculatorThreadInfo()->m_protocol.checksumByteSize();
    }
    return ret;
}

bool ChecksumCalculatorThreadInfo::writeChecksum(void* buf,
                                                 size_t bufLen,
                                                 void* outputChecksum,
                                                 size_t outputChecksumLen) {
    bool ret = false;
    if(getChecksumCalculatorThreadInfo() != nullptr) {
        ChecksumCalculator& protocol =
                    getChecksumCalculatorThreadInfo()->m_protocol;
        protocol.addBuffer(buf, bufLen);
        ret =  protocol.writeChecksum(outputChecksum, outputChecksumLen);   
    }
    return ret;
}

bool ChecksumCalculatorThreadInfo::validate(void* buf,
                                            size_t bufLen,
                                            void* checksum,
                                            size_t checksumLen) {
    bool ret = false;
    if(getChecksumCalculatorThreadInfo() != nullptr) {
      ChecksumCalculator& protocol =
              getChecksumCalculatorThreadInfo()->m_protocol;
      protocol.addBuffer(buf, bufLen);
      ret =  protocol.validate(checksum, checksumLen);
    }
    return ret;    
}

void ChecksumCalculatorThreadInfo::validOrDie(void* buf,
                                              size_t bufLen,
                                              void* checksum,
                                              size_t checksumLen,
                                              const char* message) {
    // We should actually call crashhandler_die(message), but I don't think we
    // can link to that library from here
    if (!validate(buf, bufLen, checksum, checksumLen)) {
        emugl_crash_reporter(emugl::LogLevel::FATAL, message);
    }
}
