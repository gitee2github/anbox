#ifndef __MOCK_RENDER_CONTROL_ENC_H
#define __MOCK_RENDER_CONTROL_ENC_H

#include <gmock/gmock.h>

struct renderControl_encoder_context_t {
    renderControl_encoder_context_t() {}

    MOCK_METHOD2(rcGetDisplayWidth, int(void *, uint32_t));
    MOCK_METHOD2(rcGetDisplayHeight, int(void *, uint32_t));
    MOCK_METHOD2(rcGetDisplayDpiX, int(void *, uint32_t));
    MOCK_METHOD2(rcGetDisplayDpiY, int(void *, uint32_t));
    MOCK_METHOD2(rcGetDisplayVsyncPeriod, int(void *, uint32_t));
    MOCK_METHOD1(rcPostAllLayersDone, void(void *));
    MOCK_METHOD1(rcPostLayer_enc, void(void *));

    void rcPostLayer(void * self,
                const char* name,
                uint32_t colorBuffer,
                float alpha,
                int32_t sourceCropLeft,
                int32_t sourceCropTop,
                int32_t sourceCropRight,
                int32_t sourceCropBottom,
                int32_t displayFrameLeft,
                int32_t displayFrameTop,
                int32_t displayFrameRight,
                int32_t displayFrameBottom) 
    {
        rcPostLayer_enc(self);
    }
};

#endif  // __MOCK_RENDER_CONTROL_ENC_H
