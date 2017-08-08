/* adx_exchange_connector.cc
   Matthieu Labour, December 2014
   Copyright (c) 2014 Datacratic.  All rights reserved. */


#include "adx_exchange_connector.h"

#include "rtbkit/plugins/bid_request/openrtb_bid_request_parser.h"
#include "rtbkit/plugins/exchange/http_auction_handler.h"
#include "rtbkit/core/agent_configuration/agent_config.h"
#include "rtbkit/openrtb/openrtb_parsing.h"
#include "soa/types/json_printing.h"
#include "soa/service/logs.h"
#include <boost/any.hpp>
#include <boost/lexical_cast.hpp>
#include "jml/utils/file_functions.h"

 
using namespace std;
using namespace Datacratic;

namespace {
	Logging::Category adxExchangeConnectorTrace("Adx Exchange Connector");
	Logging::Category adxExchangeConnectorError("[ERROR] Adx Exchange Connector", adxExchangeConnectorTrace);
}
 
namespace RTBKIT  { 

/*****************************************************************************/
/* ADX EXCHANGE CONNECTOR                                                 */
/*****************************************************************************/

	std::string AdxExchangeConnector::nobid = "nobid";

	AdxExchangeConnector::
	AdxExchangeConnector(ServiceBase & owner, const std::string & name)
		: OpenRTBExchangeConnector(owner, name) {
		this->auctionResource = "/auctions";
		this->auctionVerb = "POST";
	}

	AdxExchangeConnector::
	AdxExchangeConnector(const std::string & name,
						 std::shared_ptr<ServiceProxies> proxies)
		: OpenRTBExchangeConnector(name, proxies) {
		this->auctionResource = "/auctions";
		this->auctionVerb = "POST";
	}
 
	std::shared_ptr<BidRequest>
	AdxExchangeConnector::
	parseBidRequest(HttpAuctionHandler & connection,
					const HttpHeader & header,
					const std::string & payload)
	{
		std::shared_ptr<BidRequest> none;

		size_t found = header.queryParams.uriEscaped().find(AdxExchangeConnector::nobid);
		if (found != string::npos) {
			connection.dropAuction("nobid");
			return none;
		}   

		// Check for JSON content-type
		if (!header.contentType.empty()) {
			static const std::string delimiter = ";";

			std::string::size_type posDelim = header.contentType.find(delimiter);
			std::string content;

			if(posDelim == std::string::npos)
				content = header.contentType;
			else {
				content = header.contentType.substr(0, posDelim);
#if 0
				std::string charset = header.contentType.substr(posDelim, header.contentType.length());
#endif
			}

			if(content != "application/json") {
				connection.sendErrorResponse("UNSUPPORTED_CONTENT_TYPE", "The request is required to use the 'Content-Type: application/json' header");
				return none;
			}
		}
		else {
			connection.sendErrorResponse("MISSING_CONTENT_TYPE_HEADER", "The request is missing the 'Content-Type' header");
			return none;
		}

		// Check for the x-openrtb-version header
		auto it = header.headers.find("x-openrtb-version");
		if (it == header.headers.end()) {
			connection.sendErrorResponse("MISSING_OPENRTB_HEADER", "The request is missing the 'x-openrtb-version' header");
			return none;
		}

		// Check that it's version 2.3
		std::string openRtbVersion = it->second;
		if (openRtbVersion != "2.3") {
			connection.sendErrorResponse("UNSUPPORTED_OPENRTB_VERSION", "The request is required to be using version 2.3 of the OpenRTB protocol but requested " + openRtbVersion);
			return none;
		}

		if(payload.empty()) {
			this->recordHit("error.emptyBidRequest");
			connection.sendErrorResponse("EMPTY_BID_REQUEST", "The request is empty");
			return none;
		}

		// Parse the bid request
		std::shared_ptr<BidRequest> result;
		try {
			ML::Parse_Context context("Bid Request", payload.c_str(), payload.size());
			result.reset(OpenRTBBidRequestParser::openRTBBidRequestParserFactory(openRtbVersion)->parseBidRequest(context,
																												  exchangeName(),
																												  exchangeName()));
		}
		catch(ML::Exception const & e) {
			this->recordHit("error.parsingBidRequest");
			throw;
		}
		catch(...) {
			throw;
		}

		// per slot: blocked type and attribute;
		std::vector<int> intv;
		for (auto& spot: result->imp) {
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

		// Check if we want some reporting
		auto verbose = header.headers.find("x-openrtb-verbose");
		if(header.headers.end() != verbose) {
			if(verbose->second == "1") {
				if(!result->auctionId.notNull()) {
					connection.sendErrorResponse("MISSING_ID", "The bid request requires the 'id' field");
					return none;
				}
			}
		}
	
//replacing carriername from aerospike
		if(result->device != NULL){
			std::string exNet = result->exchange + " " + result->device->carrier.utf8String();
			result->device->carrier = OpenRTBExchangeConnector::changeNetworkName(exNet);
		}
	
		OpenRTBExchangeConnector::getAudienceId(result);

		OpenRTBExchangeConnector::getExchangeName(result);

		if(result->ext["udi"].isMember("imei")){
			OpenRTBExchangeConnector::getIMEIcode(result);
		};
				if(result->site){
			string temp = result->site->page.host();
			if(!(result->site->id)&&!result->site->page.empty()){
				result->site->id = Id("adatrix_"+temp);
			}
			if(result->site->name.empty()&&result->site->domain.empty())result->site->name = temp;
		}
		if(result->app){
			if(!(result->app->id)){
				result->app->id = Id(result->app->bundle.utf8String());
			}
		}
		cout<<"Bidrequest after parsing in adx exchange connector : "<<result->toJson()<<endl;
		return result;
	}



	ExchangeConnector::ExchangeCompatibility
	AdxExchangeConnector::
	getCampaignCompatibility(const AgentConfig & config,
							 bool includeReasons) const {
		ExchangeCompatibility result;
		result.setCompatible();

		auto cpinfo = std::make_shared<CampaignInfo>();
 
		const Json::Value & pconf = config.providerConfig["adx"];
 
		try {
			cpinfo->seat = Id(pconf["seat"].asString());
			if (!cpinfo->seat)
				result.setIncompatible("providerConfig.adx.seat is null",
									   includeReasons);
		} catch (const std::exception & exc) {
			result.setIncompatible
				(string("providerConfig.adx.seat parsing error: ")
				 + exc.what(), includeReasons);
			return result;
		}

		try {
			cpinfo->iurl = pconf["iurl"].asString();
			if (!cpinfo->iurl.size())
				result.setIncompatible("providerConfig.adx.iurl is null",
									   includeReasons);
		} catch (const std::exception & exc) {
			result.setIncompatible
				(string("providerConfig.adx.iurl parsing error: ")
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
						("creative[].providerConfig.adx." + string(fieldName)
						 + " must be specified", includeReasons);
					return;
				}

				const Json::Value & val = config[fieldName];

				jsonDecode(val, field);
			} catch (const std::exception & exc) {
				result.setIncompatible("creative[].providerConfig.adx."
									   + string(fieldName) + ": error parsing field: "
									   + exc.what(), includeReasons);
				return;
			}
		}

	} // file scope


 
	ExchangeConnector::ExchangeCompatibility
	AdxExchangeConnector::
	getCreativeCompatibility(const Creative & creative, bool includeReasons) const {
		ExchangeCompatibility result;
		result.setCompatible();
    
		auto crinfo = std::make_shared<CreativeInfo>();
		const Json::Value & pconf = creative.providerConfig["adx"];
    
		getAttr(result, pconf, "adm", crinfo->adm, includeReasons);
		getAttr(result, pconf, "adomain", crinfo->adomain, includeReasons);
//		getAttr(result, pconf, "mimeTypes", crinfo->mimeTypes, includeReasons);
		getAttr(result, pconf, "cat", crinfo->cat, includeReasons);
		getAttr(result, pconf, "type", crinfo->type, includeReasons);
		getAttr(result, pconf, "attr", crinfo->attr, includeReasons);
		getAttr(result, pconf, "crid", crinfo->crid, includeReasons);
		getAttr(result, pconf, "impression_tracking_url", crinfo->impression_tracking_url, includeReasons);
		crinfo->adformat = creative.adformat;
		if (crinfo->adformat.empty())
			result.setIncompatible
				("creative[].adformat is null",
				 includeReasons);
		result.info = crinfo;
 
		return result;
	}

	bool
	AdxExchangeConnector::
	bidRequestCreativeFilter(const BidRequest & request,
							 const AgentConfig & config,
							 const void * info) const {
		const auto crinfo = reinterpret_cast<const CreativeInfo*>(info);
//bcat
		const auto& blocked_categories = request.restrictions.get("bcat");
		//		std::cerr<<"request.bcat : "<<request.restrictions.get("bcat")<<std::endl;
		for (const auto& cat: crinfo->cat)
			if (blocked_categories.contains(cat)) {
				this->recordHit ("blockedCategory");
				return false;
			}
//badv
		const auto& badv = request.restrictions.get("badv");
		for (const auto& adomain: crinfo->adomain)
			if (badv.contains(adomain)) {
				this->recordHit ("blockedAdvertiser");
				return false;
			}

		// now go through the spots.
//btype and battr
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

	void
	AdxExchangeConnector::
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

		// Put in the variable parts
		std::string cid  = auction.request->imp[spotNum].ext["billing_id"][0].toStringNoNewLine();
		std::string cid2 = cid.substr(1,cid.length()-2);
		b.cid = Id(cid2);
		b.id = Id(auction.id, auction.request->imp[0].id);
		b.impid = auction.request->imp[spotNum].id;
		b.price.val = getAmountIn<CPM>(resp.price.maxPrice);
		if(crinfo->adformat == "banner"){
			b.adm = crinfo->adm;
			b.w = creative.format.width;
			b.h = creative.format.height;
		}
		else if(crinfo->adformat == "video"){
		  string vastxml = creative.videoConfig["adm"].asString();
		  //		  vastxml.replace(vastxml.find("${AUCTION_ID}"), 13, b.id.toString().substr(0, b.id.toString().length()-2));
			b.adm = vastxml;   //have to send VAST xml for video ad
			b.h = creative.videoConfig["h"].asInt();
			b.w = creative.videoConfig["w"].asInt();
		}
		b.adomain = crinfo->adomain;
		b.crid = crinfo->crid;
		int j = 0;
		for(auto i:crinfo->impression_tracking_url){
		  //		  i.replace(i.find("${AUCTION_IMP_ID}"), 17, b.impid.toString());
		  i.replace(i.find("${AUCTION_ID}"), 13, b.id.toString().substr(0, b.id.toString().length()-2));
		  b.ext["impression_tracking_url"][j] = i;
		  j++;
		};
		response.cur = "USD";
	}

} // namespace RTBKIT
 
namespace {
	using namespace RTBKIT;
 
	struct Init {
		Init() {
			ExchangeConnector::registerFactory<AdxExchangeConnector>();
		}
	} init;
}
