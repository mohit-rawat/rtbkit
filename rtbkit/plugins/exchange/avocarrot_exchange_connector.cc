/* avocarrot_exchange_connector.cc
   Jeremy Barnes, 15 March 2013

   Implementation of the Avocarrot exchange connector.
*/
#include <algorithm>
#include <boost/tokenizer.hpp>

#include "avocarrot_exchange_connector.h"
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
	Logging::Category avocarrotExchangeConnectorTrace("Avocarrot Exchange Connector");
	Logging::Category avocarrotExchangeConnectorError("[ERROR] Avocarrot Exchange Connector", avocarrotExchangeConnectorTrace);
}

namespace RTBKIT {

/*****************************************************************************/
/* AVOCARROT EXCHANGE CONNECTOR                                              */
/*****************************************************************************/

	AvocarrotExchangeConnector::
	AvocarrotExchangeConnector(ServiceBase & owner, const std::string & name)
		: OpenRTBExchangeConnector(owner, name) {
		this->auctionResource = "/auctions";
		this->auctionVerb = "POST";
	}

	AvocarrotExchangeConnector::
	AvocarrotExchangeConnector(const std::string & name,
						   std::shared_ptr<ServiceProxies> proxies)
		: OpenRTBExchangeConnector(name, proxies) {
		this->auctionResource = "/auctions";
		this->auctionVerb = "POST";
	}

	ExchangeConnector::ExchangeCompatibility
	AvocarrotExchangeConnector::
	getCampaignCompatibility(const AgentConfig & config,
							 bool includeReasons) const {
		ExchangeCompatibility result;
		result.setCompatible();

		auto cpinfo = std::make_shared<CampaignInfo>();
 
		const Json::Value & pconf = config.providerConfig["avocarrot"];
 
		try {
			cpinfo->seat = Id(pconf["seat"].asString());
			if (!cpinfo->seat)
				result.setIncompatible("providerConfig.avocarrot.seat is null",
									   includeReasons);
		} catch (const std::exception & exc) {
			result.setIncompatible
				(string("providerConfig.avocarrot.seat parsing error: ")
				 + exc.what(), includeReasons);
			return result;
		}

		try {
			cpinfo->iurl = pconf["iurl"].asString();
			if (!cpinfo->iurl.size())
				result.setIncompatible("providerConfig.avocarrot.iurl is null",
									   includeReasons);
		} catch (const std::exception & exc) {
			result.setIncompatible
				(string("providerConfig.avocarrot.iurl parsing error: ")
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
						("creative[].providerConfig.avocarrot." + string(fieldName)
						 + " must be specified", includeReasons);
					return;
				}

				const Json::Value & val = config[fieldName];

				jsonDecode(val, field);
			} catch (const std::exception & exc) {
				result.setIncompatible("creative[].providerConfig.avocarrot."
									   + string(fieldName) + ": error parsing field: "
									   + exc.what(), includeReasons);
				return;
			}
		}

	} // file scope

	ExchangeConnector::ExchangeCompatibility
	AvocarrotExchangeConnector::
	getCreativeCompatibility(const Creative & creative,
							 bool includeReasons) const {
		ExchangeCompatibility result;
		result.setCompatible();
    
		auto crinfo = std::make_shared<CreativeInfo>();
		const Json::Value & pconf = creative.providerConfig["avocarrot"];
    
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

	char *str_replace(char *orig, char *rep, char *with) {
		char *result; // the return string
		char *ins;    // the next insert point
		char *tmp;    // varies
		int len_rep;  // length of rep
		int len_with; // length of with
		int len_front; // distance between rep and end of last rep
		int count;    // number of replacements

		if (!orig)
			return NULL;
		if (!rep)
			rep = (char*)"";
		len_rep = strlen(rep);
		if (!with)
			with = (char*)"";
		len_with = strlen(with);

		ins = orig;
		for (count = 0; (tmp = strstr(ins, rep)); ++count) {
			ins = tmp + len_rep;
		}

		// first time through the loop, all the variable are set correctly
		// from here on,
		//    tmp points to the end of the result string
		//    ins points to the next occurrence of rep in orig
		//    orig points to the remainder of orig after "end of rep"
		tmp = result = (char*)malloc(strlen(orig) + (len_with - len_rep) * count + 1);

		if (!result)
			return NULL;
		while (count--) {
			ins = strstr(orig, rep);
			len_front = ins - orig;
			tmp = strncpy(tmp, orig, len_front) + len_front;
			tmp = strcpy(tmp, with) + len_with;
			orig += len_front + len_rep; // move to next "end of rep"
		}
		strcpy(tmp, orig);
		return result;
	}
	
	std::shared_ptr<BidRequest>
	AvocarrotExchangeConnector::
	parseBidRequest(HttpAuctionHandler & connection,
					const HttpHeader & header,
					const std::string & payload) {

	  //  std::cerr<<"bidstring : "<<payload<<std::endl;
		std::shared_ptr<BidRequest> res;

		std::shared_ptr<BidRequest> none; 
	  connection.dropAuction();
	  return none;
	  std::cout<<"======================================adx check 1"<<std::endl;
		// Check for JSON content-type
		if (header.contentType != "application/json") {
			connection.sendErrorResponse("non-JSON request");
			return res;
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
//		std::cout<<"pubnat"<<std::endl;
		std::string abc = payload;
//		std::cout<<escape_json(abc)<<std::endl;
		char *cstr = &abc[0u];
		char *cstr1;
		char *cstr2;
		char *cstr3;
		cstr1 = str_replace(cstr, (char*)"\\", (char*)"");
		cstr2 = str_replace(cstr1, (char*)"\"{", (char*)"{");
		cstr3 = str_replace(cstr2, (char*)"}\"", (char*)"}");
		abc = cstr3;
/*		Json::Value aa;
		aa["acd"] = "abc";
		std::cout<<escape_json(aa.toStringNoNewLine())<<std::endl;
*/		
		// Parse the bid request
		// TODO Check with Avocarrot if they send the x-openrtb-version header
		// and if they support 2.2 now.
		ML::Parse_Context context("Bid Request", cstr3, abc.size());
		res.reset(OpenRTBBidRequestParser::openRTBBidRequestParserFactory("2.3")->parseBidRequest(context, exchangeName(), exchangeName()));

		// get restrictions enforced by Avocarrot.
		//1) blocked category
		std::vector<std::string> strv;
		for (const auto& cat: res->blockedCategories)
			strv.push_back(cat.val);
		res->restrictions.addStrings("blockedCategories", strv);

		for(int i = 0; i< res->imp.size(); i++){
			if(res->imp[i].native == NULL){}else{
//			res->imp[i].native->request.native.ver = res->imp[i].native->request.ver;
				res->imp[i].native->request.native.layout = res->imp[i].native->request.layout;
				res->imp[i].native->request.native.adunit = res->imp[i].native->request.adunit;
				res->imp[i].native->request.native.context = res->imp[i].native->request.context;
				res->imp[i].native->request.native.contextsubtype = res->imp[i].native->request.contextsubtype;
				res->imp[i].native->request.native.plcmttype = res->imp[i].native->request.plcmttype;
				res->imp[i].native->request.native.plcmtcnt = res->imp[i].native->request.plcmtcnt;
				res->imp[i].native->request.native.seq = res->imp[i].native->request.seq;
				res->imp[i].native->request.native.ext = res->imp[i].native->request.ext;
				for(auto j = res->imp[i].native->request.assets.begin(); j!=res->imp[i].native->request.assets.end(); j++){
					res->imp[i].native->request.native.assets.push_back(*j);
				}
				//								std::cout<<"request after parsing in avocarrot excon "<<res->toJson()<<std::endl;
			}
		}
		
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
			if(spot.native.get()){
				intv.clear();
				for (const auto& a: spot.native->battr) {
					intv.push_back (a);
				}
				spot.restrictions.addInts("blockedAttrs", intv);
			}
		}

		OpenRTBExchangeConnector::getAudienceId(res);
		OpenRTBExchangeConnector::getExchangeName(res);
//	std::cerr<<"bidrequest : "<<res->toJson()<<std::endl;  
		free(cstr1);
		free(cstr2);
		free(cstr3);
//		std::cout<<res->toJson()<<std::endl;
		return res;
	}


	void
	AvocarrotExchangeConnector::
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
		b.cid = Id(resp.agent);
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
		//		b.psattr = crinfo->attr;

		Json::Value adm;
		int j = 0;
		for(auto i:crinfo->imptrackers){
			adm["native"]["imptrackers"][j] = i;
			j++;
		};
//additional fields required for native	
			// Get the nativeConfig data for this creative
		if(crinfo->adformat == "native"){
			auto ninfo = creative.nativeConfig;
			Json::Value AssetList = resp.bidData[spotNum].ext["assetList"];
			std::cout<<"AssetList "<<AssetList<<std::endl;			
			std::cout<<"ninfo "<<ninfo<<std::endl;
			adm["native"]["link"]["url"] = ninfo["link"]["url"];
			std::cout<<"avocarrot check 1"<<std::endl;			
			Json::Value admAsset(Json::arrayValue);
			
//populating title asset
			if(AssetList.isMember("title")){
				for(auto i : AssetList["title"].getMemberNames()){
				  std::cout<<" assetlist title members "<<i<<std::endl;
					Json::Value temptitleAsset;
					temptitleAsset["id"] = std::stoi(i);
					for(Json::Value j : ninfo["assets"]["titles"]){
						if( j["id"].asInt() == AssetList["title"][i].asInt()){
						  std::cout<<"ninfo title id "<<j["id"].asInt()<<std::endl;
						  std::cout<<"Assetlist title id "<<AssetList["title"][i].asInt()<<std::endl;
							temptitleAsset["title"]["text"] = j["title"];
							break;
						}
					};
					admAsset.append(temptitleAsset);
				}
			}
			
//populating image assets		
			if(AssetList.isMember("images")){
				for(auto i : AssetList["images"].getMemberNames()){
			std::cout<<"avocarrot check 2"<<std::endl;			
					Json::Value tempimgAsset;
					tempimgAsset["id"] = std::stoi(i);
					for(Json::Value j : ninfo["assets"]["images"]){
						if( j["id"].asInt() == AssetList["images"][i].asInt()){
							tempimgAsset["img"]["url"] = j["url"];
							std::cout<<"avocarrot check 3"<<std::endl;
							break;
						}
					};
					admAsset.append(tempimgAsset);
				}
			}
			
//populating data assets
			if(AssetList.isMember("data")){
				for(auto i : AssetList["data"].getMemberNames()){
				  std::cout<<"avocarrot check 4"<<std::endl;
					Json::Value tempdataAsset;
					tempdataAsset["id"] = std::stoi(i);
					for(Json::Value j : ninfo["assets"]["data"]){
						if( j["id"].asInt() == AssetList["data"][i].asInt()){
							tempdataAsset["data"]["value"] = j["value"];
							std::cout<<"avocarrot check 5"<<std::endl;
							break;
						}
					};
					admAsset.append(tempdataAsset);
				}
			}
			adm["native"]["imptrackers"] = ninfo["imptrackers"];
			adm["native"]["version"] = "1.1";
			adm["native"]["assets"] = admAsset;
			std::cout<<"avocarrot check 6"<<std::endl;
//		std::string abc = adm.toStringNoNewLine();
//		std::cout<<escape_json(abc)<<std::endl;
			char *cstr = &adm.toStringNoNewLine()[0u];
			//			char *cstr1;
			//			cstr1 = str_replace(cstr, (char*)"/", (char*)"\\/");
			b.adm = cstr;
			//			free(cstr1);
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
	AvocarrotExchangeConnector::
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
			ExchangeConnector::registerFactory<AvocarrotExchangeConnector>();
		}
	} atInit;
}
