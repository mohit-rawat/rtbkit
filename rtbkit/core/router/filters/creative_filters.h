/** creative_filter.h                                 -*- C++ -*-
    Rémi Attab, 09 Aug 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Filters related to creatives.

*/

#pragma once

#include "generic_creative_filters.h"
#include "priority.h"
#include "rtbkit/common/exchange_connector.h"

#include <unordered_map>
#include <unordered_set>
#include <mutex>

namespace RTBKIT {


/******************************************************************************/
/* VIDEO DURATION FILTER                                                      */
/******************************************************************************/
/*
struct DurationFilter : public IterativeCreativeFilter<DurationFilter>
{
	static constexpr const char* name = "DurationFilter";
	unsigned priority() const { return 10; }
	bool filterCreative(FilterState &state, const AdSpot &spot,
                        const AgentConfig &config, const Creative &creative) const
		{
			if (creative.adformat=="video" && spot.video){
				int duration = creative.videoConfig["duration"].asInt();
				if(duration >= spot.video->minduration.val && duration <=spot.video->maxduration.val){
						return true;
					}else{return false;};
			}else{return true;};
		}
};
*/
/******************************************************************************/
/* VIDEO FILTER                                                               */
/******************************************************************************/

	struct VideoFilter : public IterativeCreativeFilter<VideoFilter>
	{
		static constexpr const char *name = "VideoFilter";
		unsigned priority() const { return 10; }
		bool filterCreative(FilterState &state, const AdSpot &spot,
							const AgentConfig &config, const Creative &creative) const{
			//      if (creative.adformat=="native"){
			if(spot.video){
				std::cerr<<"====1===="<<std::endl;
				Datacratic::List<int> protocols;
				for(auto i : spot.video->protocols){
					protocols.push_back(i.val);
				}
				std::vector<std::string> mimes;
				for(auto i: spot.video->mimes ){
					mimes.push_back(i.type);
				}
				if(
					(spot.video->minduration.val <= creative.videoConfig["duration"].asInt()) && (spot.video->maxduration.val >= creative.videoConfig["duration"].asInt()) && (std::find(mimes.begin(), mimes.end(), creative.videoConfig["mimetype"].asString()) != mimes.end()) && (std::find(protocols.begin(), protocols.end(), creative.videoConfig["protocol"].asInt()) != protocols.end())){
					return true;
				}else{return false;}
			}else{
				return true;}
			return true;
		}
	};


/******************************************************************************/
/* ADFORMAT FILTER                                                            */
/******************************************************************************/

struct AdformatFilter : public IterativeCreativeFilter<AdformatFilter>
{
    static constexpr const char *name = "AdformatFilter";
    unsigned priority() const { return 10; }
    bool filterCreative(FilterState &state, const AdSpot &spot,
                        const AgentConfig &config, const Creative &creative) const{
//		std::cerr<<"creativematrix-- : "<<state.creatives(impIndex).print()<<std::endl;
        for (const auto& format : spot.formats){
			std::cerr<<"format-- : "<<format.print()<<std::endl;
		};
		if(creative.adformat=="video"){
			std::cerr<<"checking"<<std::endl;
			if(spot.video){
				return true;
			}else{return false;};	
		}else if (creative.adformat=="banner"){
			if(spot.banner){
				return true;	
			}else{return false;}
		}else{
			return true;
		}
	} 		
};

/******************************************************************************/
/* NATIVE TITLELENGTH FILTER                                                  */
/******************************************************************************/

  struct NativeTitleLengthFilter : public IterativeCreativeFilter<NativeTitleLengthFilter>
  {
    static constexpr const char *name = "NativeTitleLengthFilter";
    unsigned priority() const { return 0xF303; }
    bool filterCreative(FilterState &state, const AdSpot &spot,
			const AgentConfig &config, const Creative &creative) const{
      int length = creative.nativeConfig["assets"]["title"]["len"].asInt();
      //      if (creative.adformat=="native"){
      if(spot.native){
	std::cerr<<"====1===="<<std::endl;
	for(auto asset : spot.native->request.native.assets){
	  if(asset.title){
	    std::cerr<<"====2===="<<std::endl;
	    if(asset.required.val == true){
	      std::cerr<<"====3===="<<std::endl;
	      if(length <= asset.title->len.val){
		std::cerr<<"req.len = "<<asset.title->len.val<<std::endl;
		std::cerr<<"config.len = "<<length<<std::endl;
		return true;
	      }else{return false;}
	    }
	    else{
	      return true;
	    }
	  }
	}
      }else{
	return true;
      }return true;
    }
  };

  /******************************************************************************/
  /* NATIVE IMAGE FILTER                                                        */
  /******************************************************************************/
  struct NativeImageFilter : public IterativeCreativeFilter<NativeImageFilter>
  {
    static constexpr const char *name = "NativeImageFilter";
    unsigned priority() const { return 0xF306; }
    bool filterCreative(FilterState &state, const AdSpot &spot,
			const AgentConfig &config, const Creative &creative) const{
      std::cerr<<"CreativeMatrix : "<<state.creatives(0).print()<<std::endl;
      std::cerr<<"impression(apspot).ext : "<<spot.ext<<std::endl;
      if(spot.native){
	std::cerr<<"====1===="<<std::endl;
	auto imageassets = creative.nativeConfig["assets"]["images"];
	std::vector<bool> unused(imageassets.size(), true);
	for(auto asset : spot.native->request.native.assets){
	  if(asset.img){
	    if(asset.required.val == true){
	      //          std::cerr<<"====3===="<<std::endl;
	      for(int i = 0; i <= unused.size(); i++){
		//                          std::cerr<<"==i====="<<i<<std::endl;
		if(i == unused.size()){
		  //          std::cerr<<"unused.size()"<<unused.size()<<std::endl;
		  return false;

		};
		if(unused[i] == true && asset.img->type.val == imageassets[i]["type"].asInt() && ((asset.img->w.val == imageassets[i]["w"].asInt()) || ((asset.img->wmin.val != -1) && (asset.img->wmin.val <= imageassets[i]["w"].asInt()))) && ((asset.img->h.val == imageassets[i]["h"].asInt()) || ((asset.img->hmin.val != -1) && (asset.img->hmin.val <= imageassets[i]["h"].asInt()))) && ((asset.img->mimes).empty() || (std::find(asset.img->mimes.begin(), asset.img->mimes.end(), imageassets[i]["mimetype"].asString()) != asset.img->mimes.end()))){
		  std::cerr<<"====5===="<<std::endl;
		  unused[i] = false;
		  state.AssetList[spot.id.toString()][std::to_string((int)(config.externalId))][std::to_string(creative.id)][std::to_string(asset.id.val)] = imageassets[i]["id"]; 
		  std::cerr<<"====6===="<<std::endl;
		  break;
		}
	      }
	    }
	  }
	}
	return true;
      }
      return true;
    }
  };


  /******************************************************************************/
  /* NATIVE DATA FILTER                                                         */
  /******************************************************************************/

  struct NativeDataFilter : public IterativeCreativeFilter<NativeDataFilter>
  {
    static constexpr const char *name = "NativeDataFilter";
    unsigned priority() const { return 0xF305; }
    bool filterCreative(FilterState &state, const AdSpot &spot,
			const AgentConfig &config, const Creative &creative) const{

      //      if (creative.adformat=="native"){
      if(spot.native){
	//          std::cerr<<"====1===="<<std::endl;
	auto dataassets = creative.nativeConfig["assets"]["data"];
	std::vector<bool> unused(dataassets.size(), true);
	for(auto asset : spot.native->request.native.assets){
	  if(asset.data){
	    if(asset.required.val == true){
	                std::cerr<<"====3===="<<std::endl;
	      for(int i = 0; i <= unused.size(); i++){
		                          std::cerr<<"==i====="<<i<<std::endl;
		if(i == unused.size()){
		  std::cerr<<"====4===="<<std::endl;
		  return false;
		};
		if(unused[i] == true && asset.data->type.val == dataassets[i]["type"].asInt() && ((asset.data->len.val == -1) ||  asset.data->len.val >= dataassets[i]["len"].asInt())){
			std::cerr<<"====5===="<<std::endl;
			unused[i] = false;
			state.AssetList[spot.id.toString()][std::to_string((int)(config.externalId))][std::to_string(creative.id)][std::to_string(asset.id.val)] = dataassets[i]["id"];
			break;
		}
	      }
	    }
	  }
	}
	return true;
      }
      return true;
    }
  };

  /******************************************************************************/
  /* NATIVE VIDEO FILTER                                                        */
  /******************************************************************************/

  struct NativeVideoFilter : public IterativeCreativeFilter<NativeVideoFilter>
  {
    static constexpr const char *name = "NativeVideoFilter";
    unsigned priority() const { return 0xF304; }
    bool filterCreative(FilterState &state, const AdSpot &spot,
			const AgentConfig &config, const Creative &creative) const{
      //      if (creative.adformat=="native"){
      if(spot.native){
	std::cerr<<"====1===="<<std::endl;
	for(auto asset : spot.native->request.native.assets){
	  if(asset.video){
	    std::cerr<<"====2===="<<std::endl;
	    if(asset.required.val == true){
	      std::cerr<<"====3===="<<std::endl;
	      if(
		 (asset.video->minduration.val <= creative.nativeConfig["assets"]["video"]["duration"].asInt()) && (asset.video->maxduration.val >= creative.nativeConfig["assets"]["video"]["duration"].asInt()) && (std::find(asset.video->mimes.begin(), asset.video->mimes.end(), creative.nativeConfig["assets"]["video"]["mimetype"].asString()) != asset.video->mimes.end()) && (std::find(asset.video->protocols.begin(), asset.video->protocols.end(), creative.nativeConfig["assets"]["video"]["protocol"].asInt()) != asset.video->protocols.end())){
		return true;
	      }else{return false;}
	    }
	    else{
	      return true;
	    }
	  }
	}
      }else{
	return true;
      }return true;
    }
  };

  /******************************************************************************/
  /* NATIVE CONTEXT FILTER                                                      */
  /******************************************************************************/

  struct NativeContextFilter : public IterativeCreativeFilter<NativeContextFilter>
  {
    static constexpr const char *name = "NativeContextFilter";
	  unsigned priority() const { return 0xF302; }
    bool filterCreative(FilterState &state, const AdSpot &spot,
			const AgentConfig &config, const Creative &creative) const{
      if(spot.native){
	std::cerr<<"====1===="<<std::endl;
	if(creative.nativeConfig["context"].asInt()){
	  if(spot.native->request.native.context.val == creative.nativeConfig["context"].asInt()){
	    return true;
	  }else{return false;}
	}else{return true;}
      }else{
	return true;
      }return true;
    }
  };

  /******************************************************************************/
  /* NATIVE CONTEXTSUBTTYPE FILTER                                              */
  /******************************************************************************/

  struct NativeContextSubtypeFilter : public IterativeCreativeFilter<NativeContextSubtypeFilter>
  {
    static constexpr const char *name = "NativeContextSubtypeFilter";
    unsigned priority() const { return 0xF301; }
    bool filterCreative(FilterState &state, const AdSpot &spot,
			const AgentConfig &config, const Creative &creative) const{
      if(spot.native){
	std::cerr<<"====1===="<<std::endl;
	if(creative.nativeConfig["contextsubtype"].asInt()){
	  if(spot.native->request.native.contextsubtype.val == creative.nativeConfig["contextSubType"].asInt()){
	    return true;
	  }else{return false;}
	}else{return true;}
      }else{
	return true;
      }return true;
    }
  };

  /******************************************************************************/
  /* NATIVE PLCMTTYPE FILTER                                                      */
  /******************************************************************************/

  struct NativePlcmttypeFilter : public IterativeCreativeFilter<NativePlcmttypeFilter>
  {
    static constexpr const char *name = "NativePlcmttypeFilter";
    unsigned priority() const { return 0xF300; }
    bool filterCreative(FilterState &state, const AdSpot &spot,
			const AgentConfig &config, const Creative &creative) const{
      if(spot.native){
	std::cerr<<"====1===="<<std::endl;
	if(creative.nativeConfig["plcmttype"].asInt()){
	  if(spot.native->request.native.plcmttype.val == creative.nativeConfig["plcmttype"].asInt()){
	    return true;
	  }else{return false;}
	}else{return true;}
      }else{
	return true;
      }return true;
    }
  };

/******************************************************************************/
/* CREATIVE FORTMAT FILTER                                                    */
/******************************************************************************/

struct CreativeFormatFilter : public CreativeFilter<CreativeFormatFilter>
{
    static constexpr const char* name = "CreativeFormat";
    unsigned priority() const { return Priority::CreativeFormat; }

    void addCreative(
            unsigned cfgIndex, unsigned crIndex, const Creative& creative)
    {
        auto formatKey = makeKey(creative.format);
        formatFilter[formatKey].set(crIndex, cfgIndex);
    }

    void removeCreative(
            unsigned cfgIndex, unsigned crIndex, const Creative& creative)
    {
        auto formatKey = makeKey(creative.format);
        formatFilter[formatKey].reset(crIndex, cfgIndex);
    }

    void filterImpression(
            FilterState& state, unsigned impIndex, const AdSpot& imp) const
    {
        // The 0x0 format means: match anything.
        CreativeMatrix creatives = get(Format(0,0));

        for (const auto& format : imp.formats)
            creatives |= get(format);

        state.narrowCreativesForImp(impIndex, creatives);
    }


private:

    typedef uint32_t FormatKey;
    static_assert(sizeof(FormatKey) == sizeof(Format),
            "Conversion of FormatKey depends on size of Format");

    FormatKey makeKey(const Format& format) const
    {
        return uint32_t(format.width << 16 | format.height);
    }

    CreativeMatrix get(const Format& format) const
    {
        auto it = formatFilter.find(makeKey(format));
        return it == formatFilter.end() ? CreativeMatrix() : it->second;
    }

    std::unordered_map<uint32_t, CreativeMatrix> formatFilter;
};


/******************************************************************************/
/* CREATIVE LANGUAGE FILTER                                                   */
/******************************************************************************/

struct CreativeLanguageFilter : public CreativeFilter<CreativeLanguageFilter>
{
    static constexpr const char* name = "CreativeLanguage";
    unsigned priority() const { return Priority::CreativeLanguage; }

    void addCreative(
            unsigned cfgIndex, unsigned crIndex, const Creative& creative)
    {
        impl.addIncludeExclude(cfgIndex, crIndex, creative.languageFilter);
    }

    void removeCreative(
            unsigned cfgIndex, unsigned crIndex, const Creative& creative)
    {
        impl.removeIncludeExclude(cfgIndex, crIndex, creative.languageFilter);
    }

    void filter(FilterState& state) const
    {
        state.narrowAllCreatives(
                impl.filter(state.request.language.utf8String()));
    }

private:
    CreativeIncludeExcludeFilter< CreativeListFilter<std::string> > impl;
};


/******************************************************************************/
/* CREATIVE LOCATION FILTER                                                   */
/******************************************************************************/

struct CreativeLocationFilter : public CreativeFilter<CreativeLocationFilter>
{
    static constexpr const char* name = "CreativeLocation";
    unsigned priority() const { return Priority::CreativeLocation; }

    void addCreative(
            unsigned cfgIndex, unsigned crIndex, const Creative& creative)
    {
        impl.addIncludeExclude(cfgIndex, crIndex, creative.locationFilter);
    }

    void removeCreative(
            unsigned cfgIndex, unsigned crIndex, const Creative& creative)
    {
        impl.removeIncludeExclude(cfgIndex, crIndex, creative.locationFilter);
    }

    void filter(FilterState& state) const
    {
        state.narrowAllCreatives(
                impl.filter(state.request.location.fullLocationString()));
    }

private:
    typedef CreativeRegexFilter<boost::u32regex, Datacratic::UnicodeString> BaseFilter;
    CreativeIncludeExcludeFilter<BaseFilter> impl;
};


/******************************************************************************/
/* CREATIVE EXCHANGE NAME FILTER                                              */
/******************************************************************************/

struct CreativeExchangeNameFilter :
        public CreativeFilter<CreativeExchangeNameFilter>
{
    static constexpr const char* name = "CreativeExchangeName";
    unsigned priority() const { return Priority::CreativeExchangeName; }


    void addCreative(
            unsigned cfgIndex, unsigned crIndex, const Creative& creative)
    {
        impl.addIncludeExclude(cfgIndex, crIndex, creative.exchangeFilter);
    }

    void removeCreative(
            unsigned cfgIndex, unsigned crIndex, const Creative& creative)
    {
        impl.removeIncludeExclude(cfgIndex, crIndex, creative.exchangeFilter);
    }

    void filter(FilterState& state) const
    {
        state.narrowAllCreatives(impl.filter(state.request.exchange));
    }

private:
    CreativeIncludeExcludeFilter< CreativeListFilter<std::string> > impl;
};


/******************************************************************************/
/* CREATIVE EXCHANGE FILTER                                                   */
/******************************************************************************/

/** \todo Find a way to write an efficient version of this. */
struct CreativeExchangeFilter : public IterativeFilter<CreativeExchangeFilter>
{
    static constexpr const char* name = "CreativeExchange";
    unsigned priority() const { return Priority::CreativeExchange; }

    void filter(FilterState& state) const
    {
        // no exchange connector means evertyhing gets filtered out.
        if (!state.exchange) {
            state.narrowAllCreatives(CreativeMatrix());
            return;
        }

        CreativeMatrix creatives;

        for (size_t cfgId = state.configs().next();
             cfgId < state.configs().size();
             cfgId = state.configs().next(cfgId+1))
        {
            const auto& config = *configs[cfgId];

            for (size_t crId = 0; crId < config.creatives.size(); ++crId) {
                const auto& creative = config.creatives[crId];

                auto exchangeInfo = getExchangeInfo(state, creative);
                if (!exchangeInfo.first) continue;

                bool ret = state.exchange->bidRequestCreativeFilter(
                        state.request, config, exchangeInfo.second);

                if (ret) creatives.set(crId, cfgId);

            }
        }

        state.narrowAllCreatives(creatives);
    }

private:

    const std::pair<bool, void*>
    getExchangeInfo(const FilterState& state, const Creative& creative) const
    {
        auto it = creative.providerData.find(state.exchange->exchangeName());

        if (it == creative.providerData.end())
            return std::make_pair(false, nullptr);
        return std::make_pair(true, it->second.get());
    }
};


/******************************************************************************/
/* CREATIVE SEGEMENTS FILTER                                                   */
/******************************************************************************/

struct CreativeSegmentsFilter : public CreativeFilter<CreativeSegmentsFilter>
{
    static constexpr const char* name = "CreativeSegments";

    unsigned priority() const { return Priority::CreativeSegments; }

    void addCreative(unsigned cfgIndex, unsigned crIndex,
                       const Creative& creative)
    {
        setCreative(cfgIndex, crIndex, creative, true);
    }

    void removeCreative(unsigned cfgIndex, unsigned crIndex,
                           const Creative& creative)
    {
        setCreative(cfgIndex, crIndex, creative, false);
    }

    virtual void setCreative(unsigned configIndex, unsigned crIndex,
                     const Creative& creative, bool value);

    void filter(FilterState& state) const ;

private:

    struct SegmentData
    {
        CreativeIncludeExcludeFilter<CreativeSegmentListFilter> ie;
        CreativeMatrix excludeIfNotPresent;
    };

    std::unordered_map<std::string, SegmentData> data;
    std::unordered_set<std::string> excludeIfNotPresent;

};


/******************************************************************************/
/* CREATIVE PMP FILTER                                                   */
/******************************************************************************/

struct CreativePMPFilter : public CreativeFilter<CreativePMPFilter>
{
    static constexpr const char* name = "CreativePMP";
    unsigned priority() const { return Priority::CreativePMP; }


    void addCreative(
            unsigned cfgIndex, unsigned crIndex, const Creative& creative)
    {
        dealFilter[Datacratic::Id(creative.dealId)].set(crIndex, cfgIndex);
    }

    void removeCreative(
            unsigned cfgIndex, unsigned crIndex, const Creative& creative)
    {
        dealFilter[Datacratic::Id(creative.dealId)].reset(crIndex, cfgIndex);
    }

    void filterImpression(
            FilterState& state, unsigned impIndex, const AdSpot& imp) const
    {
        CreativeMatrix creatives;
        if (imp.pmp && imp.pmp->privateAuction.val == 1){
            const auto pmp = *imp.pmp;
            for (const auto& deal : pmp.deals) 
                creatives |= get(deal.id);
        }else {
            creatives |= get(Datacratic::Id("")); //If filter is not set its a No-Deal agent
        }

        state.narrowCreativesForImp(impIndex, creatives);
    }

private:
    std::unordered_map<Datacratic::Id, CreativeMatrix> dealFilter;

    CreativeMatrix get(const Datacratic::Id dealId) const
    {
        auto it = dealFilter.find(dealId);
        return it == dealFilter.end() ? CreativeMatrix() : it->second;
    }
};

} // namespace RTBKIT
