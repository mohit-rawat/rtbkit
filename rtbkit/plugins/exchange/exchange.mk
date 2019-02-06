# RTBKIT exchange makefile

LIBRTB_EXCHANGE_SOURCES := \
	http_exchange_connector.cc \
	http_auction_handler.cc

LIBRTB_EXCHANGE_LINK := \
	zeromq boost_thread utils endpoint services rtb bid_request

$(eval $(call library,exchange,$(LIBRTB_EXCHANGE_SOURCES),$(LIBRTB_EXCHANGE_LINK)))

$(eval $(call library,openrtb_exchange,openrtb_exchange_connector.cc,exchange bid_test_utils aerospike openrtb_bid_request))
$(eval $(call library,rubicon_exchange,rubicon_exchange_connector.cc,openrtb_exchange))
$(eval $(call library,mopub_exchange,mopub_exchange_connector.cc,openrtb_exchange))
$(eval $(call library,smaato_exchange,smaato_exchange_connector.cc,openrtb_exchange))
$(eval $(call library,mobfox_exchange,mobfox_exchange_connector.cc,openrtb_exchange))
$(eval $(call library,pubnative_exchange,pubnative_exchange_connector.cc,openrtb_exchange))
$(eval $(call library,bidswitch_exchange,bidswitch_exchange_connector.cc,openrtb_exchange))
$(eval $(call library,nexage_exchange,nexage_exchange_connector.cc,openrtb_exchange))
$(eval $(call library,appnexus_exchange,appnexus_exchange_connector.cc,exchange bid_test_utils appnexus_bid_request))
$(eval $(call library,gumgum_exchange,gumgum_exchange_connector.cc,exchange bid_test_utils openrtb_bid_request))
$(eval $(call library,fbx_exchange,fbx_exchange_connector.cc,exchange bid_test_utils fbx_bid_request))
$(eval $(call library,adx_exchange,adx_exchange_connector.cc,openrtb_exchange))
$(eval $(call library,rtbkit_exchange,rtbkit_exchange_connector.cc,openrtb_exchange))
$(eval $(call library,casale_exchange,casale_exchange_connector.cc,openrtb_exchange))
$(eval $(call library,afmobi_exchange,afmobi_exchange_connector.cc,openrtb_exchange))
$(eval $(call library,adcolony_exchange,adcolony_exchange_connector.cc,openrtb_exchange))
$(eval $(call library,avocarrot_exchange,avocarrot_exchange_connector.cc,openrtb_exchange))

$(eval $(call include_sub_make,exchange_testing,testing,exchange_testing.mk))
