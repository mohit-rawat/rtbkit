#pragma once

#include "rtbkit/core/agent_configuration/agent_configuration_listener.h"
#include "rtbkit/plugins/augmentor/augmentor_base.h"
#include "soa/service/zmq_named_pub_sub.h"
#include "soa/service/service_base.h"

#include <string>
#include <memory>

namespace RTBKIT {

struct FrequencyCapStorage;

struct AgentConfigEntry;

/******************************************************************************/
/* MSISDN AUGMENTOR                                                    */
/******************************************************************************/

struct MsisdnAugmentor :
   public RTBKIT::SyncAugmentor
{

    MsisdnAugmentor(
            std::shared_ptr<Datacratic::ServiceProxies> services,
            const std::string& serviceName,
            const std::string& augmentorName = "adx-msisdn");

    void init();

private:

    virtual RTBKIT::AugmentationList
    onRequest(const RTBKIT::AugmentationRequest& request);
    bool isInMsisdnList(std::string adxid);

    RTBKIT::AgentConfigurationListener agentConfig;
};


} // namespace RTBKIT
