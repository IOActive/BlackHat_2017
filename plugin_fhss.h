/*
 *  Copyright (C) 2017, IOActive www.ioactive.com
 *
 *  This file is part of Plugin_FHSS Blackhat'17
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

typedef struct _CHANNEL{
    int index;
    unsigned int position;
    unsigned int offsetLow;
    unsigned int offsetHigh;
    float centerFreq;
    float timeLow;
    float timeHigh;
}CHANNEL, *PCHANNEL;


typedef struct _CAPTURE{
    FILE *file_input;
    float baseband;
    float bandwith;
    float rate;
    unsigned int block_size;
    unsigned int sample_rate;
    unsigned int nsamples;
    float center_freq;
    float abs_l_freq;
    float abs_h_freq;
    float freq_resolution;
    float time_duration;
    float complex *dataset;
}CAPTURE,*PCAPTURE;


typedef struct _REPORT{
    FILE *file_report;
    CAPTURE *capture_in;
} REPORT, *PREPORT;

typedef struct _PLUGIN_PARAMS{
    char *description;    
    void *custom_params;
} PLUGIN_PARAMS, *PPLUGIN_PARAMS;


///////// PLUGIN_HOPPING
#define MAX_FREQS   10
#define MAX_BLOCKS  30

typedef struct _HOPPING_BURST {
    int blockindex;
    float freqs[MAX_BLOCKS][MAX_FREQS];
    size_t bin_index[MAX_BLOCKS][MAX_FREQS];
} HOPPING_BURST, *PHOPPING_BURST;

typedef struct _PLUGIN_HOPPING {
    int nfft;
    float amplitude_peak;   //
    float amplitude_valley; 
    float psd_threshold;
    size_t first_block;
} PLUGIN_HOPPING, *PPLUGIN_HOPPING;


