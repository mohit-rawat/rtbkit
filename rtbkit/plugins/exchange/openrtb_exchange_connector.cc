/* openrtb_exchange_connector.cc
   Eric Robert, 7 May 2013
   
   Implementation of the OpenRTB exchange connector.
*/

#include "openrtb_exchange_connector.h"
#include "rtbkit/common/testing/exchange_source.h"
#include "rtbkit/plugins/bid_request/openrtb_bid_source.h"
#include "rtbkit/plugins/bid_request/openrtb_bid_request_parser.h"
#include "rtbkit/plugins/exchange/http_auction_handler.h"
#include "rtbkit/core/agent_configuration/agent_config.h"
#include "rtbkit/openrtb/openrtb_parsing.h"
#include "soa/types/json_printing.h"
#include <boost/any.hpp>
#include <boost/lexical_cast.hpp>
#include "jml/utils/file_functions.h"
#include "jml/arch/info.h"
#include "jml/utils/rng.h"
#include "aerospike/as_config.h"
#include "aerospike/aerospike_key.h"
using namespace Datacratic;

namespace Datacratic {

template<typename T, int I, typename S>
Json::Value jsonEncode(const ML::compact_vector<T, I, S> & vec)
{
    Json::Value result(Json::arrayValue);
    for (unsigned i = 0;  i < vec.size();  ++i)
        result[i] = jsonEncode(vec[i]);
    return result;
}

template<typename T, int I, typename S>
ML::compact_vector<T, I, S>
jsonDecode(const Json::Value & val, ML::compact_vector<T, I, S> *)
{
    ExcAssert(val.isArray());
    ML::compact_vector<T, I, S> res;
    res.reserve(val.size());
    for (unsigned i = 0;  i < val.size();  ++i)
        res.push_back(jsonDecode(val[i], (T*)0));
    return res;
}

} // namespace Datacratic

namespace OpenRTB {

template<typename T>
Json::Value jsonEncode(const OpenRTB::List<T> & vec)
{
    using Datacratic::jsonEncode;
    Json::Value result(Json::arrayValue);
    for (unsigned i = 0;  i < vec.size();  ++i)
        result[i] = jsonEncode(vec[i]);
    return result;
}

template<typename T>
OpenRTB::List<T>
jsonDecode(const Json::Value & val, OpenRTB::List<T> *)
{
    using Datacratic::jsonDecode;
    ExcAssert(val.isArray());
    OpenRTB::List<T> res;
    res.reserve(val.size());
    for (unsigned i = 0;  i < val.size();  ++i)
        res.push_back(jsonDecode(val[i], (T*)0));
    return res;
}

} // namespace OpenRTB

namespace RTBKIT {

BOOST_STATIC_ASSERT(hasFromJson<Datacratic::Id>::value == true);
BOOST_STATIC_ASSERT(hasFromJson<int>::value == false);

/*****************************************************************************/
/* OPENRTB EXCHANGE CONNECTOR                                                */
/*****************************************************************************/

OpenRTBExchangeConnector::
OpenRTBExchangeConnector(ServiceBase & owner, const std::string & name)
    : HttpExchangeConnector(name, owner)
{
		as_config_init(&config);
		config.hosts[0].addr = "127.0.0.1";
		config.hosts[0].port = 3000;
		aerospike_init(&as, &config);
		aerospike_connect(&as, &err);
}

OpenRTBExchangeConnector::
OpenRTBExchangeConnector(const std::string & name,
                         std::shared_ptr<ServiceProxies> proxies)
    : HttpExchangeConnector(name, proxies)
{
}

std::shared_ptr<BidRequest>
OpenRTBExchangeConnector::
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

    // Check that it's version 2.1
    std::string openRtbVersion = it->second;
    if (openRtbVersion != "2.1" && openRtbVersion != "2.2") {
        connection.sendErrorResponse("UNSUPPORTED_OPENRTB_VERSION", "The request is required to be using version 2.1 or 2.2 of the OpenRTB protocol but requested " + openRtbVersion);
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

    return result;
}

double
OpenRTBExchangeConnector::
getTimeAvailableMs(HttpAuctionHandler & connection,
                   const HttpHeader & header,
                   const std::string & payload)
{
    // Scan the payload quickly for the tmax parameter.
    static const std::string toFind = "\"tmax\":";
    std::string::size_type pos = payload.find(toFind);
    if (pos == std::string::npos)
        return 30.0;
    
    int tmax = atoi(payload.c_str() + pos + toFind.length());
    return (absoluteTimeMax < tmax) ? absoluteTimeMax : tmax;
}

HttpResponse
OpenRTBExchangeConnector::
getResponse(const HttpAuctionHandler & connection,
            const HttpHeader & requestHeader,
            const Auction & auction) const
{
    const Auction::Data * current = auction.getCurrentData();

    if (current->hasError())
        return getErrorResponse(connection,
                                current->error + ": " + current->details);

    OpenRTB::BidResponse response;
    response.id = auction.id;

    response.ext = getResponseExt(connection, auction);

    // Create a spot for each of the bid responses
    for (unsigned spotNum = 0; spotNum < current->responses.size(); ++spotNum) {
        if (!current->hasValidResponse(spotNum))
            continue;

        setSeatBid(auction, spotNum, response);
    }

    if (response.seatbid.empty())
        return HttpResponse(204, "none", "");

    static Datacratic::DefaultDescription<OpenRTB::BidResponse> desc;
    std::ostringstream stream;
    StreamJsonPrintingContext context(stream);
    desc.printJsonTyped(&response, context);

    return HttpResponse(200, "application/json", stream.str());
}

Json::Value
OpenRTBExchangeConnector::
getResponseExt(const HttpAuctionHandler & connection,
               const Auction & auction) const
{
    return {};
}

HttpResponse
OpenRTBExchangeConnector::
getDroppedAuctionResponse(const HttpAuctionHandler & connection,
                          const std::string & reason) const
{
    return HttpResponse(204, "none", "");
}

HttpResponse
OpenRTBExchangeConnector::
getErrorResponse(const HttpAuctionHandler & connection,
                 const std::string & message) const
{
    Json::Value response;
    response["error"] = message;
    return HttpResponse(400, response);
}

std::string
OpenRTBExchangeConnector::
getBidSourceConfiguration() const
{
    auto suffix = std::to_string(port());
    return ML::format("{\"type\":\"openrtb\",\"url\":\"%s\"}",
                      ML::fqdn_hostname(suffix) + ":" + suffix);
}

void
OpenRTBExchangeConnector::
setSeatBid(Auction const & auction,
           int spotNum,
           OpenRTB::BidResponse & response) const
{
    const Auction::Data * data = auction.getCurrentData();

    // Get the winning bid
    auto & resp = data->winningResponse(spotNum);

    int seatIndex = 0;
    if(response.seatbid.empty()) {
        response.seatbid.emplace_back();
    }

    OpenRTB::SeatBid & seatBid = response.seatbid.at(seatIndex);

    // Add a new bid to the array
    seatBid.bid.emplace_back();

    // Put in the variable parts
    auto & b = seatBid.bid.back();
    b.cid = Id(resp.agentConfig->externalId);
    b.crid = Id(resp.creativeId);
    b.id = Id(auction.id, auction.request->imp[0].id);
    b.impid = auction.request->imp[spotNum].id;
    b.price.val = getAmountIn<CPM>(resp.price.maxPrice);
}

  void
  OpenRTBExchangeConnector::
  getAudienceId(std::shared_ptr<BidRequest> res)
  {
	  std::string deviceid;
	  std::string deviceidgroup;
	  if(res->device!=NULL){
		  if(!res->device->ifa.empty()){
			  deviceid = res->device->ifa;
			  deviceidgroup = "ifa";
		  }
		  else if(!res->device->didsha1.empty()){
			  deviceid = res->device->didsha1;
			  deviceidgroup = "didsha1";
		  }
		  else if(!res->device->didmd5.empty()){
			  deviceid = res->device->didmd5;
			  deviceidgroup = "didmd5";
		  }
		  else if(!res->device->dpidsha1.empty()){
			  deviceid = res->device->dpidsha1;
			  deviceidgroup = "dpidsha1";
		  }
		  else if(!res->device->dpidmd5.empty()){
			  deviceid = res->device->dpidmd5;
			  deviceidgroup = "dpidmd5";
		  }
		  else if(!res->device->macsha1.empty()){
			  deviceid = res->device->macsha1;
			  deviceidgroup = "macsha1";
		  }
		  else if(!res->device->macmd5.empty()){
			  deviceid = res->device->macmd5;
			  deviceidgroup = "macmd5";
		  };
	  }
	  if(deviceid.empty() && res->user!=NULL){
		  if(!res->user->id.toString().empty()){
			  deviceid = res->user->id.toString();
			  if(res->exchange == "mopub"){
				  deviceidgroup = "mopubid";
			  }
			  else if(res->exchange == "smaato"){
			    std::cerr<<"check1============="<<std::endl;
				  deviceidgroup = "smaatoid";
			  };
		  }
		  else if(!res->user->buyeruid.toString().empty()){
			  deviceid = res->user->buyeruid.toString();
			  if(res->exchange == "mopub"){
				  deviceidgroup = "mopubbuyerid";
			  }
			  else if(res->exchange == "smaato"){
				  deviceidgroup = "smaatobuyerid";
			  };
		  };
	  }
	  if(deviceid.empty()){
		  struct timeval now;
		  gettimeofday (&now, NULL);
		  auto t0 = now.tv_usec + (unsigned long long)now.tv_sec * 1000000;
		  deviceid = "s_"+ to_string(t0);
		  deviceidgroup  = "timeid";
	  };


	  if(deviceidgroup == "timeid"){
		  res->ext["audience"] = deviceid;
	  }else{

		  as_record* p_rec = NULL;
		  as_record_init(p_rec, 2);
		  std::string abc;

		  as_key key;
<<<<<<< HEAD
		  as_key_init(&key, "audience", deviceidgroup.c_str(), deviceid.c_str());
		  std::cerr<<deviceidgroup<<std::endl;
		  std::cerr<<deviceid<<std::endl;
=======
		  as_key_init(&key, "test", deviceidgroup.c_str(), deviceid.c_str());
		  std::cerr<<"deviceidgrup : "<<deviceidgroup<<std::endl;
		  std::cerr<<"deviceid : "<<deviceid<<std::endl;
		  std::cerr<<"deviceid.empty() : "<<deviceid.empty()<<std::endl;
>>>>>>> 95674f6325331ff7acf3891ba5ce72c9e5514a15
		  int audienceid = 0;

		  if (aerospike_key_get(&as, &err, NULL, &key, &p_rec) == AEROSPIKE_ERR_RECORD_NOT_FOUND) {
		    std::cerr<<"check1"<<std::endl;
			  as_operations ops;
			  as_operations_inita(&ops, 2);
			  as_operations_add_incr(&ops, "count", 1);
			  as_operations_add_read(&ops, "count");

			  as_record *rec = NULL;
			  as_key counterkey;
<<<<<<< HEAD
			  as_key_init(&counterkey, "audience", "audienceidcounter", "counter");
=======
			    std::cerr<<"check 1.1"<<std::endl;
			  as_key_init(&counterkey, "test", "audienceidcounter", "counter");
			    std::cerr<<"check 1.5"<<std::endl;
>>>>>>> 95674f6325331ff7acf3891ba5ce72c9e5514a15
			  if (aerospike_key_operate(&as, &err, NULL, &counterkey, &ops, &rec) != AEROSPIKE_OK) {
			    std::cerr<<"check2"<<std::endl;
				  fprintf(stderr, "err(%d) %s at [%s:%d]\n", err.code, err.message, err.file, err.line);
			  }
			  else {
			    std::cerr<<"check3"<<std::endl;
				  audienceid = as_record_get_int64(rec, "count", 0);
			  };
			  as_record rec_auid;
			  as_record_inita(&rec_auid, 1);
			  if(audienceid != 0){
			    std::cerr<<"check4"<<std::endl;
				  as_record_set_int64(&rec_auid, "audienceid", audienceid);
				  aerospike_key_put(&as, &err, NULL, &key, &rec_auid);
			  };

		  }else{
		    std::cerr<<"check6"<<std::endl;
			  aerospike_key_get(&as, &err, NULL, &key, &p_rec);
			  int64_t temp = 1;
			  audienceid = as_record_get_int64(p_rec, "audienceid", temp);
		  }
		  res->ext["audience"] = to_string(audienceid);
		  std::cerr<<"audienceid"<<res->ext["audience"];
	  }
  }


  void
  OpenRTBExchangeConnector::
  changeCountryCode(std::shared_ptr<BidRequest> res)
  {
	  as_key key;
	  as_record* p_rec = NULL;
	  as_key_init(&key, "test", "locationSet", res->location.countryCode.c_str());

	  if (aerospike_key_get(&as, &err, NULL, &key, &p_rec) == AEROSPIKE_ERR_RECORD_NOT_FOUND) {
		  std::cerr<<"key not correct"<<std::endl;
		  res->location.countryCode = "/";
	  }else{
		  aerospike_key_get(&as, &err, NULL, &key, &p_rec);
		  res->location.countryCode = as_record_get_str(p_rec, "val");
	  }
  }
  void
  OpenRTBExchangeConnector::
  getExchangeName(std::shared_ptr<BidRequest> res)
  {
	  res->ext["exchange"] = res->exchange;
  }

} // namespace RTBKIT

namespace {
    using namespace RTBKIT;

    struct AtInit {
        AtInit() {
            ExchangeConnector::registerFactory<OpenRTBExchangeConnector>();
        }
    } atInit;
}
