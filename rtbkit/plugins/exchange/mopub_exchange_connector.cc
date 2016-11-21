/* mopub_exchange_connector.cc
   Jeremy Barnes, 15 March 2013

   Implementation of the MoPub exchange connector.
*/
#include <algorithm>
#include <boost/tokenizer.hpp>

#include "mopub_exchange_connector.h"
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
Logging::Category mopubExchangeConnectorTrace("MoPub Exchange Connector");
Logging::Category mopubExchangeConnectorError("[ERROR] MoPub Exchange Connector", mopubExchangeConnectorTrace);
}

namespace RTBKIT {

/*****************************************************************************/
/* MOPUB EXCHANGE CONNECTOR                                                */
/*****************************************************************************/

MoPubExchangeConnector::
MoPubExchangeConnector(ServiceBase & owner, const std::string & name)
    : OpenRTBExchangeConnector(owner, name) {
    this->auctionResource = "/auctions";
    this->auctionVerb = "POST";
}

MoPubExchangeConnector::
MoPubExchangeConnector(const std::string & name,
                       std::shared_ptr<ServiceProxies> proxies)
    : OpenRTBExchangeConnector(name, proxies) {
    this->auctionResource = "/auctions";
    this->auctionVerb = "POST";
}

ExchangeConnector::ExchangeCompatibility
MoPubExchangeConnector::
getCampaignCompatibility(const AgentConfig & config,
                         bool includeReasons) const {
    ExchangeCompatibility result;
    result.setCompatible();

    auto cpinfo = std::make_shared<CampaignInfo>();

    const Json::Value & pconf = config.providerConfig["mopub"];

    try {
        cpinfo->seat = Id(pconf["seat"].asString());
        if (!cpinfo->seat)
            result.setIncompatible("providerConfig.mopub.seat is null",
                                   includeReasons);
    } catch (const std::exception & exc) {
        result.setIncompatible
        (string("providerConfig.mopub.seat parsing error: ")
         + exc.what(), includeReasons);
        return result;
    }

    try {
        cpinfo->iurl = pconf["iurl"].asString();
        if (!cpinfo->iurl.size())
            result.setIncompatible("providerConfig.mopub.iurl is null",
                                   includeReasons);
    } catch (const std::exception & exc) {
        result.setIncompatible
        (string("providerConfig.mopub.iurl parsing error: ")
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
            ("creative[].providerConfig.mopub." + string(fieldName)
             + " must be specified", includeReasons);
            return;
        }

        const Json::Value & val = config[fieldName];

        jsonDecode(val, field);
    } catch (const std::exception & exc) {
        result.setIncompatible("creative[].providerConfig.mopub."
                               + string(fieldName) + ": error parsing field: "
                               + exc.what(), includeReasons);
        return;
    }
}

} // file scope

ExchangeConnector::ExchangeCompatibility
MoPubExchangeConnector::
getCreativeCompatibility(const Creative & creative,
                         bool includeReasons) const {
    ExchangeCompatibility result;
    result.setCompatible();

    auto crinfo = std::make_shared<CreativeInfo>();

    const Json::Value & pconf = creative.providerConfig["mopub"];

    const Json::Value & vconf = creative.videoConfig;

    std::string tmp;

    boost::char_separator<char> sep(" ,");

    // 1.  Must have mopub.attr containing creative attributes.  These
    //     turn into MoPubCreativeAttribute filters.
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


    // 2. Must have mopub.type containing attribute type.
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

    // 3. Must have mopub.cat containing attribute type.
    getAttr(result, pconf, "cat", tmp, includeReasons);
    if (!tmp.empty())  {
        boost::tokenizer<boost::char_separator<char>> tok(tmp, sep);
        auto& strs = crinfo->cat;
        copy(tok.begin(),tok.end(),inserter(strs,strs.begin()));
    }
    tmp.clear();

    // 4.  Must have mopub.adm that includes MoPub's macro
    getAttr(result, pconf, "adm", crinfo->adm, includeReasons);
    if (crinfo->adm.find("${AUCTION_PRICE}") == string::npos)
        result.setIncompatible
        ("creative[].providerConfig.mopub.adm ad markup must contain "
         "encrypted win price macro ${AUCTION_PRICE}",
         includeReasons);

    // 5.  Must have creative ID in mopub.crid
    getAttr(result, pconf, "crid", crinfo->crid, includeReasons);
    if (!crinfo->crid)
        result.setIncompatible
        ("creative[].providerConfig.mopub.crid is null",
         includeReasons);

    // 6.  Must have advertiser names array in mopub.adomain
    getAttr(result, pconf, "adomain", crinfo->adomain,  includeReasons);
    if (crinfo->adomain.empty())
        result.setIncompatible
        ("creative[].providerConfig.mopub.adomain is empty",
         includeReasons);

    getAttr(result, pconf, "nurl", crinfo->nurl, includeReasons);
    if (crinfo->nurl.empty())
        result.setIncompatible
        ("creative[].providerConfig.mopub.nurl is empty",
         includeReasons);
	if(creative.adformat != "native"){
	  getAttr(result, pconf, "imptrackers", crinfo->imptrackers, includeReasons);
	}
	if(creative.providerConfig["mopub"]["isApp"] == 1){
		getAttr(result, pconf, "bundle", crinfo->bundle, includeReasons);
	}
	if(creative.adformat == "video"){
	  std::cerr<<"=================="<<creative.adformat<<std::endl;
	  getAttr(result, vconf, "duration", crinfo->duration, includeReasons);
	}
	if(creative.adformat == "video" || creative.adformat == "native"){
	  getAttr(result, pconf, "crtype", crinfo->crtype, includeReasons);
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
MoPubExchangeConnector::
parseBidRequest(HttpAuctionHandler & connection,
                const HttpHeader & header,
                const std::string & payload) {



//  std::cerr<<"bidstring : "<<abc<<std::endl;
    std::shared_ptr<BidRequest> res;

    std::shared_ptr<BidRequest> none; 

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

	std::string abc = payload;
	char *cstr = &abc[0u];
	char *cstr1;
	char *cstr2;
	char *cstr3;
	cstr1 = str_replace(cstr, (char*)"\\", (char*)"");
	cstr2 = str_replace(cstr1, (char*)"\"{", (char*)"{");
	cstr3 = str_replace(cstr2, (char*)"}\"", (char*)"}");
	abc = cstr3;
    // Parse the bid request
    // TODO Check with MoPub if they send the x-openrtb-version header
    // and if they support 2.2 now.
    ML::Parse_Context context("Bid Request", cstr3, abc.size());
    res.reset(OpenRTBBidRequestParser::openRTBBidRequestParserFactory("2.3")->parseBidRequest(context, exchangeName(), exchangeName()));

    // get restrictions enforced by MoPub.
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
      if(spot.native.get()){
		  intv.clear();
		  for (const auto& a: spot.native->battr) {
			  intv.push_back (a);
		  }
		  spot.restrictions.addInts("blockedAttrs", intv);
      }
        // Check for a video bid
        if(spot.ext.isMember("video")) {
            auto video = spot.ext["video"];

            if(video.isMember("linearity")) {
                spot.video->linearity.val = video["linearity"].asInt();
            }

            if(video.isMember("type")) {

                bool vast = false;
                bool html = false;
                // Type is defined as an array in MoPub 2.1 spec
                for(auto it = video["type"].begin(); it != video["type"].end(); it++) {
                    
                    const std::string &s = (*it).asString();

                    if(s == "VAST 2.0") {
                        // If VAST 2.0 is there.. protocol will be 2
                        // according to Table 6.7 of OpenRTB 2.1
                        spot.video->protocol.val = 2;
                        vast = true;
                    }

                    if(s == "HTML5") {
                        // Not sure what to do with this
                        html = true;
                    }
                }

                if(html && vast) {
                    // TO DO figure out which openrtb video protocol when we have both these tags
                    // for now, assume protocol = vast 2.0
                    spot.video->protocol.val = 2;
                }
            }

            /** Minimum video duration
             *  Maximum video duration
             *  Making sure that max >= min
             */

            int minduration = -1;

            if(video.isMember("minduration")) {
                minduration = video["minduration"].asInt();
                spot.video->minduration = minduration;   
            }

            if(video.isMember("maxduration")) {
                if(video.isMember("minduration") && 
                   minduration <= video["maxduration"].asInt()) {
                    spot.video->maxduration = video["maxduration"].asInt();   
                } else {
                    // Makes no sense that maxduration < minduration
                    THROW(mopubExchangeConnectorError) << "minduration cannot be higher than maxduration" << endl;
                }
            }

            // Since MoPub removes the Mime type, we will add none as a Mime-Type
            spot.video->mimes.push_back(OpenRTB::MimeType("none"));

            // Note : There is no need to add height and width since they
            // should be included in the banner object and thus will be populated
            // in the format object of the AdSpot object.
        }

        if(spot.ext.isMember("mraid")) {
            LOG(mopubExchangeConnectorTrace) << "Mobile Rich Media Ad Interface Definition is not supported." << endl;
        }
    }

	OpenRTBExchangeConnector::getAudienceId(res);
	OpenRTBExchangeConnector::getExchangeName(res);
//	std::cerr<<"bidrequest : "<<res->toJson()<<std::endl;  
	free(cstr1);
	free(cstr2);
	free(cstr3);
    return res;
}


void
MoPubExchangeConnector::
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
	int j = 0;
	for(auto i:crinfo->imptrackers){
		b.ext["imptrackers"][j] = i;
		j++;
	};
	b.ext["crtype"] = crinfo->crtype;
	b.ext["duration"] = crinfo->duration;
	b.psattr = crinfo->attr;
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
MoPubExchangeConnector::
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
        ExchangeConnector::registerFactory<MoPubExchangeConnector>();
	RTBKIT::FilterRegistry::registerFilter<RTBKIT::NativeTitleLengthFilter>();
	RTBKIT::FilterRegistry::registerFilter<RTBKIT::NativeImageFilter>();
	RTBKIT::FilterRegistry::registerFilter<RTBKIT::NativeDataFilter>();
	RTBKIT::FilterRegistry::registerFilter<RTBKIT::NativeVideoFilter>();
	RTBKIT::FilterRegistry::registerFilter<RTBKIT::NativeContextFilter>();
	RTBKIT::FilterRegistry::registerFilter<RTBKIT::NativeContextSubtypeFilter>();
	RTBKIT::FilterRegistry::registerFilter<RTBKIT::NativePlcmttypeFilter>();
    }
} atInit;
}

