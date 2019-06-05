#include <algorithm>
#include <boost/tokenizer.hpp>

#include "opera_exchange_connector.h"
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
Logging::Category operaExchangeConnectorTrace("Opera Exchange Connector");
Logging::Category operaExchangeConnectorError("[ERROR] Opera Exchange Connector", operaExchangeConnectorTrace);
}

namespace RTBKIT {

/*****************************************************************************/
/* OPERA EXCHANGE CONNECTOR                                                */
/*****************************************************************************/

OperaExchangeConnector::
OperaExchangeConnector(ServiceBase & owner, const std::string & name)
    : OpenRTBExchangeConnector(owner, name) {
    this->auctionResource = "/auctions";
    this->auctionVerb = "POST";
}

OperaExchangeConnector::
OperaExchangeConnector(const std::string & name,
                       std::shared_ptr<ServiceProxies> proxies)
    : OpenRTBExchangeConnector(name, proxies) {
    this->auctionResource = "/auctions";
    this->auctionVerb = "POST";
}

ExchangeConnector::ExchangeCompatibility
OperaExchangeConnector::
getCampaignCompatibility(const AgentConfig & config,
                         bool includeReasons) const {
    ExchangeCompatibility result;
    result.setCompatible();

    auto cpinfo = std::make_shared<CampaignInfo>();

    const Json::Value & pconf = config.providerConfig["opera"];

    try {
        cpinfo->seat = Id(pconf["seat"].asString());
        if (!cpinfo->seat)
            result.setIncompatible("providerConfig.opera.seat is null",
                                   includeReasons);
    } catch (const std::exception & exc) {
        result.setIncompatible
        (string("providerConfig.opera.seat parsing error: ")
         + exc.what(), includeReasons);
        return result;
    }

    try {
        cpinfo->iurl = pconf["iurl"].asString();
        if (!cpinfo->iurl.size())
            result.setIncompatible("providerConfig.opera.iurl is null",
                                   includeReasons);
    } catch (const std::exception & exc) {
        result.setIncompatible
        (string("providerConfig.opera.iurl parsing error: ")
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
            ("creative[].providerConfig.opera." + string(fieldName)
             + " must be specified", includeReasons);
            return;
        }

        const Json::Value & val = config[fieldName];

        jsonDecode(val, field);
    } catch (const std::exception & exc) {
        result.setIncompatible("creative[].providerConfig.opera."
                               + string(fieldName) + ": error parsing field: "
                               + exc.what(), includeReasons);
        return;
    }
}

} // file scope

ExchangeConnector::ExchangeCompatibility
OperaExchangeConnector::
getCreativeCompatibility(const Creative & creative,
                         bool includeReasons) const {
    ExchangeCompatibility result;
    result.setCompatible();

    auto crinfo = std::make_shared<CreativeInfo>();

    const Json::Value & pconf = creative.providerConfig["opera"];

    const Json::Value & vconf = creative.videoConfig;

    std::string tmp;

    boost::char_separator<char> sep(" ,");

    // 1.  Must have opera.attr containing creative attributes.  These
    //     turn into OperaCreativeAttribute filters.
    getAttr(result, pconf, "attr", tmp, includeReasons);
    if (!tmp.empty())  {
        boost::tokenizer<boost::char_separator<char>> tok(tmp, sep);
        auto& ints = crinfo->attr;
        std::transform(tok.begin(), tok.end(),
        std::inserter(ints, ints.begin()),[&](const std::string& s) {
            return atoi(s.data());
        });
    }
    tmp.clear();


    // 2. Must have opera.type containing attribute type.
    getAttr(result, pconf, "type", tmp, includeReasons);
    if (!tmp.empty())  {
        boost::tokenizer<boost::char_separator<char>> tok(tmp, sep);
        auto& ints = crinfo->type;
        std::transform(tok.begin(), tok.end(),
        std::inserter(ints, ints.begin()),[&](const std::string& s) {
            return atoi(s.data());
        });
    }
    tmp.clear();

    // 3. Must have opera.cat containing attribute type.
    getAttr(result, pconf, "cat", tmp, includeReasons);
    if (!tmp.empty())  {
        boost::tokenizer<boost::char_separator<char>> tok(tmp, sep);
        auto& strs = crinfo->cat;
        copy(tok.begin(),tok.end(),inserter(strs,strs.begin()));
    }
    tmp.clear();

    // 4.  Must have opera.adm that includes Opera's macro
    getAttr(result, pconf, "adm", crinfo->adm, includeReasons);
    getAttr(result, pconf, "crid", crinfo->crid, includeReasons);
    if (!crinfo->crid)
        result.setIncompatible
        ("creative[].providerConfig.opera.crid is null",
         includeReasons);

    // 6.  Must have advertiser names array in opera.adomain
    getAttr(result, pconf, "adomain", crinfo->adomain,  includeReasons);
    if (crinfo->adomain.empty())
        result.setIncompatible
        ("creative[].providerConfig.opera.adomain is empty",
         includeReasons);

    getAttr(result, pconf, "nurl", crinfo->nurl, includeReasons);
    if (crinfo->nurl.empty())
        result.setIncompatible
        ("creative[].providerConfig.opera.nurl is empty",
         includeReasons);
	if(creative.adformat != "native"){
	  getAttr(result, pconf, "imptrackers", crinfo->imptrackers, includeReasons);
	}
	if(creative.providerConfig["opera"]["isApp"] == 1){
		getAttr(result, pconf, "bundle", crinfo->bundle, includeReasons);
	}
	if(creative.adformat == "video"){
	  getAttr(result, vconf, "duration", crinfo->duration, includeReasons);
	}

    // Cache the information
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
  OperaExchangeConnector::
  parseBidRequest(HttpAuctionHandler & connection,
		  const HttpHeader & header,
		  const std::string & payload)
  {
    std::shared_ptr<BidRequest> none;
    
    
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
    if (openRtbVersion != "2.5") {
      connection.sendErrorResponse("UNSUPPORTED_OPENRTB_VERSION", "The request is required to be using version 2.5 of the OpenRTB protocol but requested " + openRtbVersion);
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
      std::cout<<"networkname : "<<result->device->carrier.utf8String()<<std::endl;
      result->device->carrier = OpenRTBExchangeConnector::changeNetworkName(exNet);
    }
    
    OpenRTBExchangeConnector::getAudienceId(result);
    
    OpenRTBExchangeConnector::getExchangeName(result);
    
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
    //		cout<<"Bidrequest after parsing in adx exchange connector : "<<result->toJson()<<endl;
    return result;
  }

void
OperaExchangeConnector::
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
    b.adm = crinfo->adm;
    b.adomain = crinfo->adomain;
    b.crid = crinfo->crid;
    b.iurl = cpinfo->iurl;
    b.nurl = crinfo->nurl;
	b.bundle = crinfo->bundle;
	b.cat = crinfo->cat;

	b.psattr = crinfo->attr;
	/*
	int j = 0;
		if(b.ext["crtype"] == "native"){
		for(auto i:crinfo->imptrackers){
			b.ext["admnative"]["native"]["imptrackers"][j] = i;
			j++;
		};
	}else{
		for(auto i:crinfo->imptrackers){
			b.ext["imptrackers"][j] = i;
			j++;
		};
	};
//additional fields required for native	
	if(b.ext["crtype"] == "native"){
		// Get the nativeConfig data for this creative
		auto ninfo = creative.nativeConfig;
		Json::Value AssetList = resp.bidData[spotNum].ext["assetList"];

		b.ext["admnative"]["native"]["link"]["url"] = ninfo["link"]["url"];

		Json::Value admAsset(Json::arrayValue);
		
//populating title asset
		if(AssetList.isMember("title")){
			for(auto i : AssetList["title"].getMemberNames()){
				Json::Value temptitleAsset;
				temptitleAsset["id"] = std::stoi(i);
				for(Json::Value j : ninfo["assets"]["titles"]){
					if( j["id"].asInt() == AssetList["title"][i].asInt()){
						temptitleAsset["title"]["text"] = j["text"];
						break;
					}
				};
				admAsset.append(temptitleAsset);
			}
		}

//populating image assets		
		if(AssetList.isMember("images")){
			for(auto i : AssetList["images"].getMemberNames()){
				Json::Value tempimgAsset;
				tempimgAsset["id"] = std::stoi(i);
				for(Json::Value j : ninfo["assets"]["images"]){
					if( j["id"].asInt() == AssetList["images"][i].asInt()){
						tempimgAsset["img"]["url"] = j["url"];
						break;
					}
				};
				admAsset.append(tempimgAsset);
			}
		}

//populating data assets
		if(AssetList.isMember("data")){
			for(auto i : AssetList["data"].getMemberNames()){
				Json::Value tempdataAsset;
				tempdataAsset["id"] = std::stoi(i);
				for(Json::Value j : ninfo["assets"]["data"]){
					if( j["id"].asInt() == AssetList["data"][i].asInt()){
						tempdataAsset["data"]["value"] = j["value"];
						break;
					}
				};
				admAsset.append(tempdataAsset);
			}
		}
		b.ext["admnative"]["native"]["assets"] = admAsset;
	}
	*/}

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
OperaExchangeConnector::
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
        ExchangeConnector::registerFactory<OperaExchangeConnector>();
    }
} atInit;
}
