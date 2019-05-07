/** augmentor_ex_runner.cc                                 -*- C++ -*-
    Rémi Attab, 22 Feb 2013
    Copyright (c) 2013 Datacratic.  All rights reserved.

    Runner for our augmentor example.

*/

#include "msisdn_augmentor.h"
//#include "augmentor_ex.h"
#include "soa/service/service_utils.h"

#include <boost/program_options/cmdline.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/positional_options.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include <iostream>
#include <thread>
#include <chrono>


using namespace std;

/******************************************************************************/
/* MAIN                                                                       */
/******************************************************************************/

int main(int argc, char** argv)
{
    using namespace boost::program_options;

    Datacratic::ServiceProxyArguments args;

    options_description options = args.makeProgramOptions();
    options.add_options() ("help,h", "Print this message");

    variables_map vm;
    store(command_line_parser(argc, argv).options(options).run(), vm);
    notify(vm);

    if (vm.count("help")) {
        cerr << options << endl;
        return 1;
    }

    auto serviceProxies = args.makeServiceProxies();
    std::cout<<"main called in augmentor_ex_runner.cc"<<std::endl;
    /*
    RTBKIT::FrequencyCapAugmentor augmentor(serviceProxies, "frequency-cap-ex");
    augmentor.init();
    augmentor.start();
    */
    RTBKIT::MsisdnAugmentor msisdn_augmentor(serviceProxies, "adx-msisdn");
    msisdn_augmentor.init();
    msisdn_augmentor.start();
    
    while (true) this_thread::sleep_for(chrono::seconds(10));

    // Won't ever reach this point but this is how you shutdown an augmentor.
    msisdn_augmentor.shutdown();

    return 0;
}
