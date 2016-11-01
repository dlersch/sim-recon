// $Id$
//
//    File: JEventProcessor_lowlevel_online.cc
//

#include <TMath.h>

#include "JEventProcessor_lowlevel_online.h"
using namespace jana;

#include <BCAL/DBCALDigiHit.h>
#include <BCAL/DBCALTDCDigiHit.h>
#include <CDC/DCDCDigiHit.h>
#include <FDC/DFDCCathodeDigiHit.h>
#include <FDC/DFDCWireDigiHit.h>
#include <FCAL/DFCALDigiHit.h>
#include <PAIR_SPECTROMETER/DPSCDigiHit.h>
#include <PAIR_SPECTROMETER/DPSCTDCDigiHit.h>
#include <PAIR_SPECTROMETER/DPSDigiHit.h>
#include <PAIR_SPECTROMETER/DPSGeometry.h>
#include <TAGGER/DTAGHTDCDigiHit.h>
#include <TAGGER/DTAGHDigiHit.h>
#include <TAGGER/DTAGHHit.h>
#include <TAGGER/DTAGHGeometry.h>
#include <TAGGER/DTAGMGeometry.h>
#include <TOF/DTOFDigiHit.h>
#include <TOF/DTOFTDCDigiHit.h>
#include <TPOL/DTPOLSectorDigiHit.h>
#include <TPOL/DTPOLHit_factory.h>
#include <RF/DRFTDCDigiTime.h>
#include <START_COUNTER/DSCDigiHit.h>
#include <START_COUNTER/DSCTDCDigiHit.h>
#include <TAGGER/DTAGMDigiHit.h>
#include <TAGGER/DTAGMTDCDigiHit.h>

#include <FCAL/DFCALGeometry.h>

// Routine used to create our JEventProcessor
#include <JANA/JApplication.h>
#include <JANA/JFactory.h>
extern "C"{
  void InitPlugin(JApplication *app){
    InitJANAPlugin(app);
    app->AddProcessor(new JEventProcessor_lowlevel_online());
  }
} // "C"


//------------------
// JEventProcessor_lowlevel_online (Constructor)
//------------------
JEventProcessor_lowlevel_online::JEventProcessor_lowlevel_online()
{
  
}

//------------------
// ~JEventProcessor_lowlevel_online (Destructor)
//------------------
JEventProcessor_lowlevel_online::~JEventProcessor_lowlevel_online()
{
  
}

//------------------
// init
//------------------
jerror_t JEventProcessor_lowlevel_online::init(void)
{
    // initialize variables
    INDIVIDUAL_CHANNEL_DATA = false;
    
	// Set a base directory
	TDirectory *base = gDirectory;
	TDirectory *maindir = gDirectory->mkdir("lowlevel_online");
    maindir->cd();


	//------------------------ BCAL -----------------------
	//TDirectory *bcaldir = gDirectory->mkdir("BCAL");
	gDirectory->mkdir("BCAL");
    
    bcal_adc_integral = new TH1I("bcal_adc_integral", "BCAL fADC250 Pulse Integral;Integral (fADC counts)", 1000, 0, 40000);
    bcal_adc_integral_pedsub = new TH1I("bcal_adc_integral_pedsub", "BCAL fADC250 Pulse Integral (Pedestal Subtracted);Integral (fADC counts)", 1000, 0, 40000);
    bcal_adc_peak = new TH1I("bcal_adc_peak", "BCAL fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000);
    bcal_adc_peak_pedsub = new TH1I("bcal_adc_peak_pedsub", "BCAL fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000);
    bcal_adc_time = new TH1I("bcal_adc_time", "BCAL fADC250 Pulse Time;Time (ns)", 550, 0, 550);
    bcal_adc_pedestal = new TH1I("bcal_adc_pedestal", "BCAL fADC250 Summed Pedestal;Pedestal Sum (fADC counts)", 164, 0, 8200);
    bcal_adc_quality = new TH1I("bcal_adc_quality", "BCAL fADC250 Quality Factor;Quality Factor", 128, 0, 128);

    if(INDIVIDUAL_CHANNEL_DATA) {
        const int NBCAL_channels = 1536;

        bcal_adc_integral_chan = new TH2I("bcal_adc_integral", "BCAL fADC250 Pulse Integral;Integral (fADC counts)", 1000, 0, 40000, NBCAL_channels, 0, NBCAL_channels);
        bcal_adc_integral_pedsub_chan = new TH2I("bcal_adc_integral_pedsub", "BCAL fADC250 Pulse Integral (Pedestal Subtracted);Integral (fADC counts)", 1000, 0, 40000, NBCAL_channels, 0, NBCAL_channels);
        bcal_adc_peak_chan = new TH2I("bcal_adc_peak", "BCAL fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000, NBCAL_channels, 0, NBCAL_channels);
        bcal_adc_peak_pedsub_chan = new TH2I("bcal_adc_peak_pedsub", "BCAL fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000, NBCAL_channels, 0, NBCAL_channels);
        bcal_adc_time_chan = new TH2I("bcal_adc_time", "BCAL fADC250 Pulse Time;Time (ns)", 550, 0, 550, NBCAL_channels, 0, NBCAL_channels);
        bcal_adc_pedestal_chan = new TH2I("bcal_adc_pedestal", "BCAL fADC250 Summed Pedestal;Pedestal Sum (fADC counts)", 164, 0, 8200, NBCAL_channels, 0, NBCAL_channels);
        bcal_adc_quality_chan = new TH2I("bcal_adc_quality", "BCAL fADC250 Quality Factor;Quality Factor", 128, 0, 128, NBCAL_channels, 0, NBCAL_channels);
    }

	// Set y-axis labels for individual channel plots
    /*
	for(int ibin = 1; ibin <= 16; ibin++)
	{
		int idy = ibin-1; // convenient to use index that starts from zero!
		int layer  = 1 + (idy%4);
		int sector = 1 + idy/4;
		
		ostringstream ss;
		ss << "D  S" << sector << "  L" << layer;
		bcal_adc_occ->GetYaxis()->SetBinLabel(ibin, ss.str().c_str());

		ss.str("");
		ss << "U  S" << sector << "  L" << layer;
		bcal_adc_occ->GetYaxis()->SetBinLabel(ibin + 17, ss.str().c_str());
	}
    */

	bcal_num_events = new TH1I("bcal_num_events", "BCAL number of events", 1, 0.0, 1.0);

	//------------------------ CDC ------------------------
    maindir->cd();
	gDirectory->mkdir("CDC");

    // Save some geometry information
	int Nstraws[28] = {42, 42, 54, 54, 66, 66, 80, 80, 93, 93, 106, 106, 123, 123, 
			 135, 135, 146, 146, 158, 158, 170, 170, 182, 182, 197, 197, 209, 209};
    Nstraws_integrated.clear();
    Nstraws_integrated.push_back(0);
    for(int i=1; i<28; i++)
        Nstraws_integrated.push_back( Nstraws[i]+Nstraws_integrated[i-1] );

    const int Nstraws_total = Nstraws_integrated[27];

    cdc_adc_integral = new TH1I("cdc_adc_integral", "CDC fADC125 Pulse Integral;Integral (fADC counts)", 1000, 0, 40000);
    cdc_adc_integral_pedsub = new TH1I("cdc_adc_integral_pedsub", "CDC fADC125 Pulse Integral (Pedestal Subtracted);Integral (fADC counts)", 1000, 0, 40000);
    cdc_adc_time = new TH1I("cdc_adc_time", "CDC fADC125 Pulse Time;Time (ns)", 550, 0, 550);
    cdc_adc_pedestal = new TH1I("cdc_adc_pedestal", "CDC fADC125 Summed Pedestal;Pedestal Sum (fADC counts)", 164, 0, 8200);
    cdc_adc_quality = new TH1I("cdc_adc_quality", "CDC fADC125 Quality Factor;Quality Factor", 128, 0, 128);

    if(INDIVIDUAL_CHANNEL_DATA) {
        cdc_adc_integral_chan = new TH2I("cdc_adc_integral", "CDC fADC125 Pulse Integral;Integral (fADC counts)", 1000, 0, 40000, Nstraws_total, 0, Nstraws_total);
        cdc_adc_integral_pedsub_chan = new TH2I("cdc_adc_integral_pedsub", "CDC fADC125 Pulse Integral (Pedestal Subtracted);Integral (fADC counts)", 
                                                1000, 0, 40000, Nstraws_total, 0, Nstraws_total);
        cdc_adc_time_chan = new TH2I("cdc_adc_time", "CDC fADC125 Pulse Time;Time (ns)", 550, 0, 550, Nstraws_total, 0, Nstraws_total);
        cdc_adc_pedestal_chan = new TH2I("cdc_adc_pedestal", "CDC fADC125 Summed Pedestal;Pedestal Sum (fADC counts)", 164, 0, 8200, Nstraws_total, 0, Nstraws_total);
        cdc_adc_quality_chan = new TH2I("cdc_adc_quality", "CDC fADC125 Quality Factor;Quality Factor", 128, 0, 128, Nstraws_total, 0, Nstraws_total);        
    }

	cdc_num_events = new TH1I("cdc_num_events", "CDC number of events", 1, 0.0, 1.0);
    
	//------------------------ FCAL -----------------------
    maindir->cd();
	gDirectory->mkdir("FCAL");

    fcal_adc_integral = new TH1I("fcal_adc_integral", "FCAL fADC250 Pulse Integral;Integral (fADC counts)", 1000, 0, 40000);
    fcal_adc_integral_pedsub = new TH1I("fcal_adc_integral_pedsub", "FCAL fADC250 Pulse Integral (Pedestal Subtracted);Integral (fADC counts)", 1000, 0, 40000);
    fcal_adc_peak = new TH1I("fcal_adc_peak", "FCAL fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000);
    fcal_adc_peak_pedsub = new TH1I("fcal_adc_peak_pedsub", "FCAL fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000);
    fcal_adc_time = new TH1I("fcal_adc_time", "FCAL fADC250 Pulse Time;Time (ns)", 550, 0, 550);
    fcal_adc_pedestal = new TH1I("fcal_adc_pedestal", "FCAL fADC250 Summed Pedestal;Pedestal Sum (fADC counts)", 164, 0, 8200);
    fcal_adc_quality = new TH1I("fcal_adc_quality", "FCAL fADC250 Quality Factor;Quality Factor", 128, 0, 128);

    if(INDIVIDUAL_CHANNEL_DATA) {
        const int NFCAL_blocks = 3600;   // use the actual number...

        fcal_adc_integral_chan = new TH2I("fcal_adc_integral", "FCAL fADC250 Pulse Integral;Integral (fADC counts)", 1000, 0, 40000, NFCAL_blocks, 0, NFCAL_blocks);
        fcal_adc_integral_pedsub_chan = new TH2I("fcal_adc_integral_pedsub", "FCAL fADC250 Pulse Integral (Pedestal Subtracted);Integral (fADC counts)", 1000, 0, 40000, NFCAL_blocks, 0, NFCAL_blocks);
        fcal_adc_peak_chan = new TH2I("fcal_adc_peak", "FCAL fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000, NFCAL_blocks, 0, NFCAL_blocks);
        fcal_adc_peak_pedsub_chan = new TH2I("fcal_adc_peak_pedsub", "FCAL fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000, NFCAL_blocks, 0, NFCAL_blocks);
        fcal_adc_time_chan = new TH2I("fcal_adc_time", "FCAL fADC250 Pulse Time;Time (ns)", 550, 0, 550, NFCAL_blocks, 0, NFCAL_blocks);
        fcal_adc_pedestal_chan = new TH2I("fcal_adc_pedestal", "FCAL fADC250 Summed Pedestal;Pedestal Sum (fADC counts)", 164, 0, 8200, NFCAL_blocks, 0, NFCAL_blocks);
        fcal_adc_quality_chan = new TH2I("fcal_adc_quality", "FCAL fADC250 Quality Factor;Quality Factor", 128, 0, 128, NFCAL_blocks, 0, NFCAL_blocks);
    }

	fcal_num_events = new TH1I("fcal_num_events", "FCAL number of events", 1, 0.0, 1.0);

	//------------------------ FDC ------------------------
    maindir->cd();
	gDirectory->mkdir("FDC");

    fdc_adc_integral = new TH1I("fdc_adc_integral", "FDC fADC125 Pulse Integral;Integral (fADC counts)", 1000, 0, 40000);
    fdc_adc_integral_pedsub = new TH1I("fdc_adc_integral_pedsub", "FDC fADC125 Pulse Integral (Pedestal Subtracted);Integral (fADC counts)", 1000, 0, 40000);
    fdc_adc_time = new TH1I("fdc_adc_time", "FDC fADC125 Pulse Time;Time (ns)", 550, 0, 550);
    fdc_adc_pedestal = new TH1I("fdc_adc_pedestal", "FDC fADC125 Summed Pedestal;Pedestal Sum (fADC counts)", 164, 0, 8200);
    fdc_adc_quality = new TH1I("fdc_adc_quality", "FDC fADC125 Quality Factor;Quality Factor", 128, 0, 128);

    if(INDIVIDUAL_CHANNEL_DATA) {
        const int NFDC_strips = 4*6*2*192;
        fdc_adc_integral_chan = new TH2I("fdc_adc_integral", "FDC fADC125 Pulse Integral;Integral (fADC counts)", 1000, 0, 40000, NFDC_strips, 0, NFDC_strips);
        fdc_adc_integral_pedsub_chan = new TH2I("fdc_adc_integral_pedsub", "FDC fADC125 Pulse Integral (Pedestal Subtracted);Integral (fADC counts)", 
                                                1000, 0, 40000, NFDC_strips, 0, NFDC_strips);
        fdc_adc_time_chan = new TH2I("fdc_adc_time", "FDC fADC125 Pulse Time;Time (ns)", 550, 0, 550, NFDC_strips, 0, NFDC_strips);
        fdc_adc_pedestal_chan = new TH2I("fdc_adc_pedestal", "FDC fADC125 Summed Pedestal;Pedestal Sum (fADC counts)", 164, 0, 8200, NFDC_strips, 0, NFDC_strips);
        fdc_adc_quality_chan = new TH2I("fdc_adc_quality", "FDC fADC125 Quality Factor;Quality Factor", 128, 0, 128, NFDC_strips, 0, NFDC_strips);        
    }

    fdc_num_events = new TH1I("fdc_num_events", "FDC number of events", 1, 0.0, 1.0);

	//------------------------ PSC ---------------------
    maindir->cd();
	gDirectory->mkdir("PSC");

    psc_adc_integral = new TH1I("psc_adc_integral", "PSC fADC250 Pulse Integral;Integral (fADC counts)", 1000, 0, 40000);
    psc_adc_integral_pedsub = new TH1I("psc_adc_integral_pedsub", "PSC fADC250 Pulse Integral (Pedestal Subtracted);Integral (fADC counts)", 1000, 0, 40000);
    psc_adc_peak = new TH1I("psc_adc_peak", "PSC fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000);
    psc_adc_peak_pedsub = new TH1I("psc_adc_peak_pedsub", "PSC fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000);
    psc_adc_time = new TH1I("psc_adc_time", "PSC fADC250 Pulse Time;Time (ns)", 550, 0, 550);
    psc_adc_pedestal = new TH1I("psc_adc_pedestal", "PSC fADC250 Summed Pedestal;Pedestal Sum (fADC counts)", 164, 0, 8200);
    psc_adc_quality = new TH1I("psc_adc_quality", "PSC fADC250 Quality Factor;Quality Factor", 128, 0, 128);

    if(INDIVIDUAL_CHANNEL_DATA) {
        const int NPSC_channels = DPSGeometry::NUM_ARMS*DPSGeometry::NUM_COARSE_COLUMNS;

        psc_adc_integral_chan = new TH2I("psc_adc_integral", "PSC fADC250 Pulse Integral;Integral (fADC counts)", 1000, 0, 40000, NPSC_channels, 0, NPSC_channels);
        psc_adc_integral_pedsub_chan = new TH2I("psc_adc_integral_pedsub", "PSC fADC250 Pulse Integral (Pedestal Subtracted);Integral (fADC counts)", 1000, 0, 40000, NPSC_channels, 0, NPSC_channels);
        psc_adc_peak_chan = new TH2I("psc_adc_peak", "PSC fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000, NPSC_channels, 0, NPSC_channels);
        psc_adc_peak_pedsub_chan = new TH2I("psc_adc_peak_pedsub", "PSC fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000, NPSC_channels, 0, NPSC_channels);
        psc_adc_time_chan = new TH2I("psc_adc_time", "PSC fADC250 Pulse Time;Time (ns)", 550, 0, 550, NPSC_channels, 0, NPSC_channels);
        psc_adc_pedestal_chan = new TH2I("psc_adc_pedestal", "PSC fADC250 Summed Pedestal;Pedestal Sum (fADC counts)", 164, 0, 8200, NPSC_channels, 0, NPSC_channels);
        psc_adc_quality_chan = new TH2I("psc_adc_quality", "PSC fADC250 Quality Factor;Quality Factor", 128, 0, 128, NPSC_channels, 0, NPSC_channels);
    }

	//------------------------ PS ---------------------
    maindir->cd();
	gDirectory->mkdir("PS");

    ps_adc_integral = new TH1I("ps_adc_integral", "PS fADC250 Pulse Integral;Integral (fADC counts)", 1000, 0, 40000);
    ps_adc_integral_pedsub = new TH1I("ps_adc_integral_pedsub", "PS fADC250 Pulse Integral (Pedestal Subtracted);Integral (fADC counts)", 1000, 0, 40000);
    ps_adc_peak = new TH1I("ps_adc_peak", "PS fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000);
    ps_adc_peak_pedsub = new TH1I("ps_adc_peak_pedsub", "PS fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000);
    ps_adc_time = new TH1I("ps_adc_time", "PS fADC250 Pulse Time;Time (ns)", 550, 0, 550);
    ps_adc_pedestal = new TH1I("ps_adc_pedestal", "PS fADC250 Summed Pedestal;Pedestal Sum (fADC counts)", 164, 0, 8200);
    ps_adc_quality = new TH1I("ps_adc_quality", "PS fADC250 Quality Factor;Quality Factor", 128, 0, 128);

    if(INDIVIDUAL_CHANNEL_DATA) {
        const int NPS_channels = DPSGeometry::NUM_ARMS*DPSGeometry::NUM_FINE_COLUMNS;

        ps_adc_integral_chan = new TH2I("ps_adc_integral", "PS fADC250 Pulse Integral;Integral (fADC counts)", 1000, 0, 40000, NPS_channels, 0, NPS_channels);
        ps_adc_integral_pedsub_chan = new TH2I("ps_adc_integral_pedsub", "PS fADC250 Pulse Integral (Pedestal Subtracted);Integral (fADC counts)", 1000, 0, 40000, NPS_channels, 0, NPS_channels);
        ps_adc_peak_chan = new TH2I("ps_adc_peak", "PS fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000, NPS_channels, 0, NPS_channels);
        ps_adc_peak_pedsub_chan = new TH2I("ps_adc_peak_pedsub", "PS fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000, NPS_channels, 0, NPS_channels);
        ps_adc_time_chan = new TH2I("ps_adc_time", "PS fADC250 Pulse Time;Time (ns)", 550, 0, 550, NPS_channels, 0, NPS_channels);
        ps_adc_pedestal_chan = new TH2I("ps_adc_pedestal", "PS fADC250 Summed Pedestal;Pedestal Sum (fADC counts)", 164, 0, 8200, NPS_channels, 0, NPS_channels);
        ps_adc_quality_chan = new TH2I("ps_adc_quality", "PS fADC250 Quality Factor;Quality Factor", 128, 0, 128, NPS_channels, 0, NPS_channels);
    }

	ps_num_events = new TH1I("ps_num_events", "PS number of events", 1, 0.0, 1.0);

	//------------------------ ST -------------------------
    maindir->cd();
	gDirectory->mkdir("ST");

    st_adc_integral = new TH1I("st_adc_integral", "ST fADC250 Pulse Integral;Integral (fADC counts)", 1000, 0, 40000);
    st_adc_integral_pedsub = new TH1I("st_adc_integral_pedsub", "ST fADC250 Pulse Integral (Pedestal Subtracted);Integral (fADC counts)", 1000, 0, 40000);
    st_adc_peak = new TH1I("st_adc_peak", "ST fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000);
    st_adc_peak_pedsub = new TH1I("st_adc_peak_pedsub", "ST fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000);
    st_adc_time = new TH1I("st_adc_time", "ST fADC250 Pulse Time;Time (ns)", 550, 0, 550);
    st_adc_pedestal = new TH1I("st_adc_pedestal", "ST fADC250 Summed Pedestal;Pedestal Sum (fADC counts)", 164, 0, 8200);
    st_adc_quality = new TH1I("st_adc_quality", "ST fADC250 Quality Factor;Quality Factor", 128, 0, 128);

    if(INDIVIDUAL_CHANNEL_DATA) {
        const int NST_sectors = 30;

        st_adc_integral_chan = new TH2I("st_adc_integral", "ST fADC250 Pulse Integral;Integral (fADC counts)", 1000, 0, 40000, NST_sectors, 0, NST_sectors);
        st_adc_integral_pedsub_chan = new TH2I("st_adc_integral_pedsub", "ST fADC250 Pulse Integral (Pedestal Subtracted);Integral (fADC counts)", 1000, 0, 40000, NST_sectors, 0, NST_sectors);
        st_adc_peak_chan = new TH2I("st_adc_peak", "ST fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000, NST_sectors, 0, NST_sectors);
        st_adc_peak_pedsub_chan = new TH2I("st_adc_peak_pedsub", "ST fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000, NST_sectors, 0, NST_sectors);
        st_adc_time_chan = new TH2I("st_adc_time", "ST fADC250 Pulse Time;Time (ns)", 550, 0, 550, NST_sectors, 0, NST_sectors);
        st_adc_pedestal_chan = new TH2I("st_adc_pedestal", "ST fADC250 Summed Pedestal;Pedestal Sum (fADC counts)", 164, 0, 8200, NST_sectors, 0, NST_sectors);
        st_adc_quality_chan = new TH2I("st_adc_quality", "ST fADC250 Quality Factor;Quality Factor", 128, 0, 128, NST_sectors, 0, NST_sectors);
    }

	st_num_events = new TH1I("st_num_events", "Start Counter number of events", 1, 0.0, 1.0);

	//------------------------ TAGH -----------------------
    maindir->cd();
	gDirectory->mkdir("TAGH");

    tagh_adc_integral = new TH1I("tagh_adc_integral", "TAGH fADC250 Pulse Integral;Integral (fADC counts)", 1000, 0, 40000);
    tagh_adc_integral_pedsub = new TH1I("tagh_adc_integral_pedsub", "TAGH fADC250 Pulse Integral (Pedestal Subtracted);Integral (fADC counts)", 1000, 0, 40000);
    tagh_adc_peak = new TH1I("tagh_adc_peak", "TAGH fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000);
    tagh_adc_peak_pedsub = new TH1I("tagh_adc_peak_pedsub", "TAGH fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000);
    tagh_adc_time = new TH1I("tagh_adc_time", "TAGH fADC250 Pulse Time;Time (ns)", 550, 0, 550);
    tagh_adc_pedestal = new TH1I("tagh_adc_pedestal", "TAGH fADC250 Summed Pedestal;Pedestal Sum (fADC counts)", 164, 0, 8200);
    tagh_adc_quality = new TH1I("tagh_adc_quality", "TAGH fADC250 Quality Factor;Quality Factor", 128, 0, 128);

    if(INDIVIDUAL_CHANNEL_DATA) {
        const int NTAGH_slots = DTAGHGeometry::kCounterCount;

        tagh_adc_integral_chan = new TH2I("tagh_adc_integral", "TAGH fADC250 Pulse Integral;Integral (fADC counts)", 1000, 0, 40000, NTAGH_slots, 0, NTAGH_slots);
        tagh_adc_integral_pedsub_chan = new TH2I("tagh_adc_integral_pedsub", "TAGH fADC250 Pulse Integral (Pedestal Subtracted);Integral (fADC counts)", 1000, 0, 40000, NTAGH_slots, 0, NTAGH_slots);
        tagh_adc_peak_chan = new TH2I("tagh_adc_peak", "TAGH fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000, NTAGH_slots, 0, NTAGH_slots);
        tagh_adc_peak_pedsub_chan = new TH2I("tagh_adc_peak_pedsub", "TAGH fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000, NTAGH_slots, 0, NTAGH_slots);
        tagh_adc_time_chan = new TH2I("tagh_adc_time", "TAGH fADC250 Pulse Time;Time (ns)", 550, 0, 550, NTAGH_slots, 0, NTAGH_slots);
        tagh_adc_pedestal_chan = new TH2I("tagh_adc_pedestal", "TAGH fADC250 Summed Pedestal;Pedestal Sum (fADC counts)", 164, 0, 8200, NTAGH_slots, 0, NTAGH_slots);
        tagh_adc_quality_chan = new TH2I("tagh_adc_quality", "TAGH fADC250 Quality Factor;Quality Factor", 128, 0, 128, NTAGH_slots, 0, NTAGH_slots);
    }

	tag_num_events = new TH1I("tag_num_events", "TAGGER number of events", 1, 0.0, 1.0);

	//------------------------ TAGM -----------------------
    maindir->cd();
	gDirectory->mkdir("TAGM");

	const uint32_t NCOLUMNS = 102;
    tagm_adc_integral = new TH1I("tagm_adc_integral", "TAGM fADC250 Pulse Integral;Integral (fADC counts)", 1000, 0, 40000);
    tagm_adc_integral_pedsub = new TH1I("tagm_adc_integral_pedsub", "TAGM fADC250 Pulse Integral (Pedestal Subtracted);Integral (fADC counts)", 1000, 0, 40000);
    tagm_adc_peak = new TH1I("tagm_adc_peak", "TAGM fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000);
    tagm_adc_peak_pedsub = new TH1I("tagm_adc_peak_pedsub", "TAGM fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000);
    tagm_adc_time = new TH1I("tagm_adc_time", "TAGM fADC250 Pulse Time;Time (ns)", 550, 0, 550);
    tagm_adc_pedestal = new TH1I("tagm_adc_pedestal", "TAGM fADC250 Summed Pedestal;Pedestal Sum (fADC counts)", 164, 0, 8200);
    tagm_adc_quality = new TH1I("tagm_adc_quality", "TAGM fADC250 Quality Factor;Quality Factor", 128, 0, 128);

    if(INDIVIDUAL_CHANNEL_DATA) {
        const int NTAGM_rows = DTAGMGeometry::kRowCount;

        tagm_adc_integral_chan = new TH2I("tagm_adc_integral", "TAGM fADC250 Pulse Integral;Integral (fADC counts)", 1000, 0, 40000, NTAGM_rows, 0, NTAGM_rows);
        tagm_adc_integral_pedsub_chan = new TH2I("tagm_adc_integral_pedsub", "TAGM fADC250 Pulse Integral (Pedestal Subtracted);Integral (fADC counts)", 1000, 0, 40000, NTAGM_rows, 0, NTAGM_rows);
        tagm_adc_peak_chan = new TH2I("tagm_adc_peak", "TAGM fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000, NTAGM_rows, 0, NTAGM_rows);
        tagm_adc_peak_pedsub_chan = new TH2I("tagm_adc_peak_pedsub", "TAGM fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000, NTAGM_rows, 0, NTAGM_rows);
        tagm_adc_time_chan = new TH2I("tagm_adc_time", "TAGM fADC250 Pulse Time;Time (ns)", 550, 0, 550, NTAGM_rows, 0, NTAGM_rows);
        tagm_adc_pedestal_chan = new TH2I("tagm_adc_pedestal", "TAGM fADC250 Summed Pedestal;Pedestal Sum (fADC counts)", 164, 0, 8200, NTAGM_rows, 0, NTAGM_rows);
        tagm_adc_quality_chan = new TH2I("tagm_adc_quality", "TAGM fADC250 Quality Factor;Quality Factor", 128, 0, 128, NTAGM_rows, 0, NTAGM_rows);
    }

	//------------------------ TOF ------------------------
    maindir->cd();
	gDirectory->mkdir("TOF");

    tof_adc_integral = new TH1I("tof_adc_integral", "TOF fADC250 Pulse Integral;Integral (fADC counts)", 1000, 0, 40000);
    tof_adc_integral_pedsub = new TH1I("tof_adc_integral_pedsub", "TOF fADC250 Pulse Integral (Pedestal Subtracted);Integral (fADC counts)", 1000, 0, 40000);
    tof_adc_peak = new TH1I("tof_adc_peak", "TOF fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000);
    tof_adc_peak_pedsub = new TH1I("tof_adc_peak_pedsub", "TOF fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000);
    tof_adc_time = new TH1I("tof_adc_time", "TOF fADC250 Pulse Time;Time (ns)", 550, 0, 550);
    tof_adc_pedestal = new TH1I("tof_adc_pedestal", "TOF fADC250 Summed Pedestal;Pedestal Sum (fADC counts)", 164, 0, 8200);
    tof_adc_quality = new TH1I("tof_adc_quality", "TOF fADC250 Quality Factor;Quality Factor", 128, 0, 128);

    if(INDIVIDUAL_CHANNEL_DATA) {
        const int NTOF_channels = 176;

        tof_adc_integral_chan = new TH2I("tof_adc_integral", "TOF fADC250 Pulse Integral;Integral (fADC counts)", 1000, 0, 40000, NTOF_channels, 0, NTOF_channels);
        tof_adc_integral_pedsub_chan = new TH2I("tof_adc_integral_pedsub", "TOF fADC250 Pulse Integral (Pedestal Subtracted);Integral (fADC counts)", 1000, 0, 40000, NTOF_channels, 0, NTOF_channels);
        tof_adc_peak_chan = new TH2I("tof_adc_peak", "TOF fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000, NTOF_channels, 0, NTOF_channels);
        tof_adc_peak_pedsub_chan = new TH2I("tof_adc_peak_pedsub", "TOF fADC250 Pulse Peak;Peak (fADC counts)", 500, 0, 1000, NTOF_channels, 0, NTOF_channels);
        tof_adc_time_chan = new TH2I("tof_adc_time", "TOF fADC250 Pulse Time;Time (ns)", 550, 0, 550, NTOF_channels, 0, NTOF_channels);
        tof_adc_pedestal_chan = new TH2I("tof_adc_pedestal", "TOF fADC250 Summed Pedestal;Pedestal Sum (fADC counts)", 164, 0, 8200, NTOF_channels, 0, NTOF_channels);
        tof_adc_quality_chan = new TH2I("tof_adc_quality", "TOF fADC250 Quality Factor;Quality Factor", 128, 0, 128, NTOF_channels, 0, NTOF_channels);
    }

	tof_num_events = new TH1I("tof_num_events", "TOF number of events", 1, 0.0, 1.0);


	// back to base dir
	base->cd();
  
	return NOERROR;
}

//------------------
// brun
//------------------
jerror_t JEventProcessor_lowlevel_online::brun(JEventLoop *eventLoop, int32_t runnumber)
{
  // This is called whenever the run number changes


  return NOERROR;
}

//------------------
// evnt
//------------------
jerror_t JEventProcessor_lowlevel_online::evnt(JEventLoop *loop, uint64_t eventnumber)
{
	vector<const DBCALDigiHit*>        bcaldigihits;
	vector<const DBCALTDCDigiHit*>     bcaltdcdigihits;
	vector<const DCDCDigiHit*>         cdcdigihits;
	vector<const DFDCCathodeDigiHit*>  fdccathodehits;
	vector<const DFDCWireDigiHit*>     fdcwirehits;
	vector<const DFCALDigiHit*>        fcaldigihits;
	vector<const DPSCDigiHit*>         pscdigihits;
	vector<const DPSCTDCDigiHit*>      psctdcdigihits;
	vector<const DPSDigiHit*>          psdigihits;
	vector<const DTOFDigiHit*>         tofdigihits;
	vector<const DTOFTDCDigiHit*>      toftdcdigihits;
	vector<const DSCDigiHit*>          scdigihits;
	vector<const DRFTDCDigiTime*>      rfdigihits;
	vector<const DSCTDCDigiHit*>       sctdcdigihits;
	vector<const DTAGMDigiHit*>        tagmdigihits;
	vector<const DTAGMTDCDigiHit*>     tagmtdcdigihits;
	vector<const DTAGHDigiHit*>        taghdigihits;
	vector<const DTAGHTDCDigiHit*>     taghtdcdigihits;
	vector<const DTPOLSectorDigiHit*>  tpoldigihits;
	loop->Get(bcaldigihits);
	loop->Get(bcaltdcdigihits);
	loop->Get(cdcdigihits);
	loop->Get(fdccathodehits);
	loop->Get(fdcwirehits);
	loop->Get(fcaldigihits);
	loop->Get(pscdigihits);
	loop->Get(psctdcdigihits);
	loop->Get(psdigihits);
	loop->Get(tofdigihits);
	loop->Get(toftdcdigihits);
	loop->Get(scdigihits);
	loop->Get(rfdigihits);
	loop->Get(sctdcdigihits);
	loop->Get(tagmdigihits);
	loop->Get(tagmtdcdigihits);
	loop->Get(taghdigihits);
	loop->Get(taghtdcdigihits);
	loop->Get(tpoldigihits);

    vector< const DFCALGeometry* > geomVec;
    loop->Get( geomVec );
    const DFCALGeometry& fcalGeom = *(geomVec[0]);

    // For oow, just histogram fADC data and don't worry about TDCs
    // The only extra information that TDCs have are time information
    // Maybe we can fold that into calibrated hit monitoring
	japp->RootFillLock(this); //ACQUIRE ROOT FILL LOCK

	//------------------------ BCAL -----------------------
	bcal_num_events->Fill(0.5);
	// fADC250
	for(unsigned int i = 0; i < bcaldigihits.size(); i++){
		const DBCALDigiHit *hit = bcaldigihits[i];

        bcal_adc_integral->Fill(hit->pulse_integral);
        bcal_adc_integral_pedsub->Fill(hit->pulse_integral - hit->pedestal*(hit->nsamples_integral/hit->nsamples_pedestal));
        bcal_adc_peak->Fill(hit->pulse_peak);
        bcal_adc_peak_pedsub->Fill(hit->pulse_peak - hit->pedestal/hit->nsamples_pedestal);
        bcal_adc_time->Fill(hit->pulse_time);
        bcal_adc_pedestal->Fill(hit->pedestal);
        bcal_adc_quality->Fill(hit->QF);

        if(INDIVIDUAL_CHANNEL_DATA) {
            int ichan = 48*(hit->module-1) - 4*(hit->sector-1) + hit->layer;
            
            // plot all of the downstream channels, then the downstream ones
            if(hit->end == DBCALGeometry::kUpstream)
                ichan += 768;

            bcal_adc_integral_chan->Fill(hit->pulse_integral, ichan);
            bcal_adc_integral_pedsub_chan->Fill(hit->pulse_integral - hit->pedestal*(hit->nsamples_integral/hit->nsamples_pedestal), ichan);
            bcal_adc_peak_chan->Fill(hit->pulse_peak, ichan);
            bcal_adc_peak_pedsub_chan->Fill(hit->pulse_peak - hit->pedestal/hit->nsamples_pedestal, ichan);
            bcal_adc_time_chan->Fill(hit->pulse_time, ichan);
            bcal_adc_pedestal_chan->Fill(hit->pedestal, ichan);
            bcal_adc_quality_chan->Fill(hit->QF, ichan);
        }
	}

	// F1TDC
	//for(unsigned int i = 0; i < bcaltdcdigihits.size(); i++){
	//	const DBCALTDCDigiHit *hit = bcaltdcdigihits[i];
    //}

	//------------------------ CDC ------------------------
	cdc_num_events->Fill(0.5);
	for(uint32_t i=0; i<cdcdigihits.size(); i++) {
		const DCDCDigiHit *hit = cdcdigihits[i];  

        cdc_adc_integral->Fill(hit->pulse_integral);
        cdc_adc_integral_pedsub->Fill(hit->pulse_integral - hit->pedestal*(hit->nsamples_integral/hit->nsamples_pedestal));
        //cdc_adc_peak->Fill(hit->pulse_peak);
        //cdc_adc_peak_pedsub->Fill(hit->pulse_peak - hit->pedestal/hit->nsamples_pedestal);
        cdc_adc_time->Fill(hit->pulse_time);
        cdc_adc_pedestal->Fill(hit->pedestal);
        cdc_adc_quality->Fill(hit->QF);

        if(INDIVIDUAL_CHANNEL_DATA) {
            int ichan = Nstraws_integrated[hit->ring] + hit->straw;
            
            cdc_adc_integral_chan->Fill(hit->pulse_integral, ichan);
            cdc_adc_integral_pedsub_chan->Fill(hit->pulse_integral - hit->pedestal*(hit->nsamples_integral/hit->nsamples_pedestal), ichan);
            //cdc_adc_peak_chan->Fill(hit->pulse_peak, ichan);
            //cdc_adc_peak_pedsub_chan->Fill(hit->pulse_peak - hit->pedestal/hit->nsamples_pedestal, ichan);
            cdc_adc_time_chan->Fill(hit->pulse_time, ichan);
            cdc_adc_pedestal_chan->Fill(hit->pedestal, ichan);
            cdc_adc_quality_chan->Fill(hit->QF, ichan);
        }
	}

	//------------------------ FCAL -----------------------
	fcal_num_events->Fill(0.5);
	for(size_t loc_i = 0; loc_i < fcaldigihits.size(); ++loc_i){
		const DFCALDigiHit *hit = fcaldigihits[loc_i];

        fcal_adc_integral->Fill(hit->pulse_integral);
        fcal_adc_integral_pedsub->Fill(hit->pulse_integral - hit->pedestal*(hit->nsamples_integral/hit->nsamples_pedestal));
        fcal_adc_peak->Fill(hit->pulse_peak);
        fcal_adc_peak_pedsub->Fill(hit->pulse_peak - hit->pedestal/hit->nsamples_pedestal);
        fcal_adc_time->Fill(hit->pulse_time);
        fcal_adc_pedestal->Fill(hit->pedestal);
        fcal_adc_quality->Fill(hit->QF);

        if(INDIVIDUAL_CHANNEL_DATA) {
            int ichan = fcalGeom.channel( hit->row, hit->column );
            
            fcal_adc_integral_chan->Fill(hit->pulse_integral, ichan);
            fcal_adc_integral_pedsub_chan->Fill(hit->pulse_integral - hit->pedestal*(hit->nsamples_integral/hit->nsamples_pedestal), ichan);
            fcal_adc_peak_chan->Fill(hit->pulse_peak, ichan);
            fcal_adc_peak_pedsub_chan->Fill(hit->pulse_peak - hit->pedestal/hit->nsamples_pedestal, ichan);
            fcal_adc_time_chan->Fill(hit->pulse_time, ichan);
            fcal_adc_pedestal_chan->Fill(hit->pedestal, ichan);
            fcal_adc_quality_chan->Fill(hit->QF, ichan);
        }
	}
	
	//------------------------ FDC ------------------------
	fdc_num_events->Fill(0.5);
	for(unsigned int i = 0; i < fdccathodehits.size(); i++){
        const DFDCCathodeDigiHit *hit = fdccathodehits[i];

        fdc_adc_integral->Fill(hit->pulse_integral);
        fdc_adc_integral_pedsub->Fill(hit->pulse_integral - hit->pedestal*(hit->nsamples_integral/hit->nsamples_pedestal));
        //fdc_adc_peak->Fill(hit->pulse_peak);
        //fdc_adc_peak_pedsub->Fill(hit->pulse_peak - hit->pedestal/hit->nsamples_pedestal);
        fdc_adc_time->Fill(hit->pulse_time);
        fdc_adc_pedestal->Fill(hit->pedestal);
        fdc_adc_quality->Fill(hit->QF);

        // fix this
        if(INDIVIDUAL_CHANNEL_DATA) {
            int ud = -1;
            if (hit->view == 3) ud = 0;
            int ichan = (hit->strip - 1) + 192*(2.*hit->chamber+ud) + 192*6*4*(hit->package-1);

            fdc_adc_integral_chan->Fill(hit->pulse_integral, ichan);
            fdc_adc_integral_pedsub_chan->Fill(hit->pulse_integral - hit->pedestal*(hit->nsamples_integral/hit->nsamples_pedestal), ichan);
            //fdc_adc_peak_chan->Fill(hit->pulse_peak, ichan);
            //fdc_adc_peak_pedsub_chan->Fill(hit->pulse_peak - hit->pedestal/hit->nsamples_pedestal, ichan);
            fdc_adc_time_chan->Fill(hit->pulse_time, ichan);
            fdc_adc_pedestal_chan->Fill(hit->pedestal, ichan);
            fdc_adc_quality_chan->Fill(hit->QF, ichan);
        }
	}
	//for(unsigned int i = 0; i < fdcwirehits.size(); i++){
	//	fdc_wire_occ->Fill((fdcwirehits[i]->package - 1)*6 + fdcwirehits[i]->chamber, fdcwirehits[i]->wire);
	//}

	//------------------------ PS/PSC ---------------------
	ps_num_events->Fill(0.5);
	for(unsigned int i=0; i < pscdigihits.size(); i++) {
        //const int Nmods = 8; 
		const DPSCDigiHit *hit = pscdigihits[i];

        psc_adc_integral->Fill(hit->pulse_integral);
        psc_adc_integral_pedsub->Fill(hit->pulse_integral - hit->pedestal*(hit->nsamples_integral/hit->nsamples_pedestal));
        psc_adc_peak->Fill(hit->pulse_peak);
        psc_adc_peak_pedsub->Fill(hit->pulse_peak - hit->pedestal/hit->nsamples_pedestal);
        psc_adc_time->Fill(hit->pulse_time);
        psc_adc_pedestal->Fill(hit->pedestal);
        psc_adc_quality->Fill(hit->QF);

        if(INDIVIDUAL_CHANNEL_DATA) {
            int ichan = hit->counter_id-1;

            psc_adc_integral_chan->Fill(hit->pulse_integral, ichan);
            psc_adc_integral_pedsub_chan->Fill(hit->pulse_integral - hit->pedestal*(hit->nsamples_integral/hit->nsamples_pedestal), ichan);
            psc_adc_peak_chan->Fill(hit->pulse_peak, ichan);
            psc_adc_peak_pedsub_chan->Fill(hit->pulse_peak - hit->pedestal/hit->nsamples_pedestal, ichan);
            psc_adc_time_chan->Fill(hit->pulse_time, ichan);
            psc_adc_pedestal_chan->Fill(hit->pedestal, ichan);
            psc_adc_quality_chan->Fill(hit->QF, ichan);
        }

	}
	//for(unsigned int i=0; i < psctdcdigihits.size(); i++) {
	//	const DPSCTDCDigiHit *hit = psctdcdigihits[i];
	//	if( hit->counter_id <= Nmods )
	//		psc_tdc_left_occ->Fill(hit->counter_id);
	//	else
	//		psc_tdc_right_occ->Fill(hit->counter_id - Nmods);
	//}
	for(unsigned int i=0; i < psdigihits.size(); i++) {
		const DPSDigiHit *hit = psdigihits[i];

        ps_adc_integral->Fill(hit->pulse_integral);
        ps_adc_integral_pedsub->Fill(hit->pulse_integral - hit->pedestal*(hit->nsamples_integral/hit->nsamples_pedestal));
        ps_adc_peak->Fill(hit->pulse_peak);
        ps_adc_peak_pedsub->Fill(hit->pulse_peak - hit->pedestal/hit->nsamples_pedestal);
        ps_adc_time->Fill(hit->pulse_time);
        ps_adc_pedestal->Fill(hit->pedestal);
        ps_adc_quality->Fill(hit->QF);

        if(INDIVIDUAL_CHANNEL_DATA) {
            int ichan = hit->column-1 + 145*hit->arm;
            
            ps_adc_integral_chan->Fill(hit->pulse_integral, ichan);
            ps_adc_integral_pedsub_chan->Fill(hit->pulse_integral - hit->pedestal*(hit->nsamples_integral/hit->nsamples_pedestal), ichan);
            ps_adc_peak_chan->Fill(hit->pulse_peak, ichan);
            ps_adc_peak_pedsub_chan->Fill(hit->pulse_peak - hit->pedestal/hit->nsamples_pedestal, ichan);
            ps_adc_time_chan->Fill(hit->pulse_time, ichan);
            ps_adc_pedestal_chan->Fill(hit->pedestal, ichan);
            ps_adc_quality_chan->Fill(hit->QF, ichan);
        }
	}

	//------------------------ RF -------------------------
	//rf_num_events->Fill(0.5);
	//for(size_t loc_i = 0; loc_i < rfdigihits.size(); ++loc_i){
    //		DetectorSystem_t locSystem = rfdigihits[loc_i]->dSystem;
    //		rf_occ->Fill(dRFBinValueMap[locSystem]);
	//}

	//------------------------ ST -------------------------
	st_num_events->Fill(0.5);
	for(uint32_t i = 0; i < scdigihits.size();    i++) { 
		const DSCDigiHit *hit = scdigihits[i];

        st_adc_integral->Fill(hit->pulse_integral);
        st_adc_integral_pedsub->Fill(hit->pulse_integral - hit->pedestal*(hit->nsamples_integral/hit->nsamples_pedestal));
        st_adc_peak->Fill(hit->pulse_peak);
        st_adc_peak_pedsub->Fill(hit->pulse_peak - hit->pedestal/hit->nsamples_pedestal);
        st_adc_time->Fill(hit->pulse_time);
        st_adc_pedestal->Fill(hit->pedestal);
        st_adc_quality->Fill(hit->QF);

        if(INDIVIDUAL_CHANNEL_DATA) {
            int ichan = hit->sector-1;
            
            st_adc_integral_chan->Fill(hit->pulse_integral, ichan);
            st_adc_integral_pedsub_chan->Fill(hit->pulse_integral - hit->pedestal*(hit->nsamples_integral/hit->nsamples_pedestal), ichan);
            st_adc_peak_chan->Fill(hit->pulse_peak, ichan);
            st_adc_peak_pedsub_chan->Fill(hit->pulse_peak - hit->pedestal/hit->nsamples_pedestal, ichan);
            st_adc_time_chan->Fill(hit->pulse_time, ichan);
            st_adc_pedestal_chan->Fill(hit->pedestal, ichan);
            st_adc_quality_chan->Fill(hit->QF, ichan);
        }
	}
	//for(uint32_t i = 0; i < sctdcdigihits.size(); i++) st_tdc_occ->Fill(sctdcdigihits[i]->sector);

	//------------------------ TAGH -----------------------
 	tag_num_events->Fill(0.5);
    for(unsigned int i=0; i < taghdigihits.size();    i++) {
		const DTAGHDigiHit *hit = taghdigihits[i];

        tagh_adc_integral->Fill(hit->pulse_integral);
        tagh_adc_integral_pedsub->Fill(hit->pulse_integral - hit->pedestal*(hit->nsamples_integral/hit->nsamples_pedestal));
        tagh_adc_peak->Fill(hit->pulse_peak);
        tagh_adc_peak_pedsub->Fill(hit->pulse_peak - hit->pedestal/hit->nsamples_pedestal);
        tagh_adc_time->Fill(hit->pulse_time);
        tagh_adc_pedestal->Fill(hit->pedestal);
        tagh_adc_quality->Fill(hit->QF);

        if(INDIVIDUAL_CHANNEL_DATA) {
            int ichan = hit->counter_id-1;
            
            tagh_adc_integral_chan->Fill(hit->pulse_integral, ichan);
            tagh_adc_integral_pedsub_chan->Fill(hit->pulse_integral - hit->pedestal*(hit->nsamples_integral/hit->nsamples_pedestal), ichan);
            tagh_adc_peak_chan->Fill(hit->pulse_peak, ichan);
            tagh_adc_peak_pedsub_chan->Fill(hit->pulse_peak - hit->pedestal/hit->nsamples_pedestal, ichan);
            tagh_adc_time_chan->Fill(hit->pulse_time, ichan);
            tagh_adc_pedestal_chan->Fill(hit->pedestal, ichan);
            tagh_adc_quality_chan->Fill(hit->QF, ichan);
        }
	}
    //for(unsigned int i=0; i < taghtdcdigihits.size(); i++) tagh_tdc_occ->Fill(taghtdcdigihits[i]->counter_id);

	//------------------------ TAGM -----------------------
	for(uint32_t i=0; i< tagmdigihits.size(); i++) {
		const DTAGMDigiHit *hit = tagmdigihits[i];
		if (hit->row != 0) continue;   // ignore individually read out columns

        tagm_adc_integral->Fill(hit->pulse_integral);
        tagm_adc_integral_pedsub->Fill(hit->pulse_integral - hit->pedestal*(hit->nsamples_integral/hit->nsamples_pedestal));
        tagm_adc_peak->Fill(hit->pulse_peak);
        tagm_adc_peak_pedsub->Fill(hit->pulse_peak - hit->pedestal/hit->nsamples_pedestal);
        tagm_adc_time->Fill(hit->pulse_time);
        tagm_adc_pedestal->Fill(hit->pedestal);
        tagm_adc_quality->Fill(hit->QF);

        if(INDIVIDUAL_CHANNEL_DATA) {
            int ichan = hit->column - 1;
            
            tagm_adc_integral_chan->Fill(hit->pulse_integral, ichan);
            tagm_adc_integral_pedsub_chan->Fill(hit->pulse_integral - hit->pedestal*(hit->nsamples_integral/hit->nsamples_pedestal), ichan);
            tagm_adc_peak_chan->Fill(hit->pulse_peak, ichan);
            tagm_adc_peak_pedsub_chan->Fill(hit->pulse_peak - hit->pedestal/hit->nsamples_pedestal, ichan);
            tagm_adc_time_chan->Fill(hit->pulse_time, ichan);
            tagm_adc_pedestal_chan->Fill(hit->pedestal, ichan);
            tagm_adc_quality_chan->Fill(hit->QF, ichan);
        }
	}

	//for(uint32_t i=0; i< tagmtdcdigihits.size(); i++) {
	//	const DTAGMTDCDigiHit *hit = tagmtdcdigihits[i];
	//	if (hit->row == 0) tagm_tdc_occ->Fill(hit->column);
	//}

	//------------------------ TPOL -----------------------
	//for(unsigned int i=0; i < tpoldigihits.size(); i++) tpol_occ->Fill(tpoldigihits[i]->sector);
    // not sure what to put here yet

	//------------------------ TOF ------------------------
	tof_num_events->Fill(0.5);
	// fADC Hits
	for(uint32_t i=0; i<tofdigihits.size(); i++) {
        const DTOFDigiHit *hit = tofdigihits[i];

        tof_adc_integral->Fill(hit->pulse_integral);
        tof_adc_integral_pedsub->Fill(hit->pulse_integral - hit->pedestal*(hit->nsamples_integral/hit->nsamples_pedestal));
        tof_adc_peak->Fill(hit->pulse_peak);
        tof_adc_peak_pedsub->Fill(hit->pulse_peak - hit->pedestal/hit->nsamples_pedestal);
        tof_adc_time->Fill(hit->pulse_time);
        tof_adc_pedestal->Fill(hit->pedestal);
        tof_adc_quality->Fill(hit->QF);

        if(INDIVIDUAL_CHANNEL_DATA) {
            int ichan = 48*2*hit->plane + 2.*(hit->bar-1) + hit->end; 
            
            tof_adc_integral_chan->Fill(hit->pulse_integral, ichan);
            tof_adc_integral_pedsub_chan->Fill(hit->pulse_integral - hit->pedestal*(hit->nsamples_integral/hit->nsamples_pedestal), ichan);
            tof_adc_peak_chan->Fill(hit->pulse_peak, ichan);
            tof_adc_peak_pedsub_chan->Fill(hit->pulse_peak - hit->pedestal/hit->nsamples_pedestal, ichan);
            tof_adc_time_chan->Fill(hit->pulse_time, ichan);
            tof_adc_pedestal_chan->Fill(hit->pedestal, ichan);
            tof_adc_quality_chan->Fill(hit->QF, ichan);
        }

    }

	// TDC Hits
	//for(uint32_t i=0; i<toftdcdigihits.size(); i++){
    //const DTOFTDCDigiHit *hit = toftdcdigihits[i];
    //}

	japp->RootFillUnLock(this); //RELEASE ROOT FILL LOCK


	return NOERROR;
}

//------------------
// erun
//------------------
jerror_t JEventProcessor_lowlevel_online::erun(void)
{
  // This is called whenever the run number changes, before it is
  // changed to give you a chance to clean up before processing
  // events from the next run number.
  return NOERROR;
}

//------------------
// fini
//------------------
jerror_t JEventProcessor_lowlevel_online::fini(void)
{
  // Called before program exit after event processing is finished.
  return NOERROR;
}


//  LocalWords:  FCAL