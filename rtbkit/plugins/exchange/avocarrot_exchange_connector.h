/* avocarrot_exchange_connector.h                                    -*- C++ -*-
   Jeremy Barnes, 12 March 2013
   Copyright (c) 2013 Datacratic Inc.  All rights reserved.

*/

#pragma once

#include "rtbkit/plugins/exchange/openrtb_exchange_connector.h"

namespace RTBKIT {


/*****************************************************************************/
/* AVOCARROT EXCHANGE CONNECTOR                                                */
/*****************************************************************************/

/** Exchange connector for Avocarrot.  This speaks their flavour of the
    OpenRTB 2.3 protocol and native version 1.0
*/

	struct AvocarrotExchangeConnector: public OpenRTBExchangeConnector {
		AvocarrotExchangeConnector(ServiceBase & owner, const std::string & name);
		AvocarrotExchangeConnector(const std::string & name,
							   std::shared_ptr<ServiceProxies> proxies);

		static std::string exchangeNameString() {
			return "avocarrot";
		}

		virtual std::string exchangeName() const {
			return exchangeNameString();
		}

		virtual std::shared_ptr<BidRequest>
			parseBidRequest(HttpAuctionHandler & connection,
							const HttpHeader & header,
							const std::string & payload);

#if 0
		virtual HttpResponse
			getResponse(const HttpAuctionHandler & connection,
						const HttpHeader & requestHeader,
						const Auction & auction) const;
#endif

		virtual double
			getTimeAvailableMs(HttpAuctionHandler & connection,
							   const HttpHeader & header,
							   const std::string & payload) {
			// TODO: check that is at it seems
			return 200.0;
		}

		/** This is the information that the Avocarrot exchange needs to keep
			for each campaign (agent).
		*/
		struct CampaignInfo {
			Id seat;          ///< ID of the Avocarrot exchange seat
			std::string iurl; ///< Image URL for content checking
		};

		virtual ExchangeCompatibility
			getCampaignCompatibility(const AgentConfig & config,
									 bool includeReasons) const;

		/** This is the information that Avocarrot needs in order to properly
			filter and serve a creative.
		*/
		struct CreativeInfo {
			std::string adm;                                ///< Ad markup
			std::vector<std::string> adomain;               ///< Advertiser domains
			Id crid;                                        ///< Creative ID
			std::set<std::string> cat;          ///< Creative category Appendix 6.1
			std::string nurl;       ///< Win notice URL
			std::set<std::string> imptrackers;       ///< Win notice URL
			std::set<int> attr;            ///< Creative attributes Appendix 6.3
			std::set<int> type;           ///< Creative type Appendix 6.2
			std::string adformat;    ///<required for sending appropriate responses for different adformats.
		};

		virtual ExchangeCompatibility
			getCreativeCompatibility(const Creative & creative,
									 bool includeReasons) const;

		virtual bool
			bidRequestCreativeFilter(const BidRequest & request,
									 const AgentConfig & config,
									 const void * info) const;

	private:
		virtual void setSeatBid(Auction const & auction,
								int spotNum,
								OpenRTB::BidResponse & response) const;
	};



} // namespace RTBKIT
