#include <cutils/log.h>
#include "HostConnection.h"

HostConnection::HostConnection() {}

HostConnection::~HostConnection() {}

HostConnection* HostConnection::get()
{
    static HostConnection *m_pHostConnection;
    if (NULL == m_pHostConnection) {
        m_pHostConnection = new HostConnection();
    }
    return m_pHostConnection;
}

renderControl_encoder_context_t* HostConnection::rcEncoder()
{
    static renderControl_encoder_context_t *m_rcEnc;
    if (NULL == m_rcEnc) {
        m_rcEnc = new renderControl_encoder_context_t();
    }
    return m_rcEnc;
}
