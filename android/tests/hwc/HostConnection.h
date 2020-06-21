#ifndef __MOCK_HOST_CONNECTION_H
#define __MOCK_HOST_CONNECTION_H

#include <gmock/gmock.h>
#include "mock_renderControl_enc.h"

class HostConnection
{
public:
    static HostConnection *get();
    ~HostConnection();

    MOCK_METHOD0(flush, void(void));
    renderControl_encoder_context_t *rcEncoder();

private:
    HostConnection();
    static HostConnection* m_pHostConnection;
    static renderControl_encoder_context_t *m_rcEnc;
};

#endif  // __MOCK_HOST_CONNECTION_H
