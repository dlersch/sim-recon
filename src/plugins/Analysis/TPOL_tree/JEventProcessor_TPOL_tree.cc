// $Id$
//
//    File: JEventProcessor_TPOL_tree.cc
// Created: Thu Feb  4 16:11:54 EST 2016
// Creator: nsparks (on Linux cua2.jlab.org 3.10.0-327.4.4.el7.x86_64 x86_64)
//
#include <iostream>
#include <cmath>
#include <stdint.h>
#include "JEventProcessor_TPOL_tree.h"
using namespace jana;
using namespace std;

#include <TRIGGER/DL1Trigger.h>
#include <TPOL/DTPOLHit_factory.h>
#include <PAIR_SPECTROMETER/DPSCPair.h>
#include <PAIR_SPECTROMETER/DPSPair.h>
#include <TAGGER/DTAGHHit.h>
#include <TAGGER/DTAGMHit.h>

const int NSECTORS = DTPOLHit_factory::NSECTORS;
const double SECTOR_DIVISION = DTPOLHit_factory::SECTOR_DIVISION;

// Routine used to create our JEventProcessor
#include <JANA/JApplication.h>
#include <JANA/JFactory.h>
#include "RCDB/Connection.h"
#include "RCDB/ConfigParser.h"
extern "C"{
    void InitPlugin(JApplication *app){
        InitJANAPlugin(app);
        app->AddProcessor(new JEventProcessor_TPOL_tree());
    }
} // "C"

//------------------
// JEventProcessor_TPOL_tree (Constructor)
//------------------
JEventProcessor_TPOL_tree::JEventProcessor_TPOL_tree()
{

}

//------------------
// ~JEventProcessor_TPOL_tree (Destructor)
//------------------
JEventProcessor_TPOL_tree::~JEventProcessor_TPOL_tree()
{

}

//------------------
// init
//------------------
jerror_t JEventProcessor_TPOL_tree::init(void)
{
    // This is called once at program startup. If you are creating
    // and filling historgrams in this plugin, you should lock the
    // ROOT mutex like this:
    //
    bool SAVE_WAVEFORMS = true;
    gPARMS->SetDefaultParameter("TPOL_tree:SAVE_WAVEFORMS",SAVE_WAVEFORMS);

    TPOL = new TTree("TPOL_tree","TPOL tree");
    TPOL->Branch("nadc",&nadc,"nadc/i");
    TPOL->Branch("eventnum",&eventnum,"eventnum/i");
    TPOL->Branch("rocid",rocid,"rocid[nadc]/i");
    TPOL->Branch("slot",slot,"slot[nadc]/i");
    TPOL->Branch("channel",channel,"channel[nadc]/i");
    TPOL->Branch("itrigger",itrigger,"itrigger[nadc]/i");
    if (SAVE_WAVEFORMS) {
        TPOL->Branch("nsamples",&nsamples,"nsamples/i");
        TPOL->Branch("waveform",waveform,"waveform[nadc][150]/i");
    }
    TPOL->Branch("w_integral",w_integral,"w_integral[nadc]/i");
    TPOL->Branch("w_min",w_min,"w_min[nadc]/i");
    TPOL->Branch("w_max",w_max,"w_max[nadc]/i");
    TPOL->Branch("w_samp1",w_samp1,"w_samp1[nadc]/i");
    TPOL->Branch("w_time",w_time,"w_time[nadc]/D");
    TPOL->Branch("sector",sector,"sector[nadc]/i");
    TPOL->Branch("phi",phi,"phi[nadc]/D");
    TPOL->Branch("E_lhit",&E_lhit,"E_lhit/D");
    TPOL->Branch("E_rhit",&E_rhit,"E_rhit/D");
    TPOL->Branch("t_lhit",&t_lhit,"t_lhit/D");
    TPOL->Branch("t_rhit",&t_rhit,"t_rhit/D");
    TPOL->Branch("ntag",&ntag,"ntag/i");
    TPOL->Branch("E_tag",E_tag,"E_tag[ntag]/D");
    TPOL->Branch("t_tag",t_tag,"t_tag[ntag]/D");
    TPOL->Branch("is_tagm",is_tagm,"is_tagm[ntag]/O");
    eventnum = 0;
    //
    return NOERROR;
}

//------------------
// brun
//------------------
jerror_t JEventProcessor_TPOL_tree::brun(JEventLoop *eventLoop, int32_t runnumber)
{
    rcdb::Connection connection("mysql://rcdb@hallddb.jlab.org/rcdb");
    auto rtvsCondition = connection.GetCondition(runnumber, "rtvs");
    auto json = rtvsCondition->ToJsonDocument();
    string fileName(json["%(config)"].GetString());
    auto file = connection.GetFile(runnumber, fileName);
    if(!file) {
        jerr<<"No trigger configuration file exists for this run number"<<endl;
    }
    string fileContent = file->GetContent();
    vector<string> SectionNames = {"TPOL"};
    auto result = rcdb::ConfigParser::Parse(fileContent, SectionNames);
    
    string readout_thresholdStr = "";	
    for (const auto &p : result.Sections["TPOL"].NameValues["FADC250_READ_THR"])
	readout_thresholdStr += p;

    readout_threshold = atof(readout_thresholdStr.c_str());

    // This is called whenever the run number changes
    return NOERROR;
}

//------------------
// evnt
//------------------
jerror_t JEventProcessor_TPOL_tree::evnt(JEventLoop *loop, uint64_t eventnumber)
{
    // This is called for every event. Use of common resources like writing
    // to a file or filling a histogram should be mutex protected. Using
    // loop->Get(...) to get reconstructed objects (and thereby activating the
    // reconstruction algorithm) should be done outside of any mutex lock
    // since multiple threads may call this method at the same time.
    //
    const DL1Trigger *trig_words = NULL;
    uint32_t trig_mask, fp_trig_mask;
    try {
        loop->GetSingle(trig_words);
    } catch(...) {};
    if (trig_words) {
        trig_mask = trig_words->trig_mask;
        fp_trig_mask = trig_words->fp_trig_mask;
    }
    else {
        trig_mask = 0;
        fp_trig_mask = 0;
    }
    int trig_bits = fp_trig_mask > 0 ? 10 + fp_trig_mask:trig_mask;
    // skim PS triggers
    if (trig_bits!=8) {
        return NOERROR;
    }
    //
    vector<const Df250WindowRawData*> windowraws;
    loop->Get(windowraws);
    // coarse PS pairs
    vector<const DPSCPair*> cpairs;
    loop->Get(cpairs);
    // fine PS pairs
    vector<const DPSPair*> fpairs;
    loop->Get(fpairs);
    // tagger hits
    vector<const DTAGHHit*> taghhits;
    loop->Get(taghhits);
    vector<const DTAGMHit*> tagmhits;
    loop->Get(tagmhits);

    japp->RootWriteLock();
    // PSC coincidences
    if (cpairs.size()>=1) {
        // take pair with smallest time difference from sorted vector
        const DPSCHit* clhit = cpairs[0]->ee.first; // left hit in coarse PS
        const DPSCHit* crhit = cpairs[0]->ee.second;// right hit in coarse PS
        // PSC,PS coincidences
        if (fpairs.size()>=1) {
            eventnum++;
            // take pair with smallest time difference from sorted vector
            const DPSPair::PSClust* flhit = fpairs[0]->ee.first;  // left hit in fine PS
            const DPSPair::PSClust* frhit = fpairs[0]->ee.second; // right hit in fine PS
            E_lhit = flhit->E; E_rhit = frhit->E;
            t_lhit = clhit->t; t_rhit = crhit->t;
            double E_pair = flhit->E+frhit->E;
            // PSC,PS,TAGX coincidences
            unsigned int htag = 0;
            double EdiffMax = 0.3; double tdiffMax = 15.0;
            for (unsigned int i=0; i < taghhits.size(); i++) {
                const DTAGHHit* tag = taghhits[i];
                if (!tag->has_TDC||!tag->has_fADC) continue;
                if (fabs(E_pair-tag->E) < EdiffMax && fabs(t_lhit-tag->t) < tdiffMax && htag < ntag_max) {
                    E_tag[htag] = tag->E;
                    t_tag[htag] = tag->t;
                    is_tagm[htag] = false;
                    htag++;
                }
            }
            for (unsigned int i=0; i < tagmhits.size(); i++) {
                const DTAGMHit* tag = tagmhits[i];
                if (!tag->has_TDC||!tag->has_fADC) continue;
                if (tag->row!=0) continue;
                if (fabs(E_pair-tag->E) < EdiffMax && fabs(t_lhit-tag->t) < tdiffMax && htag < ntag_max) {
                    E_tag[htag] = tag->E;
                    t_tag[htag] = tag->t;
                    is_tagm[htag] = true;
                    htag++;
                }
            }
            ntag = htag;
            if (ntag>ntag_max) jerr << "TPOL_tree plugin error: ntag exceeds ntag_max(" << ntag_max << ")." << endl;
            unsigned int hit = 0;
            for(unsigned int i=0; i< windowraws.size(); i++) {
                const Df250WindowRawData *windowraw = windowraws[i];
                if (windowraw->rocid!=84) continue;
                if (!(windowraw->slot==13||windowraw->slot==14)) continue; // azimuthal sectors, rings: 15,16
                rocid[hit] = windowraw->rocid;
                slot[hit] = windowraw->slot;
                channel[hit] = windowraw->channel;
                itrigger[hit] = windowraw->itrigger;
                // Get a vector of the samples for this channel
                const vector<uint16_t> &samplesvector = windowraw->samples;
                nsamples = samplesvector.size();
                // loop over the samples to calculate integral, min, max
                if (nsamples==0) jerr << "Raw samples vector is empty." << endl;
                if (samplesvector[0] > readout_threshold) continue; // require first sample below readout threshold
                for (uint16_t c_samp=0; c_samp<nsamples; c_samp++) {
                    waveform[hit][c_samp] = samplesvector[c_samp];
                    if (c_samp==0) {  // use first sample for initialization
                        w_integral[hit] = samplesvector[0];
                        w_min[hit] = samplesvector[0];
                        w_max[hit] = samplesvector[0];
                        w_samp1[hit] = samplesvector[0];
                    } else {
                        w_integral[hit] += samplesvector[c_samp];
                        if (w_min[hit] > samplesvector[c_samp]) w_min[hit] = samplesvector[c_samp];
                        if (w_max[hit] < samplesvector[c_samp]) w_max[hit] = samplesvector[c_samp];
                    }
                }
                w_time[hit] = 0.0625*GetPulseTime(samplesvector,w_min[hit],w_max[hit],readout_threshold-100.0);
                sector[hit] = GetSector(slot[hit],channel[hit]);
                phi[hit] = GetPhi(sector[hit]);
                if (nsamples==100) {
                    for (uint16_t c_samp=100; c_samp<150; c_samp++) waveform[hit][c_samp] = 0;
                }
                hit++;
            }
            nadc = hit;
            if (nadc>nmax) jerr << "TPOL_tree plugin error: nadc exceeds nmax(" << nmax << ")." << endl;
            // Fill tree
            TPOL->Fill();
        }
    }
    japp->RootUnLock();
    //
    return NOERROR;
}

int JEventProcessor_TPOL_tree::GetSector(int slot,int channel)
{
    int sector = 0;
    if (slot == 13) sector = 25 - channel;
    if (slot == 14) {
        if (channel <= 8) sector = 9 - channel;
        else sector = NSECTORS + 9 - channel;
    }
    // fix cable swap
    if (sector == 9) sector = 6;
    else if (sector == 6) sector = 9;
    if (sector == 0) jerr << "sector did not change from initial value (0)." << endl;
    return sector;
}
double JEventProcessor_TPOL_tree::GetPhi(int sector)
{
    double phi = -10.0;
    if(sector <= 8) phi = (sector + 23)*SECTOR_DIVISION + 0.5*SECTOR_DIVISION;
    if(sector >= 9) phi = (sector - 9)*SECTOR_DIVISION + 0.5*SECTOR_DIVISION;
    return phi;
}
double JEventProcessor_TPOL_tree::GetPulseTime(const vector<uint16_t> waveform,double w_min,double w_max,double minpeakheight)
{
    // find the time to cross half peak height
    int lastbelowsamp=0; double peakheight = w_max-w_min;
    double threshold = w_min + peakheight/2.0;
    double  firstaboveheight=0, lastbelowheight=0;
    double w_time=0;
    if (peakheight > minpeakheight) {
        for (uint16_t c_samp=0; c_samp<waveform.size(); c_samp++) {
            if (waveform[c_samp]>threshold) {
                firstaboveheight = waveform[c_samp];
                lastbelowsamp = c_samp-1;
                lastbelowheight = waveform[c_samp-1];
                break;
            }
        }
        w_time =  lastbelowsamp + (threshold-lastbelowheight)/(firstaboveheight-lastbelowheight);
    }
    return 64.0*w_time;
}
//------------------
// erun
//------------------
jerror_t JEventProcessor_TPOL_tree::erun(void)
{
    // This is called whenever the run number changes, before it is
    // changed to give you a chance to clean up before processing
    // events from the next run number.
    return NOERROR;
}

//------------------
// fini
//------------------
jerror_t JEventProcessor_TPOL_tree::fini(void)
{
    // Called before program exit after event processing is finished.
    return NOERROR;
}
