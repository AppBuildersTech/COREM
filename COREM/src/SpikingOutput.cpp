#include <algorithm> // for std::sort
#include <iostream>

// For file writing:
#include <fstream>
#include <iterator>
#include <string>

#include <random> // To generate neuron noise
#include <ctime> // To log the current time in output spike file
#include <limits>

#include "SpikingOutput.h"

SpikingOutput::SpikingOutput(int x, int y, double temporal_step, string output_filename):module(x,y,temporal_step){
    // input-to-spike-time conversion parameters (default values)
    Min_period=0.0; // Max. firing frequency is inifite Hz
    Longest_sustained_period=numeric_limits<double>::infinity(); // Neuron start firing from 0Hz
    Input_threshold=0; // Neuron start firing from just above 0 input
    Spk_freq_per_inp=1; // Neuron fires 1 spike/second (1Hz) when input is 1
    Spike_std_dev=0.0; // No stochastic output
    Min_period_std_dev=0.0; // Hard refractory period
    if(output_filename.compare("") != 0) 
        out_spk_filename=output_filename;
    else
        out_spk_filename="results/spikes.spk";

    // Save all input images by default
    Start_time=0.0;
    End_time=numeric_limits<double>::infinity();

    // Process al input pixels by default
    First_inp_ind=0;
    Inp_ind_inc=1;
    Total_inputs=~0UL; // Practcally infinite 
    
    Random_init=0.0; // Same initial state for all neurons
    
    normal_distribution<double>::param_type init_norm_dist_params(0.0, Min_period_std_dev/1000.0); // Random numbers from a Gaussian distribution of  mean=0, sigma=Noise_std_dev/1000 seconds
    norm_dist.param(init_norm_dist_params); // Set initial params of normal distribution
    
    uniform_real_distribution<double>::param_type init_unif_dist_params(0.0, 1.0); // Random numbers distributed uniformly between 0 and 1 (not included)
    unif_dist.param(init_unif_dist_params); // Set initial params of uniform distribution
    
    // Parameters of gamma distribution are set in inp_pixel_to_period()
    
    // Input buffer
    inputImage=new CImg<double> (sizeY, sizeX, 1, 1, 0);

    // Internal state variables: initial value
    last_firing_period=new CImg<double> (sizeY, sizeX, 1, 1, numeric_limits<double>::infinity()); // Previous input = 0 -> perdiod=inifinity
    next_spk_time=new CImg<double> (sizeY, sizeX, 1, 1, 0.0); // Next predicted spike time = 0s
}

SpikingOutput::SpikingOutput(const SpikingOutput &copy):module(copy){
    Min_period = copy.Min_period;
    Longest_sustained_period = copy.Longest_sustained_period;
    Input_threshold = copy.Input_threshold;
    Spk_freq_per_inp = copy.Spk_freq_per_inp;
    Spike_std_dev = copy.Spike_std_dev;
    Min_period_std_dev = copy.Min_period_std_dev;
    out_spk_filename = copy.out_spk_filename;
    Random_init = copy.Random_init;
    norm_dist = copy.norm_dist;
    unif_dist = copy.unif_dist;
    gam_dist = copy.gam_dist;

    inputImage=new CImg<double>(*copy.inputImage);
    last_firing_period=new CImg<double>(*copy.last_firing_period);
    next_spk_time=new CImg<double>(*copy.next_spk_time);
}

SpikingOutput::~SpikingOutput(){
    // Save generated spikes before destructing the object
    cout << "Saving output spike file: " << out_spk_filename << "... " << flush;
    cout << (SaveFile(out_spk_filename)?"Ok":"Fail") << endl;
    
    delete inputImage;
    delete last_firing_period;
    delete next_spk_time;
}

//------------------------------------------------------------------------------//

void SpikingOutput::randomize_state(){
    const double last_firing_per = 1.0; // This particular value is not relevant (if >0), since what matters is the resulting next_spk_time/last_firing_period ratio
    CImg<double>::iterator last_firing_period_it = last_firing_period->begin();
    CImg<double>::iterator next_spk_time_it = next_spk_time->begin();

    while(last_firing_period_it < last_firing_period->end()){ // For every spiking output
        // To randomize the state of each output, we set the last firing period
        // to 1 (that is, next_spk_time-last_spk_time = 1), and we choose
        // randomly the time at which each output fired last between 0 and 1.
        // In this way (next_spk_time-tslot_start) / last_firing_period is
        // a number between 0 and 1, and the first firing phase is random.
        *last_firing_period_it = last_firing_per;
        *next_spk_time_it = (1.0 - Random_init*unif_dist(rand_gen))*last_firing_per; // random number in the interval [0,1) seconds from unif. dist.
        last_firing_period_it++;
        next_spk_time_it++;
    }
}

//------------------------------------------------------------------------------//

bool SpikingOutput::allocateValues(){
    module::allocateValues(); // Use the allocateValues() method of the base class
    double last_per, last_spk;
    double first_spk_delay = 1; // Configure the delay of the first spike in proportion of the first period

    // Set parameters of distributions for random number generation
    normal_distribution<double>::param_type init_params(0.0, Min_period_std_dev/1000.0); // (mean=0, sigma=Limit_std_dev/1000 seconds)
    norm_dist.param(init_params); // Set initial params of normal distribution
    
    if(first_spk_delay == 0){ // No delay
        last_spk=0.0;
        last_per=numeric_limits<double>::infinity(); // For a 0 input the firing period is infinity
    } else { // The delay will be: t_first_spk = last_spk/last_per * first_firing_period
        last_spk=first_spk_delay;
        last_per=1.0;
    }
    // Resize initial value
    inputImage->assign(sizeY, sizeX, 1, 1, 0);
    last_firing_period->assign(sizeY, sizeX, 1, 1, last_per);
    next_spk_time->assign(sizeY, sizeX, 1, 1, last_spk);

    if(Random_init != 0) // If parameter Random_init is differnt from 0, init the state of outputs randomly
        randomize_state();
    return(true);
}

bool SpikingOutput::set_Min_period(double min_spk_per){
    bool ret_correct;
    if (min_spk_per>=0) {
        Min_period = min_spk_per;
        ret_correct=true;
    } else
        ret_correct=false;
    return(ret_correct); // Return value is used to inform the caller function if value has been set
}

bool SpikingOutput::set_Longest_sustained_period(double max_spk_per){
    bool ret_correct;
    if (max_spk_per>=0) {
        Longest_sustained_period = max_spk_per;
        ret_correct=true;
    } else
        ret_correct=false;
    return(ret_correct);
}

bool SpikingOutput::set_Input_threshold(double input_threshold){
    Input_threshold = input_threshold;
    return(true);
}

bool SpikingOutput::set_Freq_per_inp(double freq_per_inp_unit){
    Spk_freq_per_inp = freq_per_inp_unit;
    return(true);
}

bool SpikingOutput::set_Spike_std_dev(double std_dev_val){
    Spike_std_dev = std_dev_val;
    return(true);
}

bool SpikingOutput::set_Min_period_std_dev(double std_dev_val){
    bool ret_correct;
    if (std_dev_val>=0) {
        Min_period_std_dev = std_dev_val;
        ret_correct=true;
    } else
        ret_correct=false;
    return(ret_correct);
}

bool SpikingOutput::set_Start_time(double start_time){
    bool ret_correct;
    if (start_time>=0) {
        Start_time = start_time;
        ret_correct=true;
    } else
        ret_correct=false;
    return(ret_correct);
}

bool SpikingOutput::set_End_time(double end_time){
    bool ret_correct;
    if (end_time>=0) {
        End_time = end_time;
        ret_correct=true;
    } else
        ret_correct=false;
    return(ret_correct);
}

bool SpikingOutput::set_Random_init(double rnd_init){
    Random_init = rnd_init;
    return(true);
}

bool SpikingOutput::set_First_inp_ind(double first_input){
    bool ret_correct;
    if (first_input>=0) {
        First_inp_ind = first_input;
        ret_correct=true;
    } else
        ret_correct=false;
    return(ret_correct);
}

bool SpikingOutput::set_Inp_ind_inc(double input_inc){
    bool ret_correct;
    if (input_inc>=0) {
        Inp_ind_inc = input_inc;
        ret_correct=true;
    } else
        ret_correct=false;
    return(ret_correct);
}

bool SpikingOutput::set_Total_inputs(double num_inputs){
    bool ret_correct;
    if (num_inputs>=0) {
        Total_inputs = num_inputs;
        ret_correct=true;
    } else
        ret_correct=false;
    return(ret_correct);
}

//------------------------------------------------------------------------------//

bool SpikingOutput::setParameters(vector<double> params, vector<string> paramID){

    bool correct = true;

    for (vector<double>::size_type i = 0;i<params.size() && correct;i++){
        const char * s = paramID[i].c_str();

        if (strcmp(s,"Min_period")==0){
            correct = set_Min_period(params[i]);
        } else if (strcmp(s,"Longest_sustained_period")==0){
            correct = set_Longest_sustained_period(params[i]);
        } else if (strcmp(s,"Input_threshold")==0){
            correct = set_Input_threshold(params[i]);
        } else if (strcmp(s,"Freq_per_inp")==0){
            correct = set_Freq_per_inp(params[i]);
        } else if (strcmp(s,"Spike_std_dev")==0){
            correct = set_Spike_std_dev(params[i]);
        } else if (strcmp(s,"Min_period_std_dev")==0){
            correct = set_Min_period_std_dev(params[i]);
        } else if (strcmp(s,"Start_time")==0){
            correct = set_Start_time(params[i]);
        } else if (strcmp(s,"End_time")==0){
            correct = set_End_time(params[i]);
        } else if (strcmp(s,"Random_init")==0){
            correct = set_Random_init(params[i]);
        } else if (strcmp(s,"First_inp_ind")==0){
            correct = set_First_inp_ind(params[i]);
        } else if (strcmp(s,"Inp_ind_inc")==0){
            correct = set_Inp_ind_inc(params[i]);
        } else if (strcmp(s,"Total_inputs")==0){
            correct = set_Total_inputs(params[i]);
        } else
            correct = false;
    }

    return correct;
}

//------------------------------------------------------------------------------//

void SpikingOutput::feedInput(double sim_time, const CImg<double>& new_input, bool isCurrent, int port){
    // Ignore port type and copy input image
    if(simTime >= Start_time && simTime+step <= End_time) // Check if the user wants to generate output for the image at current time
        *inputImage = new_input;
    else
        inputImage->assign(); // Reset the input image to an empty image so that no spikes are generated during this sim. time step

    // Update the current simulation time
    simTime = sim_time;
}

double SpikingOutput::inp_pixel_to_period(double pixel_value){
    double firing_period_sec; // Calculated firing period in seconds
    double cur_min_period_sec; // Current (used for this calculation) firing period in seconds (refractory period)
    double max_period_sec; // Longest firind period: Offset firing period added to resultant firing period

    // Convert periods from ms to s:
    max_period_sec = Longest_sustained_period/1000.0;
    if(Min_period_std_dev > 0.0) // Soft Min_period limit chosen
        cur_min_period_sec = Min_period/1000.0 + norm_dist(rand_gen); // / sqrt(step/1000.0); //  Add Gaussian white noise. We divide by sqrt(step/1000) to make the noise independent of the time step length (As in DOI:10.1523/JNEUROSCI.3305-05.2005)
    else // No noise in freq limit: use hard limits
        cur_min_period_sec = Min_period/1000.0;


    if(pixel_value < Input_threshold) // If value under threshold, output activity is 0Hz
        firing_period_sec = numeric_limits<double>::infinity();
    else
        firing_period_sec = 1.0 / ((pixel_value-Input_threshold)*Spk_freq_per_inp + 1.0/max_period_sec);

    // If stochastic output is enabled, inp_pixel_to_period() draws the firing period from a gamma distribution as observed in rat ganglion cells: doi:10.1017/S095252380808067X
    if(Spike_std_dev != 0.0 && isfinite(firing_period_sec)) {
        double gam_k, gam_theta;
        double spk_variance_sec; // Variance of the interspike intervals in seconds
        
        // If the user set Spike_std_dev to a negative number, the gamma distribution variance is set to the current firing_period_sec.
        // The ratio of the variance to the mean is called the Fano factor in a distribution. So, if Spike_std_dev < 0, then
        // Fano actor=1, therefore periods are drawn from a Poisson distribution.
        if(Spike_std_dev > 0.0)
            spk_variance_sec = (Spike_std_dev/1000.0)*(Spike_std_dev/1000.0);
        else
            spk_variance_sec = firing_period_sec;

        // Set params of gamma distribution according to current firing period and specified std dev
        // mean firing rate=k*tetha variance=k*theta^2
        // Solving the above eq. for k and theta we get:
        gam_k = firing_period_sec*firing_period_sec/spk_variance_sec;
        gam_theta = spk_variance_sec/firing_period_sec;
        gamma_distribution<double>::param_type init_gam_dist_params(gam_k, gam_theta);
        gam_dist.param(init_gam_dist_params);

//cout << "mean_fir_per: " << firing_period_sec << " std_dev_fir_per: " << Spike_std_dev/1000 << " k: " << gam_k << " theta: " << gam_theta << " rnd per: " << endl;
        firing_period_sec = gam_dist(rand_gen);

    }

    // Firing rate is saturated
    if(firing_period_sec < cur_min_period_sec)
        firing_period_sec = cur_min_period_sec;

    return(firing_period_sec);
}

//------------------------------------------------------------------------------//
// function used to compare spikes according to time (and neuron index) when
// sorting in SpikingOutput::update()
bool spk_time_comp(spike_t spk1, spike_t spk2){
    bool comp_result;
    double time_diff;
    
    time_diff = spk2.time - spk1.time;
    if(time_diff > 0)
        comp_result=true; // spk1.time < spk2.time
    else if(time_diff < 0)
        comp_result=false; // spk1.time > spk2.timeinp_pix_per
    else // This subordering it is implemented just to ease the visual inspection of the output file
        comp_result = spk1.neuron < spk2.neuron;
    
    return(comp_result);
    }

// This method basically gerates spike times during current simulation time slot.
// For this, this method calculates the firing period (ISI) corresponding to the current input and
// generates 1 spike after each period.
// The extra complexity arises when calculating the time of the first spike of this series.
// If the previous input is the same as the current one, it is easy, we use the same period to space
// the spike series. But when the previous input is different from the current one, we calculate the
// time of the next spike taking into account: the firing period correspondind to the last input,
// the time of the spike that would be fired is the last input continued during this sim. slot, the 
// firing period correspondind to the current input, and the start of the current sim. slot.
// Considering these variables we calculate the ISI between the last series spike and the new series
// first spike. We do it in a way that is consistent with the previous and the current input.
// The intuitive interpretation of this calculation is that if a spike of the last series occurred
// very recently, it pushes forward the first spike of the current series (in order not to generate
// a very small ISI), especially if the firing period of the current input is large orthe firing
// period of the last input was small.
// The final outcome is that the resulting complete series of spikes is relatively smooth (if the
// input does not changes abruptly, of course) and it does not depend on the sim. slot times.
// One key point for this caculation is to consider what ISI we get when we get a zero input.
// In this implementation we consider that a zero input normally does not reset the pushing effect
// of the last elicited spike, however, another convection could be followed.
// Method precondition and postcondition: last_firing_period must not be zero,
// next_spk_time must not be infinite and next_spk_time higher or equals than current tslot_start.
void SpikingOutput::update(){
    unsigned long out_neu_idx; // Index to the current neuron (or image pixel)
    CImg<double>::iterator inp_img_it = inputImage->begin();
    CImg<double>::iterator last_firing_period_it = last_firing_period->begin();
    CImg<double>::iterator next_spk_time_it = next_spk_time->begin();
    vector<spike_t> slot_spks; // Temporal vector of output spikes for current sim. time slot

    out_neu_idx=0UL;
    inp_img_it+=First_inp_ind; // start from the pixl selected by user
    last_firing_period_it+=First_inp_ind;
    next_spk_time_it+=First_inp_ind;
    // For each input image pixel:
    while(inp_img_it<inputImage->end() && out_neu_idx<Total_inputs){ // we use inp_img_it.end() as upper bound for all iterators
        spike_t new_spk;
        // Intermediate variables used to calculate next spike time
        double inp_pix_per;
        double old_per, told_next_spk;
        double tslot_start, slot_len;

        // All calculations are done in whole units, so convert class time properties (in ms) into seconds
        tslot_start = simTime / 1000.0; // Start time of the current sim. slot: Convert it into seconds
        slot_len = step / 1000.0; // Length in time of a simulation slot (step)

        inp_pix_per = inp_pixel_to_period(*inp_img_it); // Convert input pixel magnitude into firing period in seconds
        // If input is not zero, we do not have to calculate spike times but the expression for updating
        // the next spike time becomes an indeterminate form, so evaluate its limit
        if(isfinite(inp_pix_per)) {

            // Firing period of the last non-zero input (in a previous sim. slot):
            old_per = *last_firing_period_it;
            // Time of the first spike that would be now generated if the current pixel magnitude were
            // the same as the previous one:
            told_next_spk = *next_spk_time_it;
            
            // To calculate the next spike time we consider how far the current slot start is from the next
            // predicted firing time of previous input (told_next_spk). If they are almost conincident,
            // the neuron was expected to fire at that time, so it can fire at the start of the slot.
            // If they are very far (in relation to the firing period), the neuron has just fired, then we delay
            // the firing up to the firing period for the current input (inp_pix_per).
            // This algorithm preverves the firing rate between slots if the input is constant.
            // The value of the fraction (told_next_spk-tslot_start)/told_next_spk should be between 0 and 1,
            // so the next spike should be emitted between the slot start and the slot start plus the current firing period (inp_pix_per)
            *next_spk_time_it = tslot_start + (told_next_spk-tslot_start) * inp_pix_per / old_per;
            *last_firing_period_it = inp_pix_per; // Update next_spk_time and last_firing_period (in case do not fire in this time step but in a following one)
//cout << "Pix: " << inp_img_it - inputImage->begin() << ": tn=" << *next_spk_time_it << " (t0=" << told_next_spk << " ts="<< tslot_start << " Tn=" << inp_pix_per << " To=" << old_per << ")" <<endl;
            new_spk.neuron = out_neu_idx; // Index of output neuron are assigned in the same way as Cimg pixel offsets
            new_spk.time = *next_spk_time_it;

            // These conditions should never be met:
            if(new_spk.time < tslot_start)
                cout << "Internal error: a spike for a previous simulation step has been generated. current step [" << tslot_start << "," << tslot_start + slot_len << ") spike time:" << new_spk.time << "s" << endl;
            if(!isfinite(*next_spk_time_it))
                cout << "Internal error: spike time could not be calculated (indeterminate form). current step [" << tslot_start << "," << tslot_start + slot_len << ") spike time:" << new_spk.time << endl;

            // We can have several spikes in a single simulation time slot
            for(;new_spk.time < tslot_start + slot_len;){
                slot_spks.push_back(new_spk);
                
                inp_pix_per = inp_pixel_to_period(*inp_img_it); // change value of inp_pix_per: this is usefull only if noise is activated
                if(isfinite(inp_pix_per)){
                    new_spk.time += inp_pix_per;
                    *next_spk_time_it = new_spk.time; // Update the time of predicted next firing for this neuron                
                    *last_firing_period_it = inp_pix_per; // Update the time of last firing for this neuron
//cout << "pix per " << inp_pix_per << ". step [" << tslot_start << "," << tslot_start + slot_len << ") spk:" << new_spk.time << endl;
                } else {
                    *next_spk_time_it = tslot_start + slot_len; // Since last spike prediction was in this sim. slot (new_spk.time), we add to the prediction the remaining silent (zero input) slot time
                    if(*last_firing_period_it == 0.0) // We must not allow that last_firing_period = 0, after exiting update() since it can cause and indeterminate form in the following spike time calculation expression
                        *last_firing_period_it=*next_spk_time_it-new_spk.time; // Use last predicted interspkie interval, this value is not relevant as next spike time will be tslot_start + slot_len
//cout << "ocurrio pix per " << inp_pix_per << ". step [" << tslot_start << "," << tslot_start + slot_len << ") spk:" << new_spk.time << endl;
                    break; // period=inf -> spike at infinite time: exit loop
                }
            }
        }
        else // Input is zero: we have to consider this special case apart. Otherwise, in the next sim. slot, next_spk_time and last_firing_period both would be inf
             // and would obtain an indeterminate form. Solving analytically the expression we get that next_spk_time must be delayed one simulation time step and 
             // and last_firing_period preserved, in this way a new prediction is correctly calculated the next time step
             // In short, we postpone the calculation of the next spike time to the next sim. slot with non-zero input.
            *next_spk_time_it = *next_spk_time_it + slot_len;
            
        // Switch to the next neuron (pixel)
        inp_img_it+=Inp_ind_inc;
        last_firing_period_it+=Inp_ind_inc;
        next_spk_time_it+=Inp_ind_inc;
        out_neu_idx++;
    }

    // Some programs may require that the spikes are issued in time order
    // So, sort the spikes of current sim. slot before inserting them in the class
    // output spike list
    std::sort(slot_spks.begin(), slot_spks.end(), spk_time_comp);
    out_spks.insert(out_spks.end(), slot_spks.begin(), slot_spks.end());
}

//------------------------------------------------------------------------------//

// Implement a method to write a spike_t object to a ouput stream
// This method is used in SpikingOutput::SaveFile()
ostream& operator<< (ostream& out, const spike_t& spk) {
    out << spk.neuron << " " << spk.time;
    return out;
}

// This function is executed when the class object is destructed to save
// the output activity (spikes) generated during the whole simulation
bool SpikingOutput::SaveFile(string spk_filename){
    bool ret_correct;
    
    ofstream out_spk_file(spk_filename, ios::out);
    ret_correct=out_spk_file.is_open();
    if(ret_correct){
        out_spk_file << "% Output activity file generated by COREM";
        // get current local time and log it on the output file
        time_t time_as_secs = time(NULL);
        if(time_as_secs != (time_t)-1){
            struct tm *time_local = localtime(&time_as_secs);
            if(time_local != NULL){
                out_spk_file << " on " << asctime(time_local); // This string includes \n
            }
            else
                out_spk_file << endl;
        }
        else
            out_spk_file << endl;
        out_spk_file << "% <neuron index from 0> <spike time in seconds>" << endl;
        ostream_iterator<spike_t> out_spk_it(out_spk_file, "\n"); // Add \n at the end of each line
        copy(out_spks.begin(), out_spks.end(), out_spk_it); // Write all spikes to file
    
        out_spk_file.close();
    }
    else
        cout << "Unable to open file for output spikes: " << spk_filename << endl;
  
    return(ret_correct);
}

//------------------------------------------------------------------------------//

// This function is neither needed nor used
CImg<double>* SpikingOutput::getOutput(){
    return inputImage;
}

//------------------------------------------------------------------------------//

bool SpikingOutput::isDummy() {
    return false;
    };