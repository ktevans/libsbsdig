#include "TSBSDBManager.h"
#include "TSBSSimDecoder.h"
#include <cassert>
#include <cmath>
#include "TMath.h"
#include "TVector2.h"
#include "TRandom3.h"
#include "TString.h"

TSBSDBManager * TSBSDBManager::fManager = NULL;

TSBSDBManager::TSBSDBManager() 
  : fErrID(-999), fErrVal(-999.)
{
}
//______________________________________________________________
TSBSDBManager::~TSBSDBManager()
{
}

//______________________________________________________________
Int_t TSBSDBManager::LoadGenInfo(const string& fileName)
{  
  // Load the experiment/setup general info
  /*
  ifstream input(fileName.c_str());
  if (!input.is_open()){
    cout<<"cannot find general information file "<<fileName
	<<". Exiting the program"<<endl;
        exit(0);
  }
  */
  std::string path = "";
  if(std::getenv("DB_DIR")) {
    path = std::string(std::getenv("DB_DIR")) + "/";
  }
  const string& PathfileName = path+fileName;
  
  cout << "File name " << fileName << endl;
  
  FILE* file = OpenFile( PathfileName.c_str(), GetInitDate() );
  
  const string prefix = "geninfo.";
  
  string exp_str;
  string specs_str;
  
  //first, load the experiment general info: expt type, number and names of spectrometers
  DBRequest request[] = {
    {"sbsexptype", &exp_str,   kString, 0, 0},
    {"nspecs",     &fNSpecs,   kInt,    0, 0},
    {"specnames",  &specs_str, kString, 0, 0},
    { 0 }
  };

  //int err = LoadDB( input, request,  prefix);
  int err = LoadDB(file, GetInitDate(), request,  prefix.c_str());
  
  if(fDebug>=2){
    cout << "err: " << err << endl;
    cout << "nspecs "<< fNSpecs << endl;
    cout << specs_str.c_str() << endl;
  }
  
  if( err ) exit(2); 
  
  //assing the right exp_type value to the exp_type flag according to the expt name
  if(exp_str.compare("gmn")==0)fSBSExpType = kGMn;
  if(exp_str.compare("gep")==0)fSBSExpType = kGEp;
  if(exp_str.compare("gen")==0)fSBSExpType = kGEn;
  if(exp_str.compare("sidis")==0)fSBSExpType = kSIDIS;
  if(exp_str.compare("a1n")==0)fSBSExpType = kA1n;
  if(exp_str.compare("tdis")==0)fSBSExpType = kTDIS;
  if(exp_str.compare("ndvcs")==0)fSBSExpType = kDVCS;
  
  //split the full string to extract the individual spectrometer names
  fSpecNames = vsplit(specs_str);
  
  //Then, loop on the spectrometers to gather the detector number and names, and the MC signal of interest
  for(int i_spec = 0; i_spec<fNSpecs; i_spec++){
    TSpectroInfo specinfo;
    double mcangle;
    int ndets;
    string dets_str;
    int nsig;
    
    string prefix2 = prefix+fSpecNames.at(i_spec)+".";
    
    if(fDebug>=3){
      cout << prefix2.c_str() << endl;
    }
    
    std::vector<int>* pid = 0;
    std::vector<int>* tid = 0;
    
    try{
      pid = new vector<int>;
      tid = new vector<int>;
      
      DBRequest request[] = {
	{"mcangle",        &mcangle,   kDouble,   0, 0},
	{"nsignal",        &nsig,      kInt,      0, 0},
	{"signal.pid",     pid,        kIntV,     0, 0},
	{"signal.tid",     tid,        kIntV,     0, 0},
	{"ndets",          &ndets,     kInt,      0, 0},
	{"detnames",       &dets_str,  kString,   0, 0},
	{ 0 }
      };
      
      //Int_t err = LoadDB (input, request, prefix2);
      Int_t err = LoadDB (file, GetInitDate(), request, prefix2.c_str());
      
      if (err){
	//input.close();
	fclose(file);
	return kInitError;
      }
      
      specinfo.SetMCAngle(mcangle);
      specinfo.SetNDets(ndets);
      std::vector<std::string> detnames = vsplit(dets_str);
      for(int i_str = 0; i_str<detnames.size(); i_str++){
	specinfo.AddDetName(detnames.at(i_str));
      }
      
      if(fDebug>=3){
	cout << specinfo.NDets() << endl;
      }
      
      for(int i_sig = 0; i_sig<nsig; i_sig++){
	TSignalInfo siginfo(pid->at(i_sig), tid->at(i_sig));
	specinfo.AddMCSignalInfo(siginfo);
      }
      fSpectroInfo.push_back(specinfo);
	
      delete pid;
      delete tid;
    }  catch(...) {
      delete pid;
      delete tid;
      //input.close();
      fclose(file);
      throw;
    }//end try / catch
    
    if(fDebug>=3){
      cout << specinfo.NDets() << endl;
    }
    
    // then loop on detectors
    for(int i_det = 0; i_det<specinfo.NDets(); i_det++){
      LoadDetInfo(fSpecNames.at(i_spec), specinfo.DetName(i_det));
    }
    
  }// end spectrometer loop
  //input.close();
  fclose(file);
  return(kOK);
}

//______________________________________________________________
Int_t TSBSDBManager::LoadDetInfo(const string& specname, const string& detname)
{
  TDetInfo detinfo(detname);
  // Include DB_DIR (standard Hall A analyzer DB path in the search)
  std::string path = "";
  if(std::getenv("DB_DIR")) {
    path = std::string(std::getenv("DB_DIR")) + "/";
  }
  const string& fileName = path+"db_"+specname+"."+detname+".dat";
  
  /*
  ifstream input(fileName.c_str());
  if (!input.is_open()){
    cout<<"cannot find geometry file "<<fileName
	<<". Exiting the program"<<endl;
    exit(0);
  }
  */
  
  FILE* file = OpenFile( fileName.c_str(), GetInitDate() );
  
  const string prefix = specname+"."+detname+".";

  string dettype_str;
  int nchan, chan_per_slot, slot_per_crate;

  //First load the parameters which will be common to *all* detectors (including digitization parameters)
  DBRequest request[] = {
    {"dettype",        &dettype_str,    kString,  0, 0},
    {"nchan",          &nchan,          kInt,     0, 0},
    {"chan_per_slot",  &chan_per_slot,  kInt,     0, 0},
    {"slot_per_crate", &slot_per_crate, kInt,     0, 0},
    { 0 }
  };
  
  Int_t err = LoadDB (file, GetInitDate(), request, prefix.c_str());
  
  if(dettype_str.compare("HCal")==0)detinfo.SetDetType(kHCal);
  if(dettype_str.compare("ECal")==0)detinfo.SetDetType(kECal);
  if(dettype_str.compare("Cher")==0) detinfo.SetDetType(kCher);
  if(dettype_str.compare("Scint")==0) detinfo.SetDetType(kScint);
  if(dettype_str.compare("GEM")==0) detinfo.SetDetType(kGEM);
  
  detinfo.SetNChan(nchan);
  detinfo.SetChanPerSlot(chan_per_slot);
  detinfo.SetSlotPerCrate(slot_per_crate);

  const string digprefix = "dig."+prefix;
  
  bool ignore_pmt = false;
  if(detinfo.DetType()==kGEM) ignore_pmt = true;

  bool ignore_adc = false;
  if(detinfo.DetType()==kScint || detinfo.DetType()==kCher) ignore_adc = true;
  ignore_adc = (ignore_pmt || ignore_adc);

  bool ignore_tdc = false;

  std::vector<double>* gain = 0;
  std::vector<double>* pedestal = 0;
  std::vector<double>* pednoise = 0;
  std::vector<double>* threshold = 0;
  
  try{
    gain = new vector<double>;
    pedestal = new vector<double>;
    pednoise = new vector<double>;
    threshold = new vector<double>;
    
    DBRequest request_dig[] = {
      {"roimpedance",   &detinfo.fDigInfo.fROImpedance,    kDouble, 0,  ignore_pmt},
      {"adcconversion", &detinfo.fDigInfo.fADCconversion,  kDouble, 0,  ignore_adc},
      {"adcbits",       &detinfo.fDigInfo.fADCbits,        kInt,    0,  ignore_adc},
      {"tdcconversion", &detinfo.fDigInfo.fTDCconversion,  kDouble, 0,  ignore_tdc},
      {"tdcbits",       &detinfo.fDigInfo.fTDCbits,        kInt,    0,  ignore_tdc},
      {"gain",          gain,             kDoubleV, 0, 0},
      {"pedestal",      pedestal,         kDoubleV, 0, 0},
      {"pednoise",      pednoise,         kDoubleV, 0, 0},
      {"threshold",     threshold,        kDoubleV, 0, 0},
      {"triggerjitter", &detinfo.fDigInfo.fTriggerJitter,  kDouble, 0,  0},
      {"triggeroffset", &detinfo.fDigInfo.fTriggerOffset,  kDouble,  0, 0},
      {"gatewidth",     &detinfo.fDigInfo.fGateWidth,      kDouble, 0,  0},
      {"spe_tau",       &detinfo.fDigInfo.fSPEtau,         kDouble,  0, ignore_pmt},
      {"spe_sigma",     &detinfo.fDigInfo.fSPEsig,         kDouble, 0,  ignore_pmt},
      {"spe_transit",   &detinfo.fDigInfo.fSPEtransittime, kDouble,  0, ignore_pmt},
      { 0 }
    };
    
    if(fDebug>=3){
      cout << digprefix.c_str() << endl;
    }
    
    //err = LoadDB (input, request_dig, digprefix);
    err = LoadDB (file, GetInitDate(), request_dig, digprefix.c_str());
    if (err){
      //input.close();
      fclose(file);
      return kInitError;
    }
    
    if(gain->size()!=detinfo.NChan()){
      cout << "warning: number of gains in input (" << gain->size() 
	   << ") does not match number of channels (" << detinfo.NChan()
	   << ")" << endl;
      cout << "First gain entry used for all channels. " << endl
	   << "If you want one gain value per channel, fix your DB" << endl<< endl;
      detinfo.fDigInfo.fGain.push_back(gain->at(0));
    }else{
      for(uint i__ = 0; i__<gain->size(); i__++){
	detinfo.fDigInfo.fGain.push_back(gain->at(i__));
      }
    }
    if(pedestal->size()!=detinfo.NChan()){
      cout << "warning: number of pedestals in input (" << pedestal->size() 
	   << ") does not match number of channels (" << detinfo.NChan()
	   << ")" << endl;
      cout << "First pedestal entry used for all channels. " << endl
	   << "If you want one pedestal value per channel, fix your DB" << endl<< endl;
      detinfo.fDigInfo.fPedestal.push_back(pedestal->at(0));
    }else{
      for(uint i__ = 0; i__<pedestal->size(); i__++){
	detinfo.fDigInfo.fPedestal.push_back(pedestal->at(i__));
      }
    }
    if(pednoise->size()!=detinfo.NChan()){
      cout << "warning: number of pedestal noises in input (" << pednoise->size() 
	   << ") does not match number of channels (" << detinfo.NChan()
	   << ")" << endl;
      cout << "First pedestal noise entry used for all channels. " << endl
	   << "If you want one pedestal noise value per channel, fix your DB" << endl<< endl;
      detinfo.fDigInfo.fPedNoise.push_back(pednoise->at(0));
    }else{
      for(uint i__ = 0; i__<pednoise->size(); i__++){
	detinfo.fDigInfo.fPedNoise.push_back(pednoise->at(i__));
      }
    }
    
    if(threshold->size()!=detinfo.NChan()){
      cout << "warning: number of thresholds in input (" << gain->size() 
	   << ") does not match number of channels (" << detinfo.NChan()
	   << ")" << endl;
      cout << "First threshold entry used for all channels. " << endl
	   << "If you want one threshold value per channel, fix your DB" << endl<< endl;
      detinfo.fDigInfo.fThreshold.push_back(threshold->at(0));
    }else{
      for(uint i__ = 0; i__<threshold->size(); i__++){
	detinfo.fDigInfo.fGain.push_back(threshold->at(i__));
      }
    }
    
    delete gain;
    delete pedestal;
    delete pednoise;
    delete threshold;
  }  catch(...) {
    delete gain;
    delete pedestal;
    delete pednoise;
    delete threshold;
    //input.close();
    fclose(file);
    throw;
  }//end try / catch
    
  const string geoprefix = "geo."+prefix;
  
  if(detinfo.DetType()==kGEM || detinfo.DetType()==kScint){
    int nplanes;
    std::vector<int>* nmodules = 0;
    
    try{
      nmodules = new vector<int>;
      DBRequest request[] = {
	{"nplanes",        &nplanes,        kInt,     0, 0},
	{"nmodules",       nmodules,        kIntV,    0, 0},
	{ 0 }
      };
       
      if(fDebug>=3){
	cout << prefix.c_str() << endl;
      }
      
      //Int_t err = LoadDB (input, request, prefix);
      Int_t err = LoadDB (file, GetInitDate(), request, prefix.c_str());
      
      if (err){
	//input.close();
	fclose(file);
	return kInitError;
      }
      
      detinfo.SetNPlanes(nplanes);
      
      
      for(int i_pl = 0; i_pl<nplanes; i_pl++){
	detinfo.AddNModules(nmodules->at(i_pl));
	
	for(int i_mod = 0; i_mod<nmodules->at(i_pl); i_mod++){
	  TGeoInfo thisGeo;
	  int nrows;
	  int ncols;
	  double xsize;
	  double ysize;
	  double zpos;
	  
	  DBRequest request_geo[] = {
	    {"nrows",     &nrows,      kInt,    0, 1},
	    {"ncols",     &ncols,      kInt,    0, 1},
	    {"xsize",     &xsize,      kDouble, 0, 1},
	    {"ysize",     &ysize,      kDouble, 0, 1},
	    {"zpos",      &zpos,       kDouble, 0, 1},
	    { 0 }
	  };
	  
	  if(fDebug>=3){
	    cout << geoprefix.c_str() << endl;
	  }
	  
	  string geoprefix_ii = geoprefix;
	  if(nplanes>1) geoprefix_ii = geoprefix_ii+std::to_string(i_pl+1)+".";
	  if(nmodules->at(i_pl)>1) geoprefix_ii = geoprefix_ii+std::to_string(i_mod+1)+".";
	  
	  if(fDebug>=3){
	    cout << geoprefix_ii.c_str() << endl;
	  }
	  
	  //err = LoadDB (input, request_geo, geoprefix+"."+std::to_string(i_pl));
	  err = LoadDB (file, GetInitDate(), request_geo, geoprefix_ii.c_str());
	  if (err){
	    //input.close();
	    fclose(file);
	    return kInitError;
	  }
	  
	  
	  thisGeo.SetNRows(nrows);
	  thisGeo.SetNCols(ncols);
	  thisGeo.SetXSize(xsize);
	  thisGeo.SetYSize(ysize);
	  thisGeo.SetZPos(zpos);
	  
	  //detinfo.fGeoInfo.push_back(thisGeo);
	}
      }//end 
      delete nmodules;
    }  catch(...) {
      delete nmodules;
      //input.close();
      fclose(file);
      throw;
    }//end try / catch
    
  }else{
    TGeoInfo thisGeo;
    int nrows;
    int ncols;
    double xsize;
    double ysize;
    double zpos;
    
    DBRequest request_geo[] = {
      {"nrows",     &nrows,      kInt,    0, 1},
      {"ncols",     &ncols,      kInt,    0, 1},
      {"xsize",     &xsize,      kDouble, 0, 1},
      {"ysize",     &ysize,      kDouble, 0, 1},
      {"zpos",      &zpos,       kDouble, 0, 1},
      { 0 }
    };
    
    err = LoadDB (file, GetInitDate(), request_geo, geoprefix.c_str());
    if (err){
      //input.close();
      fclose(file);
      return kInitError;
    }
    
    thisGeo.SetNRows(nrows);
    thisGeo.SetNCols(ncols);
    thisGeo.SetXSize(xsize);
    thisGeo.SetYSize(ysize);
    thisGeo.SetZPos(zpos);
        
    //detinfo.fGeoInfo.push_back(thisGeo);
  }
  
   
  fDetInfo.push_back(detinfo);
  
  //input.close();
  fclose(file);
  return(kOK);
}

//function to retrieve the coorect detector information from the detector name
const TDetInfo & TSBSDBManager::GetDetInfo(const char* detname)
{
  //check if the detectors databases is loaded in the first place
  if(fDetInfo.size()==0){
    cout << "Detector info has not been loaded by the manager yet; Exiting." << endl;
    cout << "To avoid this, make sure you load the DB information with the DB manager before you declare any detector." << endl;
    exit(2);
  }
  
  // if so, loop on list of detectors.
  for(int i = 0; i<fDetInfo.size(); i++){
    if(fDetInfo.at(i).DetName().compare(detname)==0){
      return fDetInfo.at(i);
    }
  }
  
  // if no valid detectors have been found exit with error message
  cout << "No detector corresponding to " << detname << " found in database. Check program or database" << endl;
  exit(2);
}

/*
//______________________________________________________________
string TSBSDBManager::FindKey( ifstream& inp, const string& key )
{
  static const string empty("");
  string line;
  string::size_type keylen = key.size();
  inp.seekg(0); // could probably be more efficient, but it's fast enough
  while( getline(inp,line) ) {
    if( line.size() <= keylen )
      continue;
    if( line.compare(0,keylen,key) == 0 ) {
      if( keylen < line.size() ) {
	string::size_type pos = line.find_first_not_of(" \t=", keylen);
	if( pos != string::npos )
	  return line.substr(pos);
      }
      break;
    }
  }
  return empty;
}
//_________________________________________________________________
int TSBSDBManager::LoadDB( ifstream& inp, DBRequest* request, const string& prefix )
{
  DBRequest* item = request;
  while( item->name ) {
    ostringstream sn(prefix, ios_base::ate);
    sn << item->name;
    const string& key = sn.str();
    string val = FindKey(inp,key);
    if( !val.empty() ) {
      istringstream sv(val);
      switch(item->type){
        case kDouble:
          sv >> *((double*)item->var);
          break;
        case kInt:
          sv >> *((Int_t*)item->var);
          break;
        case kInt:
          sv >> *((Int_t*)item->var);
          break;
        case kString:
          sv >> *((string*)item->var);
          break;
        default:
          return 1;
        break;
      }
      if( !sv ) {
	cerr << "Error converting key/value = " << key << "/" << val << endl;
	return 1;
      }
    } else {
      cerr << "key \"" << key << "\" not found" << endl;
      return 2;
    }
    ++item;
  }
  return 0;
}
*/

/*
//______________________________________________________________
void TSBSDBManager::LoadGeneralInfo(const string& fileName)
{  
  ifstream input(fileName.c_str());
  if (!input.is_open()){
    cout<<"cannot find general information file "<<fileName
	<<". Exiting the program"<<endl;
        exit(0);
  }
  const string prefix = "generalinfo.";
  DBRequest request[] = {
    {"g4sbs_exptype",  &fg4sbsExpType , kInt,    0, 1},
    {"g4sbs_dettype",  &fg4sbsDetType , kInt,    0, 1},
    {"ndetectors",     &fNDetectors   , kInt,    0, 1},
    {"chan_per_slot",  &fChanPerSlot  , kInt,    0, 1},
    {"slot_per_crate", &fSlotPerCrate , kInt,    0, 1},
    {"nsignal",        &fNSigParticle , kInt,    0, 1},
    { 0 }
  };
  int pid, tid;
  DBRequest signalRequest[] = {
    {"pid",                 &pid,                   kInt, 0, 1},
    {"tid",                 &tid,                   kInt, 0, 1},
    { 0 }
  };
  int err = LoadDB( input, request,  prefix);
  
  if( err ) exit(2); 
  
  for (int i=0; i<fNSigParticle; i++){
    ostringstream signal_prefix(prefix, ios_base::ate);
    signal_prefix<<"signal"<<i+1<<".";
    
    err = LoadDB(input, signalRequest, signal_prefix.str());
    
    fSigPID.push_back(pid);
    fSigTID.push_back(tid);
    
    if( err ) exit(2); 
  }
  
  for (int i=0; i<fNDetectors; i++){
    vector<GeoInfo> thisInfo;
    thisInfo.clear();
    fGeoInfo = thisInfo;
  }
}


//______________________________________________________________
void TSBSDBManager::LoadGeoInfo(const string& prefix)
{
  // Include DB_DIR (standard Hall A analyzer DB path in the search)
  std::string path = "";
  if(std::getenv("DB_DIR")) {
    path = std::string(std::getenv("DB_DIR")) + "/";
  }
  const string& fileName = path+"db_"+prefix+".dat";
    
  ifstream input(fileName.c_str());
  if (!input.is_open()){
    cout<<"cannot find geometry file "<<fileName
	<<". Exiting the program"<<endl;
    exit(0);
  }
  
  GeoInfo thisGeo;
  
  DBRequest request[] = {
    {"zckov_in",     &thisGeo.fZCkovIn,      kDouble, 0, 1},
    {"n_radiator",   &thisGeo.fNradiator,    kDouble, 0, 1},
    {"l_radiator",   &thisGeo.fLradiator,    kDouble, 0, 1},
    {"npmts",        &thisGeo.fNPMTs,        kInt,    0, 1},
    {"npmtrows",     &thisGeo.fNPMTrows,     kInt,    0, 1},
    {"npmtcolsmax",  &thisGeo.fNPMTcolsMax,  kInt,    0, 1},
    {"pmtdistx",     &thisGeo.fPMTdistX,     kDouble, 0, 1},
    {"pmtdisty",     &thisGeo.fPMTdistY,     kDouble, 0, 1},
    {"x_tcpmt",      &thisGeo.fX_TCPMT,      kDouble, 0, 1},
    {"y_tcpmt",      &thisGeo.fY_TCPMT,      kDouble, 0, 1},
    { 0 }
  };
  
  for (int i=0; i<fNDetectors; i++){
    //map<int, vector<GeoInfo> >::iterator it = fGeoInfo.find(i);
    //if (it == fGeoInfo.end()) { cout<<"unexpected detector id "<<i<<endl; }
    ostringstream detector_prefix(prefix, ios_base::ate);
    detector_prefix<<".cher"<<i<<".";
    
    int err = LoadDB(input, request,detector_prefix.str());
    if( err ) exit(2);
    
    thisGeo.fPMTmatrixHext = (thisGeo.fNPMTcolsMax-1)*thisGeo.fPMTdistY;
    thisGeo.fPMTmatrixVext = (thisGeo.fNPMTrows-1)*thisGeo.fPMTdistX;
    
    fGeoInfo.push_back(thisGeo);
  }
  
  
}

//_____________________________________________________________________
const int & TSBSDBManager::GetSigPID(unsigned int i)
{
    if ( i >= fSigPID.size() ){ 
        cout<<"only "<<fSigPID.size()<<" signal particle registered"<<endl;
        return fErrID;
    }
    return fSigPID[i];
}
//______________________________________________________________________
const int & TSBSDBManager::GetSigTID(unsigned int i)
{
    if ( i >= fSigPID.size() ){ 
        cout<<"only "<<fSigPID.size()<<" signal particle registered"<<endl;
        return fErrID;
    }
    return fSigTID[i];
}

//_________________________________________________________________________
bool TSBSDBManager::CheckIndex(int i)
{
    if (i >= fNDetectors || i < 0){
        cout<<"invalid Detector ID requested: "<<i<<endl;
        return false;
    }

    return true;
}

//_________________________________________________________________________
const double & TSBSDBManager::GetZCkovIn(int i)
{
  if (!CheckIndex(i)) return fErrVal;
  return fGeoInfo.at(i).fZCkovIn;
}

//_________________________________________________________________________
const double & TSBSDBManager::GetNradiator(int i)
{
  if (!CheckIndex(i)) return fErrVal;
  return fGeoInfo.at(i).fNradiator;
}

//_________________________________________________________________________
const double & TSBSDBManager::GetLradiator(int i)
{
  if (!CheckIndex(i)) return fErrVal;
  return fGeoInfo.at(i).fLradiator;
}

//_________________________________________________________________________
const int & TSBSDBManager::GetNPMTs(int i)
{
  if (!CheckIndex(i)) return fErrID;
  return fGeoInfo.at(i).fNPMTs;
}

//_________________________________________________________________________
const int & TSBSDBManager::GetNPMTrows(int i)
{
  if (!CheckIndex(i)) return fErrID;
  return fGeoInfo.at(i).fNPMTrows;
}

//_________________________________________________________________________
const int & TSBSDBManager::GetNPMTcolsMax(int i)
{
  if (!CheckIndex(i)) return fErrID;
  return fGeoInfo.at(i).fNPMTcolsMax;
}

//_________________________________________________________________________
const double & TSBSDBManager::GetPMTmatrixHext(int i)
{
  if (!CheckIndex(i)) return fErrVal;
  return fGeoInfo.at(i).fPMTmatrixHext;
}

//_________________________________________________________________________
const double & TSBSDBManager::GetPMTmatrixVext(int i)
{
  if (!CheckIndex(i)) return fErrVal;
  return fGeoInfo.at(i).fPMTmatrixVext;
}

//_________________________________________________________________________
const double & TSBSDBManager::GetPMTdistX(int i)
{
  if (!CheckIndex(i)) return fErrVal;
  return fGeoInfo.at(i).fPMTdistX;
}

//_________________________________________________________________________
const double & TSBSDBManager::GetPMTdistY(int i)
{
  if (!CheckIndex(i)) return fErrVal;
  return fGeoInfo.at(i).fPMTdistY;
}

//_________________________________________________________________________
const double & TSBSDBManager::GetX_TCPMTs(int i)
{
  if (!CheckIndex(i)) return fErrVal;
  return fGeoInfo.at(i).fX_TCPMT;
}

//_________________________________________________________________________
const double & TSBSDBManager::GetY_TCPMTs(int i)
{
  if (!CheckIndex(i)) return fErrVal;
  return fGeoInfo.at(i).fY_TCPMT;
}
*/
