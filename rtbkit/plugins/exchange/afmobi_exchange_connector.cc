/* afmobi_exchange_connector.cc
   Jeremy Barnes, 15 March 2013

   Implementation of the Afmobi exchange connector.
*/
#include <algorithm>
#include <boost/tokenizer.hpp>

#include "afmobi_exchange_connector.h"
#include "rtbkit/plugins/bid_request/openrtb_bid_request_parser.h"
#include "rtbkit/plugins/exchange/http_auction_handler.h"
#include "rtbkit/core/agent_configuration/agent_config.h"
#include "rtbkit/openrtb/openrtb_parsing.h"
#include "soa/types/json_printing.h"
#include "soa/service/logs.h"
#include <boost/any.hpp>
#include <boost/lexical_cast.hpp>
#include "jml/utils/file_functions.h"
#include "rtbkit/plugins/exchange/openrtb_exchange_connector.h"
#include "rtbkit/core/router/filters/creative_filters.cc"
#include "crypto++/blowfish.h"
#include "crypto++/modes.h"
#include "crypto++/filters.h"


using namespace std;
using namespace Datacratic;

namespace {
	Logging::Category afmobiExchangeConnectorTrace("Afmobi Exchange Connector");
	Logging::Category afmobiExchangeConnectorError("[ERROR] Afmobi Exchange Connector", afmobiExchangeConnectorTrace);
}

namespace RTBKIT {

/*****************************************************************************/
/* AFMOBI EXCHANGE CONNECTOR                                                 */
/*****************************************************************************/

	AfmobiExchangeConnector::
	AfmobiExchangeConnector(ServiceBase & owner, const std::string & name)
		: OpenRTBExchangeConnector(owner, name) {
		this->auctionResource = "/auctions";
		this->auctionVerb = "POST";
	}

	AfmobiExchangeConnector::
	AfmobiExchangeConnector(const std::string & name,
						   std::shared_ptr<ServiceProxies> proxies)
		: OpenRTBExchangeConnector(name, proxies) {
		this->auctionResource = "/auctions";
		this->auctionVerb = "POST";
	}

	ExchangeConnector::ExchangeCompatibility
	AfmobiExchangeConnector::
	getCampaignCompatibility(const AgentConfig & config,
							 bool includeReasons) const {
		ExchangeCompatibility result;
		result.setCompatible();

		auto cpinfo = std::make_shared<CampaignInfo>();
 
		const Json::Value & pconf = config.providerConfig["afmobi"];
 
		try {
			cpinfo->seat = Id(pconf["seat"].asString());
			if (!cpinfo->seat)
				result.setIncompatible("providerConfig.afmobi.seat is null",
									   includeReasons);
		} catch (const std::exception & exc) {
			result.setIncompatible
				(string("providerConfig.afmobi.seat parsing error: ")
				 + exc.what(), includeReasons);
			return result;
		}

		try {
			cpinfo->iurl = pconf["iurl"].asString();
			if (!cpinfo->iurl.size())
				result.setIncompatible("providerConfig.afmobi.iurl is null",
									   includeReasons);
		} catch (const std::exception & exc) {
			result.setIncompatible
				(string("providerConfig.afmobi.iurl parsing error: ")
				 + exc.what(), includeReasons);
			return result;
		}
	
		result.info = cpinfo;
		return result;
	}

	namespace {

		using Datacratic::jsonDecode;

/** Given a configuration field, convert it to the appropriate JSON */
		template<typename T>
		void getAttr(ExchangeConnector::ExchangeCompatibility & result,
					 const Json::Value & config,
					 const char * fieldName,
					 T & field,
					 bool includeReasons) {
			try {
				if (!config.isMember(fieldName)) {
					result.setIncompatible
						("creative[].providerConfig.afmobi." + string(fieldName)
						 + " must be specified", includeReasons);
					return;
				}

				const Json::Value & val = config[fieldName];

				jsonDecode(val, field);
			} catch (const std::exception & exc) {
				result.setIncompatible("creative[].providerConfig.afmobi."
									   + string(fieldName) + ": error parsing field: "
									   + exc.what(), includeReasons);
				return;
			}
		}

	} // file scope

	ExchangeConnector::ExchangeCompatibility
	AfmobiExchangeConnector::
	getCreativeCompatibility(const Creative & creative,
							 bool includeReasons) const {
		ExchangeCompatibility result;
		result.setCompatible();
    
		auto crinfo = std::make_shared<CreativeInfo>();
		const Json::Value & pconf = creative.providerConfig["afmobi"];
    
		getAttr(result, pconf, "nurl", crinfo->nurl, includeReasons);
		getAttr(result, pconf, "adomain", crinfo->adomain, includeReasons);
		getAttr(result, pconf, "cat", crinfo->cat, includeReasons);
		getAttr(result, pconf, "attr", crinfo->attr, includeReasons);
		getAttr(result, pconf, "type", crinfo->type, includeReasons);
		getAttr(result, pconf, "crid", crinfo->crid, includeReasons);
		//Must have creative.adformat specified
		crinfo->adformat = creative.adformat;
		if (crinfo->adformat.empty())
			result.setIncompatible
				("creative[].adformat is null",
				 includeReasons);
//check for adm only if it is a banner ad
		if(crinfo->adformat=="banner"){
			getAttr(result, pconf, "adm", crinfo->adm, includeReasons);
		}
		result.info = crinfo;
 
		return result;
	}


	
	std::shared_ptr<BidRequest>
	AfmobiExchangeConnector::
	parseBidRequest(HttpAuctionHandler & connection,
					const HttpHeader & header,
					const std::string & payload) {

	    std::cerr<<"bidstring : "<<payload<<std::endl;
		std::shared_ptr<BidRequest> res;

		std::shared_ptr<BidRequest> none; 

		// Check for JSON content-type
		if (header.contentType != "application/json") {
			connection.sendErrorResponse("non-JSON request");
			std::cout<<"afmobi check -1-"<<std::endl;
			return res;
		}

		// Check for the x-openrtb-version header
		auto it = header.headers.find("x-openrtb-version");
		if (it == header.headers.end()) {
			connection.sendErrorResponse("MISSING_OPENRTB_HEADER", "The request is missing the 'x-openrtb-version' header");
			std::cout<<"afmobi check -2-"<<std::endl;
			return none;
		}

		// Check that it's version 2.4
		std::string openRtbVersion = it->second;
		if (openRtbVersion != "2.4") {
			connection.sendErrorResponse("UNSUPPORTED_OPENRTB_VERSION", "The request is required to be using version 2.4 of the OpenRTB protocol but requested " + openRtbVersion);
			std::cout<<"afmobi check -3-"<<std::endl;
			return none;
		}
		ML::Parse_Context context("Bid Request", payload.c_str(), payload.size());
		res.reset(OpenRTBBidRequestParser::openRTBBidRequestParserFactory("2.4")->parseBidRequest(context, exchangeName(), exchangeName()));

		// get restrictions enforced by Afmobi.
		//1) blocked category
		std::vector<std::string> strv;
		for (const auto& cat: res->blockedCategories)
			strv.push_back(cat.val);
		res->restrictions.addStrings("blockedCategories", strv);

		
		//2) per slot: blocked type and attribute;
		//3) per slot: check ext field if we have video object.
		//4) per slot: check for mraid object.. not supported for now
		std::vector<int> intv;
		for (auto& spot: res->imp) {
		  if(spot.banner.get()){
		    for (const auto& t: spot.banner->btype) {
		      intv.push_back (t.val);
		    }
		    spot.restrictions.addInts("blockedTypes", intv);
		    intv.clear();
		    for (const auto& a: spot.banner->battr) {
		      intv.push_back (a.val);
		    }
		    spot.restrictions.addInts("blockedAttrs", intv);
		  }
		}

		OpenRTBExchangeConnector::getAudienceId(res);
		OpenRTBExchangeConnector::getExchangeName(res);
//	std::cerr<<"bidrequest : "<<res->toJson()<<std::endl;  
//		std::cout<<res->toJson()<<std::endl;
		return res;
	}


	void
	AfmobiExchangeConnector::
	setSeatBid(Auction const & auction,
			   int spotNum,
			   OpenRTB::BidResponse & response) const {

		const Auction::Data * current = auction.getCurrentData();

		// Get the winning bid
		auto & resp = current->winningResponse(spotNum);

		// Find how the agent is configured.  We need to copy some of the
		// fields into the bid.
		const AgentConfig * config =
			std::static_pointer_cast<const AgentConfig>(resp.agentConfig).get();

		std::string en = exchangeName();

		// Get the exchange specific data for this campaign
		auto cpinfo = config->getProviderData<CampaignInfo>(en);

		// Put in the fixed parts from the creative
		int creativeIndex = resp.agentCreativeIndex;

		auto & creative = config->creatives.at(creativeIndex);

		// Get the exchange specific data for this creative
		auto crinfo = creative.getProviderData<CreativeInfo>(en);
	
		// Find the index in the seats array
		int seatIndex = 0;
		while(response.seatbid.size() != seatIndex) {
			if(response.seatbid[seatIndex].seat == cpinfo->seat) break;
			++seatIndex;
		}

		// Create if required
		if(seatIndex == response.seatbid.size()) {
			response.seatbid.emplace_back();
			response.seatbid.back().seat = cpinfo->seat;
		}

		// Get the seatBid object
		OpenRTB::SeatBid & seatBid = response.seatbid.at(seatIndex);

		// Add a new bid to the array
		seatBid.bid.emplace_back();
		auto & b = seatBid.bid.back();

//	std::cerr<<"resp--------"<<resp.toJson()<<std::endl;
//	std::cerr<<"biddata-----"<<resp.bidData.toJsonStr()<<std::endl;
		// Put in the variable parts
		b.id = Id(auction.id, auction.request->imp[0].id);
		b.impid = auction.request->imp[spotNum].id;
		b.price.val = getAmountIn<CPM>(resp.price.maxPrice);
		b.adomain = crinfo->adomain;
		b.crid = crinfo->crid;
		b.nurl = crinfo->nurl;
		b.cat = crinfo->cat;
		if(crinfo->adformat == "banner"){
			b.adm = crinfo->adm;
			b.iurl = cpinfo->iurl;
		}
	}
	
	template <typename T>
	bool disjoint (const set<T>& s1, const set<T>& s2) {
		auto i = s1.begin(), j = s2.begin();
		while (i != s1.end() && j != s2.end()) {
			if (*i == *j)
				return false;
			else if (*i < *j)
				++i;
			else
				++j;
		}
		return true;
	}
	bool
	AfmobiExchangeConnector::
	bidRequestCreativeFilter(const BidRequest & request,
							 const AgentConfig & config,
							 const void * info) const {
		const auto crinfo = reinterpret_cast<const CreativeInfo*>(info);

		// 1) we first check for blocked content categories
		const auto& blocked_categories = request.restrictions.get("blockedCategories");
		for (const auto& cat: crinfo->cat)
			if (blocked_categories.contains(cat)) {
				this->recordHit ("blockedCategory");
				return false;
			}

		// 2) now go throught the spots.
		for (const auto& spot: request.imp) {
			const auto& blocked_types = spot.restrictions.get("blockedTypes");
			for (const auto& t: crinfo->type)
				if (blocked_types.contains(t)) {
					this->recordHit ("blockedType");
					return false;
				}

			const auto& blocked_attr = spot.restrictions.get("blockedAttrs");
			for (const auto& a: crinfo->attr)
				if (blocked_attr.contains(a)) {
					this->recordHit ("blockedAttr");
					return false;
				}
		}

		return true;
	}

} // namespace RTBKIT

namespace {
	using namespace RTBKIT;

	struct AtInit {
		AtInit() {
			ExchangeConnector::registerFactory<AfmobiExchangeConnector>();
		}
	} atInit;
}
