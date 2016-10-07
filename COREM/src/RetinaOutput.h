#ifndef RETINAOUTPUT_H
#define RETINAOUTPUT_H

/* BeginDocumentation
 * Name: RetinaOutput
 *
 * Description: Special retina module in charge of managing the retina output.
 * In particular it can (instantly) convert the output into spike times and save
 * them in a file.
 *
 * Author: Pablo Martinez Cañada. University of Granada. CITIC-UGR. Spain.
 * <pablomc@ugr.es>
 * Author: Richard R. Carrillo. University of Granada. CITIC-UGR. Spain.
 *
 * SeeAlso: module
 */

#include <iostream>
#include <vector>

#include "module.h"

using namespace cimg_library;
using namespace std;

class RetinaOutput:public module{
protected:
    // image buffers
    CImg<double> *inputImage; // Buffer used to temporally store the input values which will be converted to spikes
    
    vector<double> out_spk_times;
    vector<double> out_spk_neurons;
    
    // conversion parameters
    double Max_freq, Min_freq; // Max. and min. number of spikes per second that a neuron can fire
    double Input_threshold; // Minimal (sustained) input value requireed for a neuron to generate some output
    double Spks_per_inp; // Conversion factor from input value to output spike frequency
    
    // membrane potential
    CImg<double> *last_spk_time, *last_refrac_end_time;

public:
    // Constructor, copy, destructor.
    RetinaOutput(int x=1,int y=1,double temporal_step=1.0);
    RetinaOutput(const RetinaOutput& copy);
    ~RetinaOutput(void);

    // Allocate values and set protected parameters
    virtual void allocateValues();
    virtual void setX(int x){sizeX=x;}
    virtual void setY(int y){sizeY=y;}

    RetinaOutput& set_Max_freq(double max_spk_freq);
    RetinaOutput& set_Min_freq(double min_spk_freq);
    RetinaOutput& set_Input_threshold(double input_threshold);
    RetinaOutput& set_Spks_per_inp(double freq_per_inp_unit);

    // New input and update of equations
    virtual void feedInput(const CImg<double> &new_input, bool isCurrent, int port);
    virtual void update();
    // set Parameters
    virtual bool setParameters(vector<double> params, vector<string> paramID);

    // Get output image (y(k))
    virtual CImg<double>* getOutput();
};

#endif // RETINAOUTPUT_H
