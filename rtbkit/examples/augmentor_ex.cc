/** augmentor_ex.cc                                 -*- C++ -*-
    Rémi Attab, 14 Feb 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Augmentor example that can be used to do extremely simplistic frequency
    capping.

*/

#include "augmentor_ex.h"

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


/******************************************************************************/
/* FREQUENCY CAP STORAGE                                                      */
/******************************************************************************/

/** A very primitive storage for frequency cap events.

    Ideally this should be replaced by some kind of low latency persistent
    storage (eg. Redis).

    Note that the locking scheme for this class is unlikely to scale well. A
    better scheme is left as an exercise to the reader.
 */
struct FrequencyCapStorage
{
    /** Returns the number of times an ad for the given account has been shown
        to the given user.
     */
    size_t get(const RTBKIT::AccountKey& account, const RTBKIT::UserIds& uids)
    {
        lock_guard<mutex> guard(lock);
        return counters[uids.exchangeId][account[0]];
    }

    /** Increments the number of times an ad for the given account has been
        shown to the given user.
     */
    void inc(const RTBKIT::AccountKey& account, const RTBKIT::UserIds& uids)
    {
        lock_guard<mutex> guard(lock);
        counters[uids.exchangeId][account[0]]++;
    }

private:

    mutex lock;
    unordered_map<Datacratic::Id, unordered_map<string, size_t> > counters;

};

	as_error err;
	as_config config;
	aerospike as;

	
/******************************************************************************/
/* FREQUENCY CAP AUGMENTOR                                                    */
/******************************************************************************/

/** Note that the serviceName and augmentorName are distinct because you may
    have multiple instances of the service that provide the same
    augmentation.
*/
FrequencyCapAugmentor::
FrequencyCapAugmentor(
        std::shared_ptr<Datacratic::ServiceProxies> services,
        const string& serviceName,
        const string& augmentorName) :
    SyncAugmentor(augmentorName, serviceName, services),
    storage(new FrequencyCapStorage()),
    agentConfig(getZmqContext()),
    palEvents(getZmqContext())
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
FrequencyCapAugmentor::
init()
{
    SyncAugmentor::init(2 /* numThreads */);

    /* Manages all the communications with the AgentConfigurationService. */
    agentConfig.init(getServices()->config);
    addSource("FrequencyCapAugmentor::agentConfig", agentConfig);

    palEvents.init(getServices()->config);

    /* This lambda will get called when the post auction loop receives a win
       on an auction.
    */
    palEvents.messageHandler = [&] (const vector<zmq::message_t>& msg)
        {
            RTBKIT::AccountKey account(msg[19].toString());
            RTBKIT::UserIds uids =
                RTBKIT::UserIds::createFromString(msg[15].toString());

            storage->inc(account, uids);
            recordHit("wins");
        };

    palEvents.connectAllServiceProviders(
            "rtbPostAuctionService", "logger", {"MATCHEDWIN"});
    addSource("FrequencyCapAugmentor::palEvents", palEvents);
}

	typedef unsigned long long timestamp_t;
	static timestamp_t
	get_timestamp ()
	{
		struct timeval now;
		gettimeofday (&now, NULL);
		return  now.tv_usec + (timestamp_t)now.tv_sec * 1000000;
	}	

/** Augments the bid request with our frequency cap information.

    This function has a 5ms window to respond (including network latency).
    Note that the augmentation is in charge of ensuring that the time
    constraints are respected and any late responses will be ignored.
*/
RTBKIT::AugmentationList
FrequencyCapAugmentor::
onRequest(const RTBKIT::AugmentationRequest& request)
{
	timestamp_t t0 = get_timestamp();
	std::cout<<"now at onrequest start : "<<Date::now().fractionalSeconds()<<std::endl;
    recordHit("requests");

    RTBKIT::AugmentationList result;

//    const RTBKIT::UserIds& uids = request.bidRequest->userIds;

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

        const RTBKIT::AccountKey& account = config.config->account;

		std::string temp = request.bidRequest->ext["audience"].asString()+ "_" + std::to_string(config.config->externalId); 
		std::cout<<"account + uids : "<<temp<<std::endl;
//        size_t count = storage->get(account, uids);
		size_t count = getCountAs(temp);
		
        /* The number of times a user has been seen by a given agent can be
           useful to make bid decisions so attach this data to the bid
           request.

           It's also recomended to place your data in an object labeled
           after the augmentor from which it originated.
        */
        result[account[0]].data = count;

        /* We tag bid requests that pass the frequency cap filtering because
           if the augmentation doesn't terminate in time or if an error
           occured, then the bid request will not receive any tags and will
           therefor be filtered out for agents that require the frequency
           capping.
        */
        if (count < getCap(request.augmentor, agent, config)) {
			std::cout<<"count < cap"<<std::endl;
            result[account].tags.insert("pass-frequency-cap-ex");
            recordHit("accounts." + account[0] + ".passed");
        }
        else{
			recordHit("accounts." + account[0] + ".capped");
			std::cout<<"count > cap"<<std::endl;
		}
    }
	timestamp_t t1 = get_timestamp();
	double secs = (t1 - t0) / 1000000.0L;
	std::cout<<"time taken : "<<secs<<std::endl;
    return result;
}

/*Returns the impression count from aerospike for for a particular
  user for a specific campaign.
*/
size_t
FrequencyCapAugmentor::
getCountAs(std::string uidcid)
{
	int impcount = 0;;
	as_key key;
	as_key_init_str(&key, "audience", "capping", uidcid.c_str());
	as_record* p_rec = NULL;
	std::string val = "imp";
	if (aerospike_key_get(&as, &err, NULL, &key, &p_rec) != AEROSPIKE_OK) {
		fprintf(stderr, "err(%d) %s at [%s:%d]\n", err.code, err.message, err.file, err.line);
	}else{
		impcount = as_record_get_int64(p_rec, val.c_str(), 0);
	}
	as_record_destroy(p_rec);
	return impcount;
}
	

/** Returns the frequency cap configured by the agent.

    This function is a bit brittle because it makes no attempt to validate
    the configuration.
*/
size_t
FrequencyCapAugmentor::
getCap( const string& augmentor,
        const string& agent,
        const RTBKIT::AgentConfigEntry& config) const
{
    for (const auto& augConfig : config.config->augmentations) {
        if (augConfig.name != augmentor) continue;
        return augConfig.config.asInt();
    }

    /* There's a race condition here where if an agent removes our augmentor
       from its configuration while there are bid requests being augmented
       for that agent then we may not find its config. A sane default is
       good to have in this scenario.
    */

    return 0;
}

} // namespace RTBKIT

