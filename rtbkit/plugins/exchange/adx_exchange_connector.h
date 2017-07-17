/* adx_exchange_connector.h
   Matthieu Labour, December 2014
   Copyright (c) 2014 Datacratic.  All rights reserved. */

#pragma once
 
#include "rtbkit/plugins/exchange/openrtb_exchange_connector.h"
 
namespace RTBKIT {

/*****************************************************************************/
/* ADX EXCHANGE CONNECTOR                                                */
/*****************************************************************************/

/** Exchange connector for Adx.  This speaks their flavour of the
    OpenRTB 2.1 protocol.
*/
	struct AdxExchangeConnector: public OpenRTBExchangeConnector {
		AdxExchangeConnector(ServiceBase & owner, const std::string & name);
		AdxExchangeConnector(const std::string & name,
								std::shared_ptr<ServiceProxies> proxies);

		static std::string nobid;
 
		static std::string exchangeNameString() {
			return "adx";
		}
 
		virtual std::string exchangeName() const {
			return exchangeNameString();
		}

		virtual std::shared_ptr<BidRequest>
			parseBidRequest(HttpAuctionHandler & connection,
							const HttpHeader & header,
							const std::string & payload);


		double getTimeAvailableMs(HttpAuctionHandler & handler,
								  const HttpHeader & header,
								  const std::string & payload) {
			return 100.0;
		}
 
		double getRoundTripTimeMs(HttpAuctionHandler & handler,
								  const HttpHeader & header) {
			return 35.0;
		}
    
		struct CampaignInfo {
			Id seat;                ///< ID of the exchange seat
			std::string iurl; ///< Image URL for content checking
		};

		virtual ExchangeCompatibility
			getCampaignCompatibility(const AgentConfig & config,
									 bool includeReasons) const;

		struct CreativeInfo {
			std::set<std::string> cat;
			std::string adm;        ///< Actual XHTML ad markup
			std::vector<std::string> adomain; ///< Advertiser Domain
//			std::vector<std::string> mimeTypes; ///< MIME Types for this creative
			std::string adformat;    ///<required for sending appropriate responses for different adformats
			std::set<int>  type;                            ///< Creative type Appendix 6.2
			std::set<int>  attr;                            ///< Creative attributes Appendix 6.3
			std::set<std::string>impression_tracking_url;       ///< impression tracking url
			Id crid;                                        ///< Creative ID
		};

		virtual ExchangeCompatibility
			getCreativeCompatibility(const Creative & creative, bool includeReasons) const;

		virtual bool
			bidRequestCreativeFilter(const BidRequest & request,
									 const AgentConfig & config,
									 const void * info) const;

	private:
		virtual void setSeatBid(Auction const & auction,
								int spotNum,
								OpenRTB::BidResponse & response) const;



	};

} // namespace RTBKit
