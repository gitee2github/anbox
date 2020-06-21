#include <stdio.h>
#include <errno.h>

#include <hardware/hwcomposer.h>
#include <utils/Log.h>
#include <cutils/log.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "gralloc_cb.h"
#include "HostConnection.h"

using namespace ::testing;

extern hwc_module_t HAL_MODULE_INFO_SYM;

namespace android {

#define NUM_DISPLAYS    (1)
#define NUM_LAYERS      (2)
#define DEFINE_HOST_CONNECTION() \
    HostConnection *hostCon = HostConnection::get(); \
    renderControl_encoder_context_t *rcEnc = (hostCon ? hostCon->rcEncoder() : NULL)

native_handle_t handles = {
    .version = sizeof(native_handle),
    .numFds = 0,
    .numInts = CB_HANDLE_NUM_INTS(0),
};
buffer_handle_t buffer = &handles;

hwc_layer_1_t layers[2] = {
    {
        .name = "org.anbox.surface.3",
        .flags = HWC_FRAMEBUFFER,
        .handle = buffer,
        .planeAlpha = 255,
        .sourceCrop = {0, 0, 540, 1002},
        .displayFrame = {0, 0, 540, 1002},
        .acquireFenceFd = 0,
        .releaseFenceFd = 0,
    },
    {
        .name = "org.anbox.surface.4",
        .flags = HWC_FRAMEBUFFER,
        .handle = buffer,
        .planeAlpha = 255,
        .sourceCrop = {0, 0, 540, 1002},
        .displayFrame = {0, 0, 540, 1002},
        .acquireFenceFd = 0,
        .releaseFenceFd = 0,
    }
};

TEST(HwcTest, set) {
    int ret;
    hw_module_t const* module;
    hwc_composer_device_1_t* mHwc = NULL;
    hwc_module_t mModule = HAL_MODULE_INFO_SYM;
    module = (hw_module_t*)&mModule;
    module->methods->open(module, HWC_HARDWARE_COMPOSER, (struct hw_device_t**)&mHwc);
    ASSERT_TRUE(NULL != mHwc);
    hwc_display_contents_1_t* mLists[NUM_DISPLAYS];
    size_t size = sizeof(hwc_display_contents_1_t) + 2*sizeof(hwc_layer_1_t);
    hwc_display_contents_1_t* pDisplay = (hwc_display_contents_1_t*)malloc(size);
    ASSERT_TRUE(NULL != pDisplay);
    mLists[0] = pDisplay;
    mLists[0]->outbuf = nullptr;
    mLists[0]->retireFenceFd = 0;
    mLists[0]->outbufAcquireFenceFd = -1;
    mLists[0]->numHwLayers = NUM_LAYERS;
    mLists[0]->hwLayers[0] = layers[0];
    mLists[0]->hwLayers[1] = layers[1];
    layers[0].flags = HWC_OVERLAY;
    layers[1].flags = HWC_FRAMEBUFFER;

    DEFINE_HOST_CONNECTION();
    ASSERT_TRUE(NULL != hostCon);
    ASSERT_TRUE(NULL != rcEnc);
    EXPECT_CALL(*hostCon, flush()).Times(3);
    EXPECT_CALL(*rcEnc, rcPostLayer_enc(_))
        .Times(2);
    EXPECT_CALL(*rcEnc, rcPostAllLayersDone(_))
        .Times(1);
    ret = mHwc->set(mHwc, NUM_DISPLAYS, mLists);
    EXPECT_EQ(ret, 0);

    if (mHwc) {
        mHwc->common.close(&mHwc->common);
        mHwc = NULL;
    }
    free(pDisplay);
    delete(rcEnc);
    delete(hostCon);
    pDisplay = NULL;
    rcEnc = NULL;
    hostCon = NULL;
}
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
