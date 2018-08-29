#ifndef TSBSSimADC_
#define TSBSSimADC_

/////////////////////////////////////////////////////////////////////
//
//   TSBSSimADC
//   G4SBS Simplified Simulated ADC Module
//
//   Right now, all it supports is an ADC sum and individual ADC samples
//
/////////////////////////////////////////////////////////////////////

#include "PipeliningModule.h"
#include "stdint.h"
#include <vector>

namespace Decoder {

  class TSBSSimADC : public PipeliningModule {   // Inheritance

  public:

    TSBSSimADC();                         // Default constructor
    TSBSSimADC(Int_t crate, Int_t slot);  // Constructor
    virtual ~TSBSSimADC();                // Virtual constructor

    // Use parent class functions
    using Module::GetData;
    using Module::LoadSlot;

    virtual void Clear(const Option_t *opt="");
    virtual void Init();
    virtual void CheckDecoderStatus() const;
    virtual void ClearDataVectors();
    virtual Int_t LoadSlot(THaSlotData *sldat, const UInt_t* evbuffer, const UInt_t *pstop);
    virtual Int_t LoadSlot(THaSlotData *sldat, const UInt_t* evbuffer, Int_t pos, Int_t len);

    // We don't need these functions for simulated data, but must be defined
    // so that this won't be an abstract class
    Int_t LoadNextEvBuffer(THaSlotData*) {};
    Int_t LoadThisBlock(THaSlotData*, std::vector<UInt_t >) {};
    Int_t Decode(const UInt_t *) { return 0; }; // use DecodeOneWord instead

    struct fadc_data_struct {
      std::vector<uint32_t> samples;
      std::vector<uint32_t> integrals;
      void clear() {
        samples.clear();
        integrals.clear();
      }
    };  // fadc_data_struct

  private:
    static const size_t NADCCHAN = 64; // Max ADC channels
    static TypeIter_t fgThisType;
    std::vector<fadc_data_struct> fadc_data;

    ClassDef(TSBSSimADC,0)  //  Generic SimADC module

  };  // TSBSSimADC class

}  // Decoder namespace

#endif
