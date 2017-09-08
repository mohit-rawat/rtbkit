/* openrtb_parsing.cc
   Jeremy Barnes, 22 February 2013
   Copyright (c) 2013 Datacratic Inc.  All rights reserved.

   Structure descriptions for OpenRTB.
*/

#include "openrtb_parsing.h"
#include "soa/types/json_parsing.h"

using namespace OpenRTB;
//using namespace RTBKIT;
using namespace std;

namespace Datacratic {

DefaultDescription<BidRequest>::
DefaultDescription()
{
    onUnknownField = [=] (BidRequest * br, JsonParsingContext & context)
        {
            //cerr << "got unknown field " << context.printPath() << endl;

            std::function<Json::Value & (int, Json::Value &)> getEntry
            = [&] (int n, Json::Value & curr) -> Json::Value &
            {
                if (n == context.path.size())
                    return curr;
                else if (context.path[n].index != -1)
                    return getEntry(n + 1, curr[context.path[n].index]);
                else return getEntry(n + 1, curr[context.path[n].fieldName()]);
            };

            getEntry(0, br->unparseable)
                = context.expectJson();
        };

    addField("id", &BidRequest::id, "Bid Request ID",
             new StringIdDescription());
    addField("imp", &BidRequest::imp, "Impressions");
    //addField("context", &BidRequest::context, "Context of bid request");
    addField("site", &BidRequest::site, "Information about site the request is on");
    addField("app", &BidRequest::app, "Information about the app the request is being shown in");
    addField("device", &BidRequest::device, "Information about the device on which the request was made");
    addField("user", &BidRequest::user, "Information about the user who is making the request");
    addField("at", &BidRequest::at, "Type of auction: 1(st) or 2(nd)");
    addField("tmax", &BidRequest::tmax, "Maximum response time (ms)");
    addField("wseat", &BidRequest::wseat, "Allowable seats");
    addField("allimps", &BidRequest::allimps, "Set to 1 if all impressions on this page are in the bid request");
    addField("cur", &BidRequest::cur, "List of acceptable currencies to bid in");
    addField("bcat", &BidRequest::bcat, "Blocked advertiser content categories");
    addField("badv", &BidRequest::badv, "Blocked adversiser domains");
    addField("bapp", &BidRequest::bapp, "Blocked applocations");
    addField("regs", &BidRequest::regs, "Legal regulations");
    addField("ext", &BidRequest::ext, "Extended fields outside of protocol");
    addField("unparseable", &BidRequest::unparseable, "Unparseable fields are collected here");
    addField("test", &BidRequest::test, "Indicator of test mode in which auctions are not billable");
	addField("bseat", &BidRequest::bseat, "Block list of buyer seats");
	addField("wlang", &BidRequest::wlang, "White list of languages for creatives using ISO-639-1-alpha-2");
	addField("source", &BidRequest::source, "data about the inventory source");
}

DefaultDescription<Source>::
DefaultDescription()
{
	addField("fd", &Source::fd, "Entity reponsible for final impreesion");
	addField("tid", &Source::tid, "Transaction ID");
	addField("pchain", &Source::pchain, "Payment ID chain");
	addField("ext", &Source::ext, "Exchange-specific fields");
}
	
DefaultDescription<Impression>::
DefaultDescription()
{
  std::cerr<<"=====check1======"<<endl;
    addField("id", &Impression::id, "Impression ID within bid request",
             new StringIdDescription());
    addField("banner", &Impression::banner, "Banner information if a banner ad");
    addField("video", &Impression::video, "Video information if a video ad");
    addField("native", &Impression::native, "Native information if a native ad");
    addField("displaymanager", &Impression::displaymanager, "Display manager that renders the ad");
    addField("displaymanagerver", &Impression::displaymanagerver, "Version of the display manager");
    addField("instl", &Impression::instl, "Is the ad interstitial");
    addField("tagid", &Impression::tagid, "Add tag ID for auction");
    addField("bidfloor", &Impression::bidfloor, "Bid floor in CPM of currency");
    addField("bidfloorcur", &Impression::bidfloorcur, "Currency for bid floor");
    addField("secure", &Impression::secure, "Does the impression require https");
    addField("iframebuster", &Impression::iframebuster, "Supported iframe busters");
    addField("pmp", &Impression::pmp, "Contains any deals eligible for the impression");
	addField("clickbrowser", &Impression::clickbrowser, "Type of browser opened on click");
	addField("exp", &Impression::exp, "Time between auction and auction impression");
	addField("metric", &Impression::metric, "Array of Metric object");
    addField("ext", &Impression::ext, "Extended impression attributes");
}

DefaultDescription<OpenRTB::Metric>::
DefaultDescription()
{
	addField("type", &Metric::type, "Type of metric being presented");
	addField("value", &Metric::value, "Number representing the value of metric");
	addField("vendor", &Metric::vendor, "Source of the value");
	addField("ext", &Metric::ext, "Exchange-specific extensions");
}

DefaultDescription<OpenRTB::Content>::
DefaultDescription()
{
    addField("id", &Content::id, "Unique identifier representing the content",
             new StringIdDescription());
    addField("episode", &Content::episode, "Unique identifier representing the episode");
    addField("title", &Content::title, "Title of the content");
    addField("series", &Content::series, "Series to which the content belongs");
    addField("season", &Content::season, "Season to which the content belongs");
    addField("url", &Content::url, "URL of the content's original location");
    addField("cat", &Content::cat, "IAB content categories of the content");
    addField("videoquality", &Content::videoquality, "Quality of the video");
    ValueDescriptionT<CSList> * kwdesc = new Utf8CommaSeparatedListDescription();
    addField("keywords", &Content::keywords, "Keywords describing the keywords", kwdesc);
    addField("contentrating", &Content::contentrating, "Rating of the content");
    addField("userrating", &Content::userrating, "User-provided rating of the content");
    addField("context", &Content::context, "Rating context");
    addField("livestream", &Content::livestream, "Is this being live streamed?");
    addField("sourcerelationship", &Content::sourcerelationship, "Relationship with content source");
    addField("producer", &Content::producer, "Content producer");
    addField("len", &Content::len, "Content length in seconds");
    addField("qagmediarating", &Content::qagmediarating, "Media rating per QAG guidelines");
    addField("embeddable", &Content::embeddable, "1 if embeddable, 0 otherwise");
    addField("language", &Content::language, "ISO 639-1 Content language");
    addField("artist", &Content::artist, "artist of the content");
    addField("genre", &Content::genre, "genre of the content");
	addField("album", &Content::album, "album of the content");
	addField("prodq", &Content::prodq, "production quality");
	addField("isrc", &Content::isrc, "International Standard Recording Code");
	addField("data", &Content::data, "Additional content data");
    addField("ext", &Content::ext, "Extensions to the protocol go here");
}
	
DefaultDescription<OpenRTB::Banner>::
DefaultDescription()
{
    addField<List<int>>("w", &Banner::w, "Width of ad in pixels",
             new FormatListDescription());
    addField<List<int>>("h", &Banner::h, "Height of ad in pixels",
             new FormatListDescription());
    addField("hmin", &Banner::hmin, "Ad minimum height");
    addField("hmax", &Banner::hmax, "Ad maximum height");
    addField("wmin", &Banner::wmin, "Ad minimum width");
    addField("wmax", &Banner::wmax, "Ad maximum width");
    addField("id", &Banner::id, "Ad ID", new StringIdDescription());
    addField("pos", &Banner::pos, "Ad position");
    addField("btype", &Banner::btype, "Blocked creative types");
    addField("battr", &Banner::battr, "Blocked creative attributes");
    addField("mimes", &Banner::mimes, "Whitelist of content MIME types (none = all)");
    addField("topframe", &Banner::topframe, "Is it in the top frame or an iframe?");
    addField("expdir", &Banner::expdir, "Expandable ad directions");
    addField("api", &Banner::api, "Supported APIs");
    addField("format", &Banner::format, "Array of format objects");
	addField("vcm", &Banner::vcm, "Companion ad rendering mode");
    addField("ext", &Banner::ext, "Extensions to the protocol go here");
}	

DefaultDescription<OpenRTB::Format>::
DefaultDescription()
{
	addField("w", &Format::w, "Width in DIPS");
	addField("h", &Format::h, "Height in DIPS");
	addField("wratio", &Format::wratio, "Relative width when size is as ratio");
	addField("hratio", &Format::hratio, "Relative height when size is as ratio");
	addField("wmin", &Format::wmin, "The minimum width in DIPS");
	addField("ext", &Format::ext, "Extensions to Format");
}
	
DefaultDescription<OpenRTB::Video>::
DefaultDescription()
{
    addField("mimes", &Video::mimes, "Content MIME types supported");
    addField("linearity", &Video::linearity, "Ad linearity");
    addField("minduration", &Video::minduration, "Minimum duration in seconds");
    addField("maxduration", &Video::maxduration, "Maximum duration in seconds");
    addField("protocol", &Video::protocol, "Bid response supported protocol");
    addField("protocols", &Video::protocols, "Bid response supported protocols");
    addField("w", &Video::w, "Width of player in pixels");
    addField("h", &Video::h, "Height of player in pixels");
    addField("startdelay", &Video::startdelay, "Starting delay in seconds of video");
    addField("sequence", &Video::sequence, "Which ad number in the video");
    addField("battr", &Video::battr, "Which creative attributes are blocked");
    addField("maxextended", &Video::maxextended, "Maximum extended video ad duration");
    addField("minbitrate", &Video::minbitrate, "Minimum bitrate for ad in kbps");
    addField("maxbitrate", &Video::maxbitrate, "Maximum bitrate for ad in kbps");
    addField("boxingallowed", &Video::boxingallowed, "Is letterboxing allowed?");
    addField("playbackmethod", &Video::playbackmethod, "Available playback methods");
    addField("delivery", &Video::delivery, "Available delivery methods");
    addField("pos", &Video::pos, "Ad position");
    addField("companionad", &Video::companionad, "List of companion banners available");
    addField("api", &Video::api, "List of supported API frameworks");
    addField("companiontype", &Video::companiontype, "List of VAST companion types");
	addField("skip", &Video::skip, "Video can be skipped or not");
	addField("skipmin", &Video::skipmin, "Minimum duration required for the video to be skipped");
	addField("skipafter", &Video::skipafter, "Seconds a video must play before skipping enabled");
	addField("placement", &Video::placement, "Placement type for the impression");
	addField("playbackend", &Video::playbackend, "Event that causes playback to end");
    addField("ext", &Video::ext, "Extensions to the protocol go here");
}

  DefaultDescription<OpenRTB::Native>::
  DefaultDescription()
  {
    std::cerr<<"====check parsing 1=="<<std::endl;
    addField("request", &Native::request, "Native request payload");
    addField("ver", &Native::ver, "Version of the Native Ad Specification");
    addField("api", &Native::api, "List of supported API frameworks for this impression");
    addField("battr", &Native::battr, "Blocked creative attributes");
    addField("ext", &Native::ext, "Placeholder for exchange-specific extensions to OpenRTB");
  }

  DefaultDescription<OpenRTB::NativeRequest>::
  DefaultDescription()
  {
    std::cerr<<"====check parsing 2=="<<std::endl;
    addField("native", &NativeRequest::native, "native object inside native request");
//not a necessary field and coming as integer in bidswitch request
//	addField("ver", &NativeRequest::ver, "Version of the Native Markup version in use");
    addField("layout", &NativeRequest::layout, "The Layout ID of the native ad unit");
    addField("adunit", &NativeRequest::adunit, "The Ad unit ID of the native ad unit");
    addField("context", &NativeRequest::context, "The context in which the ad appears");
    addField("contextsubtype", &NativeRequest::contextsubtype, "A more detailed context in which the ad appears");
    addField("plcmttype", &NativeRequest::plcmttype, "The design/format/layout of the ad unit being offered");
    addField("plcmtcnt", &NativeRequest::plcmtcnt, "The number of identical placements in this Layout");
    addField("seq", &NativeRequest::seq, "0 for the first ad, 1 for the second ad, and so on");
    addField("assets", &NativeRequest::assets, "array of Asset objects");
    addField("ext", &NativeRequest::ext, "Placeholder for exchange-specific extensions to OpenRTB");
  }

  DefaultDescription<OpenRTB::NativeSub>::
  DefaultDescription()
  {
    addField("ver", &NativeSub::ver, "Version of the Native Markup version in use");
    addField("layout", &NativeSub::layout, "The Layout ID of the native ad unit");
    addField("adunit", &NativeSub::adunit, "The Ad unit ID of the native ad unit");
    addField("context", &NativeSub::context, "The context in which the ad appears");
    addField("contextsubtype", &NativeSub::contextsubtype, "A more detailed context in which the ad appears");
    addField("plcmttype", &NativeSub::plcmttype, "The design/format/layout of the ad unit being offered");
    addField("plcmtcnt", &NativeSub::plcmtcnt, "The number of identical placements in this Layout");
    addField("seq", &NativeSub::seq, "0 for the first ad, 1 for the second ad, and so on");
    addField("assets", &NativeSub::assets, "array of Asset objects");
    addField("ext", &NativeSub::ext, "Placeholder for exchange-specific extensions to OpenRTB");
  }

  DefaultDescription<OpenRTB::NativeTitle>::
  DefaultDescription()
  {
    addField("len", &NativeTitle::len, "Maximum length of the text in the title element");
    addField("ext", &NativeTitle::ext, "");
  }

  DefaultDescription<OpenRTB::NativeImg>::
  DefaultDescription()
  {
    addField("type", &NativeImg::type, "Type ID of the image element supported by the publisher");
    addField("w", &NativeImg::w, "Width of the image in pixels");
    addField("wmin", &NativeImg::wmin, "The minimum requested width of the image in pixels");
    addField("h", &NativeImg::h, "Height of the image in pixels");
    addField("hmin", &NativeImg::hmin, "The minimum requested height of the image in pixels");
    addField("mimes", &NativeImg::mimes, "Whitelist of content MIME types supported");
    addField("ext", &NativeImg::ext, "Placeholder for exchange-specific extensions to OpenRTB");
  }

  DefaultDescription<OpenRTB::NativeVideo>::
  DefaultDescription()
  {
    addField("mimes", &NativeVideo::mimes, "content MIME types supported");
    addField("minduration", &NativeVideo::minduration, "Minimum video ad duration in seconds");
    addField("maxduration", &NativeVideo::maxduration, "Maximum video ad duration in seconds");
    addField("protocols", &NativeVideo::protocols, "array of video protocols the publisher can accept in the bid response");
    addField("ext", &NativeVideo::ext, "");
  }

  DefaultDescription<OpenRTB::NativeData>::
  DefaultDescription()
  {
    addField("type", &NativeData::type, "Type ID of the element supported by the publisher");
    addField("len", &NativeData::len, "Maximum length of the text");
    addField("ext", &NativeData::ext, "Placeholder for exchange-specific extensions to OpenRTB");
  }

  DefaultDescription<OpenRTB::Asset>::
  DefaultDescription()
  {
    addField("id", &Asset::id, "Unique asset ID, assigned by exchange");
    addField("required", &Asset::required, "Set to 1 if asset is required");
    addField("title", &Asset::title, "Title object for title assets");
    addField("img", &Asset::img, "Image object for image assets");
    addField("video", &Asset::video, "Video object for video assets");
    addField("data", &Asset::data, "Data object for ratings, prices");
    addField("ext", &Asset::ext, "Placeholder for exchange-specific extensions to OpenRTB");
  } 

DefaultDescription<OpenRTB::Publisher>::
DefaultDescription()
{
    addField("id", &Publisher::id, "Unique ID representing the publisher/producer",
            new StringIdDescription());
    addField("name", &Publisher::name, "Publisher/producer name");
    addField("cat", &Publisher::cat, "Content categories");
    addField("domain", &Publisher::domain, "Domain name of publisher");
    addField("ext", &Publisher::ext, "Extensions to the protocol go here");
}

DefaultDescription<OpenRTB::Context>::
DefaultDescription()
{
    addField("id", &Context::id, "Site or app ID on the exchange",
            new StringIdDescription());
    addField("name", &Context::name, "Site or app name");
    addField("domain", &Context::domain, "Site or app domain");
    addField("cat", &Context::cat, "IAB content categories for the site/app");
    addField("sectioncat", &Context::sectioncat, "IAB content categories for site/app section");
    addField("pagecat", &Context::pagecat, "IAB content categories for site/app page");
    addField("privacypolicy", &Context::privacypolicy, "Site or app has a privacy policy");
    addField("publisher", &Context::publisher, "Publisher of site or app");
    addField("content", &Context::content, "Content of site or app");
    ValueDescriptionT<CSList> * kwdesc = new Utf8CommaSeparatedListDescription();
    addField("keywords", &Context::keywords, "Keywords describing siter or app", kwdesc);
    addField("ext", &Context::ext, "Extensions to the protocol go here");
}

DefaultDescription<OpenRTB::Site>::
DefaultDescription()
{
    addParent<OpenRTB::Context>();

    //addField("id",   &Context::id,   "Site ID");
    addField("page",   &SiteInfo::page,   "URL of the page");
    addField("ref",    &SiteInfo::ref,    "Referrer URL to the page");
    addField("search", &SiteInfo::search, "Search string to page");
    addField("mobile", &Site::mobile, "Mobile-optimized signal or not");
}

DefaultDescription<OpenRTB::App>::
DefaultDescription()
{
    addParent<OpenRTB::Context>();

    addField("ver",    &AppInfo::ver,     "Application version");
    addField("bundle", &AppInfo::bundle,  "Application bundle name");
    addField("paid",   &AppInfo::paid,    "Is a paid version of the app");
    addField("storeurl", &AppInfo::storeurl, "App store url");
}

DefaultDescription<OpenRTB::Geo>::
DefaultDescription()
{
    addField("lat", &Geo::lat, "Latiture of user in degrees from equator");
    addField("lon", &Geo::lon, "Longtitude of user in degrees (-180 to 180)");
    addField("country", &Geo::country, "ISO 3166-1 country code");
    addField("region", &Geo::region, "ISO 3166-2 Region code");
    addField("regionfips104", &Geo::regionfips104, "FIPS 10-4 region code");
    addField("metro", &Geo::metro, "Metropolitan region (Google Metro code");
    addField("city", &Geo::city, "City name (UN Code for Trade and Transport)");
    addField("zip", &Geo::zip, "Zip or postal code");
    addField("type", &Geo::type, "Source of location data");
    addField("ext", &Geo::ext, "Extensions to the protocol go here");
    addField("utcoffset", &Geo::utcoffset, "Local time as the number +/- of minutes from UTC");
    /// Datacratic extension
    addField("dma", &Geo::dma, "DMA code");
    /// Rubicon extension
    addField("latlonconsent", &Geo::latlonconsent, "User has given consent for lat/lon information to be used");
	addField("lastfix", &Geo::lastfix, "Seconds since this geolocation fix was established");
	addField("accuracy", &Geo::accuracy, "Location accuracy in meters");
	addField("ipservice", &Geo::ipservice, "Service used to provide geolocation frim IP");
}

DefaultDescription<OpenRTB::Device>::
DefaultDescription()
{
    addField("dnt", &Device::dnt, "Is do not track set");
    addField("ua", &Device::ua, "User agent of device");
    addField("ip", &Device::ip, "IP address of device");
    addField("geo", &Device::geo, "Geographic location of device");
    addField("didsha1", &Device::didsha1, "SHA-1 Device ID");
    addField("didmd5", &Device::didmd5, "MD5 Device ID");
    addField("dpidsha1", &Device::dpidsha1, "SHA-1 Device Platform ID");
    addField("dpidmd5", &Device::dpidmd5, "MD5 Device Platform ID");
    addField("macsha1", &Device::macsha1, "SHA-1 Mac Address");
    addField("macmd5", &Device::macmd5, "MD5 Mac Address");
    addField("ipv6", &Device::ipv6, "Device IPv6 address");
    addField("carrier", &Device::carrier, "Carrier or ISP derived from IP address");
    addField("language", &Device::language, "Browser language");
    addField("make", &Device::make, "Device make");
    addField("model", &Device::model, "Device model");
    addField("os", &Device::os, "Device OS");
    addField("osv", &Device::osv, "Device OS version");
    addField("js", &Device::js, "Javascript is supported");
    addField("connectiontype", &Device::connectiontype, "Device connection type");
    addField("devicetype", &Device::devicetype, "Device type");
    addField("flashver", &Device::flashver, "Flash version on device");
    addField("ifa", &Device::ifa, "Native identifier for advertisers");
    addField("h", &Device::h, "height of screen in pixels");
    addField("w", &Device::w, "width of screen in pixels");
    addField("lmt", &Device::lmt, "limit ad tracking");
    addField("hwv", &Device::hwv, "hardware version");
    addField("pxratio", &Device::pxratio, "The ratio of physical pixels to device independent pixels");
	addField("geofetch", &Device::geofetch, "Is geolocation API available or not");
    addField("ppi", &Device::ppi, "Screen size as pixels per linear inch");
	addField("mccmnc", &Device::mccmnc, "Mobile carrier as the MCC-MNC code");
    addField("ext", &Device::ext, "Extensions to device field go here");
}

DefaultDescription<OpenRTB::Segment>::
DefaultDescription()
{
    addField("id", &Segment::id, "Segment ID", new StringIdDescription());
    addField("name", &Segment::name, "Segment name");
    addField("value", &Segment::value, "Segment value");
    addField("ext", &Segment::ext, "Extensions to the protocol go here");
    /// Datacratic extension
    addField("segmentusecost", &Segment::segmentusecost, "Segment use cost in CPM");
}

DefaultDescription<OpenRTB::Data>::
DefaultDescription()
{
    addField("id", &Data::id, "Segment ID", new StringIdDescription());
    addField("name", &Data::name, "Segment name");
    addField("segment", &Data::segment, "Data segment");
    addField("ext", &Data::ext, "Extensions to the protocol go here");
    /// Datacratic extension
    addField("datausecost", &Data::datausecost, "Cost of using data in CPM");
    addField("usecostcurrency", &Data::usecostcurrency, "Currency for use cost");
}

DefaultDescription<OpenRTB::User>::
DefaultDescription()
{
    addField("id", &User::id, "Exchange specific user ID", new StringIdDescription());
    addField("buyeruid", &User::buyeruid, "Seat specific user ID",
            new StringIdDescription());
    addField("yob", &User::yob, "Year of birth");
    addField("gender", &User::gender, "Gender");
    ValueDescriptionT<CSList> * kwdesc = new Utf8CommaSeparatedListDescription();
    addField("keywords", &User::keywords, "Keywords about user", kwdesc);
    addField("customdata", &User::customdata, "Exchange-specific custom data");
    addField("geo", &User::geo, "Geolocation of user at registration");
    addField("data", &User::data, "User segment data");
    addField("ext", &User::ext, "Extensions to the protocol go here");
    /// Rubicon extension
    addField("tz", &User::tz, "Timezone offset of user in seconds wrt GMT");
    addField("sessiondepth", &User::sessiondepth, "Session depth of user in visits");
}

DefaultDescription<OpenRTB::Bid>::
DefaultDescription()
{
    addField("id", &Bid::id, "Bidder's ID to identify the bid",
             new StringIdDescription());
    addField("impid", &Bid::impid, "ID of impression",
             new StringIdDescription());
    addField("price", &Bid::price, "CPM price to bid for the impression");
    addField("adid", &Bid::adid, "ID of ad to be served if bid is won",
             new StringIdDescription());
    addField("nurl", &Bid::nurl, "Win notice/ad markup URL");
    addField("adm", &Bid::adm, "Ad markup");
    addField("adomain", &Bid::adomain, "Advertiser domain(s)");
    addField("iurl", &Bid::iurl, "Image URL for content checking");
    addField("cid", &Bid::cid, "Campaign ID",
             new StringIdDescription());
    addField("crid", &Bid::crid, "Creative ID",
             new StringIdDescription());
//    addField("attr", &Bid::attr, "Creative attributes");
    addField("dealid", &Bid::dealid, "Deal Id for PMP Auction");
    addField("w", &Bid::w, "width of ad in pixels");
    addField("h", &Bid::h, "height of ad in pixels");
    addField("bundle", &Bid::bundle, "Bundle or package name of the app being advertised");
    addField("cat", &Bid::cat, "IAB content categories of the creative");
    addField("attr", &Bid::psattr, "just to publish attr");
    addField("ext", &Bid::ext, "Extensions");
    addField("burl", &Bid::burl, "Billing notice url");
	addField("qagmediarating", &Bid::qagmediarating, "Creative media rating per IQG guidelines");
	addField("protocol", &Bid::protocol, "Video response protocol of the markup");
	addField("lurl", &Bid::lurl, "Loss notice URL");
	addField("tactic", &Bid::tactic, "Tactic ID");
	addField("language", &Bid::language, "Language of the creative");
	addField("wratio", &Bid::wratio, "Relative width of the creative");
	addField("hratio", &Bid::hratio, "Relative height of the creative");
}

DefaultDescription<OpenRTB::SeatBid>::
DefaultDescription()
{
    addField("bid", &SeatBid::bid, "Bids made for this seat");
    addField("seat", &SeatBid::seat, "Seat name who is bidding",
             new StringIdDescription());
    addField("group", &SeatBid::group, "Do we require all bids to be won in a group?");
    addField("ext", &SeatBid::ext, "Extensions");
}

DefaultDescription<OpenRTB::BidResponse>::
DefaultDescription()
{
    addField("id", &BidResponse::id, "ID of auction",
             new StringIdDescription());
    addField("seatbid", &BidResponse::seatbid, "Array of bids for each seat");
    addField("bidid", &BidResponse::bidid, "Bidder's internal ID for this bid",
             new StringIdDescription());
    addField("cur", &BidResponse::cur, "Currency in which we're bidding");
    addField("customData", &BidResponse::customData, "Custom data to be stored for user");
    addField("ext", &BidResponse::ext, "Extensions");
}

DefaultDescription<OpenRTB::Deal>::
DefaultDescription()
{
    addField("id", &Deal::id, "Id of the deal", new StringIdDescription);
    addField("bidfloor", &Deal::bidfloor, "bid floor");
    addField("bidfloorcur", &Deal::bidfloorcur, "Currency of the deal");
    addField("wseat", &Deal::wseat, "List of buyer seats allowed");
    addField("wadomain", &Deal::wadomain, "List of advertiser domains allowed");
    addField("at", &Deal::at, "Auction type");
    addField("ext", &Deal::ext, "Extensions");
}

DefaultDescription<OpenRTB::PMP>::
DefaultDescription()
{
    addField("private_auction", &PMP::privateAuction, "is a private auction");
    addField("deals", &PMP::deals, "Deals");
    addField("ext", &PMP::ext, "Extensions");
}

DefaultDescription<OpenRTB::Regulations>::
DefaultDescription()
{
    addField("coppa", &Regulations::coppa, "is coppa regulated traffic");
    addField("ext", &Regulations::ext, "Extensions");
}


} // namespace Datacratic
