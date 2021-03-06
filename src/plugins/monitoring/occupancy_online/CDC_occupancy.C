
// The following are special comments used by RootSpy to know
// which histograms to fetch for the macro.
//
// hnamepath: /occupancy/cdc_num_events
// hnamepath: /occupancy/cdc_axes
// hnamepath: /occupancy/cdc_occ_ring_01
// hnamepath: /occupancy/cdc_occ_ring_02
// hnamepath: /occupancy/cdc_occ_ring_03
// hnamepath: /occupancy/cdc_occ_ring_04
// hnamepath: /occupancy/cdc_occ_ring_05
// hnamepath: /occupancy/cdc_occ_ring_06
// hnamepath: /occupancy/cdc_occ_ring_07
// hnamepath: /occupancy/cdc_occ_ring_08
// hnamepath: /occupancy/cdc_occ_ring_09
// hnamepath: /occupancy/cdc_occ_ring_10
// hnamepath: /occupancy/cdc_occ_ring_11
// hnamepath: /occupancy/cdc_occ_ring_12
// hnamepath: /occupancy/cdc_occ_ring_13
// hnamepath: /occupancy/cdc_occ_ring_14
// hnamepath: /occupancy/cdc_occ_ring_15
// hnamepath: /occupancy/cdc_occ_ring_16
// hnamepath: /occupancy/cdc_occ_ring_17
// hnamepath: /occupancy/cdc_occ_ring_18
// hnamepath: /occupancy/cdc_occ_ring_19
// hnamepath: /occupancy/cdc_occ_ring_20
// hnamepath: /occupancy/cdc_occ_ring_21
// hnamepath: /occupancy/cdc_occ_ring_22
// hnamepath: /occupancy/cdc_occ_ring_23
// hnamepath: /occupancy/cdc_occ_ring_24
// hnamepath: /occupancy/cdc_occ_ring_25
// hnamepath: /occupancy/cdc_occ_ring_26
// hnamepath: /occupancy/cdc_occ_ring_27
// hnamepath: /occupancy/cdc_occ_ring_28
//
// e-mail: davidl@jlab.org
// e-mail: njarvis@jlab.org
// e-mail: zihlmann@jlab.org
// e-mail: tbritton@jlab.org
//


{
	// RootSpy saves the current directory and style before
	// calling the macro and restores it after so it is OK to
	// change them and not change them back.

	TDirectory *dir = (TDirectory*)gDirectory->FindObjectAny("occupancy");
	if(dir) dir->cd();

	double Nevents = 1.0;
	TH1I *cdc_num_events = (TH1I*)gDirectory->FindObjectAny("cdc_num_events");
	if(cdc_num_events) Nevents = (double)cdc_num_events->GetBinContent(1);
 	TH2D *cdc_axes = (TH2D *)gDirectory->FindObjectAny("cdc_axes");
 	if(!cdc_axes) return;

	// Just for testing
	if(gPad == NULL){
		TCanvas *c1 = new TCanvas("c1");
		c1->cd(0);
		c1->Draw();
		c1->Update();
	}
	if(!gPad) return;

	TCanvas *c1 = gPad->GetCanvas();
	if(!c1) return;
	c1->cd(0);

	// Draw axes
	double minScale = 0.0, maxScale = 0.10;
	cdc_axes->SetStats(0);
	cdc_axes->Fill(100,100); // without this, the color ramp is not drawn
	cdc_axes->GetZaxis()->SetRangeUser(minScale, maxScale);
	cdc_axes->Draw("colz");
	
	// Draw inner and outer circles so we can see if outer ring is missing
	TEllipse *e = new TEllipse(0.0, 0.0, 56.0, 56.0);
	e->SetLineWidth(3);
	e->Draw();
	e = new TEllipse(0.0, 0.0, 9.55, 9.55);
	e->SetLineWidth(2);
	e->Draw();

	// Draw All rings
	for(unsigned int iring=1; iring<=28; iring++){
		char hname[256];
		sprintf(hname, "cdc_occ_ring_%02d", iring);
		TH1 *h = (TH1*)(dir->Get(hname));
		if(h){
			sprintf(hname, "cdc_occ_ring_norm_%02d", iring);
			TH1 *hh = (TH1*)h->Clone(hname);
			hh->Scale(1.0/Nevents);
			hh->GetZaxis()->SetRangeUser(minScale, maxScale);
			hh->SetStats(0);
			hh->Draw("same col pol");  // draw remaining histos without overwriting color palette
		}
	}
	
	char str[256];
	sprintf(str,"%g events", Nevents);
	TLatex lat;
	lat.SetTextAlign(22);
	lat.SetTextSize(0.035);
	lat.DrawLatex(0.0, 61.0, str);

}
