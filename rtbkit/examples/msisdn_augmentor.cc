#include "msisdn_augmentor.h"

#include "rtbkit/core/agent_configuration/agent_configuration_listener.h"
#include "rtbkit/core/agent_configuration/agent_config.h"
#include "rtbkit/plugins/augmentor/augmentor_base.h"
#include "rtbkit/common/bid_request.h"
#include "soa/service/zmq_named_pub_sub.h"
#include "jml/utils/exc_assert.h"

#include <unordered_map>
#include <mutex>

#include "aerospike/as_config.h"
#include "aerospike/aerospike_key.h"

using namespace std;

namespace RTBKIT {

  as_error err;
  as_config config;
  aerospike as;
  
	
MsisdnAugmentor::
MsisdnAugmentor(
        std::shared_ptr<Datacratic::ServiceProxies> services,
        const string& serviceName,
        const string& augmentorName) :
    SyncAugmentor(augmentorName, serviceName, services),
    agentConfig(getZmqContext())
{
    recordHit("up");
	as_config_init(&config);
	config.hosts[0].addr = "127.0.0.1";
	config.hosts[0].port = 3000;
	aerospike_init(&as, &config);
	aerospike_connect(&as, &err);
}


/** Sets up the internal components of the augmentor.

    Note that SyncAugmentorBase is a MessageLoop so we can attach all our
    other service providers to our message loop to cut down on the number of
    polling threads which in turns reduces the number of context switches.
*/
void
MsisdnAugmentor::
init()
{
    SyncAugmentor::init(2 /* numThreads */);

    /* Manages all the communications with the AgentConfigurationService. */
    agentConfig.init(getServices()->config);
    addSource("MsisdnAugmentor::agentConfig", agentConfig);
}

	typedef unsigned long long timestamp_t;
	static timestamp_t
	get_timestamp ()
	{
		struct timeval now;
		gettimeofday (&now, NULL);
		return  now.tv_usec + (timestamp_t)now.tv_sec * 1000000;
	}	

RTBKIT::AugmentationList
MsisdnAugmentor::
onRequest(const RTBKIT::AugmentationRequest& request)
{
  std::cout<<"reached onRequeest in msisdn augmentor"<<std::endl;
  timestamp_t t0 = get_timestamp();
  RTBKIT::AugmentationList result;
  
  for (const string& agent : request.agents) {
    
    RTBKIT::AgentConfigEntry config = agentConfig.getAgentEntry(agent);
    
    /* When a new agent comes online there's a race condition where the
       router may send us a bid request for that agent before we receive
       its configuration. This check keeps us safe in that scenario.
    */
    if (!config.valid()) {
      recordHit("unknownConfig");
      continue;
    }
    bool validId = false;
    const RTBKIT::AccountKey& account = config.config->account;
    if(request.bidRequest->user->id.toString().empty()){
      recordHit("accounts." + account[0] + ".msisdn not in list");
    }else{
      validId = isInMsisdnList(request.bidRequest->user->id.toString());
    }
    std::cout<<"validid in onrequest : "<<validId<<std::endl;
    if (validId) {
      result[account].tags.insert("pass-adx-msisdn");
      recordHit("accounts." + account[0] + ".msisdn in list");
    }
    else{
      recordHit("accounts." + account[0] + ".msisdn not in list");
      std::cout<<"count > cap"<<std::endl;
    }
  }
  timestamp_t t1 = get_timestamp();
  double secs = (t1 - t0) / 1000000.0L;
  std::cout<<"time taken : "<<secs<<std::endl;
  return result;
}

bool
MsisdnAugmentor::
isInMsisdnList(std::string adxid)
{
  std::cout<<"adxid : "<<adxid<<std::endl;
        bool validid = false;
	as_key key;
	as_key_init_str(&key, "audience", "adxmsisdn", adxid.c_str());
	as_record* p_rec = NULL;
	std::string val = "imp";
	if (aerospike_key_get(&as, &err, NULL, &key, &p_rec) != AEROSPIKE_OK) {
		fprintf(stderr, "err(%d) %s at [%s:%d]\n", err.code, err.message, err.file, err.line);
		validid = false;
	}else{
	  validid = true;
	}
	as_record_destroy(p_rec);
    std::cout<<"validid in isInMsisdnList : "<<validid<<std::endl;
	return validid;
}

} // namespace RTBKIT
